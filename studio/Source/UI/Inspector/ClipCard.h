#pragma once

#include "InspectorCardBase.h"

namespace oae::studio {

/** Modular inspector card managing instrument parameters, timing, envelopes, fades, and AHDSR. */
class ClipCard : public InspectorCardBase {
public:
    ClipCard(const std::string& path,
             size_t track_index,
             size_t instrument_index,
             bool expanded,
             AuthoringModel& model,
             int& edit_depth,
             std::function<void()> on_edit_end,
             std::function<void(bool)> onToggle = nullptr);

    ~ClipCard() override = default;

    void refresh() override;
    void reset_to_default() override;
    nlohmann::json serialize_values() const override;
    bool deserialize_values(const nlohmann::json& data) override;

private:
    void build_ui();

    std::string path_;
    size_t track_index_;
    size_t instrument_index_;
};

} // namespace oae::studio
