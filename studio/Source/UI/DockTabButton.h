#pragma once

#include <functional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace oae::studio {

/** A graphite dock tab with independent selection, keyboard focus and drag state. */
class DockTabButton : public juce::TextButton {
public:
    enum class Mode {
        tab,
        action
    };

    explicit DockTabButton(juce::String title, Mode mode = Mode::tab)
        : juce::TextButton(std::move(title)), mode_(mode)
    {
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::dockHeaderBg));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(OaeTheme::Colors::dockTabActiveBg));
        setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::dockTabText));
        setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::dockTabActiveText));
    }

    /** Sets whether this tab represents the selected panel in its group. */
    void setTabSelected(bool selected)
    {
        if (tabSelected_ == selected) return;
        tabSelected_ = selected;
        repaint();
    }

    /** Shows a single-panel dock title with neutral header styling. */
    void setNeutralTitle(bool neutral)
    {
        if (neutralTitle_ == neutral) return;
        neutralTitle_ = neutral;
        repaint();
    }

    /** Returns whether this tab represents the selected panel in its group. */
    bool isTabSelected() const noexcept { return tabSelected_; }

    /** Chooses dock-tab painting or the standard TextButton action style. */
    void setMode(Mode mode)
    {
        if (mode_ == mode) return;
        mode_ = mode;
        repaint();
    }

    /** Controls the visible keyboard-focus outline without changing focus behavior. */
    void setShowFocusIndicator(bool show)
    {
        if (showFocusIndicator_ == show) return;
        showFocusIndicator_ = show;
        repaint();
    }

    /** Configures the text colour used when this tab is selected. */
    void setActiveTextColour(juce::Colour colour)
    {
        activeTextColour_ = colour;
        hasCustomActiveTextColour_ = true;
        repaint();
    }

    /** Configures the border colour used when this tab is selected. */
    void setActiveBorderColour(juce::Colour colour)
    {
        activeBorderColour_ = colour;
        repaint();
    }

    std::function<void(const juce::MouseEvent&)> onTabDrag;
    std::function<void(const juce::MouseEvent&)> onTabDrop;

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStarted_ = false;
        juce::TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragStarted_ && onTabDrag && event.getDistanceFromDragStart() >= dragThresholdPx) {
            dragStarted_ = true;
        }

        if (dragStarted_) {
            setState(juce::Button::buttonNormal);
            if (onTabDrag) onTabDrag(event);
            return;
        }

        juce::TextButton::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (!dragStarted_) {
            juce::TextButton::mouseUp(event);
            return;
        }

        setState(juce::Button::buttonNormal);
        juce::TextButton::mouseUp(event);
        dragStarted_ = false;
        if (onTabDrop) onTabDrop(event);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        if (mode_ == Mode::action) {
            juce::TextButton::paintButton(g, isMouseOverButton, isButtonDown);
        } else {
            paintDockTab(g, isMouseOverButton, isButtonDown);
        }

        if (showFocusIndicator_ && hasKeyboardFocus(true)) {
            const auto focusBounds = getLocalBounds().toFloat().reduced(1.5f);
            g.setColour(juce::Colour(OaeTheme::Colors::dockTabFocus));
            g.drawRect(focusBounds, 1.5f);
        }
    }

private:
    static constexpr int dragThresholdPx = 5;

    void paintDockTab(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
    {
        auto background = juce::Colour(OaeTheme::Colors::dockHeaderBg);
        if (neutralTitle_) {
            background = juce::Colour(OaeTheme::Colors::dockHeaderBg);
        } else if (tabSelected_) {
            background = juce::Colour(OaeTheme::Colors::dockTabActiveBg);
        } else if (isMouseOverButton) {
            background = juce::Colour(OaeTheme::Colors::dockTabHoverBg);
        } else {
            background = juce::Colour(OaeTheme::Colors::dockTabInactiveBg);
        }

        if (isButtonDown) background = background.darker(OaeTheme::States::pressedDarkening);
        if (!isEnabled()) background = background.withAlpha(0.55f);

        const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
        const float radius = OaeTheme::Edges::cornerRadius;
        g.setColour(background);
        if (neutralTitle_) {
            g.fillRect(getLocalBounds().toFloat());
        } else {
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(tabSelected_ ? activeBorderColour_ : juce::Colour(OaeTheme::Colors::dockTabBorder));
            g.drawRoundedRectangle(bounds, radius, OaeTheme::Edges::borderStroke);
        }

        const auto activeText = hasCustomActiveTextColour_
                                    ? activeTextColour_
                                    : juce::Colour(OaeTheme::Colors::dockTabActiveText);
        auto textColour = neutralTitle_ ? juce::Colour(OaeTheme::Colors::textSecondary)
                                        : tabSelected_ ? activeText
                                                       : juce::Colour(OaeTheme::Colors::dockTabText);
        if (!isEnabled()) textColour = juce::Colour(OaeTheme::Colors::textMuted);
        g.setColour(textColour);
        g.setFont(getLookAndFeel().getTextButtonFont(*this, getHeight()));
        const auto alignment = neutralTitle_ ? juce::Justification::centredLeft
                                             : juce::Justification::centred;
        g.drawFittedText(getButtonText(),
                         getLocalBounds().reduced(OaeTheme::Edges::buttonTextInsetX, 0),
                         alignment,
                         1);
    }

    Mode mode_{ Mode::tab };
    bool tabSelected_{ false };
    bool neutralTitle_{ false };
    bool showFocusIndicator_{ true };
    bool dragStarted_{ false };
    bool hasCustomActiveTextColour_{ false };
    juce::Colour activeTextColour_{ OaeTheme::Colors::dockTabActiveText };
    juce::Colour activeBorderColour_{ OaeTheme::Colors::accentCyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DockTabButton)
};

} // namespace oae::studio
