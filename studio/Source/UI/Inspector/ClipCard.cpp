#include "ClipCard.h"
#include "../../Timeline/TimelineSnap.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace oae::studio {

namespace {
juce::String make_sample_metadata_line(AuthoringModel& model, const nlohmann::json& ev, const std::string& sampleId)
{
    if (sampleId.empty()) return {};
    const auto& root = model.json();
    if (!root.contains("samples") || !root["samples"].is_array()) return {};
    fs::path filePath;
    for (const auto& s : root["samples"]) {
        if (s.value("id", "") == sampleId) {
            const std::string rel = s.value("file", "");
            if (!rel.empty()) filePath = model.document().get_project_dir() / fs::u8path(rel);
            break;
        }
    }
    if (filePath.empty()) return {};
    const juce::File jFile(juce::String::fromUTF8(filePath.u8string().c_str()));
    if (!jFile.existsAsFile()) return {};

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(jFile));
    if (!reader || reader->sampleRate <= 0.0) return {};

    const double kHz = reader->sampleRate / 1000.0;
    const juce::String ch = (reader->numChannels == 1) ? "Mono"
                          : (reader->numChannels == 2) ? "Stereo"
                                                       : (juce::String(reader->numChannels) + "ch");
    const double durSec = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    juce::String line = juce::String::formatted("%.1f kHz | %s | %.1fs", kHz, ch.toRawUTF8(), durSec);

    if (ev.contains("logic_items") && ev["logic_items"].is_array()) {
        bool hasTempo = false;
        for (const auto& li : ev["logic_items"]) {
            if (li.value("kind", "") == "tempo") { hasTempo = true; break; }
        }
        if (hasTempo) {
            const auto segs = TimelineSnap::extract_tempo_segments(&ev["logic_items"]);
            const auto seg = TimelineSnap::get_active_tempo(0.0, segs);
            const double barSec = seg.bar_duration_ms() / 1000.0;
            if (barSec > 0.0) {
                const int bars = std::max(1, static_cast<int>(std::round(durSec / barSec)));
                line += " | " + juce::String(bars) + " Bars";
            }
        }
    }
    return line;
}
juce::String make_clip_title(AuthoringModel& model, const std::string& path, size_t ti)
{
    const auto name = model.get_track_name(path, ti);
    return juce::String::fromUTF8("CLIP \xe2\x80\x94 ") + juce::String::fromUTF8(name.c_str());
}

juce::String make_clip_badge(AuthoringModel& model, const std::string& path, size_t ti, size_t ii)
{
    const auto* live = find_live_instrument(model.json(), path, ti, ii);
    const auto kind = live ? live->value("kind", "single") : "single";
    return kind_badge_label(kind);
}

juce::Colour make_clip_colour(AuthoringModel& model, const std::string& path, size_t ti)
{
    const auto hex = model.get_track_color(path, ti);
    return OaeTheme::TrackColors::parseHexColor(hex, ti);
}
} // namespace

ClipCard::ClipCard(const std::string& path,
                   size_t track_index,
                   size_t instrument_index,
                   bool expanded,
                   AuthoringModel& model,
                   int& edit_depth,
                   std::function<void()> on_edit_end,
                   std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_clip",
                        "clip",
                        make_clip_title(model, path, track_index),
                        make_clip_badge(model, path, track_index, instrument_index),
                        make_clip_colour(model, path, track_index),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
    , track_index_(track_index)
    , instrument_index_(instrument_index)
{
    build_ui();
}

void ClipCard::refresh()
{
    header_.set_title(make_clip_title(model_, path_, track_index_));
    header_.set_badge(make_clip_badge(model_, path_, track_index_, instrument_index_));
    header_.set_accent_colour(make_clip_colour(model_, path_, track_index_));
    build_ui();
    resized();
    repaint();
}

void ClipCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto* inst_ptr = find_live_instrument(model_.json(), path_, track_index_, instrument_index_);
    if (!inst_ptr) return;
    const auto& instrument = *inst_ptr;
    const auto kind = instrument.value("kind", "single");

    const auto path = path_;
    const auto sel_track = track_index_;
    const auto sel_inst = instrument_index_;

    const auto& root = model_.json();
    const nlohmann::json* ev_ptr = nullptr;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& item : root["events"]) {
            if (item.value("path", "") == path) { ev_ptr = &item; break; }
        }
    }
    const auto& ev = ev_ptr ? *ev_ptr : nlohmann::json::object();

    const auto metaLine = make_sample_metadata_line(model_, ev, instrument.value("sample_id", ""));
    if (metaLine.isNotEmpty()) {
        auto* metaLabel = new juce::Label("sample_meta", metaLine);
        metaLabel->setFont(juce::FontOptions(OaeTheme::Typography::caption));
        metaLabel->setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textMuted));
        register_layout_item(body_, metaLabel, y_, 240, 18);
        body_.addAndMakeVisible(metaLabel);
        comps_.emplace_back(metaLabel);
        y_ += 22;
    }

    if (kind == "single" || kind == "multi" || kind == "scatterer" || kind == "programmer" || kind == "event") {
        auto* async_toggle = new juce::ToggleButton("Async");
        async_toggle->setComponentID("inspector_instrument_async");
        async_toggle->setToggleState(instrument.value("async", false), juce::dontSendNotification);
        async_toggle->onClick = [this, path, sel_track, sel_inst, async_toggle]() {
            const bool next_async = async_toggle->getToggleState();
            model_.modify([path, sel_track, sel_inst, next_async](nlohmann::json& r) {
                for (auto& item : r["events"]) {
                    if (item.value("path", "") == path && item.contains("tracks") &&
                        sel_track < item["tracks"].size() &&
                        item["tracks"][sel_track].contains("instruments") &&
                        sel_inst < item["tracks"][sel_track]["instruments"].size()) {
                        item["tracks"][sel_track]["instruments"][sel_inst]["async"] = next_async;
                        break;
                    }
                }
            });
        };
        add_labeled_row("Playback", async_toggle);
    }

    if (kind == "event" || kind == "command") {
        const bool is_command = (kind == "command");
        const auto command_type = is_command ? instrument.value("command_type", "start_event") : "";
        if (is_command) {
            auto* cmd_type_combo = new juce::ComboBox();
            cmd_type_combo->setComponentID("inspector_command_type");
            const bool has_snapshots = !model_.get_snapshot_paths().empty();
            const std::array<const char*, 4> types = { "start_event", "stop_event", "snapshot_on", "snapshot_off" };
            for (size_t i = 0; i < types.size(); ++i) {
                const int item_id = static_cast<int>(i + 1);
                const bool is_snap = (i >= 2);
                cmd_type_combo->addItem(types[i], item_id);
                cmd_type_combo->setItemEnabled(item_id, !is_snap || has_snapshots);
                if (command_type == types[i]) {
                    cmd_type_combo->setSelectedId(item_id, juce::dontSendNotification);
                }
            }
            cmd_type_combo->onChange = [this, path, sel_track, sel_inst, cmd_type_combo, command_type]() {
                if (cmd_type_combo->getSelectedId() != 0) {
                    const auto next_type = cmd_type_combo->getText().toStdString();
                    if (!model_.set_command_type(path, sel_track, sel_inst, next_type)) {
                        cmd_type_combo->setText(command_type, juce::dontSendNotification);
                    }
                }
            };
            add_labeled_row("Command Type", cmd_type_combo);
        }

        const auto current_target = instrument.value(is_command ? "command_target" : "target_event", "");
        const auto targets = model_.get_instrument_targets(path, command_type);
        auto* target_combo = new juce::ComboBox();
        target_combo->setComponentID("inspector_instrument_target");
        target_combo->setTextWhenNothingSelected(targets.empty() ? "No valid targets" : "Select a valid target");
        for (size_t i = 0; i < targets.size(); ++i) {
            const auto id = static_cast<int>(i + 1);
            target_combo->addItem(targets[i], id);
            if (targets[i] == current_target) target_combo->setSelectedId(id, juce::dontSendNotification);
        }
        target_combo->setEnabled(!targets.empty());
        target_combo->onChange = [this, path, sel_track, sel_inst, target_combo]() {
            if (target_combo->getSelectedId() != 0) {
                model_.set_instrument_target(path, sel_track, sel_inst, target_combo->getText().toStdString());
            }
        };
        add_labeled_row(is_command ? "Command Target" : "Target Event", target_combo);
    } else if (kind == "single") {
        auto* sampleCombo = new juce::ComboBox();
        sampleCombo->setComponentID("inspector.clip.sample");
        int sIdx = 1;
        int curSIdx = 1;
        for (const auto& s : model_.get_sample_ids()) {
            sampleCombo->addItem(s, sIdx);
            if (s == instrument.value("sample_id", "")) curSIdx = sIdx;
            ++sIdx;
        }
        sampleCombo->setSelectedId(curSIdx, juce::dontSendNotification);
        sampleCombo->onChange = [this, path, sel_track, sel_inst, sampleCombo]() {
            const auto newS = sampleCombo->getText().toStdString();
            model_.modify([path, sel_track, sel_inst, newS](nlohmann::json& r) {
                for (auto& item : r["events"]) {
                    if (item.value("path", "") == path && sel_track < item["tracks"].size()) {
                        auto& inList = item["tracks"][sel_track]["instruments"];
                        if (sel_inst < inList.size()) inList[sel_inst]["sample_id"] = newS;
                    }
                }
            });
        };
        add_labeled_row("Sample", sampleCombo);
    } else if (kind == "multi" || kind == "scatterer") {
        auto* pmodeCombo = new juce::ComboBox();
        pmodeCombo->setComponentID("inspector.clip.play_mode");
        pmodeCombo->addItem("random", 1);
        pmodeCombo->addItem("random_no_repeat", 2);
        pmodeCombo->addItem("sequential", 3);
        pmodeCombo->addItem("shuffle", 4);
        const auto curMode = instrument.value("playlist_mode", "random");
        pmodeCombo->setSelectedId(curMode == "random_no_repeat" ? 2 : curMode == "sequential" ? 3 : curMode == "shuffle" ? 4 : 1, juce::dontSendNotification);
        pmodeCombo->onChange = [this, path, sel_track, sel_inst, pmodeCombo]() {
            const auto nm = pmodeCombo->getText().toStdString();
            model_.modify([path, sel_track, sel_inst, nm](nlohmann::json& r) {
                for (auto& item : r["events"]) {
                    if (item.value("path", "") == path && sel_track < item["tracks"].size()) {
                        auto& inList = item["tracks"][sel_track]["instruments"];
                        if (sel_inst < inList.size()) inList[sel_inst]["playlist_mode"] = nm;
                    }
                }
            });
        };
        add_labeled_row("Mode", pmodeCombo);

        if (kind == "scatterer") {
            const double s_min = instrument.value("spawn_interval_min_ms", 200.0);
            const double s_max = instrument.value("spawn_interval_max_ms", 1000.0);
            const uint32_t s_poly = instrument.value("polyphony", 4u);
            const uint32_t s_total = instrument.value("total_sounds", 0u);
            const float s_dmin = instrument.value("min_scatter_distance", 1.0f);
            const float s_dmax = instrument.value("max_scatter_distance", 10.0f);

            auto* minIntS = create_slider(10.0, 5000.0, s_min, " ms",
                [this, path, sel_track, sel_inst, s_max, s_poly, s_total, s_dmin, s_dmax](double v) {
                    model_.set_scatterer_properties(path, sel_track, sel_inst, v, std::max(v, s_max), s_poly, s_total, s_dmin, s_dmax);
                });
            minIntS->setComponentID("inspector.clip.scatter_min_int");
            add_labeled_row("Int Min", minIntS);

            auto* maxIntS = create_slider(10.0, 10000.0, s_max, " ms",
                [this, path, sel_track, sel_inst, s_min, s_poly, s_total, s_dmin, s_dmax](double v) {
                    model_.set_scatterer_properties(path, sel_track, sel_inst, std::min(s_min, v), v, s_poly, s_total, s_dmin, s_dmax);
                });
            maxIntS->setComponentID("inspector.clip.scatter_max_int");
            add_labeled_row("Int Max", maxIntS);

            auto* polyS = create_slider(1.0, 32.0, static_cast<double>(s_poly), "",
                [this, path, sel_track, sel_inst, s_min, s_max, s_total, s_dmin, s_dmax](double v) {
                    model_.set_scatterer_properties(path, sel_track, sel_inst, s_min, s_max, static_cast<uint32_t>(std::llround(v)), s_total, s_dmin, s_dmax);
                });
            polyS->setComponentID("inspector.clip.scatter_poly");
            polyS->setRange(polyS->getMinimum(), polyS->getMaximum(), 1.0);
            add_labeled_row("Polyphony", polyS);

            auto* totalS = create_slider(0.0, 64.0, static_cast<double>(s_total), "",
                [this, path, sel_track, sel_inst, s_min, s_max, s_poly, s_dmin, s_dmax](double v) {
                    model_.set_scatterer_properties(path, sel_track, sel_inst, s_min, s_max, s_poly, static_cast<uint32_t>(std::llround(v)), s_dmin, s_dmax);
                });
            totalS->setComponentID("inspector.clip.scatter_total");
            totalS->setRange(totalS->getMinimum(), totalS->getMaximum(), 1.0);
            add_labeled_row("Total", totalS);

            auto* dMinS = create_slider(0.0, 100.0, static_cast<double>(s_dmin), " m",
                [this, path, sel_track, sel_inst, s_min, s_max, s_poly, s_total, s_dmax](double v) {
                    const float f = static_cast<float>(v);
                    model_.set_scatterer_properties(path, sel_track, sel_inst, s_min, s_max, s_poly, s_total, f, std::max(f, s_dmax));
                });
            dMinS->setComponentID("inspector.clip.scatter_dist_min");
            add_labeled_row("Dist Min", dMinS);

            auto* dMaxS = create_slider(0.0, 200.0, static_cast<double>(s_dmax), " m",
                [this, path, sel_track, sel_inst, s_min, s_max, s_poly, s_total, s_dmin](double v) {
                    const float f = static_cast<float>(v);
                    model_.set_scatterer_properties(path, sel_track, sel_inst, s_min, s_max, s_poly, s_total, std::min(s_dmin, f), f);
                });
            dMaxS->setComponentID("inspector.clip.scatter_dist_max");
            add_labeled_row("Dist Max", dMaxS);
        }
    } else if (kind == "programmer") {
        const auto curKey = instrument.value("programmer_key", "");
        const auto tableEntries = model_.get_audio_table_entries();
        auto* keyCombo = new juce::ComboBox();
        keyCombo->setComponentID("inspector.clip.prog_key");
        keyCombo->setEditableText(true);
        int kid = 1;
        std::string mappedSample;
        for (const auto& entry : tableEntries) {
            keyCombo->addItem(entry.first, kid++);
            if (entry.first == curKey) mappedSample = entry.second;
        }
        keyCombo->setText(juce::String::fromUTF8(curKey.c_str()), juce::dontSendNotification);
        keyCombo->onChange = [this, path, sel_track, sel_inst, keyCombo]() {
            model_.set_programmer_key(path, sel_track, sel_inst, keyCombo->getText().toStdString());
        };
        add_labeled_row("Key", keyCombo);

        auto* tableSampleCombo = new juce::ComboBox();
        tableSampleCombo->setComponentID("inspector.clip.prog_sample");
        tableSampleCombo->addItem("(Unmapped)", 1);
        int sid_idx = 2;
        int matched_sid = 1;
        for (const auto& s : model_.get_sample_ids()) {
            tableSampleCombo->addItem(s, sid_idx);
            if (s == mappedSample) matched_sid = sid_idx;
            ++sid_idx;
        }
        tableSampleCombo->setSelectedId(matched_sid, juce::dontSendNotification);
        tableSampleCombo->onChange = [this, tableSampleCombo, keyCombo]() {
            const auto activeKey = keyCombo->getText().toStdString();
            if (activeKey.empty()) return;
            if (tableSampleCombo->getSelectedId() == 1) {
                model_.remove_audio_table_entry(activeKey);
            } else {
                model_.set_audio_table_entry(activeKey, tableSampleCombo->getText().toStdString());
            }
        };
        add_labeled_row("Table Sample", tableSampleCombo);
    }

    // Clip Timing & Envelope Subgroup
    add_section_header("TIMING & ENVELOPE");
    const double clipOff = instrument.value("offset_ms", 0.0);
    const double clipLen = std::max(10.0, instrument.value("length_ms", 500.0));
    const double clipStartOff = instrument.value("start_offset_ms", 0.0);
    const double clipFadeIn = instrument.value("fade_in_ms", 0.0);
    const double clipFadeOut = instrument.value("fade_out_ms", 0.0);

    auto* offSlider = create_slider(0.0, 60000.0, clipOff, " ms",
        [this, path, sel_track, sel_inst, clipLen, clipStartOff](double v) {
            const auto* live = find_live_instrument(model_.json(), path, sel_track, sel_inst);
            const double len = live ? live->value("length_ms", clipLen) : clipLen;
            const double soff = live ? live->value("start_offset_ms", clipStartOff) : clipStartOff;
            model_.resize_instrument(path, sel_track, sel_inst, v, len, soff);
        });
    offSlider->setComponentID("inspector.clip.offset");
    add_labeled_row("Start Time", offSlider);

    auto* lenSlider = create_slider(10.0, 60000.0, clipLen, " ms",
        [this, path, sel_track, sel_inst, clipOff, clipStartOff](double v) {
            const auto* live = find_live_instrument(model_.json(), path, sel_track, sel_inst);
            const double off = live ? live->value("offset_ms", clipOff) : clipOff;
            const double soff = live ? live->value("start_offset_ms", clipStartOff) : clipStartOff;
            model_.resize_instrument(path, sel_track, sel_inst, off, v, soff);
        });
    lenSlider->setComponentID("inspector.clip.length");
    add_labeled_row("Length", lenSlider);

    if (kind != "command" && kind != "silence") {
        auto* startOffSlider = create_slider(0.0, 60000.0, clipStartOff, " ms",
            [this, path, sel_track, sel_inst, clipOff, clipLen](double v) {
                const auto* live = find_live_instrument(model_.json(), path, sel_track, sel_inst);
                const double off = live ? live->value("offset_ms", clipOff) : clipOff;
                const double len = live ? live->value("length_ms", clipLen) : clipLen;
                model_.resize_instrument(path, sel_track, sel_inst, off, len, v);
            });
        startOffSlider->setComponentID("inspector.clip.start_offset");
        add_labeled_row("Sample Start", startOffSlider);

        auto* clipPitchSlider = create_slider(-24.0, 24.0, instrument.value("pitch_semitones_base", 0.0), " st",
            [this, path, sel_track, sel_inst](double v) {
                model_.modify([path, sel_track, sel_inst, v](nlohmann::json& r) {
                    for (auto& item : r["events"]) {
                        if (item.value("path", "") == path && sel_track < item["tracks"].size()) {
                            auto& inList = item["tracks"][sel_track]["instruments"];
                            if (sel_inst < inList.size()) inList[sel_inst]["pitch_semitones_base"] = v;
                        }
                    }
                });
            });
        clipPitchSlider->setComponentID("inspector.clip.pitch");
        clipPitchSlider->setDoubleClickReturnValue(true, 0.0, {});
        add_labeled_row("Clip Pitch", clipPitchSlider);

        auto* loopBtn = new juce::ToggleButton("Loop Clip");
        loopBtn->setComponentID("inspector.clip.loop");
        loopBtn->setToggleState(instrument.value("loop", false), juce::dontSendNotification);
        loopBtn->onClick = [this, path, sel_track, sel_inst, loopBtn]() {
            const bool loopVal = loopBtn->getToggleState();
            model_.modify([path, sel_track, sel_inst, loopVal](nlohmann::json& r) {
                for (auto& item : r["events"]) {
                    if (item.value("path", "") == path && sel_track < item["tracks"].size()) {
                        auto& inList = item["tracks"][sel_track]["instruments"];
                        if (sel_inst < inList.size()) inList[sel_inst]["loop"] = loopVal;
                    }
                }
            });
        };
        add_labeled_row("Looping", loopBtn);

        auto* fadeInSlider = create_slider(0.0, std::max(10.0, clipLen), clipFadeIn, " ms",
            [this, path, sel_track, sel_inst, clipFadeOut](double v) {
                const auto* live = find_live_instrument(model_.json(), path, sel_track, sel_inst);
                const double fout = live ? live->value("fade_out_ms", clipFadeOut) : clipFadeOut;
                model_.set_instrument_fades(path, sel_track, sel_inst, v, fout);
            });
        fadeInSlider->setComponentID("inspector.clip.fade_in");
        add_labeled_row("Fade In", fadeInSlider);

        auto* fadeOutSlider = create_slider(0.0, std::max(10.0, clipLen), clipFadeOut, " ms",
            [this, path, sel_track, sel_inst, clipFadeIn](double v) {
                const auto* live = find_live_instrument(model_.json(), path, sel_track, sel_inst);
                const double fin = live ? live->value("fade_in_ms", clipFadeIn) : clipFadeIn;
                model_.set_instrument_fades(path, sel_track, sel_inst, fin, v);
            });
        fadeOutSlider->setComponentID("inspector.clip.fade_out");
        add_labeled_row("Fade Out", fadeOutSlider);

        // Parameter Trigger Sheet Subgroup
        std::vector<std::string> sheetParams;
        std::set<std::string> seenSheetParams;
        if (ev.contains("parameters") && ev["parameters"].is_array()) {
            for (const auto& lp : ev["parameters"]) {
                const auto pname = lp.value("name", "");
                if (!pname.empty() && seenSheetParams.insert(pname).second) sheetParams.push_back(pname);
            }
        }
        if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
            for (const auto& gp : root["global_parameters"]) {
                const auto pname = gp.value("name", "");
                if (!pname.empty() && seenSheetParams.insert(pname).second) sheetParams.push_back(pname);
            }
        }

        const std::string curSheet = instrument.value("sheet_parameter", "");
        auto* sheetCombo = new juce::ComboBox();
        sheetCombo->setComponentID("inspector.clip.sheet_param");
        sheetCombo->addItem("(Timeline)", 1);
        int sheetSel = 1;
        for (size_t sp = 0; sp < sheetParams.size(); ++sp) {
            const int itemId = static_cast<int>(sp + 2);
            sheetCombo->addItem(juce::String::fromUTF8(sheetParams[sp].c_str()), itemId);
            if (sheetParams[sp] == curSheet) sheetSel = itemId;
        }
        sheetCombo->setSelectedId(sheetSel, juce::dontSendNotification);
        sheetCombo->onChange = [this, path, sel_track, sel_inst, sheetCombo]() {
            if (sheetCombo->getSelectedId() <= 1) {
                model_.set_instrument_sheet_range(path, sel_track, sel_inst, "", 0.0, 0.0);
            } else {
                const std::string pName = sheetCombo->getText().toStdString();
                double domMin = 0.0, domMax = 1.0;
                const auto& liveRoot = model_.json();
                const nlohmann::json* evPtr = nullptr;
                if (liveRoot.contains("events") && liveRoot["events"].is_array()) {
                    for (const auto& item : liveRoot["events"]) {
                        if (item.value("path", "") == path) { evPtr = &item; break; }
                    }
                }
                if (evPtr) {
                    if (const auto* pDef = resolve_event_param_any_scope(liveRoot, *evPtr, pName)) {
                        std::tie(domMin, domMax) = parameter_json_domain(*pDef);
                    }
                }
                model_.set_instrument_sheet_range(path, sel_track, sel_inst, pName, domMin, domMax);
            }
        };
        add_labeled_row("Trigger Sheet", sheetCombo);

        if (!curSheet.empty()) {
            double domMin = 0.0, domMax = 1.0;
            if (const auto* pDef = resolve_event_param_any_scope(root, ev, curSheet)) {
                std::tie(domMin, domMax) = parameter_json_domain(*pDef);
            }
            const double curMin = instrument.value("param_min", domMin);
            const double curMax = instrument.value("param_max", domMax);
            auto* sMinSlider = create_slider(domMin, domMax, curMin, "",
                [this, path, sel_track, sel_inst, curSheet, curMax](double v) {
                    model_.set_instrument_sheet_range(path, sel_track, sel_inst, curSheet, v, std::max(v, curMax));
                });
            sMinSlider->setComponentID("inspector.clip.sheet_min");
            sMinSlider->setEnabled(domMax > domMin);
            add_labeled_row("Sheet Min", sMinSlider);

            auto* sMaxSlider = create_slider(domMin, domMax, curMax, "",
                [this, path, sel_track, sel_inst, curSheet, curMin](double v) {
                    model_.set_instrument_sheet_range(path, sel_track, sel_inst, curSheet, std::min(curMin, v), v);
                });
            sMaxSlider->setComponentID("inspector.clip.sheet_max");
            sMaxSlider->setEnabled(domMax > domMin);
            add_labeled_row("Sheet Max", sMaxSlider);
        }

        const ItemSelection instSel{ ItemType::Event, path, static_cast<int>(sel_track), static_cast<int>(sel_inst), -1 };
        append_modulation_section(body_, comps_, y_, model_, edit_depth_, on_edit_end_, instSel, instrument, "CLIP ", "inspector.clip");
    }

    auto* dupClipBtn = new juce::TextButton("Duplicate Clip");
    dupClipBtn->setComponentID("inspector.clip.duplicate");
    register_layout_item(body_, dupClipBtn, y_, 118, 22);
    dupClipBtn->onClick = [this, path, sel_track, sel_inst]() {
        model_.duplicate_instrument(path, sel_track, sel_inst);
    };
    body_.addAndMakeVisible(dupClipBtn);
    comps_.emplace_back(dupClipBtn);

    auto* delClipBtn = new juce::TextButton("Delete Clip");
    delClipBtn->setComponentID("inspector.clip.delete");
    register_layout_item(body_, delClipBtn, y_, 118, 22);
    delClipBtn->onClick = [this, path, sel_track, sel_inst]() {
        model_.remove_instrument(path, sel_track, sel_inst);
    };
    body_.addAndMakeVisible(delClipBtn);
    comps_.emplace_back(delClipBtn);
    y_ += 26;
}

void ClipCard::reset_to_default()
{
    const auto path = path_;
    const auto sel_track = track_index_;
    const auto sel_inst = instrument_index_;
    model_.modify([path, sel_track, sel_inst](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path && item.contains("tracks") &&
                sel_track < item["tracks"].size() &&
                item["tracks"][sel_track].contains("instruments") &&
                sel_inst < item["tracks"][sel_track]["instruments"].size()) {
                auto& inst = item["tracks"][sel_track]["instruments"][sel_inst];
                inst["pitch_semitones_base"] = 0.0;
                inst["loop"] = false;
                inst["fade_in_ms"] = 0.0;
                inst["fade_out_ms"] = 0.0;
                inst["async"] = false;
                break;
            }
        }
    });
}

nlohmann::json ClipCard::serialize_values() const
{
    const auto* live = find_live_instrument(model_.json(), path_, track_index_, instrument_index_);
    if (!live) return nlohmann::json::object();
    nlohmann::json out = nlohmann::json::object();
    out["async"] = live->value("async", false);
    out["pitch_semitones_base"] = live->value("pitch_semitones_base", 0.0);
    out["loop"] = live->value("loop", false);
    out["fade_in_ms"] = live->value("fade_in_ms", 0.0);
    out["fade_out_ms"] = live->value("fade_out_ms", 0.0);
    out["length_ms"] = live->value("length_ms", 500.0);
    if (live->contains("modulators")) {
        out["modulators"] = (*live)["modulators"];
    }
    return out;
}

bool ClipCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    const auto sel_track = track_index_;
    const auto sel_inst = instrument_index_;
    model_.modify([path, sel_track, sel_inst, data](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path && item.contains("tracks") &&
                sel_track < item["tracks"].size() &&
                item["tracks"][sel_track].contains("instruments") &&
                sel_inst < item["tracks"][sel_track]["instruments"].size()) {
                auto& inst = item["tracks"][sel_track]["instruments"][sel_inst];
                if (data.contains("async")) inst["async"] = data["async"];
                if (data.contains("pitch_semitones_base")) inst["pitch_semitones_base"] = data["pitch_semitones_base"];
                if (data.contains("loop")) inst["loop"] = data["loop"];
                if (data.contains("fade_in_ms")) inst["fade_in_ms"] = data["fade_in_ms"];
                if (data.contains("fade_out_ms")) inst["fade_out_ms"] = data["fade_out_ms"];
                if (data.contains("modulators")) inst["modulators"] = data["modulators"];
                break;
            }
        }
    });
    return true;
}

} // namespace oae::studio
