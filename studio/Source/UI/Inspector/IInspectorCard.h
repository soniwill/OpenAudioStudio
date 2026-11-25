#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nlohmann/json.hpp>
#include <string_view>

namespace oae::studio {

/** Abstract interface for modular inspector section cards. */
class IInspectorCard : public juce::Component {
public:
    ~IInspectorCard() override = default;

    /** Returns the unique identifier for this card type (e.g. "event_header", "track", "clip", "bus", etc.). */
    virtual std::string_view get_card_type_id() const = 0;

    /** Refreshes UI controls to match current model state. */
    virtual void refresh() = 0;

    /** Resets the parameters managed by this card to factory defaults with an atomic undo step. */
    virtual void reset_to_default() = 0;

    /** Serializes the editable parameters of this card into a JSON object for clipboard copy. */
    virtual nlohmann::json serialize_values() const = 0;

    /** Deserializes values from a JSON object and applies them with an atomic undo step.
     * Returns true if applied successfully, false if incompatible. */
    virtual bool deserialize_values(const nlohmann::json& data) = 0;

    /** Returns true if the card is currently expanded. */
    virtual bool is_card_expanded() const noexcept = 0;

    /** Sets the expansion state of the card. */
    virtual void set_card_expanded(bool expanded) = 0;

    /** Calculates the required height for a given width. */
    virtual int height_for_width(int width) = 0;
};

} // namespace oae::studio
