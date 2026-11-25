#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace oae::studio {

/** Slider with a relative drag that can change sensitivity without jumping. */
class OaeSlider : public juce::Slider {
public:
    using juce::Slider::Slider;

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragging_ = event.mods.isLeftButtonDown() && isEnabled();
        if (dragging_) {
            lastPointer_ = dragPointer_ = event.position;
            snapsToMouse_ = getSliderSnapsToMousePosition();
            setSliderSnapsToMousePosition(false);
        }
        juce::Slider::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragging_) {
            juce::Slider::mouseDrag(event);
            return;
        }

        const auto delta = event.position - lastPointer_;
        lastPointer_ = event.position;
        float scale = event.mods.isShiftDown() ? OaeTheme::Sliders::fineDragScale : 1.0f;
        if (isHorizontal() || isVertical()) {
            const auto layout = getLookAndFeel().getSliderLayout(*this);
            const int span = isHorizontal() ? layout.sliderBounds.getWidth() : layout.sliderBounds.getHeight();
            if (span > 0) {
                scale *= static_cast<float>(getMouseDragSensitivity()) / static_cast<float>(span);
            }
        }
        dragPointer_ += delta * scale;
        juce::Slider::mouseDrag(event.withNewPosition(dragPointer_));
    }

    std::function<void(double previous, double restored)> onReset;

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        const double previous = getValue();
        juce::Component::SafePointer<OaeSlider> safe(this);
        juce::Slider::mouseDoubleClick(event);
        if (safe && onReset && previous != getValue())
            onReset(previous, getValue());
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (dragging_) {
            dragging_ = false;
            setSliderSnapsToMousePosition(snapsToMouse_);
        }
        juce::Slider::mouseUp(event);
    }

private:
    juce::Point<float> lastPointer_;
    juce::Point<float> dragPointer_;
    bool dragging_{ false };
    bool snapsToMouse_{ true };
};

} // namespace oae::studio
