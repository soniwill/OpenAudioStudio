#include "UiControlTestFixture.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace oae::studio::testing {

namespace {

constexpr uint32_t kWavSampleRate = 48000;
constexpr double kTwoPi = 6.28318530717958647692;

void write_u16_le(std::ofstream& out, uint16_t v)
{
    const char bytes[2] = {
        static_cast<char>(v & 0xFFu),
        static_cast<char>((v >> 8u) & 0xFFu)
    };
    out.write(bytes, 2);
}

void write_u32_le(std::ofstream& out, uint32_t v)
{
    const char bytes[4] = {
        static_cast<char>(v & 0xFFu),
        static_cast<char>((v >> 8u) & 0xFFu),
        static_cast<char>((v >> 16u) & 0xFFu),
        static_cast<char>((v >> 24u) & 0xFFu)
    };
    out.write(bytes, 4);
}

bool write_synthetic_wav(const std::filesystem::path& path,
                         double freq1Hz,
                         double amp1,
                         double freq2Hz,
                         double amp2,
                         double durationSec)
{
    const uint32_t numSamples = static_cast<uint32_t>(std::round(durationSec * static_cast<double>(kWavSampleRate)));
    std::vector<int16_t> pcm(numSamples, 0);
    for (uint32_t i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kWavSampleRate);
        double sample = amp1 * std::sin(kTwoPi * freq1Hz * t);
        if (amp2 > 0.0 && freq2Hz > 0.0) {
            sample += amp2 * std::sin(kTwoPi * freq2Hz * t);
        }
        sample = std::clamp(sample, -0.98, 0.98);
        pcm[i] = static_cast<int16_t>(std::round(sample * 32767.0));
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    const uint32_t dataSize = numSamples * sizeof(int16_t);
    const uint32_t riffSize = 36u + dataSize;
    out.write("RIFF", 4);
    write_u32_le(out, riffSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32_le(out, 16u);
    write_u16_le(out, 1u);
    write_u16_le(out, 1u);
    write_u32_le(out, kWavSampleRate);
    write_u32_le(out, kWavSampleRate * sizeof(int16_t));
    write_u16_le(out, sizeof(int16_t));
    write_u16_le(out, 16u);
    out.write("data", 4);
    write_u32_le(out, dataSize);
    out.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(dataSize));
    return out.good();
}

} // namespace

UiControlTestFixture::UiControlTestFixture()
{
    static std::atomic<uint32_t> counter{ 1 };
    const auto nowTick = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string folderName = "oae_ui_ctrl_test_" + std::to_string(nowTick) + "_" + std::to_string(counter.fetch_add(1));
    tempDir_ = std::filesystem::temp_directory_path() / folderName;
    projectPath_ = tempDir_ / "project.json";
    layoutFilePath_ = tempDir_ / "oae_studio_layout.json";
    sample440Path_ = tempDir_ / "assets" / "tone_440.wav";
    sample880Path_ = tempDir_ / "assets" / "tone_880.wav";
    sampleDualPath_ = tempDir_ / "assets" / "dualtone_220_4000.wav";
}

UiControlTestFixture::~UiControlTestFixture()
{
    std::error_code ec;
    if (!tempDir_.empty() && std::filesystem::exists(tempDir_, ec)) {
        std::filesystem::remove_all(tempDir_, ec);
    }
}

bool UiControlTestFixture::initialize(std::string& outError)
{
    std::error_code ec;
    std::filesystem::create_directories(tempDir_ / "assets", ec);
    if (ec) {
        outError = "Failed to create temporary fixture directory: " + ec.message();
        return false;
    }

    if (!write_synthetic_wav(sample440Path_, 440.0, 0.5, 0.0, 0.0, 2.0) ||
        !write_synthetic_wav(sample880Path_, 880.0, 0.5, 0.0, 0.0, 2.0) ||
        !write_synthetic_wav(sampleDualPath_, 220.0, 0.25, 4000.0, 0.25, 2.0)) {
        outError = "Failed to write synthetic fixture WAV files";
        return false;
    }

    nlohmann::json root = {
        { "version", 1 },
        { "name", "UiControlVerificationFixture" },
        { "mixer", {
            { "master", {
                { "volume", 1.0 },
                { "mute", false },
                { "effects", nlohmann::json::array() }
            }}
        }},
        { "buses", nlohmann::json::array({
            {
                { "path", "bus:/sfx" },
                { "parent", "bus:/" },
                { "volume", 1.0 },
                { "effects", nlohmann::json::array({
                    {
                        { "type", "lowpass" },
                        { "position", 0 },
                        { "bypass", false },
                        { "wet", 1.0 },
                        { "params", nlohmann::json::array({ 20000.0, 0.707 }) }
                    },
                    {
                        { "type", "eq3" },
                        { "position", 0 },
                        { "bypass", false },
                        { "wet", 1.0 },
                        { "params", nlohmann::json::array({ 0.0, 0.0, 0.0, 250.0, 2500.0 }) }
                    }
                }) }
            },
            {
                { "path", "bus:/music" },
                { "parent", "bus:/" },
                { "volume", 1.0 },
                { "effects", nlohmann::json::array() }
            }
        })},
        { "vcas", nlohmann::json::array({
            {
                { "path", "vca:/master_vca" },
                { "buses", nlohmann::json::array({ "bus:/sfx", "bus:/music" }) },
                { "events", nlohmann::json::array({ "event:/test/main", "event:/test/nested" }) }
            }
        })},
        { "global_parameters", nlohmann::json::array({
            {
                { "name", "Intensity" },
                { "kind", "continuous" },
                { "min", 0.0 },
                { "max", 1.0 },
                { "default_value", 0.5 },
                { "seek_speed", 0.0 },
                { "seek_speed_down", 0.0 },
                { "velocity", 0.0 },
                { "hold_during_playback", false }
            },
            {
                { "name", "Stage" },
                { "kind", "discrete" },
                { "min", 0.0 },
                { "max", 3.0 },
                { "default_value", 1.0 },
                { "seek_speed", 0.0 },
                { "seek_speed_down", 0.0 },
                { "velocity", 0.0 },
                { "hold_during_playback", false }
            },
            {
                { "name", "Surface" },
                { "kind", "labeled" },
                { "default_value", 0 },
                { "labels", nlohmann::json::array({ "Wood", "Stone", "Metal" }) },
                { "seek_speed", 0.0 },
                { "seek_speed_down", 0.0 },
                { "velocity", 0.0 },
                { "hold_during_playback", false }
            }
        })},
        { "samples", nlohmann::json::array({
            { { "id", "tone_440" }, { "file", "assets/tone_440.wav" }, { "stream", false } },
            { { "id", "tone_880" }, { "file", "assets/tone_880.wav" }, { "stream", false } },
            { { "id", "dualtone_220_4000" }, { "file", "assets/dualtone_220_4000.wav" }, { "stream", false } }
        })},
        { "audio_table", nlohmann::json::array({
            { { "key", "prog_tone" }, { "sample_id", "tone_440" } }
        })},
        { "effect_automations", nlohmann::json::array() },
        { "events", nlohmann::json::array({
            {
                { "path", "event:/test/main" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", 0.0 },
                { "pitch_semitones", 0.0 },
                { "pan", 0.0 },
                { "max_instances", 16 },
                { "steal_mode", "oldest" },
                { "cooldown_ms", 0 },
                { "priority", 128 },
                { "stop_fadeout_ms", 0 },
                { "parameters", nlohmann::json::array({
                    {
                        { "name", "LocalGain" },
                        { "kind", "continuous" },
                        { "min", 0.0 },
                        { "max", 1.0 },
                        { "default_value", 1.0 }
                    },
                    {
                        { "name", "LocalMode" },
                        { "kind", "labeled" },
                        { "default_value", 1 },
                        { "labels", nlohmann::json::array({ "Off", "On" }) }
                    }
                })},
                { "logic_items", nlohmann::json::array({
                    { { "kind", "marker" }, { "name", "Intro" }, { "position_ms", 0.0 } },
                    {
                        { "kind", "loop_region" },
                        { "start_ms", 0.0 },
                        { "length_ms", 2000.0 },
                        { "iterations", -1 },
                        { "until_key_off", false },
                        { "condition", {
                            { "probability_pct", 100.0 },
                            { "terms", nlohmann::json::array({
                                {
                                    { "param", { { "scope", "global" }, { "name", "Intensity" } } },
                                    { "min", 0.0 },
                                    { "max", 1.0 }
                                }
                            })}
                        }}
                    },
                    { { "kind", "sustain_point" }, { "position_ms", 2100.0 } },
                    {
                        { "kind", "transition_marker" },
                        { "position_ms", 2800.0 },
                        { "target_marker", "Intro" },
                        { "quantization", "immediate" },
                        { "crossfade_ms", 0.0 }
                    },
                    {
                        { "kind", "tempo" },
                        { "position_ms", 3500.0 },
                        { "bpm", 120.0 },
                        { "ts_upper", 4 },
                        { "ts_lower", 4 }
                    }
                })},
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "tone_440" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 2000.0 },
                                { "start_offset_ms", 0.0 },
                                { "fade_in_ms", 0.0 },
                                { "fade_out_ms", 0.0 },
                                { "pitch_semitones_base", 0.0 },
                                { "loop", true },
                                { "async", false },
                                { "sheet_parameter", "Intensity" },
                                { "param_min", 0.0 },
                                { "param_max", 1.0 }
                            }
                        })}
                    },
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "multi" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 2000.0 },
                                { "loop", true },
                                { "async", false },
                                { "playlist_mode", "sequential" },
                                { "playlist_scope", "instance" },
                                { "playlist", nlohmann::json::array({
                                    { { "sample_id", "tone_880" }, { "weight", 50.0 } },
                                    { { "sample_id", "tone_440" }, { "weight", 50.0 } }
                                })}
                            }
                        })}
                    },
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "scatterer" },
                                { "offset_ms", 2100.0 },
                                { "length_ms", 400.0 },
                                { "loop", false },
                                { "async", true },
                                { "playlist_mode", "random" },
                                { "spawn_interval_min_ms", 150.0 },
                                { "spawn_interval_max_ms", 500.0 },
                                { "polyphony", 4 },
                                { "total_sounds", 0 },
                                { "min_scatter_distance", 1.0 },
                                { "max_scatter_distance", 10.0 },
                                { "playlist", nlohmann::json::array({
                                    { { "sample_id", "tone_440" }, { "weight", 1.0 } }
                                })}
                            },
                            {
                                { "kind", "event" },
                                { "target_event", "event:/test/nested" },
                                { "offset_ms", 2550.0 },
                                { "length_ms", 400.0 },
                                { "loop", false },
                                { "async", false }
                            },
                            {
                                { "kind", "command" },
                                { "command_type", "start_event" },
                                { "command_target", "event:/test/nested" },
                                { "offset_ms", 3000.0 }
                            },
                            {
                                { "kind", "programmer" },
                                { "programmer_key", "prog_tone" },
                                { "offset_ms", 3400.0 },
                                { "length_ms", 400.0 },
                                { "loop", false },
                                { "async", false }
                            }
                        })}
                    }
                })}
            },
            {
                { "path", "event:/test/fx_probe" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", 0.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "dualtone_220_4000" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 2000.0 },
                                { "loop", true }
                            }
                        })}
                    }
                })}
            },
            {
                { "path", "event:/test/nested" },
                { "kind", "event" },
                { "bus_path", "bus:/music" },
                { "volume_db", 0.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "tone_880" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 2000.0 },
                                { "loop", true }
                            }
                        })}
                    }
                })}
            },
            {
                { "path", "snapshot:/duck" },
                { "kind", "snapshot" },
                { "priority", 120 },
                { "snapshot_mode", "override" },
                { "fade_in_ms", 0.0 },
                { "snapshot_targets", nlohmann::json::array({
                    {
                        { "bus_path", "bus:/music" },
                        { "value", -18.0 },
                        { "param_id", -1 },
                        { "position", 0 },
                        { "effect_index", 0 }
                    }
                })}
            }
        })}
    };

    std::ofstream projOut(projectPath_, std::ios::binary);
    if (!projOut.is_open()) {
        outError = "Failed to write fixture project.json";
        return false;
    }
    projOut << root.dump(2);
    projOut.close();
    return true;
}

bool UiControlTestFixture::load_into(MainComponent& mainComp, std::string& outError)
{
    if (!mainComp.document().open(projectPath_)) {
        outError = "ProjectDocument::open failed on fixture project.json";
        return false;
    }

    auto& model = mainComp.authoring_model();
    const std::string mainEv = "event:/test/main";

    model.set_track_name(mainEv, 0, "Lead 440");
    model.set_track_name(mainEv, 1, "Harmonic 880");
    model.set_track_name(mainEv, 2, "Showcase Clips");
    model.set_event_user_property(mainEv, "notes", "Fixture verification event");
    model.set_event_user_property(mainEv, "tags", "test, ui");
    model.set_event_user_property(mainEv, "env", "headless_qa");

    model.add_modulator({ ItemType::Event, mainEv, -1, -1, -1 }, {
        { "kind", "random" },
        { "target", "pitch" },
        { "range", 0.0 }
    });
    model.add_modulator({ ItemType::Event, mainEv, -1, -1, -1 }, {
        { "kind", "ahdsr" },
        { "attack_ms", 5.0 },
        { "hold_ms", 0.0 },
        { "decay_ms", 20.0 },
        { "sustain", 1.0 },
        { "release_ms", 50.0 },
        { "release_on_key_off", true }
    });

    model.add_modulator({ ItemType::Event, mainEv, 0, 0, -1 }, {
        { "kind", "random" },
        { "target", "volume" },
        { "range", 0.0 }
    });
    model.add_modulator({ ItemType::Event, mainEv, 0, 0, -1 }, {
        { "kind", "ahdsr" },
        { "attack_ms", 5.0 },
        { "hold_ms", 0.0 },
        { "decay_ms", 20.0 },
        { "sustain", 1.0 },
        { "release_ms", 50.0 },
        { "release_on_key_off", true }
    });

    model.add_parameter_automation(mainEv, -1, "volume", "global", "Intensity", nlohmann::json::array({
        { { "x", 0.0 }, { "y", -24.0 }, { "shape", "linear" } },
        { { "x", 0.5 }, { "y", 0.0 }, { "shape", "linear" } },
        { { "x", 1.0 }, { "y", 0.0 }, { "shape", "linear" } }
    }));

    model.set_track_timeline_volume_points(mainEv, 0, nlohmann::json::array({
        { { "x", 0.0 }, { "y", 0.0 }, { "shape", "linear" } },
        { { "x", 1000.0 }, { "y", -1.0 }, { "shape", "linear" } },
        { { "x", 2000.0 }, { "y", 0.0 }, { "shape", "linear" } }
    }), true);

    model.set_track_layer_mask(mainEv, 0, "global", "Intensity", 0.0, 1.0, 0.05, true);
    model.set_track_layer_mask(mainEv, 1, "global", "Intensity", 0.2, 1.0, 0.05, true);

    model.add_effect({ ItemType::Event, mainEv, 0, -1, -1 }, "lowpass", 0);

    model.add_effect_automation("bus:/sfx", 0, 1, "eq3", 0, "Intensity", nlohmann::json::array({
        { { "x", 0.0 }, { "y", 0.0 }, { "shape", "linear" } },
        { { "x", 1.0 }, { "y", 0.0 }, { "shape", "linear" } }
    }));

    mainComp.document().save();
    goldenJson_ = model.create_immutable_snapshot();
    model.clear_history();

    model.select({ ItemType::Event, mainEv, -1, -1, -1 });
    model.notify_project_changed();

    if (!mainComp.preview_rebuilder().rebuild_synchronously() ||
        mainComp.preview_rebuilder().get_loaded_bank() == 0) {
        outError = "PreviewRebuilder::rebuild_synchronously failed to compile or load fixture preview bank: " +
                   mainComp.diagnostics().get_log_text().toStdString();
        return false;
    }

    mainComp.transport().set_global_parameter("Intensity", 0.5f);
    mainComp.transport().set_global_parameter("Stage", 1.0f);
    mainComp.transport().set_global_parameter("Surface", 0.0f);
    return true;
}

void UiControlTestFixture::restore_golden_model(MainComponent& mainComp)
{
    if (goldenJson_.is_null()) {
        return;
    }
    auto& model = mainComp.authoring_model();
    model.modify([this](nlohmann::json& root) {
        root = goldenJson_;
    });
    model.clear_history();
}

} // namespace oae::studio::testing
