#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace oae::studio {

/** Color presets mapped to OaeTheme::Colors accents for tutorial spotlight overlays. */
enum class HighlightColor {
    Cyan,
    Amber,
    Green
};

/** Non-interactive spotlight overlay drawn over MainComponent to highlight a target control. */
class TutorialHighlightOverlay : public juce::Component {
public:
    /** Initializes the overlay as hidden and transparent to mouse and keyboard input. */
    TutorialHighlightOverlay();

    /** Returns false so pointer hit-testing and getComponentAt always reach underlying controls. */
    bool hitTest(int x, int y) override;

    /** Renders the four-quadrant dim scrim, accent border, and unclipped label pill. */
    void paint(juce::Graphics& g) override;

    /** Checks whether comp and its ancestors below root are marked visible without requiring desktop display. */
    static bool is_visible_under_root(const juce::Component* comp, const juce::Component* root) noexcept;

    /** Activates or updates the spotlight over target using the specified label, dim opacity, and accent color. */
    void set_highlight(juce::Component* target,
                       const juce::String& label,
                       float dimOpacity,
                       HighlightColor color);

    /** Deactivates and hides the overlay and schedules a parent repaint. */
    void clear();

    /** Synchronizes visibility when the highlighted target is hidden, resized to zero area, or destroyed. */
    void refresh_target_visibility();

    /** Returns whether a spotlight highlight is currently active. */
    bool is_active() const noexcept { return active_; }

    /** Returns the currently highlighted target component, or nullptr if cleared or destroyed. */
    juce::Component* target_component() const noexcept { return target_.getComponent(); }

    /** Returns the active badge label text. */
    const juce::String& label() const noexcept { return label_; }

    /** Returns the active peripheral dim opacity clamped to [0.0, 0.85]. */
    float dim_opacity() const noexcept { return dimOpacity_; }

    /** Returns the active accent color preset. */
    HighlightColor color() const noexcept { return color_; }

private:
    static juce::Colour resolve_accent(HighlightColor color) noexcept;
    static juce::Rectangle<int> compute_pill_bounds(const juce::Rectangle<int>& fullBounds,
                                                    const juce::Rectangle<int>& targetRect,
                                                    int pillWidth,
                                                    int pillHeight,
                                                    int margin) noexcept;

    bool active_{ false };
    juce::Component::SafePointer<juce::Component> target_;
    juce::String label_;
    float dimOpacity_{ 0.45f };
    HighlightColor color_{ HighlightColor::Cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TutorialHighlightOverlay)
};

} // namespace oae::studio
