#include "SampleCard.h"

namespace oae::studio {

SampleCard::SampleCard(const std::string& id,
                       bool expanded,
                       AuthoringModel& model,
                       int& edit_depth,
                       std::function<void()> on_edit_end,
                       std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_sample",
                        "sample",
                        "AUDIO ASSET: " + juce::String(id),
                        "Sample",
                        juce::Colour(OaeTheme::Colors::accentAmber),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , id_(id)
{
    build_ui();
}

void SampleCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void SampleCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    std::string file;
    bool is_stream = false;
    if (root.contains("samples") && root["samples"].is_array()) {
        for (const auto& s : root["samples"]) {
            if (s.value("id", "") == id_) {
                file = s.value("file", "");
                is_stream = s.value("stream", false);
                break;
            }
        }
    }

    auto* fileLabel = new juce::Label("", file);
    add_labeled_row("File", fileLabel);

    const auto id = id_;
    auto* streamToggle = new juce::ToggleButton("Stream from Disk");
    streamToggle->setComponentID("inspector_sample_stream");
    streamToggle->setToggleState(is_stream, juce::dontSendNotification);
    streamToggle->onClick = [this, id, streamToggle]() {
        const bool val = streamToggle->getToggleState();
        model_.modify([id, val](nlohmann::json& r) {
            if (r.contains("samples") && r["samples"].is_array()) {
                for (auto& s : r["samples"]) {
                    if (s.value("id", "") == id) {
                        s["stream"] = val;
                        break;
                    }
                }
            }
        });
    };
    add_labeled_row("Loading", streamToggle);
}

void SampleCard::reset_to_default()
{
    const auto id = id_;
    model_.modify([id](nlohmann::json& r) {
        if (r.contains("samples") && r["samples"].is_array()) {
            for (auto& s : r["samples"]) {
                if (s.value("id", "") == id) {
                    s["stream"] = false;
                    break;
                }
            }
        }
    });
}

nlohmann::json SampleCard::serialize_values() const
{
    const auto& root = model_.json();
    nlohmann::json out = nlohmann::json::object();
    if (root.contains("samples") && root["samples"].is_array()) {
        for (const auto& s : root["samples"]) {
            if (s.value("id", "") == id_) {
                out["stream"] = s.value("stream", false);
                out["file"] = s.value("file", "");
                break;
            }
        }
    }
    return out;
}

bool SampleCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto id = id_;
    model_.modify([id, data](nlohmann::json& r) {
        if (r.contains("samples") && r["samples"].is_array()) {
            for (auto& s : r["samples"]) {
                if (s.value("id", "") == id) {
                    if (data.contains("stream")) s["stream"] = data["stream"];
                    break;
                }
            }
        }
    });
    return true;
}

} // namespace oae::studio
