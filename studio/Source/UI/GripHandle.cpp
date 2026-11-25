#include "GripHandle.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

GripHandle::GripHandle(const juce::String& componentId)
{
    setComponentID(componentId);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTooltip("Drag to reorder or float | Double-click to minimize/expand | Right-click for options");
}

void GripHandle::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) {
        return;
    }

    if (isHovered_ || isDragging_) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised).brighter(0.12f));
        g.fillRoundedRectangle(bounds.reduced(1.0f, 2.0f), OaeTheme::Edges::cornerRadius);
    }

    const juce::Colour dotCol = isHovered_ || isDragging_
                                    ? juce::Colour(OaeTheme::Colors::textPrimary)
                                    : juce::Colour(OaeTheme::Colors::textMuted);
    g.setColour(dotCol);

    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    constexpr float dx = 2.0f;
    constexpr float dy = 3.5f;
    constexpr float dotSize = 1.8f;

    for (int row = -2; row <= 1; ++row) {
        const float y = cy + (static_cast<float>(row) + 0.5f) * dy - dotSize * 0.5f;
        g.fillEllipse(cx - dx - dotSize * 0.5f, y, dotSize, dotSize);
        g.fillEllipse(cx + dx - dotSize * 0.5f, y, dotSize, dotSize);
    }
}

void GripHandle::mouseEnter(const juce::MouseEvent&)
{
    isHovered_ = true;
    repaint();
}

void GripHandle::mouseExit(const juce::MouseEvent&)
{
    isHovered_ = false;
    repaint();
}

void GripHandle::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        show_context_menu();
        return;
    }
    isDragging_ = false;
    mouseDownScreen_ = e.getScreenPosition();
}

void GripHandle::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        return;
    }
    if (!isDragging_) {
        if (e.getScreenPosition().getDistanceFrom(mouseDownScreen_) < 4) {
            return;
        }
        isDragging_ = true;
        repaint();
        if (onDragStart) {
            onDragStart(e);
        }
    }
    if (isDragging_ && onDragMove) {
        onDragMove(e);
    }
}

void GripHandle::mouseUp(const juce::MouseEvent& e)
{
    if (isDragging_) {
        isDragging_ = false;
        repaint();
        if (onDragEnd) {
            onDragEnd(e);
        }
    }
}

void GripHandle::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        return;
    }
    isDragging_ = false;
    if (onDoubleClick) {
        onDoubleClick();
    } else if (onToggleMinimized) {
        onToggleMinimized();
    }
}

void GripHandle::show_context_menu()
{
    juce::PopupMenu menu;
    menu.addItem(1, isMinimized_ ? "Expand Bar" : "Minimize Bar");
    menu.addItem(2, isFloating_ ? "Dock Back to Top" : "Float Window");

    UiTestDialogSeam::show_popup_menu("grip_context_menu", menu,
        juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis = juce::Component::SafePointer<GripHandle>(this)](int choice) {
                           if (safeThis == nullptr) {
                               return;
                           }
                           if (choice == 1 && safeThis->onToggleMinimized) {
                               safeThis->onToggleMinimized();
                           } else if (choice == 2 && safeThis->onToggleFloating) {
                               safeThis->onToggleFloating();
                           }
                       });
}

std::vector<TestableTarget> GripHandle::get_testable_targets() const
{
    const auto b = getLocalBounds();
    if (b.isEmpty()) return {};
    return {{ "grip", "drag", b, isEnabled() }};
}

} // namespace oae::studio
