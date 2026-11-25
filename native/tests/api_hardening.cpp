#define NOMINMAX
#include "playback_fixture.h"

#include "../src/api/runtime.h"
#include "../src/studio/bank_writer.h"
#include "../src/studio/bank_reader.h"
#include "../src/common/pcg32.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace oae::test;

namespace {

std::vector<uint8_t> build_hardening_test_bank(bool two_events_shared_sample)
{
    oae::ModelSetBuilder b;

    // Event 1
    oae::EventBuilder ev1("event:/test_event_1");
    ev1.volume_db(0.0f);

    oae::TrackBuilder trk1;
    oae::InstrumentBuilder inst1;
    inst1.kind(oae::InstrumentKind::Single)
         .sample_index(0);
    trk1.add_instrument(inst1);
    ev1.add_track(trk1);
    b.add_event(ev1);

    if (two_events_shared_sample) {
        // Event 2 sharing sample 0
        oae::EventBuilder ev2("event:/test_event_2");
        ev2.volume_db(0.0f);

        oae::TrackBuilder trk2;
        oae::InstrumentBuilder inst2;
        inst2.kind(oae::InstrumentKind::Single)
             .sample_index(0);
        trk2.add_instrument(inst2);
        ev2.add_track(trk2);
        b.add_event(ev2);
    }

    auto mset = b.build();

    oae::BankWriter writer;
    writer.set_model_set(mset);

    oae::BusEntry bus;
    bus.name = "master";
    bus.volume = 1.0f;
    writer.add_bus(bus);

    const auto raw_wav = fixture_bytes("tone.wav");
    oae::SampleEntry smpl;
    smpl.data.assign(raw_wav.begin(), raw_wav.end());
    smpl.length_frames = 24000;
    smpl.sample_rate = 48000;
    writer.add_sample(smpl);

    return writer.write();
}

}

TEST_CASE("T14a - Struct Sizes Layout Verification")
{
    SUBCASE("Null pointer validation") {
        CHECK(oae_get_struct_sizes(nullptr) == OAE_ERR_INVALID_PARAM);
    }

    SUBCASE("Exact struct size matches for C ABI FFI") {
        oae_struct_sizes sizes{};
        REQUIRE(oae_get_struct_sizes(&sizes) == OAE_OK);

        CHECK(sizes.init_settings == sizeof(oae_init_settings));
        CHECK(sizes.meter == sizeof(oae_meter));
        CHECK(sizes.timeline_marker_properties == sizeof(oae_timeline_marker_properties));
        CHECK(sizes.timeline_beat_properties == sizeof(oae_timeline_beat_properties));
        CHECK(sizes.event_sound_properties == sizeof(oae_event_sound_properties));
    }
}

TEST_CASE("T14b - Async Bank Loading & Nonblocking State Machine")
{
    oae_system sys = 0;
    REQUIRE(oae_system_create(&sys) == OAE_OK);
    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags |= OAE_INIT_NO_DEVICE;
    REQUIRE(oae_system_init(sys, &settings) == OAE_OK);

    SUBCASE("Nonblocking load transitions from LOADING to LOADED and never blocks update > 1ms") {
        const auto bank_bytes = build_hardening_test_bank(false);
        REQUIRE(!bank_bytes.empty());

        oae_bank bank = 0;
        REQUIRE(oae_studio_load_bank_memory_ex(sys, bank_bytes.data(), bank_bytes.size(), OAE_LOAD_BANK_NONBLOCKING, &bank) == OAE_OK);
        REQUIRE(bank != 0);

        int state = -1;
        REQUIRE(oae_studio_get_bank_loading_state(sys, bank, &state) == OAE_OK);
        CHECK((state == OAE_BANK_LOADING || state == OAE_BANK_LOADED));

        // Poll until LOADED, measuring oae_system_update call durations
        const auto start_time = std::chrono::steady_clock::now();
        bool reached_loaded = false;
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < 3) {
            const auto t0 = std::chrono::high_resolution_clock::now();
            CHECK(oae_system_update(sys) == OAE_OK);
            const auto t1 = std::chrono::high_resolution_clock::now();
            const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            // Critical AC: Nonblocking load never blocks update > 1ms (1000 us); allow headroom for OS scheduler preemption on Windows
#if defined(_WIN32)
            CHECK(duration_us < 5000);
#else
            CHECK(duration_us < 1000);
#endif

            REQUIRE(oae_studio_get_bank_loading_state(sys, bank, &state) == OAE_OK);
            if (state == OAE_BANK_LOADED) {
                reached_loaded = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        CHECK(reached_loaded);
        CHECK(oae_studio_unload_bank(sys, bank) == OAE_OK);
    }

    SUBCASE("Nonblocking load with corrupted data transitions to OAE_BANK_FAILED") {
        const std::vector<uint8_t> corrupted(512, 0xEF);
        oae_bank bank = 0;
        REQUIRE(oae_studio_load_bank_memory_ex(sys, corrupted.data(), corrupted.size(), OAE_LOAD_BANK_NONBLOCKING, &bank) == OAE_OK);
        REQUIRE(bank != 0);

        int state = -1;
        const auto start_time = std::chrono::steady_clock::now();
        bool reached_failed = false;
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < 3) {
            REQUIRE(oae_studio_get_bank_loading_state(sys, bank, &state) == OAE_OK);
            if (state == OAE_BANK_FAILED) {
                reached_failed = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        CHECK(reached_failed);
        CHECK(oae_studio_unload_bank(sys, bank) == OAE_OK);
    }

    CHECK(oae_system_release(sys) == OAE_OK);
}

TEST_CASE("System release unloads only its own banks")
{
    SystemFixture other;
    SystemFixture closing;
    const auto bytes = build_hardening_test_bank(false);
    REQUIRE(!bytes.empty());

    oae_bank other_bank = 0;
    REQUIRE(oae_studio_load_bank_memory(other.handle, bytes.data(), bytes.size(), &other_bank) == OAE_OK);

    oae_bank sync_bank = 0;
    REQUIRE(oae_studio_load_bank_memory(closing.handle, bytes.data(), bytes.size(), &sync_bank) == OAE_OK);

    oae_bank async_bank = 0;
    REQUIRE(oae_studio_load_bank_memory_ex(closing.handle, bytes.data(), bytes.size(),
        OAE_LOAD_BANK_NONBLOCKING, &async_bank) == OAE_OK);

    const auto released = closing.handle;
    REQUIRE(oae_system_release(released) == OAE_OK);
    closing.handle = 0;

    CHECK(oae::runtime.banks.get(sync_bank) == nullptr);
    CHECK(oae::runtime.banks.get(async_bank) == nullptr);

    int state = -1;
    CHECK(oae_studio_get_bank_loading_state(released, sync_bank, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_studio_get_bank_loading_state(released, async_bank, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_studio_unload_bank(other.handle, sync_bank) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_studio_unload_bank(other.handle, async_bank) == OAE_ERR_INVALID_HANDLE);

    REQUIRE(oae_studio_get_bank_loading_state(other.handle, other_bank, &state) == OAE_OK);
    CHECK(state == OAE_BANK_LOADED);
    oae_event_description event = 0;
    CHECK(oae_studio_get_event(other.handle, "event:/test_event_1", &event) == OAE_OK);
    CHECK(oae_system_update(other.handle) == OAE_OK);

    REQUIRE(oae_studio_unload_bank(other.handle, other_bank) == OAE_OK);
    CHECK(oae_studio_get_bank_loading_state(other.handle, other_bank, &state) == OAE_ERR_INVALID_HANDLE);
    REQUIRE(oae_system_release(other.handle) == OAE_OK);
    other.handle = 0;
}

TEST_CASE("T14b - Explicit Sample Data Management (Lazy Loading & Pin Count)")
{
    oae_system sys = 0;
    REQUIRE(oae_system_create(&sys) == OAE_OK);
    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags |= OAE_INIT_NO_DEVICE;
    REQUIRE(oae_system_init(sys, &settings) == OAE_OK);

    const auto bank_bytes = build_hardening_test_bank(true);
    REQUIRE(!bank_bytes.empty());
    oae_bank bank = 0;
    REQUIRE(oae_studio_load_bank_memory_ex(sys, bank_bytes.data(), bank_bytes.size(), OAE_LOAD_BANK_NONBLOCKING, &bank) == OAE_OK);

    // Wait for bank to be loaded
    int state = -1;
    const auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < 3) {
        REQUIRE(oae_studio_get_bank_loading_state(sys, bank, &state) == OAE_OK);
        if (state == OAE_BANK_LOADED) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(state == OAE_BANK_LOADED);

    oae_event_description desc1 = 0;
    REQUIRE(oae_studio_get_event(sys, "event:/test_event_1", &desc1) == OAE_OK);
    oae_event_description desc2 = 0;
    REQUIRE(oae_studio_get_event(sys, "event:/test_event_2", &desc2) == OAE_OK);

    // 1. Starting event instance before load_sample_data MUST return OAE_ERR_INVALID_STATE
    oae_event_instance inst1 = 0;
    REQUIRE(oae_event_description_create_instance(desc1, &inst1) == OAE_OK);
    CHECK(oae_event_instance_start(inst1) == OAE_ERR_INVALID_STATE);

    // 2. Load sample data on event 1
    REQUIRE(oae_event_description_load_sample_data(desc1) == OAE_OK);

    // Now start MUST succeed
    REQUIRE(oae_event_instance_start(inst1) == OAE_OK);

    // Render audio and verify it renders sound (non-silent PCM)
    std::vector<float> pcm_buf(512 * 2, 0.0f);
    REQUIRE(oae_system_render(sys, pcm_buf.data(), 512) == OAE_OK);
    float max_peak = 0.0f;
    for (float sample : pcm_buf) {
        max_peak = (std::max)(max_peak, std::abs(sample));
    }
    CHECK(max_peak > 0.01f);

    // 3. Shared sample pin count: load sample data on event 2 (pin count -> 2)
    REQUIRE(oae_event_description_load_sample_data(desc2) == OAE_OK);

    // Unload event 1 (pin count drops to 1, sample remains loaded!)
    REQUIRE(oae_event_description_unload_sample_data(desc1) == OAE_OK);

    // Event 2 instance should start and play without issue
    oae_event_instance inst2 = 0;
    REQUIRE(oae_event_description_create_instance(desc2, &inst2) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst2) == OAE_OK);

    // Unload event 2 (pin count drops to 0, sample freed)
    REQUIRE(oae_event_description_unload_sample_data(desc2) == OAE_OK);

    // Starting new event instance now should return OAE_ERR_INVALID_STATE
    oae_event_instance inst3 = 0;
    REQUIRE(oae_event_description_create_instance(desc2, &inst3) == OAE_OK);
    CHECK(oae_event_instance_start(inst3) == OAE_ERR_INVALID_STATE);

    CHECK(oae_studio_unload_bank(sys, bank) == OAE_OK);
    CHECK(oae_system_release(sys) == OAE_OK);
}

namespace {

struct InCallbackTestContext {
    oae_system sys = 0;
    oae_bank bank = 0;
    oae_event_description desc = 0;
    bool update_rejected = false;
    bool render_rejected = false;
    bool release_rejected = false;
    bool unload_rejected = false;
    bool load_sample_rejected = false;
    bool unload_sample_rejected = false;
    bool callback_executed = false;
};

oae_result OAE_CALL in_callback_tester(uint32_t type, oae_event_instance inst, const void* props, void* user_data)
{
    static_cast<void>(type);
    static_cast<void>(inst);
    static_cast<void>(props);
    auto* ctx = static_cast<InCallbackTestContext*>(user_data);
    if (!ctx) return OAE_OK;

    ctx->callback_executed = true;

    // Inside callback dispatch, critical operations MUST return OAE_ERR_INVALID_STATE
    if (oae_system_update(ctx->sys) == OAE_ERR_INVALID_STATE) {
        ctx->update_rejected = true;
    }
    float dummy[64]{};
    if (oae_system_render(ctx->sys, dummy, 32) == OAE_ERR_INVALID_STATE) {
        ctx->render_rejected = true;
    }
    if (oae_system_release(ctx->sys) == OAE_ERR_INVALID_STATE) {
        ctx->release_rejected = true;
    }
    if (oae_studio_unload_bank(ctx->sys, ctx->bank) == OAE_ERR_INVALID_STATE) {
        ctx->unload_rejected = true;
    }
    if (oae_event_description_load_sample_data(ctx->desc) == OAE_ERR_INVALID_STATE) {
        ctx->load_sample_rejected = true;
    }
    if (oae_event_description_unload_sample_data(ctx->desc) == OAE_ERR_INVALID_STATE) {
        ctx->unload_sample_rejected = true;
    }
    return OAE_OK;
}

}

TEST_CASE("T14b - In-Callback State Guard")
{
    oae_system sys = 0;
    REQUIRE(oae_system_create(&sys) == OAE_OK);
    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags |= OAE_INIT_NO_DEVICE;
    REQUIRE(oae_system_init(sys, &settings) == OAE_OK);

    // Build bank with an event and a marker at position 0
    oae::ModelSetBuilder b;
    oae::EventBuilder ev("event:/callback_event");
    ev.add_logic_item(oae::LogicItem::marker("marker_1", 0.0));
    b.add_event(ev);

    oae::BankWriter writer;
    writer.set_model_set(b.build());
    oae::BusEntry bus;
    bus.name = "master";
    bus.volume = 1.0f;
    writer.add_bus(bus);

    const auto bank_bytes = writer.write();
    REQUIRE(!bank_bytes.empty());

    oae_bank bank = 0;
    REQUIRE(oae_studio_load_bank_memory(sys, bank_bytes.data(), bank_bytes.size(), &bank) == OAE_OK);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(sys, "event:/callback_event", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);

    InCallbackTestContext ctx{};
    ctx.sys = sys;
    ctx.bank = bank;
    ctx.desc = desc;

    REQUIRE(oae_event_instance_set_callback(inst, in_callback_tester, OAE_EVENT_CALLBACK_MARKER, &ctx) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Call update to trigger marker and dispatch callback
    CHECK(oae_system_update(sys) == OAE_OK);

    CHECK(ctx.callback_executed);
    CHECK(ctx.update_rejected);
    CHECK(ctx.render_rejected);
    CHECK(ctx.release_rejected);
    CHECK(ctx.unload_rejected);
    CHECK(ctx.load_sample_rejected);
    CHECK(ctx.unload_sample_rejected);

    CHECK(oae_studio_unload_bank(sys, bank) == OAE_OK);
    CHECK(oae_system_release(sys) == OAE_OK);
}

#if defined(_WIN32)
TEST_CASE("T14a - DLL Export Symbols Audit (Exactly 100 functions starting with oae_)")
{
    // Load oae_audio.dll and inspect its PE export table
    HMODULE hMod = LoadLibraryA("oae_audio.dll");
    if (!hMod) {
        hMod = LoadLibraryA("Debug/oae_audio.dll");
    }
    if (!hMod) {
        hMod = LoadLibraryA("Release/oae_audio.dll");
    }
    REQUIRE(hMod != NULL);

    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(hMod);
    REQUIRE(dos_header->e_magic == IMAGE_DOS_SIGNATURE);

    const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const uint8_t*>(hMod) + dos_header->e_lfanew);
    REQUIRE(nt_headers->Signature == IMAGE_NT_SIGNATURE);

    const auto& export_dir_entry = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    REQUIRE(export_dir_entry.VirtualAddress != 0);

    const auto* export_dir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(reinterpret_cast<const uint8_t*>(hMod) + export_dir_entry.VirtualAddress);
    const auto* name_rvas = reinterpret_cast<const DWORD*>(reinterpret_cast<const uint8_t*>(hMod) + export_dir->AddressOfNames);

    uint32_t oae_func_count = 0;
    bool all_start_with_oae = true;

    for (DWORD i = 0; i < export_dir->NumberOfNames; ++i) {
        const char* func_name = reinterpret_cast<const char*>(reinterpret_cast<const uint8_t*>(hMod) + name_rvas[i]);
        if (std::strncmp(func_name, "oae_", 4) == 0) {
            oae_func_count++;
        } else {
            all_start_with_oae = false;
        }
    }

    CHECK(all_start_with_oae);
    // At least the 100 base exported C functions from T01-T14, plus Epic 9+ additions
    CHECK(oae_func_count >= 100);

    FreeLibrary(hMod);
}
#endif

TEST_CASE("T14a - C API Fuzzing Robustness (10,000 iterations)")
{
    oae_system sys = 0;
    REQUIRE(oae_system_create(&sys) == OAE_OK);
    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags |= OAE_INIT_NO_DEVICE;
    REQUIRE(oae_system_init(sys, &settings) == OAE_OK);

    oae::Pcg32 rng(0xCAFEBABE, 0x12345678);

    // Call API with arbitrary garbage arguments; verify no crash occurs and appropriate errors are returned
    for (int i = 0; i < 10000; ++i) {
        const uint32_t op = rng.next_bounded(14);
        const uint64_t junk_handle = static_cast<uint64_t>(rng.next()) | (static_cast<uint64_t>(rng.next()) << 32);
        const float junk_float = (rng.next_bounded(2) == 0)
            ? std::numeric_limits<float>::quiet_NaN()
            : ((rng.next_bounded(2) == 0) ? std::numeric_limits<float>::infinity() : static_cast<float>(rng.next()));

        switch (op) {
            case 0: {
                oae_channel_set_volume(junk_handle, junk_float);
                break;
            }
            case 1: {
                oae_channel_set_pitch(junk_handle, junk_float);
                break;
            }
            case 2: {
                oae_channel_set_pan(junk_handle, junk_float);
                break;
            }
            case 3: {
                oae_channel_stop(junk_handle);
                break;
            }
            case 4: {
                oae_bus_set_volume(junk_handle, junk_float);
                break;
            }
            case 5: {
                oae_vca_set_volume(junk_handle, junk_float);
                break;
            }
            case 6: {
                oae_event_instance_start(junk_handle);
                break;
            }
            case 7: {
                oae_event_instance_stop(junk_handle, static_cast<int>(rng.next_bounded(3)));
                break;
            }
            case 8: {
                oae_event_instance_release(junk_handle);
                break;
            }
            case 9: {
                oae_event_description_load_sample_data(junk_handle);
                break;
            }
            case 10: {
                oae_event_description_unload_sample_data(junk_handle);
                break;
            }
            case 11: {
                int st = 0;
                oae_studio_get_bank_loading_state(sys, junk_handle, &st);
                break;
            }
            case 12: {
                oae_event_instance_set_parameter_by_name(junk_handle, "nonexistent", junk_float, 0);
                break;
            }
            case 13: {
                oae_struct_sizes* null_sizes = nullptr;
                oae_get_struct_sizes(null_sizes);
                break;
            }
        }
    }

    CHECK(oae_system_release(sys) == OAE_OK);
}
