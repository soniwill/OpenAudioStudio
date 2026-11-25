#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace oae::studio {

/** Ballistic Peak and RMS VU meter component supporting dBFS, K-12/14/20, and EBU R128 LUFS scales. */
class OaeMeterComponent : public juce::Component {
public:
    enum class Orientation {
        VerticalMono,
        HorizontalStereo
    };

    OaeMeterComponent();
    ~OaeMeterComponent() override = default;

    void set_orientation(Orientation orientation);
    Orientation get_orientation() const noexcept { return orientation_; }

    void paint(juce::Graphics& g) override;
    void set_levels(float peak_linear, float rms_linear, float dt_seconds = 1.0f / OaeTheme::Animation::meterRefreshHz);
    void set_stereo_levels(float peak_l, float peak_r, float rms_l, float rms_r,
                           float dt_seconds = 1.0f / OaeTheme::Animation::meterRefreshHz);
    void set_lufs_level(float momentary_lufs, float true_peak_dbtp) noexcept;
    void clear();

    void set_scale(MeterScale scale);
    MeterScale get_scale() const noexcept { return scale_; }

    float get_peak_db() const noexcept { return meter_.peak_dbfs(); }
    float get_rms_db() const noexcept { return meter_.rms_dbfs(); }
    float get_peak_hold_db() const noexcept { return meter_.peak_hold_dbfs(); }
    float get_display_rms() const noexcept;
    float get_display_peak() const noexcept;
    float get_display_peak_hold() const noexcept;

private:
    Orientation orientation_{ Orientation::VerticalMono };
    MeterScale scale_{ MeterScale::DbfsNominal };
    BallisticMeter meter_;
    BallisticMeter meterRight_;
    float lufs_momentary_{ OaeTheme::Animation::meterFloorDbfs };
    float true_peak_dbtp_{ OaeTheme::Animation::meterFloorDbfs };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OaeMeterComponent)
};

} // namespace oae::studio
