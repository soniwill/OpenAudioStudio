#pragma once

#include "Theme.h"
#include "Testing/UiTestHooks.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace oae::studio {

/** Compact vertical grip handle (~12px) for toolbar docks with drag, double-click, and context menu gestures. */
class GripHandle : public juce::Component, public juce::SettableTooltipClient, public ITestableCustomControl {
public:
    static constexpr int kDefaultWidth = 12;

    /** Constructs a GripHandle with optional accessibility/component ID. */
    explicit GripHandle(const juce::String& componentId = "grip_handle");
    ~GripHandle() override = default;

    /** Sets whether the owning toolbar is currently in minimized (compact) mode for context menu labels. */
    void set_minimized(bool minimized) noexcept { isMinimized_ = minimized; }

    /** Sets whether the owning toolbar is currently floating in an independent window. */
    void set_floating(bool floating) noexcept { isFloating_ = floating; }

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    std::vector<TestableTarget> get_testable_targets() const override;

    std::function<void(const juce::MouseEvent&)> onDragStart;
    std::function<void(const juce::MouseEvent&)> onDragMove;
    std::function<void(const juce::MouseEvent&)> onDragEnd;
    std::function<void()> onDoubleClick;
    std::function<void()> onToggleMinimized;
    std::function<void()> onToggleFloating;

private:
    void show_context_menu();

    bool isHovered_ = false;
    bool isDragging_ = false;
    bool isMinimized_ = false;
    bool isFloating_ = false;
    juce::Point<int> mouseDownScreen_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GripHandle)
};

} // namespace oae::studio
