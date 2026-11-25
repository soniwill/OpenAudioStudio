#include "InspectorClipboard.h"

namespace oae::studio {

std::string InspectorClipboard::active_type_id_;
nlohmann::json InspectorClipboard::active_payload_;

void InspectorClipboard::copy(std::string_view card_type_id, const nlohmann::json& payload)
{
    active_type_id_ = std::string(card_type_id);
    active_payload_ = payload;
}

bool InspectorClipboard::has_data_for(std::string_view card_type_id)
{
    return !active_type_id_.empty() && active_type_id_ == card_type_id && !active_payload_.is_null();
}

nlohmann::json InspectorClipboard::get_payload(std::string_view card_type_id)
{
    if (has_data_for(card_type_id)) {
        return active_payload_;
    }
    return nlohmann::json(nullptr);
}

void InspectorClipboard::clear()
{
    active_type_id_.clear();
    active_payload_ = nullptr;
}

std::string InspectorClipboard::current_type_id()
{
    return active_type_id_;
}

} // namespace oae::studio
