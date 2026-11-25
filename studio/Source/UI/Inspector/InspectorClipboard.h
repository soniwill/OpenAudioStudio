#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace oae::studio {

/** In-memory clipboard for copying and pasting parameter values between compatible inspector cards. */
class InspectorClipboard {
public:
    /** Stores payload for the given card type ID. */
    static void copy(std::string_view card_type_id, const nlohmann::json& payload);

    /** Returns true if the clipboard currently has data matching the given card type ID. */
    static bool has_data_for(std::string_view card_type_id);

    /** Returns the current payload, or null json if none or type mismatch. */
    static nlohmann::json get_payload(std::string_view card_type_id);

    /** Clears the clipboard contents. */
    static void clear();

    /** Returns the active card type ID currently in clipboard. */
    static std::string current_type_id();

private:
    static std::string active_type_id_;
    static nlohmann::json active_payload_;
};

} // namespace oae::studio
