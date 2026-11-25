#pragma once

#include "IInspectorCard.h"
#include "../VectorIcons.h"
#include "../Theme.h"
#include "Testing/UiTestHooks.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace oae::studio {

/** Standardized header component for modular inspector section cards. */
class InspectorCardHeader : public juce::Component, public ITestableCustomControl {
public:
    static constexpr int kHeaderHeight = 28;

    InspectorCardHeader(const juce::String& title,
                        const juce::String& badge,
                        juce::Colour accentColour,
                        bool expanded,
                        IInspectorCard* card = nullptr,
                        std::function<void(bool)> onToggle = nullptr);

    ~InspectorCardHeader() override = default;

    void set_title(const juce::String& title);
    void set_badge(const juce::String& badge);
    void set_accent_colour(juce::Colour colour);
    void set_expanded(bool expanded);
    bool is_expanded() const noexcept { return expanded_; }

    /** Assigns unique header and action button ComponentIDs scoped by parent card ID. */
    void set_card_id(const juce::String& cardId);
    std::vector<TestableTarget> get_testable_targets() const override;

    void set_card(IInspectorCard* card) noexcept { card_ = card; }
    void set_on_toggle(std::function<void(bool)> onToggle) { onToggle_ = std::move(onToggle); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void show_context_menu();

    juce::String title_;
    juce::String badge_;
    juce::Colour accentColour_;
    bool expanded_{ true };
    IInspectorCard* card_{ nullptr };
    std::function<void(bool)> onToggle_;

    VectorIconButton moreButton_{ "Card Actions", VectorIconType::MoreHorizontal };
};

} // namespace oae::studio
