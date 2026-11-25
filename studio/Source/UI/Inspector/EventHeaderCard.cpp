#include "EventHeaderCard.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

EventHeaderCard::EventHeaderCard(const std::string& path,
                                 bool expanded,
                                 AuthoringModel& model,
                                 int& edit_depth,
                                 std::function<void()> on_edit_end,
                                 std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_event",
                        "event_header",
                        juce::String::fromUTF8("EVENT \xe2\x80\x94 ") + juce::String::fromUTF8(path.c_str()),
                        "Event",
                        juce::Colour(OaeTheme::Colors::accentCyan),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
{
    build_ui();
}

void EventHeaderCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void EventHeaderCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* ev_ptr = nullptr;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == path_) {
                ev_ptr = &ev;
                break;
            }
        }
    }
    if (!ev_ptr) return;
    const auto& ev = *ev_ptr;

    auto* busCombo = new juce::ComboBox();
    busCombo->setComponentID("inspector.event.bus");
    int idx = 1;
    int selectedIdx = 1;
    for (const auto& b : model_.get_bus_paths()) {
        busCombo->addItem(b, idx);
        if (b == ev.value("bus_path", "bus:/")) selectedIdx = idx;
        ++idx;
    }
    busCombo->setSelectedId(selectedIdx, juce::dontSendNotification);
    const auto path = path_;
    busCombo->onChange = [this, path, busCombo]() {
        const auto newBus = busCombo->getText().toStdString();
        model_.modify([path, newBus](nlohmann::json& r) {
            for (auto& item : r["events"]) {
                if (item.value("path", "") == path) { item["bus_path"] = newBus; break; }
            }
        });
    };
    add_labeled_row("Bus Route", busCombo);

    auto* volSlider = create_slider(-80.0, 10.0, ev.value("volume_db", 0.0), " dB",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["volume_db"] = v;
            });
        });
    volSlider->setComponentID("inspector_volume");
    volSlider->setDoubleClickReturnValue(true, 0.0, {});
    add_labeled_row("Volume", volSlider);

    auto* pitchSlider = create_slider(-24.0, 24.0, ev.value("pitch_semitones", 0.0), " st",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["pitch_semitones"] = v;
            });
        });
    pitchSlider->setComponentID("inspector_pitch");
    pitchSlider->setDoubleClickReturnValue(true, 0.0, {});
    add_labeled_row("Pitch", pitchSlider);

    auto* panSlider = create_slider(-1.0, 1.0, ev.value("pan", 0.0), "",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["pan"] = v;
            });
        });
    panSlider->setComponentID("inspector_pan");
    panSlider->setDoubleClickReturnValue(true, 0.0, {});
    add_labeled_row("Pan", panSlider);

    auto* maxInstSlider = create_slider(1.0, 64.0, ev.value("max_instances", 64), "",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["max_instances"] = static_cast<int>(v);
            });
        });
    maxInstSlider->setComponentID("inspector_max_inst");
    maxInstSlider->setRange(maxInstSlider->getMinimum(), maxInstSlider->getMaximum(), 1.0);
    add_labeled_row("Max Inst", maxInstSlider);

    auto* stealCombo = new juce::ComboBox();
    stealCombo->setComponentID("inspector.event.stealing");
    stealCombo->addItem("oldest", 1);
    stealCombo->addItem("quietest", 2);
    stealCombo->addItem("none", 3);
    const auto curSteal = ev.value("steal_mode", "oldest");
    stealCombo->setSelectedId(curSteal == "quietest" ? 2 : curSteal == "none" ? 3 : 1, juce::dontSendNotification);
    stealCombo->onChange = [this, path, stealCombo]() {
        const auto mode = stealCombo->getText().toStdString();
        model_.modify([path, mode](nlohmann::json& r) {
            for (auto& item : r["events"]) if (item.value("path", "") == path) item["steal_mode"] = mode;
        });
    };
    add_labeled_row("Steal Mode", stealCombo);

    auto* coolSlider = create_slider(0.0, 5000.0, ev.value("cooldown_ms", 0), " ms",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["cooldown_ms"] = static_cast<int>(v);
            });
        });
    coolSlider->setComponentID("inspector.event.cooldown");
    add_labeled_row("Cooldown", coolSlider);

    auto* prioSlider = create_slider(0.0, 256.0, ev.value("priority", 128), "",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["priority"] = static_cast<int>(v);
            });
        });
    prioSlider->setComponentID("inspector.event.priority");
    prioSlider->setRange(prioSlider->getMinimum(), prioSlider->getMaximum(), 1.0);
    add_labeled_row("Priority", prioSlider);

    auto* fadeoutSlider = create_slider(0.0, 5000.0, ev.value("stop_fadeout_ms", 0), " ms",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["stop_fadeout_ms"] = static_cast<int>(v);
            });
        });
    fadeoutSlider->setComponentID("inspector.event.fadeout");
    add_labeled_row("Stop Fade", fadeoutSlider);

    auto* addTrackBtn = new juce::TextButton("+ Add Track");
    addTrackBtn->setComponentID("inspector.event.add_track");
    register_layout_item(body_, addTrackBtn, y_, 160, 22);
    addTrackBtn->onClick = [this, path]() { model_.add_track(path); };
    body_.addAndMakeVisible(addTrackBtn);
    comps_.emplace_back(addTrackBtn);
    y_ += 28;

    add_section_header("LOCAL PARAMETERS");
    auto* addLocalBtn = new juce::TextButton("+ Add Local Param");
    addLocalBtn->setComponentID("inspector.event.add_local_param");
    register_layout_item(body_, addLocalBtn, y_, 140, 22);
    addLocalBtn->onClick = [this, path, addLocalBtn]() {
        juce::PopupMenu m;
        m.addItem(1, "Continuous");
        m.addItem(2, "Discrete");
        m.addItem(3, "Labeled");
        juce::Component::SafePointer<juce::TextButton> btn(addLocalBtn);
        UiTestDialogSeam::show_popup_menu("inspector_add_local_param", m,
            juce::PopupMenu::Options().withTargetComponent(addLocalBtn),
            [this, path, btn](int res) {
                if (!btn || res == 0) return;
                const std::string kindStr = (res == 2) ? "discrete" : ((res == 3) ? "labeled" : "continuous");
                const auto existing = model_.get_local_parameter_names(path);
                int idxSuffix = 1;
                std::string candidate = "Param1";
                while (std::find(existing.begin(), existing.end(), candidate) != existing.end()) {
                    candidate = "Param" + std::to_string(++idxSuffix);
                }
                model_.add_local_parameter(path, candidate, kindStr);
            });
    };
    body_.addAndMakeVisible(addLocalBtn);
    comps_.emplace_back(addLocalBtn);
    y_ += 26;

    if (ev.contains("parameters") && ev["parameters"].is_array()) {
        size_t localIdx = 0;
        for (const auto& lp : ev["parameters"]) {
            if (!lp.is_object()) continue;
            append_parameter_editor_controls(body_, comps_, y_, model_, edit_depth_, on_edit_end_,
                                             lp, true, path_, "inspector.event.param." + std::to_string(localIdx++));
        }
    }

    add_section_header("USER PROPERTIES & NOTES");
    const auto userProps = (ev.contains("user_props") && ev["user_props"].is_object())
        ? ev["user_props"] : nlohmann::json::object();

    const auto curNotes = userProps.value("notes", "");
    auto* notesEdit = new juce::TextEditor();
    notesEdit->setComponentID("inspector.event.notes");
    notesEdit->setText(juce::String::fromUTF8(curNotes.c_str()), false);
    auto commit_notes = [this, path, notesEdit]() {
        const auto val = notesEdit->getText().toStdString();
        if (val.empty()) model_.remove_event_user_property(path, "notes");
        else model_.set_event_user_property(path, "notes", val);
    };
    notesEdit->onReturnKey = commit_notes;
    notesEdit->onFocusLost = commit_notes;
    add_labeled_row("Notes", notesEdit);

    const auto curTags = userProps.value("tags", "");
    auto* tagsEdit = new juce::TextEditor();
    tagsEdit->setComponentID("inspector.event.tags");
    tagsEdit->setText(juce::String::fromUTF8(curTags.c_str()), false);
    auto commit_tags = [this, path, tagsEdit]() {
        const auto val = tagsEdit->getText().toStdString();
        if (val.empty()) model_.remove_event_user_property(path, "tags");
        else model_.set_event_user_property(path, "tags", val);
    };
    tagsEdit->onReturnKey = commit_tags;
    tagsEdit->onFocusLost = commit_tags;
    add_labeled_row("Tags", tagsEdit);

    size_t propIdx = 0;
    for (const auto& item : userProps.items()) {
        const auto& propKey = item.key();
        if (propKey == "notes" || propKey == "tags" || is_internal_user_prop(propKey) || !item.value().is_string()) {
            continue;
        }
        const std::string propPrefix = "inspector.event.user_prop." + std::to_string(propIdx++);
        auto* keyLbl = new juce::Label("", juce::String::fromUTF8(propKey.c_str()));
        keyLbl->setFont(juce::FontOptions(11.0f));
        keyLbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        register_layout_item(body_, keyLbl, y_, 86, 22);
        body_.addAndMakeVisible(keyLbl);
        comps_.emplace_back(keyLbl);

        auto* valEdit = new juce::TextEditor();
        valEdit->setComponentID(propPrefix + ".value");
        valEdit->setText(juce::String::fromUTF8(item.value().get<std::string>().c_str()), false);
        register_layout_item(body_, valEdit, y_, 138, 22);
        auto commit_prop = [this, path, propKey, valEdit]() {
            model_.set_event_user_property(path, propKey, valEdit->getText().toStdString());
        };
        valEdit->onReturnKey = commit_prop;
        valEdit->onFocusLost = commit_prop;
        body_.addAndMakeVisible(valEdit);
        comps_.emplace_back(valEdit);

        auto* delPropBtn = new juce::TextButton("X");
        delPropBtn->setComponentID(propPrefix + ".remove");
        register_layout_item(body_, delPropBtn, y_, 24, 22);
        delPropBtn->onClick = [this, path, propKey]() {
            model_.remove_event_user_property(path, propKey);
        };
        body_.addAndMakeVisible(delPropBtn);
        comps_.emplace_back(delPropBtn);
        y_ += 26;
    }

    auto* newKeyEdit = new juce::TextEditor();
    newKeyEdit->setComponentID("inspector.event.user_prop.new_key");
    newKeyEdit->setTextToShowWhenEmpty("key", juce::Colours::grey);
    register_layout_item(body_, newKeyEdit, y_, 86, 22);
    body_.addAndMakeVisible(newKeyEdit);
    comps_.emplace_back(newKeyEdit);

    auto* newValEdit = new juce::TextEditor();
    newValEdit->setComponentID("inspector.event.user_prop.new_value");
    newValEdit->setTextToShowWhenEmpty("value", juce::Colours::grey);
    register_layout_item(body_, newValEdit, y_, 118, 22);
    body_.addAndMakeVisible(newValEdit);
    comps_.emplace_back(newValEdit);

    auto* addPropBtn = new juce::TextButton("+");
    addPropBtn->setComponentID("inspector.event.user_prop.add");
    register_layout_item(body_, addPropBtn, y_, 44, 22);
    addPropBtn->onClick = [this, path, newKeyEdit, newValEdit]() {
        const auto k = newKeyEdit->getText().trim().toStdString();
        if (!k.empty()) {
            model_.set_event_user_property(path, k, newValEdit->getText().toStdString());
        }
    };
    body_.addAndMakeVisible(addPropBtn);
    comps_.emplace_back(addPropBtn);
    y_ += 26;

    const ItemSelection evSel{ ItemType::Event, path_, -1, -1, -1 };
    append_modulation_section(body_, comps_, y_, model_, edit_depth_, on_edit_end_, evSel, ev, "EVENT ", "inspector.event");
    append_param_automations_section(body_, comps_, y_, model_, edit_depth_, on_edit_end_, path_, -1, ev, "inspector.event");
}

void EventHeaderCard::reset_to_default()
{
    const auto path = path_;
    model_.modify([path](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path) {
                item["volume_db"] = 0.0;
                item["pitch_semitones"] = 0.0;
                item["pan"] = 0.0;
                item["bus_path"] = "bus:/";
                item["max_instances"] = 64;
                item["steal_mode"] = "oldest";
                item["cooldown_ms"] = 0;
                item["priority"] = 128;
                item["stop_fadeout_ms"] = 0;
                break;
            }
        }
    });
}

nlohmann::json EventHeaderCard::serialize_values() const
{
    const auto& root = model_.json();
    if (!root.contains("events") || !root["events"].is_array()) return nlohmann::json::object();
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == path_) {
            nlohmann::json out = nlohmann::json::object();
            out["volume_db"] = ev.value("volume_db", 0.0);
            out["pitch_semitones"] = ev.value("pitch_semitones", 0.0);
            out["pan"] = ev.value("pan", 0.0);
            out["bus_path"] = ev.value("bus_path", "bus:/");
            out["max_instances"] = ev.value("max_instances", 64);
            out["steal_mode"] = ev.value("steal_mode", "oldest");
            out["cooldown_ms"] = ev.value("cooldown_ms", 0);
            out["priority"] = ev.value("priority", 128);
            out["stop_fadeout_ms"] = ev.value("stop_fadeout_ms", 0);
            return out;
        }
    }
    return nlohmann::json::object();
}

bool EventHeaderCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    model_.modify([path, data](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path) {
                if (data.contains("volume_db")) item["volume_db"] = data["volume_db"];
                if (data.contains("pitch_semitones")) item["pitch_semitones"] = data["pitch_semitones"];
                if (data.contains("pan")) item["pan"] = data["pan"];
                if (data.contains("bus_path")) item["bus_path"] = data["bus_path"];
                if (data.contains("max_instances")) item["max_instances"] = data["max_instances"];
                if (data.contains("steal_mode")) item["steal_mode"] = data["steal_mode"];
                if (data.contains("cooldown_ms")) item["cooldown_ms"] = data["cooldown_ms"];
                if (data.contains("priority")) item["priority"] = data["priority"];
                if (data.contains("stop_fadeout_ms")) item["stop_fadeout_ms"] = data["stop_fadeout_ms"];
                break;
            }
        }
    });
    return true;
}

} // namespace oae::studio
