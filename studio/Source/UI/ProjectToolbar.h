#pragma once

#include "GripHandle.h"
#include "OaePrimitives.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace oae::studio {

/** Modular project action toolbar dock with tactile grip handle and Figma-aligned buttons. */
class ProjectToolbar : public juce::Component {
public:
    static constexpr int kBarHeight = 36;

    ProjectToolbar();
    ~ProjectToolbar() override = default;

    /** Returns the preferred height for this toolbar dock. */
    int preferred_height() const noexcept { return kBarHeight; }

    /** Returns the preferred width for the current minimized/maximized state. */
    int preferred_width() const noexcept;

    /** Toggles between minimized (compact) and maximized (full) modes. */
    void set_minimized(bool minimized);

    /** Returns true when the toolbar is in minimized (compact) mode. */
    bool is_minimized() const noexcept { return minimized_; }

    /** Updates the floating state reflected in the grip context menu. */
    void set_floating(bool floating) noexcept { gripHandle_.set_floating(floating); }

    /** Returns the left-edge tactile grip handle for docking and gesture wiring. */
    GripHandle& grip_handle() noexcept { return gripHandle_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    OaeToolbarButton newProjectButton{ "New", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton openProjectButton{ "Open", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton saveProjectButton{ "Save", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton buildBankButton{ "Build Bank [F7]", OaeToolbarButton::Variant::AccentCyan };
    OaeToolbarButton helpButton{ "?", OaeToolbarButton::Variant::Neutral };

    std::function<void()> onLayoutChanged;

private:
    GripHandle gripHandle_{ "grip_project_toolbar" };
    juce::Label brandLabel_{ "brand", "OPENAUDIO" };
    bool minimized_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectToolbar)
};

} // namespace oae::studio
