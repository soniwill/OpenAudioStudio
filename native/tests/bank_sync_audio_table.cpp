#include "playback_fixture.h"

#include "../src/api/runtime.h"
#include "../src/core/bank.h"
#include "../src/studio/bank_reader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace oae::test;

namespace {

fs::path get_bankc_executable() {
    const std::vector<fs::path> candidates = {
        "oae_bankc.exe",
        "Debug/oae_bankc.exe",
        "Release/oae_bankc.exe",
        "../Debug/oae_bankc.exe",
        "../Release/oae_bankc.exe",
        "./oae_bankc"
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return fs::canonical(c);
        }
    }
    return {};
}

void write_json_file(const fs::path& path, const json& j) {
    std::ofstream os(path);
    REQUIRE(os.is_open());
    os << j.dump(2);
}

int run_bankc(const fs::path& exe, const fs::path& proj, const fs::path& out_bank) {
#if defined(_WIN32)
    const std::string full_cmd = "\"\"" + exe.string() + "\" \"" + proj.string() + "\" -o \"" + out_bank.string() + "\"\"";
#else
    const std::string full_cmd = "\"" + exe.string() + "\" \"" + proj.string() + "\" -o \"" + out_bank.string() + "\"";
#endif
    return std::system(full_cmd.c_str());
}

} // namespace

// Regression for the synchronous bank-load path (oae_studio_load_bank_file /
// oae_studio_load_bank_memory): the ModelSet reconstruction used to discard the
// audio_table, silencing Programmer instruments without a user callback.
TEST_CASE("Synchronous bank load preserves audio_table for Programmer playback") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bank_sync_audio_table";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "voice_line.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    json project = {
        {"version", 1},
        {"samples", json::array({{{"id", "voice_01"}, {"file", "audio/voice_line.wav"}}})},
        {"buses", json::array({{{"path", "bus:/vo"}, {"volume", 1.0}}})},
        {"audio_table", json::array({{{"key", "npc_greet"}, {"sample_id", "voice_01"}}})},
        {"events", json::array({
            {
                {"path", "event:/vo/greet"},
                {"kind", "event"},
                {"bus_path", "bus:/vo"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            {
                                {"kind", "programmer"},
                                {"offset_ms", 0.0},
                                {"length_ms", 500.0},
                                {"programmer_key", "npc_greet"}
                            }
                        })}
                    }
                })}
            }
        })}
    };

    const auto manifest = directory / "project.json";
    const auto bank_path = directory / "programmer.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank_path) == 0);

    // Load through the synchronous C API path (not BankReader directly).
    SystemFixture fix;
    oae_bank bank_handle = 0;
    REQUIRE(oae_studio_load_bank_file(fix.handle, bank_path.string().c_str(), &bank_handle) == OAE_OK);
    REQUIRE(bank_handle != 0);

    // The reconstructed ModelSet must contain the audio_table entries with
    // the sound handle rebound to the sound created by the load.
    const auto* loaded = oae::runtime.banks.get(bank_handle);
    REQUIRE(loaded != nullptr);
    const auto model_set = loaded->model_set();
    REQUIRE(model_set != nullptr);
    const auto* ev = model_set->find_event_by_path("event:/vo/greet");
    REQUIRE(ev != nullptr);
    REQUIRE(ev->tracks.size() == 1);
    REQUIRE(ev->tracks[0].instruments.size() == 1);
    CHECK(ev->tracks[0].instruments[0].kind == oae::InstrumentKind::Programmer);
    CHECK(ev->tracks[0].instruments[0].programmer_key == "npc_greet");

    const auto* entry = model_set->find_audio_table_entry("npc_greet");
    REQUIRE(entry != nullptr);
    CHECK(entry->sample_index == 0);
    CHECK(entry->sound_handle != 0);

    // Programmer without a user callback must resolve the key through the
    // audio_table and produce audible output.
    oae_event_description ev_desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/vo/greet", &ev_desc) == OAE_OK);
    REQUIRE(ev_desc != 0);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(ev_desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    std::array<float, 1024> pcm{};
    float max_peak = 0.0f;
    for (int frame = 0; frame < 10; ++frame) {
        oae_system_update(fix.handle);
        REQUIRE(oae_system_render(fix.handle, pcm.data(), 512) == OAE_OK);
        for (float s : pcm) {
            max_peak = std::max(max_peak, std::abs(s));
        }
    }
    CHECK(max_peak > 0.01f);

    oae_event_instance_release(inst);
    REQUIRE(oae_studio_unload_bank(fix.handle, bank_handle) == OAE_OK);
    fs::remove_all(directory, ec);
}
