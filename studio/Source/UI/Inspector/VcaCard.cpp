#include "VcaCard.h"

namespace oae::studio {

VcaCard::VcaCard(const std::string& path,
                 bool expanded,
                 AuthoringModel& model,
                 int& edit_depth,
                 std::function<void()> on_edit_end,
                 std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_vca",
                        "vca",
                        "VCA: " + juce::String(path),
                        "VCA",
                        juce::Colour(OaeTheme::Colors::accentGreen),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
{
    build_ui();
}

void VcaCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void VcaCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* vca_ptr = nullptr;
    if (root.contains("vcas") && root["vcas"].is_array()) {
        for (const auto& v : root["vcas"]) {
            if (v.value("path", "") == path_) { vca_ptr = &v; break; }
        }
    }
    const double curVol = vca_ptr ? vca_ptr->value("volume", 1.0) : 1.0;

    const auto path = path_;
    auto* volSlider = create_slider(0.0, 2.0, curVol, "",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                if (r.contains("vcas") && r["vcas"].is_array()) {
                    for (auto& item : r["vcas"]) {
                        if (item.value("path", "") == path) { item["volume"] = v; break; }
                    }
                }
            });
        });
    volSlider->setComponentID("inspector_volume");
    volSlider->setDoubleClickReturnValue(true, 1.0, {});
    add_labeled_row("Volume", volSlider);
}

void VcaCard::reset_to_default()
{
    const auto path = path_;
    model_.modify([path](nlohmann::json& r) {
        if (r.contains("vcas") && r["vcas"].is_array()) {
            for (auto& item : r["vcas"]) {
                if (item.value("path", "") == path) {
                    item["volume"] = 1.0;
                    break;
                }
            }
        }
    });
}

nlohmann::json VcaCard::serialize_values() const
{
    const auto& root = model_.json();
    nlohmann::json out = nlohmann::json::object();
    if (root.contains("vcas") && root["vcas"].is_array()) {
        for (const auto& v : root["vcas"]) {
            if (v.value("path", "") == path_) {
                out["volume"] = v.value("volume", 1.0);
                break;
            }
        }
    }
    return out;
}

bool VcaCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    model_.modify([path, data](nlohmann::json& r) {
        if (r.contains("vcas") && r["vcas"].is_array()) {
            for (auto& item : r["vcas"]) {
                if (item.value("path", "") == path) {
                    if (data.contains("volume")) item["volume"] = data["volume"];
                    break;
                }
            }
        }
    });
    return true;
}

} // namespace oae::studio
