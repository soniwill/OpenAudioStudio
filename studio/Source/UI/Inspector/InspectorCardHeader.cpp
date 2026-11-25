#include "InspectorCardHeader.h"
#include "InspectorClipboard.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

InspectorCardHeader::InspectorCardHeader(const juce::String& title,
                                         const juce::String& badge,
                                         juce::Colour accentColour,
                                         bool expanded,
                                         IInspectorCard* card,
                                         std::function<void(bool)> onToggle)
    : title_(title)
    , badge_(badge)
    , accentColour_(accentColour)
    , expanded_(expanded)
    , card_(card)
    , onToggle_(std::move(onToggle))
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    moreButton_.setTooltip("Card Actions");
    moreButton_.setComponentID("inspector_card_more_actions");
    moreButton_.onClick = [this]() { show_context_menu(); };
    addAndMakeVisible(moreButton_);
}

void InspectorCardHeader::set_card_id(const juce::String& cardId)
{
    setComponentID(cardId + ".header");
    moreButton_.setComponentID(cardId + ".more_actions");
}

std::vector<TestableTarget> InspectorCardHeader::get_testable_targets() const
{
    const int collapseW = std::max(0, getWidth() - 28);
    if (collapseW <= 0 || getHeight() <= 0) return {};
    return {{ "collapse", "click", { 0, 0, collapseW, getHeight() }, isEnabled() }};
}

void InspectorCardHeader::set_title(const juce::String& title)
{
    if (title_ != title) {
        title_ = title;
        repaint();
    }
}

void InspectorCardHeader::set_badge(const juce::String& badge)
{
    if (badge_ != badge) {
        badge_ = badge;
        repaint();
    }
}

void InspectorCardHeader::set_accent_colour(juce::Colour colour)
{
    if (accentColour_ != colour) {
        accentColour_ = colour;
        repaint();
    }
}

void InspectorCardHeader::set_expanded(bool expanded)
{
    if (expanded_ != expanded) {
        expanded_ = expanded;
        repaint();
    }
}

void InspectorCardHeader::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 4.0f || bounds.getHeight() < 4.0f) {
        return;
    }

    // Header background
    g.setColour(juce::Colour(OaeTheme::Colors::cardHeaderBg));
    g.fillRect(bounds);

    // Bottom border if expanded
    if (expanded_) {
        g.setColour(juce::Colour(OaeTheme::Colors::cardBorder));
        g.drawHorizontalLine(getHeight() - 1, bounds.getX(), bounds.getRight());
    }

    // Chevron
    juce::Path chevron;
    const float cx = 13.0f;
    const float cy = bounds.getCentreY();
    if (expanded_) {
        chevron.addTriangle(cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 3.0f);
    } else {
        chevron.addTriangle(cx - 2.0f, cy - 3.5f, cx + 3.0f, cy, cx - 2.0f, cy + 3.5f);
    }
    g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
    g.fillPath(chevron);

    // Right-aligned plain text badge in accentCyan (placed left of moreButton_)
    const float buttonLeft = bounds.getRight() - 26.0f;
    float badgeLeft = buttonLeft;
    if (badge_.isNotEmpty() && bounds.getWidth() >= 140.0f) {
        const float badgeW = std::min(88.0f, std::max(34.0f, static_cast<float>(badge_.length()) * 6.2f + 6.0f));
        badgeLeft = buttonLeft - badgeW - 6.0f;
        const auto badgeRect = juce::Rectangle<float>(badgeLeft, 0.0f, badgeW, bounds.getHeight());
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.setFont(juce::FontOptions(OaeTheme::Typography::micro, juce::Font::bold));
        g.drawText(badge_, badgeRect, juce::Justification::centredRight, true);
    }

    // Title
    g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
    g.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
    const float titleX = 24.0f;
    const float titleW = std::max(10.0f, badgeLeft - titleX - 4.0f);
    g.drawText(title_,
               juce::Rectangle<float>(titleX, 0.0f, titleW, bounds.getHeight()),
               juce::Justification::centredLeft, true);
}

void InspectorCardHeader::resized()
{
    moreButton_.setBounds(getWidth() - 24, (getHeight() - 20) / 2, 20, 20);
}

void InspectorCardHeader::mouseUp(const juce::MouseEvent& e)
{
    if (e.getPosition().x < getWidth() - 26) {
        expanded_ = !expanded_;
        repaint();
        if (onToggle_) {
            onToggle_(expanded_);
        }
    }
}

void InspectorCardHeader::show_context_menu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Reset to Default", card_ != nullptr);
    menu.addSeparator();
    menu.addItem(2, "Copy Values", card_ != nullptr);
    const bool canPaste = (card_ != nullptr) && InspectorClipboard::has_data_for(card_->get_card_type_id());
    menu.addItem(3, "Paste Values", canPaste);

    juce::Component::SafePointer<InspectorCardHeader> safeThis(this);
    UiTestDialogSeam::show_popup_menu("inspector_card_more", menu,
        juce::PopupMenu::Options().withTargetComponent(&moreButton_),
        [safeThis](int result) {
        if (!safeThis || !safeThis->card_) return;
        if (result == 1) {
            safeThis->card_->reset_to_default();
        } else if (result == 2) {
            InspectorClipboard::copy(safeThis->card_->get_card_type_id(), safeThis->card_->serialize_values());
        } else if (result == 3) {
            safeThis->card_->deserialize_values(InspectorClipboard::get_payload(safeThis->card_->get_card_type_id()));
        }
    });
}

} // namespace oae::studio
