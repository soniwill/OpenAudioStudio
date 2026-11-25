#include "playback_fixture.h"
#include "../src/studio/bank_reader.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

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

TEST_CASE("T13: Bank compiler CLI end-to-end compilation, publication, catalog and playback")
{
    const fs::path bankc_exe = get_bankc_executable();
    REQUIRE(!bankc_exe.empty());

    const fs::path temp_dir = fs::temp_directory_path() / "oae_test_bankc_e2e";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    // Copy fixture wav to temp dir
    const fs::path audio_src = fs::path(fixture_path("tone.wav"));
    const fs::path audio_dst = temp_dir / "laser.wav";
    fs::copy_file(audio_src, audio_dst, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    // 1. Build valid project.json
    json proj;
    proj["version"] = 1;
    proj["samples"] = json::array({
        { {"id", "laser_sfx"}, {"file", "laser.wav"} }
    });
    proj["buses"] = json::array({
        { {"path", "bus:/sfx"}, {"volume", 0.8f} }
    });
    proj["global_parameters"] = json::array({
        { {"name", "Intensity"}, {"kind", "continuous"}, {"min", 0.0f}, {"max", 100.0f}, {"default_value", 50.0f} }
    });
    proj["events"] = json::array({
        {
            {"path", "event:/weapons/laser"},
            {"kind", "event"},
            {"bus_path", "bus:/sfx"},
            {"volume_db", -2.0f},
            {"pitch_semitones", 1.0f},
            {"tracks", json::array({
                {
                    {"volume_db", 0.0f},
                    {"instruments", json::array({
                        {
                            {"kind", "single"},
                            {"sample_id", "laser_sfx"},
                            {"offset_ms", 0.0},
                            {"length_ms", 300.0},
                            {"loop", false}
                        }
                    })}
                }
            })},
            {"logic_items", json::array({
                { {"kind", "marker"}, {"name", "hit"}, {"position_ms", 150.0} }
            })},
            {"automations", json::array({
                {
                    {"target", "volume"},
                    {"is_timeline", true},
                    {"curve", {
                        {"points", json::array({
                            { {"x", 0.0f}, {"y", 0.0f}, {"shape", "linear"} },
                            { {"x", 300.0f}, {"y", -12.0f}, {"shape", "ease_out"} }
                        })}
                    }}
                }
            })}
        },
        {
            {"path", "snapshot:/duck_sfx"},
            {"kind", "snapshot"},
            {"priority", 150},
            {"snapshot_mode", "override"},
            {"fade_in_ms", 100.0},
            {"snapshot_targets", json::array({
                {
                    {"bus_path", "bus:/sfx"},
                    {"value", -6.0f},
                    {"param_id", -1},
                    {"position", 0},
                    {"effect_index", 0}
                }
            })}
        }
    });
    proj["vcas"] = json::array({
        {
            {"path", "vca:/weapons"},
            {"buses", json::array({"bus:/sfx"})},
            {"events", json::array({"event:/weapons/laser"})}
        }
    });

    const fs::path proj_path = temp_dir / "project.json";
    write_json_file(proj_path, proj);

    const fs::path bank_path = temp_dir / "weapons.bank";
    const fs::path catalog_path = temp_dir / "weapons.bank.catalog.json";

    // 2. Run compiler
    const int ret = run_bankc(bankc_exe, proj_path, bank_path);
    CHECK(ret == 0);

    // 3. Verify publication artifacts exist
    CHECK(fs::exists(bank_path));
    CHECK(fs::exists(catalog_path));
    if (fs::exists(temp_dir / ".bankc-stage")) for (const auto& entry : fs::recursive_directory_iterator(temp_dir / ".bankc-stage")) {
        CHECK(entry.path().filename() != "journal.json");
        CHECK(entry.path().filename() != "bank.new");
    }

    // 4. Verify catalog contents
    std::ifstream cat_file(catalog_path);
    REQUIRE(cat_file.is_open());
    json cat_json;
    cat_file >> cat_json;
    CHECK(cat_json["version"] == 1);
    CHECK(cat_json["bank_size"] == fs::file_size(bank_path));
    CHECK(cat_json["events"].is_array());
    CHECK(cat_json["events"].size() == 2);

    // 5. Load generated bank and play audio
    SystemFixture fix;
    oae_bank bank_handle = 0;
    const auto load_res = oae_studio_load_bank_file(fix.handle, bank_path.string().c_str(), &bank_handle);
    REQUIRE(load_res == OAE_OK);
    REQUIRE(bank_handle != 0);

    oae_event_description laser_desc = 0;
    CHECK(oae_studio_get_event(fix.handle, "event:/weapons/laser", &laser_desc) == OAE_OK);
    REQUIRE(laser_desc != 0);

    oae_event_instance laser_inst = 0;
    CHECK(oae_event_description_create_instance(laser_desc, &laser_inst) == OAE_OK);
    REQUIRE(laser_inst != 0);
    CHECK(oae_event_instance_start(laser_inst) == OAE_OK);

    // Render 10 blocks and verify audio is produced
    std::array<float, 1024> pcm_buf{};
    float max_peak = 0.0f;
    for (int frame = 0; frame < 10; ++frame) {
        oae_system_update(fix.handle);
        CHECK(oae_system_render(fix.handle, pcm_buf.data(), 512) == OAE_OK);
        for (float s : pcm_buf) {
            max_peak = std::max(max_peak, std::abs(s));
        }
    }
    CHECK(max_peak > 0.01f);

    // Unload bank
    CHECK(oae_studio_unload_bank(fix.handle, bank_handle) == OAE_OK);
    fs::remove_all(temp_dir, ec);
}

TEST_CASE("T13: Bank compiler diagnostics on invalid references and cycles")
{
    const fs::path bankc_exe = get_bankc_executable();
    REQUIRE(!bankc_exe.empty());

    const fs::path temp_dir = fs::temp_directory_path() / "oae_test_bankc_errors";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    const fs::path out_bank = temp_dir / "err.bank";

    // 1. Missing audio file
    {
        json p;
        p["version"] = 1;
        p["samples"] = json::array({
            { {"id", "missing_sfx"}, {"file", "non_existent_audio.wav"} }
        });
        const fs::path p_path = temp_dir / "missing_file.json";
        write_json_file(p_path, p);

        const int ret = run_bankc(bankc_exe, p_path, out_bank);
        CHECK(ret != 0);
    }

    // 2. Missing sample in instrument
    {
        json p;
        p["version"] = 1;
        p["events"] = json::array({
            {
                {"path", "event:/test"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            { {"kind", "single"}, {"sample_id", "ghost_sample"} }
                        })}
                    }
                })}
            }
        });
        const fs::path p_path = temp_dir / "ghost_sample.json";
        write_json_file(p_path, p);

        const int ret = run_bankc(bankc_exe, p_path, out_bank);
        CHECK(ret != 0);
    }

    // 3. Nested event cycle: A -> B -> A
    {
        json p;
        p["version"] = 1;
        p["events"] = json::array({
            {
                {"path", "event:/a"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            { {"kind", "event"}, {"target_event", "event:/b"} }
                        })}
                    }
                })}
            },
            {
                {"path", "event:/b"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            { {"kind", "event"}, {"target_event", "event:/a"} }
                        })}
                    }
                })}
            }
        });
        const fs::path p_path = temp_dir / "cycle.json";
        write_json_file(p_path, p);

        const int ret = run_bankc(bankc_exe, p_path, out_bank);
        CHECK(ret != 0);
    }

    // 4. Missing target marker in transition
    {
        json p;
        p["version"] = 1;
        p["events"] = json::array({
            {
                {"path", "event:/transition_bad"},
                {"logic_items", json::array({
                    { {"kind", "transition_marker"}, {"target_marker", "unexisting_marker"}, {"position_ms", 100.0} }
                })}
            }
        });
        const fs::path p_path = temp_dir / "bad_marker.json";
        write_json_file(p_path, p);

        const int ret = run_bankc(bankc_exe, p_path, out_bank);
        CHECK(ret != 0);
    }

    // 5. Missing bus in event
    {
        json p;
        p["version"] = 1;
        p["events"] = json::array({
            {
                {"path", "event:/bad_bus"},
                {"bus_path", "bus:/does_not_exist"}
            }
        });
        const fs::path p_path = temp_dir / "bad_bus.json";
        write_json_file(p_path, p);

        const int ret = run_bankc(bankc_exe, p_path, out_bank);
        CHECK(ret != 0);
    }

    fs::remove_all(temp_dir, ec);
}

TEST_CASE("T13: Publication preserves invalid destinations and rolls back catalog")
{
    const auto bankc_exe = get_bankc_executable();
    REQUIRE(!bankc_exe.empty());
    const auto temp_dir = fs::temp_directory_path() / "oae_test_bankc_publication";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    REQUIRE(fs::create_directories(temp_dir));
    const auto project = temp_dir / "project.json";
    const auto bank = temp_dir / "output.bank";
    const auto catalog = temp_dir / "output.bank.catalog.json";
    write_json_file(project, json{{"version", 1}});

    SUBCASE("Bank destination directory is preserved and new catalog removed") {
        REQUIRE(fs::create_directory(bank));
        CHECK(run_bankc(bankc_exe, project, bank) != 0);
        CHECK(fs::is_directory(bank));
        CHECK(!fs::exists(catalog));
    }

    SUBCASE("Bank replacement failure restores previous catalog") {
        REQUIRE(fs::create_directory(bank));
        const json previous{{"previous", "catalog preserved exactly"}};
        write_json_file(catalog, previous);
        CHECK(run_bankc(bankc_exe, project, bank) != 0);
        CHECK(fs::is_directory(bank));
        std::ifstream input(catalog);
        REQUIRE(input.is_open());
        const std::string actual((std::istreambuf_iterator<char>(input)), {});
        CHECK(actual == previous.dump(2));
    }

    SUBCASE("Catalog destination directory is preserved without publishing bank") {
        REQUIRE(fs::create_directory(catalog));
        CHECK(run_bankc(bankc_exe, project, bank) != 0);
        CHECK(fs::is_directory(catalog));
        CHECK(!fs::exists(bank));
    }

    SUBCASE("Existing bank and catalog can be replaced") {
        REQUIRE(run_bankc(bankc_exe, project, bank) == 0);
        write_json_file(project, json{{"version", 1}, {"events", json::array({{{"path", "event:/replacement"}}})}});
        REQUIRE(run_bankc(bankc_exe, project, bank) == 0);
        std::ifstream input(catalog);
        REQUIRE(input.is_open());
        const auto published = json::parse(input);
        CHECK(published["events"].size() == 1);
        CHECK(published["events"][0]["path"] == "event:/replacement");
        CHECK(published["bank_size"] == fs::file_size(bank));
    }

    if (fs::exists(temp_dir / ".bankc-stage")) for (const auto& entry : fs::recursive_directory_iterator(temp_dir / ".bankc-stage")) {
        CHECK(entry.path().filename() != "journal.json");
        CHECK(entry.path().filename() != "bank.new");
    }
    fs::remove_all(temp_dir, ec);
}

TEST_CASE("T13: Complete authoring example survives compilation and rejects invalid edits")
{
    const auto bankc = get_bankc_executable();
    REQUIRE(!bankc.empty());
    const auto source_root = fs::path(fixture_path("tone.wav")).parent_path().parent_path().parent_path().parent_path();
    std::ifstream example(source_root / "docs/examples/project-v1.proposed.json");
    REQUIRE(example.is_open());
    json project;
    example >> project;
    const auto directory = fs::temp_directory_path() / "oae_test_complete_authoring";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "assets");
    fs::copy_file(fixture_path("tone.wav"), directory / "assets/tone.wav");
    const auto manifest = directory / "project.json";
    const auto bank = directory / "demo.bank";
    const auto catalog = directory / "demo.bank.catalog.json";
    project["events"][1]["user_props"] = {{"category", "demo"}};
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);
    const auto bytes = [](const fs::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
    };
    const auto original_bank = bytes(bank);
    const auto original_catalog = bytes(catalog);
    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse(original_bank.data(), original_bank.size(), parsed) == OAE_OK);
    const auto* demo = parsed.model_set->find_event_by_path("event:/demo");
    REQUIRE(demo != nullptr);
    REQUIRE(demo->tracks.size() == 2);
    REQUIRE(demo->tracks[0].instruments.size() == 1);
    const auto& multi = demo->tracks[0].instruments[0];
    CHECK(multi.kind == oae::InstrumentKind::Multi);
    CHECK(multi.playlist.size() == 2);
    CHECK(multi.condition.terms.size() == 1);
    CHECK(multi.modulators.size() == 1);
    CHECK(demo->parameters[0].labels.size() == 2);
    CHECK(demo->automations[0].param.name == "RPM");
    CHECK(demo->user_props.size() == 1);
    CHECK(demo->logic_items[2].position_ms == 100);
    CHECK(demo->logic_items[2].end_ms == 150);
    CHECK(parsed.model_set->effect_automations().size() == 1);

    const auto reject = [&](json invalid) {
        write_json_file(manifest, invalid);
        CHECK(run_bankc(bankc, manifest, bank) != 0);
        CHECK(bytes(bank) == original_bank);
        CHECK(bytes(catalog) == original_catalog);
    };
    auto invalid = project;
    invalid["events"][1]["tracks"][0]["instruments"][0]["playlist"][0]["weight"] = 0;
    reject(invalid);
    invalid = project;
    invalid["events"][1]["tracks"][0]["instruments"][0]["playlist"][1]["target_event"] = "event:/demo";
    reject(invalid);
    invalid = project;
    invalid["events"][1]["automations"][0]["param"]["scope"] = "local";
    reject(invalid);
    invalid = project;
    invalid["events"][1]["automations"][0]["curve"]["points"][1]["x"] = 0;
    reject(invalid);
    invalid = project;
    invalid["events"][1]["modulators"][0]["range"] = 49;
    reject(invalid);
    invalid = project;
    invalid["buses"][0]["effects"][0]["params"][0] = -1;
    reject(invalid);
    invalid = project;
    invalid["mixer"]["master"]["mute"] = true;
    reject(invalid);
    invalid = project;
    invalid["events"][1]["unknown"] = 1;
    reject(invalid);
    invalid = project;
    invalid["samples"][0]["file"] = "../tone.wav";
    reject(invalid);

    auto migrated = project;
    auto& automation = migrated["events"][1]["automations"][0];
    automation.erase("param"); automation["param_name"] = "RPM";
    auto& random = migrated["events"][1]["modulators"][0];
    random.erase("range"); random["min"] = -2; random["max"] = 2;
    write_json_file(manifest, migrated);
    CHECK(run_bankc(bankc, manifest, bank) == 0);
    migrated["events"][1]["modulators"][0]["min"] = -1;
    reject(migrated);
    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler round-trips Scatterer instruments with SCTR chunk") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_scatterer_roundtrip";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "grain.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    json project = {
        {"version", 1},
        {"samples", json::array({{{"id", "grain"}, {"file", "audio/grain.wav"}}})},
        {"buses", json::array({{{"path", "bus:/sfx"}, {"volume", 1.0}}})},
        {"events", json::array({
            {
                {"path", "event:/sfx/rain"},
                {"kind", "event"},
                {"bus_path", "bus:/sfx"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            {
                                {"kind", "scatterer"},
                                {"offset_ms", 0.0},
                                {"length_ms", 2000.0},
                                {"playlist_mode", "random"},
                                {"playlist_scope", "instance"},
                                {"spawn_interval_min_ms", 60.0},
                                {"spawn_interval_max_ms", 140.0},
                                {"polyphony", 3},
                                {"total_sounds", 5},
                                {"min_scatter_distance", 2.0},
                                {"max_scatter_distance", 12.0},
                                {"playlist", json::array({
                                    {{"sample_id", "grain"}, {"weight", 1.0}}
                                })}
                            }
                        })}
                    }
                })}
            }
        })}
    };

    const auto manifest = directory / "project.json";
    const auto bank = directory / "scatterer.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);

    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse_file(bank, parsed) == OAE_OK);
    const auto* ev = parsed.model_set->find_event_by_path("event:/sfx/rain");
    REQUIRE(ev != nullptr);
    REQUIRE(ev->tracks.size() == 1);
    REQUIRE(ev->tracks[0].instruments.size() == 1);
    const auto& sct = ev->tracks[0].instruments[0];
    CHECK(sct.kind == oae::InstrumentKind::Scatterer);
    CHECK(sct.spawn_interval_min_ms == doctest::Approx(60.0));
    CHECK(sct.spawn_interval_max_ms == doctest::Approx(140.0));
    CHECK(sct.polyphony == 3);
    CHECK(sct.total_sounds == 5);
    CHECK(sct.min_scatter_distance == doctest::Approx(2.0f));
    CHECK(sct.max_scatter_distance == doctest::Approx(12.0f));
    CHECK(sct.playlist.size() == 1);

    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler round-trips Programmer instruments and audio_table") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_programmer_roundtrip";
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
    const auto bank = directory / "programmer.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);

    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse_file(bank, parsed) == OAE_OK);
    const auto* ev = parsed.model_set->find_event_by_path("event:/vo/greet");
    REQUIRE(ev != nullptr);
    REQUIRE(ev->tracks.size() == 1);
    REQUIRE(ev->tracks[0].instruments.size() == 1);
    const auto& prg = ev->tracks[0].instruments[0];
    CHECK(prg.kind == oae::InstrumentKind::Programmer);
    CHECK(prg.programmer_key == "npc_greet");

    const auto* entry = parsed.model_set->find_audio_table_entry("npc_greet");
    REQUIRE(entry != nullptr);
    CHECK(entry->sample_index == 0);

    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler rejects invalid audio_table entries") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_programmer_invalid";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "voice_line.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    const auto write_and_compile = [&](const json& audio_table) {
        json project = {
            {"version", 1},
            {"samples", json::array({{{"id", "voice_01"}, {"file", "audio/voice_line.wav"}}})},
            {"audio_table", audio_table},
            {"events", json::array()}
        };
        const auto manifest = directory / "project.json";
        const auto bank = directory / "invalid.bank";
        write_json_file(manifest, project);
        return run_bankc(bankc, manifest, bank);
    };

    CHECK(write_and_compile(json::array({{{"key", ""}, {"sample_id", "voice_01"}}})) != 0);
    CHECK(write_and_compile(json::array({
        {{"key", "dup"}, {"sample_id", "voice_01"}},
        {{"key", "dup"}, {"sample_id", "voice_01"}}
    })) != 0);
    CHECK(write_and_compile(json::array({{{"key", "missing_sample"}, {"sample_id", "does_not_exist"}}})) != 0);

    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler parses async instrument field and defaults omitted to false") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_async_roundtrip";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "tone.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    json project = {
        {"version", 1},
        {"samples", json::array({{{"id", "tone_01"}, {"file", "audio/tone.wav"}}})},
        {"events", json::array({
            {
                {"path", "event:/sfx/async_test"},
                {"kind", "event"},
                {"tracks", json::array({
                    {
                        {"volume_db", 0.0},
                        {"instruments", json::array({
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 0.0},
                                {"length_ms", 500.0},
                                {"loop", true},
                                {"async", true}
                            },
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 500.0},
                                {"length_ms", 500.0}
                            }
                        })}
                    }
                })}
            }
        })}
    };

    const auto manifest = directory / "project.json";
    const auto bank = directory / "async.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);

    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse_file(bank, parsed) == OAE_OK);
    const auto* ev = parsed.model_set->find_event_by_path("event:/sfx/async_test");
    REQUIRE(ev != nullptr);
    REQUIRE(ev->tracks.size() == 1);
    REQUIRE(ev->tracks[0].instruments.size() == 2);
    CHECK(ev->tracks[0].instruments[0].async == true);
    CHECK(ev->tracks[0].instruments[1].async == false);

    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler transitions resolve canonical marker at runtime") {
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_transitions_runtime";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "tone.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    json project = {
        {"version", 1},
        {"samples", json::array({{{"id", "tone_01"}, {"file", "audio/tone.wav"}}})},
        {"events", json::array({
            {
                {"path", "event:/music/tm_jump"},
                {"kind", "event"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 0.0},
                                {"length_ms", 10000.0},
                                {"loop", true}
                            }
                        })}
                    }
                })},
                {"logic_items", json::array({
                    {
                        {"kind", "transition_marker"},
                        {"position_ms", 200.0},
                        {"target_marker", "chorus"},
                        {"quantization", "immediate"},
                        {"crossfade_ms", 50.0}
                    },
                    {
                        {"kind", "marker"},
                        {"name", "chorus"},
                        {"position_ms", 4000.0}
                    }
                })}
            },
            {
                {"path", "event:/music/tr_bar_jump"},
                {"kind", "event"},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 0.0},
                                {"length_ms", 12000.0},
                                {"loop", true}
                            }
                        })}
                    }
                })},
                {"logic_items", json::array({
                    {
                        {"kind", "tempo"},
                        {"position_ms", 0.0},
                        {"bpm", 120.0},
                        {"ts_upper", 4},
                        {"ts_lower", 4}
                    },
                    {
                        {"kind", "transition_region"},
                        {"start_ms", 0.0},
                        {"length_ms", 3000.0},
                        {"target_marker", "outro"},
                        {"quantization", "bar"},
                        {"crossfade_ms", 0.0}
                    },
                    {
                        {"kind", "marker"},
                        {"name", "outro"},
                        {"position_ms", 8000.0}
                    }
                })}
            }
        })}
    };

    const auto manifest = directory / "project.json";
    const auto bank = directory / "transitions.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);

    SystemFixture fix;
    oae_bank bank_handle = 0;
    REQUIRE(oae_studio_load_bank_file(fix.handle, bank.string().c_str(), &bank_handle) == OAE_OK);

    {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(fix.handle, "event:/music/tm_jump", &desc) == OAE_OK);
        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        render(fix.handle, 12000);
        REQUIRE(oae_system_update(fix.handle) == OAE_OK);
        int32_t pos_ms = 0;
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 4000);
        oae_event_instance_release(inst);
    }

    {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(fix.handle, "event:/music/tr_bar_jump", &desc) == OAE_OK);
        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        render(fix.handle, 48000);
        REQUIRE(oae_system_update(fix.handle) == OAE_OK);
        int32_t pos_ms = 0;
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms < 8000);

        render(fix.handle, 48000);
        REQUIRE(oae_system_update(fix.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms >= 8000);
        oae_event_instance_release(inst);
    }

    CHECK(oae_studio_unload_bank(fix.handle, bank_handle) == OAE_OK);
    fs::remove_all(directory, ec);
}

TEST_CASE("Bank compiler round-trips parameter modifiers and parameter sheets")
{
    const auto bankc = get_bankc_executable();
    REQUIRE_FALSE(bankc.empty());

    const auto directory = fs::temp_directory_path() / "oae_bankc_params_psht";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory / "audio", ec);

    const auto wav_path = directory / "audio" / "tone.wav";
    fs::copy_file(fs::path(fixture_path("tone.wav")), wav_path, fs::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);

    json project = {
        {"version", 1},
        {"samples", json::array({{{"id", "tone_01"}, {"file", "audio/tone.wav"}}})},
        {"global_parameters", json::array({
            {
                {"name", "Intensity"},
                {"kind", "continuous"},
                {"min", 0.0},
                {"max", 100.0},
                {"default_value", 20.0},
                {"seek_speed", 15.0},
                {"seek_speed_down", 5.0},
                {"velocity", 12.5},
                {"hold_during_playback", true}
            },
            {
                {"name", "State"},
                {"kind", "labeled"},
                {"labels", json::array({"Idle", "Combat"})},
                {"default_value", 0},
                {"hold_during_playback", true}
            }
        })},
        {"events", json::array({
            {
                {"path", "event:/gameplay/engine"},
                {"kind", "event"},
                {"parameters", json::array({
                    {
                        {"name", "RPM"},
                        {"kind", "discrete"},
                        {"min", 0},
                        {"max", 8},
                        {"default_value", 2},
                        {"seek_speed", 4.0},
                        {"velocity", -1.5},
                        {"hold_during_playback", false}
                    }
                })},
                {"user_props", {{"notes", "Engine loop"}, {"tags", "vehicle,sfx"}}},
                {"tracks", json::array({
                    {
                        {"instruments", json::array({
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 0.0},
                                {"length_ms", 2000.0},
                                {"sheet_parameter", "RPM"},
                                {"param_min", 2.0},
                                {"param_max", 6.0}
                            },
                            {
                                {"kind", "single"},
                                {"sample_id", "tone_01"},
                                {"offset_ms", 0.0},
                                {"length_ms", 2000.0},
                                {"sheet_parameter", "Intensity"},
                                {"param_min", 10.0},
                                {"param_max", 50.0}
                            }
                        })}
                    }
                })}
            }
        })}
    };

    const auto manifest = directory / "project.json";
    const auto bank = directory / "params_psht.bank";
    write_json_file(manifest, project);
    REQUIRE(run_bankc(bankc, manifest, bank) == 0);

    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse_file(bank, parsed) == OAE_OK);
    REQUIRE(parsed.model_set->global_parameters().size() == 2);
    CHECK(parsed.model_set->global_parameters()[0].velocity == doctest::Approx(12.5f));
    CHECK(parsed.model_set->global_parameters()[0].hold_during_playback == true);
    CHECK(parsed.model_set->global_parameters()[1].kind == oae::ParameterKind::Labeled);
    CHECK(parsed.model_set->global_parameters()[1].hold_during_playback == true);

    const auto* ev = parsed.model_set->find_event_by_path("event:/gameplay/engine");
    REQUIRE(ev != nullptr);
    REQUIRE(ev->parameters.size() == 1);
    CHECK(ev->parameters[0].kind == oae::ParameterKind::Discrete);
    CHECK(ev->parameters[0].seek_speed == doctest::Approx(4.0f));
    CHECK(ev->parameters[0].velocity == doctest::Approx(-1.5f));
    CHECK(ev->parameters[0].hold_during_playback == false);
    REQUIRE(ev->tracks.size() == 1);
    REQUIRE(ev->tracks[0].instruments.size() == 2);
    CHECK(ev->tracks[0].instruments[0].sheet_parameter == "RPM");
    CHECK(ev->tracks[0].instruments[0].param_min == doctest::Approx(2.0));
    CHECK(ev->tracks[0].instruments[0].param_max == doctest::Approx(6.0));
    CHECK(ev->tracks[0].instruments[1].sheet_parameter == "Intensity");
    CHECK(ev->tracks[0].instruments[1].param_min == doctest::Approx(10.0));
    CHECK(ev->tracks[0].instruments[1].param_max == doctest::Approx(50.0));

    // Reject unknown sheet_parameter
    project["events"][0]["tracks"][0]["instruments"][0]["sheet_parameter"] = "MissingParam";
    write_json_file(manifest, project);
    CHECK(run_bankc(bankc, manifest, bank) != 0);

    fs::remove_all(directory, ec);
}
