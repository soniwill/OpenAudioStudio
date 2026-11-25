#pragma once

#include "UiTestHooks.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oae::studio::testing {

/** Physical reachability and hit-test result produced before any synthetic UI input is dispatched. */
struct PreflightResult {
    bool ok{ false };
    std::string code;
    std::string detail;
    juce::Point<int> rootPoint;
    juce::Point<int> localPoint;
};

/** Realistic UI interaction driver enforcing physical visibility, bounds, clipping, and hitTest pre-flight. */
class UiInteractionDriver {
public:
    using ViewportState = std::vector<std::pair<juce::Viewport*, juce::Point<int>>>;

    /** Resolves the top-level content root (MainComponent or floating window content) for a component. */
    static juce::Component* find_root_for(juce::Component& comp) noexcept;

    /** Scrolls any ancestor juce::Viewport instances so the target rectangle is visible, returning prior positions. */
    static ViewportState scroll_into_view(juce::Component& target,
                                          std::optional<juce::Rectangle<int>> localSubRect = std::nullopt);

    /** Restores previously saved juce::Viewport scroll positions. */
    static void restore_viewports(const ViewportState& saved);

    /** Runs the mandatory physical pre-flight check (visibility, enabled, bounds, clipping, and hitTest). */
    static PreflightResult preflight(juce::Component& target,
                                     std::optional<juce::Point<int>> localPoint = std::nullopt,
                                     bool autoScrollViewport = true,
                                     std::optional<juce::Rectangle<int>> localSubRect = std::nullopt);

    /** Drains pending async messages on the JUCE MessageManager dispatch loop. */
    static void drain_messages(int durationMs = 2);

    /** Synthesizes a realistic mouse click (left or right) after passing physical pre-flight. */
    static PreflightResult click(juce::Component& target,
                                 bool rightClick = false,
                                 std::optional<juce::Point<int>> localPt = std::nullopt);

    /** Synthesizes a realistic mouse double-click after passing physical pre-flight. */
    static PreflightResult double_click(juce::Component& target,
                                        std::optional<juce::Point<int>> localPt = std::nullopt);

    /** Synthesizes a stepped mouse drag between two local coordinates after passing physical pre-flight. */
    static PreflightResult drag(juce::Component& target,
                                juce::Point<int> localStart,
                                juce::Point<int> localEnd,
                                bool shiftModifier = false,
                                int steps = 6,
                                bool ctrlModifier = false);

    /** Synthesizes a relative mouse drag on a juce::Slider (supporting OaeSlider and Shift fine-drag). */
    static PreflightResult drag_slider(juce::Slider& slider,
                                       double normalizedDelta,
                                       bool shiftModifier = false);

    /** Synthesizes a mouse wheel movement over the target component after passing physical pre-flight. */
    static PreflightResult wheel(juce::Component& target,
                                 float deltaY,
                                 bool shiftModifier = false,
                                 bool ctrlModifier = false,
                                 std::optional<juce::Point<int>> localPt = std::nullopt);

    /** Synthesizes a keyboard key press after verifying pre-flight reachability and focusing the component. */
    static PreflightResult key_press(juce::Component& target, const juce::KeyPress& key);

    /** Selects a ComboBox item by item ID after passing physical pre-flight and validating item state. */
    static PreflightResult select_combo_item(juce::ComboBox& combo, int itemId);

    /** Selects a ComboBox item by 0-based index after passing physical pre-flight. */
    static PreflightResult select_combo_index(juce::ComboBox& combo, int index);

    /** Focuses a TextEditor via click, replaces its text character-by-character, and optionally commits via Return/unfocus. */
    static PreflightResult type_text(juce::TextEditor& editor,
                                     const std::string& text,
                                     bool pressReturn = true,
                                     bool unfocus = true);

    /** Resolves a named target from an ITestableCustomControl and clicks its center. */
    static PreflightResult click_target(juce::Component& customComp,
                                        const std::string& targetId,
                                        bool rightClick = false);

    /** Resolves a named target from an ITestableCustomControl and double-clicks its center. */
    static PreflightResult double_click_target(juce::Component& customComp,
                                               const std::string& targetId);

    /** Resolves a named target from an ITestableCustomControl and drags from its center by deltaOffset. */
    static PreflightResult drag_target(juce::Component& customComp,
                                       const std::string& targetId,
                                       juce::Point<int> deltaOffset,
                                       bool shiftModifier = false,
                                       int steps = 6,
                                       bool ctrlModifier = false);

    /** Resolves a named target from an ITestableCustomControl and dispatches a mouse wheel event at its center. */
    static PreflightResult wheel_target(juce::Component& customComp,
                                        const std::string& targetId,
                                        float deltaY,
                                        bool shiftModifier = false,
                                        bool ctrlModifier = false);

    /** Built-in negative test proving invisible, disabled, and occluded controls fail pre-flight without firing callbacks. */
    static bool run_preflight_negative_self_test(juce::Component& host, std::string& outError);
};

} // namespace oae::studio::testing
