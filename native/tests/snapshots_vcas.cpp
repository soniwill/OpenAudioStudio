#include "playback_fixture.h"
#include "../src/core/studio_model.h"
#include "../src/core/system.h"
#include "../src/core/vca.h"
#include "../src/api/runtime.h"

#include <cmath>
#include <vector>

using namespace oae::test;

namespace {

oae_sound make_sine_tone(oae_system system, double hz, double amp)
{
    constexpr std::uint32_t frames = 48000;
    const auto bytes = frames * 2;
    std::vector<std::uint8_t> wav{
        'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,
        0x80,0xBB,0,0,0,0x77,1,0,2,0,16,0,'d','a','t','a',0,0,0,0
    };
    for (unsigned i = 0; i < 4; ++i) {
        wav[4 + i] = static_cast<std::uint8_t>((bytes + 36) >> (i * 8));
        wav[40 + i] = static_cast<std::uint8_t>(bytes >> (i * 8));
    }
    wav.resize(44 + bytes);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const double s = amp * std::sin(2.0 * 3.14159265358979323846 * hz * i / 48000.0);
        const auto val = static_cast<std::int16_t>(32767.0 * s);
        wav[44 + 2 * i] = static_cast<std::uint8_t>(val);
        wav[45 + 2 * i] = static_cast<std::uint8_t>(val >> 8);
    }
    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), OAE_SOUND_LOOP, &sound) == OAE_OK);
    return sound;
}

double compute_rms(const float* data, std::size_t size)
{
    if (size == 0) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return std::sqrt(sum / static_cast<double>(size));
}

}

TEST_CASE("T11: VCA volume scaling and strict fader preservation")
{
    SystemFixture fix;

    // Create bus:/music
    oae_bus music_bus = 0;
    REQUIRE(oae_bus_create(fix.handle, "music", 0, &music_bus) == OAE_OK);
    REQUIRE(oae_bus_set_volume(music_bus, 0.8f) == OAE_OK);

    // Register a ModelSet with a VCA
    oae::ModelSetBuilder b;
    oae::VcaModel vca_mod;
    vca_mod.path = "vca:/music";
    vca_mod.bus_paths.push_back("bus:/music");
    vca_mod.default_volume = 1.0f;
    b.add_vca(vca_mod);

    auto mset = b.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(mset);

    // Acquire VCA handle
    oae_vca vca = 0;
    REQUIRE(oae_studio_get_vca(fix.handle, "vca:/music", &vca) == OAE_OK);
    REQUIRE(vca != 0);

    float read_vca_vol = -1.0f;
    REQUIRE(oae_vca_get_volume(vca, &read_vca_vol) == OAE_OK);
    CHECK(read_vca_vol == 1.0f);

    // Play tone on music_bus
    auto snd = make_sine_tone(fix.handle, 440.0, 0.8);
    oae_channel ch = 0;
    REQUIRE(oae_system_play_sound(fix.handle, snd, music_bus, false, &ch) == OAE_OK);

    // Render baseline
    std::vector<float> buf(2048 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const double rms_baseline = compute_rms(buf.data(), buf.size());
    CHECK(rms_baseline > 0.05);

    // Set VCA volume to 0.5
    REQUIRE(oae_vca_set_volume(vca, 0.5f) == OAE_OK);
    REQUIRE(oae_vca_get_volume(vca, &read_vca_vol) == OAE_OK);
    CHECK(read_vca_vol == 0.5f);

    // Crucial Invariant: Bus fader volume must remain untouched!
    float bus_vol = 0.0f;
    REQUIRE(oae_bus_get_volume(music_bus, &bus_vol) == OAE_OK);
    CHECK(bus_vol == 0.8f);

    // Render with VCA = 0.5 (let de-click settle)
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_vca_half = compute_rms(buf.data(), buf.size());

    // RMS should be halved (+/- 5%)
    const double expected_rms = rms_baseline * 0.5;
    CHECK(std::abs(rms_vca_half - expected_rms) / expected_rms < 0.05);

    // Bus fader still exactly 0.8f
    REQUIRE(oae_bus_get_volume(music_bus, &bus_vol) == OAE_OK);
    CHECK(bus_vol == 0.8f);

    // Clean up
    oae_channel_stop(ch);
    oae_sound_release(snd);
}

TEST_CASE("T11: Snapshot DuckMusic relative dB attenuation and intensity")
{
    SystemFixture fix;

    // Create bus:/music
    oae_bus music_bus = 0;
    REQUIRE(oae_bus_create(fix.handle, "music", 0, &music_bus) == OAE_OK);

    // Build ModelSet with Snapshot "snapshot:/DuckMusic"
    oae::ModelSetBuilder b;

    oae::SnapshotTarget target;
    target.bus_path = "bus:/music";
    target.param_id = -1; // Bus volume
    target.value = -9.0f; // -9 dB

    oae::EventBuilder snap_builder("snapshot:/DuckMusic");
    snap_builder.kind(oae::EventKind::Snapshot)
                .snapshot_mode(oae::SnapshotMode::Override)
                .intensity_param("intensity")
                .fade_in_ms(0)
                .stop_fadeout_ms(0)
                .priority(100)
                .add_parameter(oae::ParameterDefinition::continuous("intensity", 0.0f, 100.0f, 100.0f))
                .add_snapshot_target(target);

    b.add_event(snap_builder);
    auto mset = b.build();

    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(mset);

    // Play tone on bus:/music
    auto snd = make_sine_tone(fix.handle, 440.0, 1.0);
    oae_channel ch = 0;
    REQUIRE(oae_system_play_sound(fix.handle, snd, music_bus, false, &ch) == OAE_OK);

    // Render baseline (0 dB delta)
    std::vector<float> buf(2048 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const double rms_base = compute_rms(buf.data(), buf.size());
    CHECK(rms_base > 0.1);

    // Instantiate and start Snapshot at 100% intensity
    oae_event_description snap_desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "snapshot:/DuckMusic", &snap_desc) == OAE_OK);

    oae_event_instance snap_inst = 0;
    REQUIRE(oae_event_description_create_instance(snap_desc, &snap_inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(snap_inst) == OAE_OK);

    // Update system to compose snapshot
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_9db = compute_rms(buf.data(), buf.size());

    // -9 dB corresponds to factor 10^(-9/20) ~ 0.3548
    const double expected_factor_9db = std::pow(10.0, -9.0 / 20.0);
    const double expected_rms_9db = rms_base * expected_factor_9db;
    CHECK(std::abs(rms_9db - expected_rms_9db) / expected_rms_9db < 0.05);

    // Now change intensity to 50% -> delta should become -4.5 dB
    REQUIRE(oae_event_instance_set_parameter_by_name(snap_inst, "intensity", 50.0f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_4_5db = compute_rms(buf.data(), buf.size());

    // -4.5 dB corresponds to factor 10^(-4.5/20) ~ 0.5956
    const double expected_factor_4_5db = std::pow(10.0, -4.5 / 20.0);
    const double expected_rms_4_5db = rms_base * expected_factor_4_5db;
    CHECK(std::abs(rms_4_5db - expected_rms_4_5db) / expected_rms_4_5db < 0.05);

    // Stop snapshot -> original volume restored (0 dB delta)
    REQUIRE(oae_event_instance_stop(snap_inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_restored = compute_rms(buf.data(), buf.size());
    CHECK(std::abs(rms_restored - rms_base) / rms_base < 0.05);

    oae_event_instance_release(snap_inst);
    oae_channel_stop(ch);
    oae_sound_release(snd);
}

TEST_CASE("T11: Dual OVERRIDE snapshots resolve by priority and restore upon stop")
{
    SystemFixture fix;

    oae_bus music_bus = 0;
    REQUIRE(oae_bus_create(fix.handle, "music", 0, &music_bus) == OAE_OK);

    oae::ModelSetBuilder b;

    // Snapshot LowPriority (-12 dB, priority 100)
    oae::SnapshotTarget target_low;
    target_low.bus_path = "bus:/music";
    target_low.param_id = -1;
    target_low.value = -12.0f;

    oae::EventBuilder low_builder("snapshot:/LowPrio");
    low_builder.kind(oae::EventKind::Snapshot)
               .snapshot_mode(oae::SnapshotMode::Override)
               .priority(100)
               .fade_in_ms(0)
               .stop_fadeout_ms(0)
               .add_snapshot_target(target_low);
    b.add_event(low_builder);

    // Snapshot HighPriority (-6 dB, priority 200)
    oae::SnapshotTarget target_high;
    target_high.bus_path = "bus:/music";
    target_high.param_id = -1;
    target_high.value = -6.0f;

    oae::EventBuilder high_builder("snapshot:/HighPrio");
    high_builder.kind(oae::EventKind::Snapshot)
                .snapshot_mode(oae::SnapshotMode::Override)
                .priority(200)
                .fade_in_ms(0)
                .stop_fadeout_ms(0)
                .add_snapshot_target(target_high);
    b.add_event(high_builder);

    auto mset = b.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(mset);

    auto snd = make_sine_tone(fix.handle, 440.0, 1.0);
    oae_channel ch = 0;
    REQUIRE(oae_system_play_sound(fix.handle, snd, music_bus, false, &ch) == OAE_OK);

    std::vector<float> buf(2048 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const double rms_base = compute_rms(buf.data(), buf.size());

    oae_event_description desc_low = 0, desc_high = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "snapshot:/LowPrio", &desc_low) == OAE_OK);
    REQUIRE(oae_studio_get_event(fix.handle, "snapshot:/HighPrio", &desc_high) == OAE_OK);

    oae_event_instance inst_low = 0, inst_high = 0;
    REQUIRE(oae_event_description_create_instance(desc_low, &inst_low) == OAE_OK);
    REQUIRE(oae_event_description_create_instance(desc_high, &inst_high) == OAE_OK);

    // Start Low Priority (-12 dB)
    REQUIRE(oae_event_instance_start(inst_low) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_low = compute_rms(buf.data(), buf.size());
    const double expected_low = rms_base * std::pow(10.0, -12.0 / 20.0);
    CHECK(std::abs(rms_low - expected_low) / expected_low < 0.05);

    // Start High Priority (-6 dB, priority 200 > 100) -> High priority MUST override!
    REQUIRE(oae_event_instance_start(inst_high) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_high = compute_rms(buf.data(), buf.size());
    const double expected_high = rms_base * std::pow(10.0, -6.0 / 20.0);
    CHECK(std::abs(rms_high - expected_high) / expected_high < 0.05);

    // Stop High Priority -> Low Priority should now resume control (-12 dB)!
    REQUIRE(oae_event_instance_stop(inst_high, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_low_resumed = compute_rms(buf.data(), buf.size());
    CHECK(std::abs(rms_low_resumed - expected_low) / expected_low < 0.05);

    // Stop Low Priority -> fully restored to baseline!
    REQUIRE(oae_event_instance_stop(inst_low, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 512) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2048) == OAE_OK);
    const double rms_final = compute_rms(buf.data(), buf.size());
    CHECK(std::abs(rms_final - rms_base) / rms_base < 0.05);

    oae_event_instance_release(inst_low);
    oae_event_instance_release(inst_high);
    oae_channel_stop(ch);
    oae_sound_release(snd);
}

TEST_CASE("T11: VCA API parameter validation and error precedence")
{
    SystemFixture fix;

    oae_vca vca = 0;
    // Null pointer output -> INVALID_PARAM
    CHECK(oae_studio_get_vca(fix.handle, "vca:/test", nullptr) == OAE_ERR_INVALID_PARAM);
    // Invalid path prefix -> INVALID_PARAM
    CHECK(oae_studio_get_vca(fix.handle, "invalid_prefix", &vca) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_studio_get_vca(fix.handle, "", &vca) == OAE_ERR_INVALID_PARAM);
    // Invalid handle -> INVALID_HANDLE
    CHECK(oae_studio_get_vca(0x99999999, "vca:/test", &vca) == OAE_ERR_INVALID_HANDLE);

    // Not found -> NOT_FOUND
    CHECK(oae_studio_get_vca(fix.handle, "vca:/non_existent", &vca) == OAE_ERR_NOT_FOUND);

    // Invalid VCA handle operations
    float vol = 0.0f;
    CHECK(oae_vca_set_volume(0, 0.5f) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_vca_get_volume(0, &vol) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_vca_get_volume(0x12345678, nullptr) == OAE_ERR_INVALID_PARAM);

    // Range checks
    CHECK(oae_vca_set_volume(0x12345678, -0.1f) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_vca_set_volume(0x12345678, 1.1f) == OAE_ERR_INVALID_PARAM);
}
