#include "ParameterCard.h"

namespace oae::studio {

ParameterCard::ParameterCard(const std::string& name,
                             bool expanded,
                             AuthoringModel& model,
                             int& edit_depth,
                             std::function<void()> on_edit_end,
                             std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_parameter",
                        "parameter",
                        "PARAMETER: " + juce::String(name),
                        "Param",
                        juce::Colour(OaeTheme::Colors::accentCyan),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , name_(name)
{
    build_ui();
}

void ParameterCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void ParameterCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* p_ptr = nullptr;
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& p : root["global_parameters"]) {
            if (p.value("name", "") == name_) { p_ptr = &p; break; }
        }
    }
    if (!p_ptr) return;
    const auto& param = *p_ptr;

    append_parameter_editor_controls(body_, comps_, y_, model_, edit_depth_, on_edit_end_, param, false, {}, "inspector.param");
}

void ParameterCard::reset_to_default()
{
    const auto* live = find_live_param(model_.json(), false, {}, name_);
    if (!live) return;
    auto next = *live;
    const auto kind = next.value("kind", "continuous");
    next["min"] = 0.0;
    next["max"] = (kind == "discrete") ? 10.0 : 1.0;
    next["default_value"] = 0.0;
    next["seek_speed"] = 0.0;
    next["seek_speed_down"] = 0.0;
    next["velocity"] = 0.0;
    next["hold_during_playback"] = false;
    model_.update_global_parameter(name_, next);
}

nlohmann::json ParameterCard::serialize_values() const
{
    const auto* live = find_live_param(model_.json(), false, {}, name_);
    if (!live) return nlohmann::json::object();
    nlohmann::json out = *live;
    return out;
}

bool ParameterCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto* live = find_live_param(model_.json(), false, {}, name_);
    if (!live) return false;
    auto next = *live;
    if (data.contains("kind")) next["kind"] = data["kind"];
    if (data.contains("min")) next["min"] = data["min"];
    if (data.contains("max")) next["max"] = data["max"];
    if (data.contains("default_value")) next["default_value"] = data["default_value"];
    if (data.contains("labels")) next["labels"] = data["labels"];
    if (data.contains("seek_speed")) next["seek_speed"] = data["seek_speed"];
    if (data.contains("seek_speed_down")) next["seek_speed_down"] = data["seek_speed_down"];
    if (data.contains("velocity")) next["velocity"] = data["velocity"];
    if (data.contains("hold_during_playback")) next["hold_during_playback"] = data["hold_during_playback"];
    return model_.update_global_parameter(name_, next);
}

} // namespace oae::studio
