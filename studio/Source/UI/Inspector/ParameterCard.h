#pragma once

#include "InspectorCardBase.h"

namespace oae::studio {

/** Modular inspector card managing global parameter properties and behavior. */
class ParameterCard : public InspectorCardBase {
public:
    ParameterCard(const std::string& name,
                  bool expanded,
                  AuthoringModel& model,
                  int& edit_depth,
                  std::function<void()> on_edit_end,
                  std::function<void(bool)> onToggle = nullptr);

    ~ParameterCard() override = default;

    void refresh() override;
    void reset_to_default() override;
    nlohmann::json serialize_values() const override;
    bool deserialize_values(const nlohmann::json& data) override;

private:
    void build_ui();

    std::string name_;
};

} // namespace oae::studio
