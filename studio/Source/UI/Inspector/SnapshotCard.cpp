#include "SnapshotCard.h"

namespace oae::studio {

SnapshotCard::SnapshotCard(const std::string& path,
                           bool expanded,
                           AuthoringModel& model,
                           int& edit_depth,
                           std::function<void()> on_edit_end,
                           std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_snapshot",
                        "snapshot",
                        "SNAPSHOT: " + juce::String(path),
                        "Snapshot",
                        juce::Colour(OaeTheme::Colors::accentPurple),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
{
    build_ui();
}

void SnapshotCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void SnapshotCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* sn_ptr = nullptr;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == path_) { sn_ptr = &ev; break; }
        }
    }
    if (!sn_ptr) return;
    const auto& sn = *sn_ptr;

    const auto path = path_;
    auto* modeCombo = new juce::ComboBox();
    modeCombo->setComponentID("inspector.snapshot.mode");
    modeCombo->addItem("override", 1);
    modeCombo->addItem("blending", 2);
    modeCombo->setSelectedId(sn.value("snapshot_mode", "override") == "blending" ? 2 : 1, juce::dontSendNotification);
    modeCombo->onChange = [this, path, modeCombo]() {
        const auto mode = modeCombo->getText().toStdString();
        model_.modify([path, mode](nlohmann::json& r) {
            for (auto& item : r["events"]) if (item.value("path", "") == path) item["snapshot_mode"] = mode;
        });
    };
    add_labeled_row("Mode", modeCombo);

    auto* fadeSlider = create_slider(0.0, 5000.0, sn.value("fade_in_ms", 100.0), " ms",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                for (auto& item : r["events"]) if (item.value("path", "") == path) item["fade_in_ms"] = v;
            });
        });
    fadeSlider->setComponentID("inspector.snapshot.fade");
    add_labeled_row("Fade In", fadeSlider);
}

void SnapshotCard::reset_to_default()
{
    const auto path = path_;
    model_.modify([path](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path) {
                item["snapshot_mode"] = "override";
                item["fade_in_ms"] = 100.0;
                break;
            }
        }
    });
}

nlohmann::json SnapshotCard::serialize_values() const
{
    const auto& root = model_.json();
    if (!root.contains("events") || !root["events"].is_array()) return nlohmann::json::object();
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == path_) {
            nlohmann::json out = nlohmann::json::object();
            out["snapshot_mode"] = ev.value("snapshot_mode", "override");
            out["fade_in_ms"] = ev.value("fade_in_ms", 100.0);
            return out;
        }
    }
    return nlohmann::json::object();
}

bool SnapshotCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    model_.modify([path, data](nlohmann::json& r) {
        if (!r.contains("events") || !r["events"].is_array()) return;
        for (auto& item : r["events"]) {
            if (item.value("path", "") == path) {
                if (data.contains("snapshot_mode")) item["snapshot_mode"] = data["snapshot_mode"];
                if (data.contains("fade_in_ms")) item["fade_in_ms"] = data["fade_in_ms"];
                break;
            }
        }
    });
    return true;
}

} // namespace oae::studio
