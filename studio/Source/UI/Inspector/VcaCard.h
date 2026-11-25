#pragma once

#include "InspectorCardBase.h"

namespace oae::studio {

/** Modular inspector card managing VCA properties. */
class VcaCard : public InspectorCardBase {
public:
    VcaCard(const std::string& path,
            bool expanded,
            AuthoringModel& model,
            int& edit_depth,
            std::function<void()> on_edit_end,
            std::function<void(bool)> onToggle = nullptr);

    ~VcaCard() override = default;

    void refresh() override;
    void reset_to_default() override;
    nlohmann::json serialize_values() const override;
    bool deserialize_values(const nlohmann::json& data) override;

private:
    void build_ui();

    std::string path_;
};

} // namespace oae::studio
