#pragma once

#include "InspectorCardBase.h"

namespace oae::studio {

/** Contextual inspector card for LayerMaskSpec parameter activation range and fade. */
class MaskTransitionCard : public InspectorCardBase {
public:
    MaskTransitionCard(const std::string& path,
                       size_t track_index,
                       bool expanded,
                       AuthoringModel& model,
                       int& edit_depth,
                       std::function<void()> on_edit_end,
                       std::function<void(bool)> onToggle = nullptr,
                       std::function<void()> onSwitchToTimeline = nullptr);

    void refresh() override;
    void reset_to_default() override;
    nlohmann::json serialize_values() const override;
    bool deserialize_values(const nlohmann::json& payload) override;

private:
    void build_ui();

    std::string path_;
    size_t track_index_{ 0 };
    std::function<void()> on_switch_to_timeline_;
};

} // namespace oae::studio
