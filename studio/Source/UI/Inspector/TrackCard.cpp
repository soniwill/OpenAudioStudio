#include "TrackCard.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

namespace {
juce::String make_track_title(AuthoringModel& model, const std::string& path, size_t ti)
{
    const auto name = model.get_track_name(path, ti);
    return juce::String::fromUTF8("TRACK \xe2\x80\x94 ") + juce::String::fromUTF8(name.c_str());
}

juce::Colour make_track_colour(AuthoringModel& model, const std::string& path, size_t ti)
{
    const auto hex = model.get_track_color(path, ti);
    return OaeTheme::TrackColors::parseHexColor(hex, ti);
}
} // namespace

TrackCard::TrackCard(const std::string& path,
                     size_t track_index,
                     bool expanded,
                     AuthoringModel& model,
                     int& edit_depth,
                     std::function<void()> on_edit_end,
                     std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_track",
                        "track",
                        make_track_title(model, path, track_index),
                        "Track " + juce::String(track_index + 1),
                        make_track_colour(model, path, track_index),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
    , track_index_(track_index)
{
    build_ui();
}

void TrackCard::refresh()
{
    header_.set_title(make_track_title(model_, path_, track_index_));
    header_.set_accent_colour(make_track_colour(model_, path_, track_index_));
    build_ui();
    resized();
    repaint();
}

void TrackCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* trk_ptr = nullptr;
    size_t track_count = 0;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                track_count = ev["tracks"].size();
                if (track_index_ < track_count) {
                    trk_ptr = &ev["tracks"][track_index_];
                }
                break;
            }
        }
    }
    if (!trk_ptr) return;
    const auto& trk = *trk_ptr;

    const auto path = path_;
    const auto ti = track_index_;
    const auto trackColorHex = model_.get_track_color(path, ti);
    const auto trackName = model_.get_track_name(path, ti);

    auto* nameEdit = new juce::TextEditor();
    nameEdit->setComponentID("inspector_track_name");
    nameEdit->setText(juce::String::fromUTF8(trackName.c_str()), false);
    auto commit_track_name = [this, path, ti, nameEdit]() {
        const auto next = nameEdit->getText().trim().toStdString();
        if (!next.empty()) {
            model_.set_track_name(path, ti, next);
        }
    };
    nameEdit->onReturnKey = commit_track_name;
    nameEdit->onFocusLost = commit_track_name;
    add_labeled_row("Track Name", nameEdit);

    auto* colorStrip = new TrackColorSwatchStrip(model_, path, ti, trackColorHex);
    add_labeled_row("Track Color", colorStrip);

    auto* tVol = create_slider(-80.0, 10.0, trk.value("volume_db", 0.0), " dB",
        [this, path, ti](double v) { model_.set_track_volume(path, ti, v); });
    tVol->setComponentID("inspector_track_volume");
    tVol->setDoubleClickReturnValue(true, 0.0, {});
    add_labeled_row("Volume", tVol);

    auto* tPan = create_slider(-1.0, 1.0, trk.value("pan", 0.0), "",
        [this, path, ti](double v) { model_.set_track_pan(path, ti, v); });
    tPan->setComponentID("inspector_track_pan");
    tPan->setDoubleClickReturnValue(true, 0.0, {});
    add_labeled_row("Pan", tPan);

    auto* stateLbl = new juce::Label("", "Routing");
    stateLbl->setFont(juce::FontOptions(11.0f));
    stateLbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    register_layout_item(body_, stateLbl, y_, 80, 22);
    body_.addAndMakeVisible(stateLbl);
    comps_.emplace_back(stateLbl);

    auto* muteBtn = new juce::ToggleButton("Mute");
    muteBtn->setComponentID("inspector_track_mute");
    muteBtn->setToggleState(model_.is_track_muted(path, ti), juce::dontSendNotification);
    muteBtn->onClick = [this, path, ti, muteBtn]() {
        model_.set_track_muted(path, ti, muteBtn->getToggleState());
    };
    register_layout_item(body_, muteBtn, y_, 76, 22);
    body_.addAndMakeVisible(muteBtn);
    comps_.emplace_back(muteBtn);

    auto* soloBtn = new juce::ToggleButton("Solo");
    soloBtn->setComponentID("inspector_track_solo");
    soloBtn->setToggleState(model_.is_track_solo(path, ti), juce::dontSendNotification);
    soloBtn->onClick = [this, path, ti, soloBtn]() {
        model_.set_track_solo(path, ti, soloBtn->getToggleState());
    };
    register_layout_item(body_, soloBtn, y_, 76, 22);
    body_.addAndMakeVisible(soloBtn);
    comps_.emplace_back(soloBtn);
    y_ += 26;

    auto* addInstBtn = new juce::TextButton("+ Add Inst");
    addInstBtn->setComponentID("inspector.track.add_inst");
    register_layout_item(body_, addInstBtn, y_, 120, 22);
    addInstBtn->onClick = [this, path, ti, addInstBtn]() {
        juce::PopupMenu m;
        m.addItem(1, "Single Instrument");
        m.addItem(2, "Multi Instrument");
        m.addItem(3, "Scatterer Instrument");
        m.addItem(4, "Programmer Instrument");
        const auto targets = model_.get_instrument_targets(path);
        juce::PopupMenu event_menu, command_menu;
        for (size_t i = 0; i < targets.size(); ++i) {
            event_menu.addItem(static_cast<int>(6 + i), targets[i]);
            command_menu.addItem(static_cast<int>(6 + targets.size() + i), targets[i]);
        }
        m.addSubMenu("Event Instrument", event_menu, !targets.empty());
        m.addSubMenu("Command Instrument", command_menu, !targets.empty());
        m.addItem(5, "Silence Instrument");
        juce::Component::SafePointer<juce::TextButton> button(addInstBtn);
        UiTestDialogSeam::show_popup_menu("inspector_add_inst", m,
            juce::PopupMenu::Options().withTargetComponent(addInstBtn),
            [this, button, path, ti, targets](int result) {
                if (!button || result == 0) return;
                if (result == 1) model_.add_instrument(path, ti, "single");
                else if (result == 2) model_.add_instrument(path, ti, "multi");
                else if (result == 3) model_.add_instrument(path, ti, "scatterer");
                else if (result == 4) model_.add_instrument(path, ti, "programmer");
                else if (result == 5) model_.add_instrument(path, ti, "silence");
                else if (result >= 6) {
                    const auto index = static_cast<size_t>(result - 6);
                    if (index < targets.size()) model_.add_instrument(path, ti, "event", targets[index]);
                    else if (index - targets.size() < targets.size()) {
                        model_.add_instrument(path, ti, "command", targets[index - targets.size()]);
                    }
                }
            });
    };
    body_.addAndMakeVisible(addInstBtn);
    comps_.emplace_back(addInstBtn);

    auto* delTrackBtn = new juce::TextButton("Delete Track");
    delTrackBtn->setComponentID("inspector.track.delete");
    delTrackBtn->setEnabled(track_count > 1);
    register_layout_item(body_, delTrackBtn, y_, 120, 22);
    delTrackBtn->onClick = [this, path, ti]() {
        model_.remove_track(path, ti);
    };
    body_.addAndMakeVisible(delTrackBtn);
    comps_.emplace_back(delTrackBtn);
    y_ += 26;

    append_param_automations_section(body_, comps_, y_, model_, edit_depth_, on_edit_end_, path, static_cast<int>(ti), trk, "inspector.track");
}

void TrackCard::reset_to_default()
{
    const auto path = path_;
    const auto ti = track_index_;
    model_.modify([path, ti](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path && item.contains("tracks") && ti < item["tracks"].size()) {
                item["tracks"][ti]["volume_db"] = 0.0;
                item["tracks"][ti]["pan"] = 0.0;
                break;
            }
        }
    });
    model_.set_track_muted(path, ti, false);
    model_.set_track_solo(path, ti, false);
}

nlohmann::json TrackCard::serialize_values() const
{
    const auto& root = model_.json();
    if (!root.contains("events") || !root["events"].is_array()) return nlohmann::json::object();
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == path_ && ev.contains("tracks") && track_index_ < ev["tracks"].size()) {
            const auto& trk = ev["tracks"][track_index_];
            nlohmann::json out = nlohmann::json::object();
            out["volume_db"] = trk.value("volume_db", 0.0);
            out["pan"] = trk.value("pan", 0.0);
            out["color"] = model_.get_track_color(path_, track_index_);
            out["mute"] = model_.is_track_muted(path_, track_index_);
            out["solo"] = model_.is_track_solo(path_, track_index_);
            return out;
        }
    }
    return nlohmann::json::object();
}

bool TrackCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    const auto ti = track_index_;
    model_.modify([path, ti, data](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path && item.contains("tracks") && ti < item["tracks"].size()) {
                if (data.contains("volume_db")) item["tracks"][ti]["volume_db"] = data["volume_db"];
                if (data.contains("pan")) item["tracks"][ti]["pan"] = data["pan"];
                break;
            }
        }
    });
    if (data.contains("color")) {
        model_.set_track_color(path, ti, data["color"].get<std::string>());
    }
    if (data.contains("mute")) {
        model_.set_track_muted(path, ti, data["mute"].get<bool>());
    }
    if (data.contains("solo")) {
        model_.set_track_solo(path, ti, data["solo"].get<bool>());
    }
    return true;
}

} // namespace oae::studio
