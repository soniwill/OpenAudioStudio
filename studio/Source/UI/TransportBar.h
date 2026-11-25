#pragma once

#include "GripHandle.h"
#include "OaeMeterComponent.h"
#include "OaePrimitives.h"
#include "Theme.h"
#include "VectorIcons.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"
#include <vector>
#include <memory>
#include <string>

namespace oae::studio {

/** Modular transport bar dock hosting playback controls, timecode, audio status, and global parameter shortcut. */
class TransportBar : public juce::Component, public AuditionTransport::Listener, public AuthoringModel::Listener {
public:
    static constexpr int kBarHeight = 36;

    TransportBar(AuthoringModel& model, AuditionTransport& transport);
    ~TransportBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    /** Returns the height required by the controls at the given width. */
    int preferred_height(int width);
    std::function<void()> onLayoutChanged;
    std::function<void()> onOpenGlobalParameters;

    void update_timecode(double time_ms);
    void update_status(const std::string& status);
    void update_params_button();
    void update_meter(float peak_l, float peak_r, float rms_l, float rms_r,
                      float dt_seconds = 1.0f / OaeTheme::Animation::meterRefreshHz);

    /** Toggles between minimized (compact) and maximized (full) transport modes. */
    void set_minimized(bool minimized);

    /** Returns true when the transport bar is in minimized (compact) mode. */
    bool is_minimized() const noexcept { return minimized_; }

    /** Updates the floating state reflected in the grip context menu. */
    void set_floating(bool floating) noexcept { gripHandle_.set_floating(floating); }

    /** Returns the left-edge tactile grip handle for docking and gesture wiring. */
    GripHandle& grip_handle() noexcept { return gripHandle_; }

    VectorIconButton playButton{ "Play", VectorIconType::Play, juce::Colour(OaeTheme::Colors::accentGreen) };
    VectorIconButton pauseButton{ "Pause", VectorIconType::Pause, juce::Colour(OaeTheme::Colors::accentAmber) };
    VectorIconButton stopButton{ "Stop", VectorIconType::Stop };
    VectorIconButton loopButton{ "Loop", VectorIconType::Loop, juce::Colour(OaeTheme::Colors::accentCyan) };
    VectorIconButton keyOffButton{ "Key Off", VectorIconType::KeyOff };

    juce::Label timecodeLabel{ "timecode", "00:00.000" };
    juce::Label statusLabel{ "status", "Ready" };
    OaeStatusBadge paramsButton{ "Params (0)", OaeStatusBadge::Variant::Cyan, false };
    OaeMeterComponent kSystemMeter;

    // AuditionTransport::Listener
    void on_transport_state_changed(TransportState new_state) override;
    void on_parameter_changed(const std::string& name, float val, bool is_global) override;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

private:
    juce::FlexBox make_layout();

    AuthoringModel& model_;
    AuditionTransport& transport_;

    GripHandle gripHandle_{ "grip_transport_bar" };
    bool minimized_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace oae::studio
