#include "OaeMeterComponent.h"

#include <algorithm>

namespace oae::studio {

namespace {

float value_to_norm(float value, float minimum, float maximum)
{
    return juce::jlimit(0.0f, 1.0f, (value - minimum) / (maximum - minimum));
}

} // namespace

OaeMeterComponent::OaeMeterComponent()
{
    setOpaque(true);
}

void OaeMeterComponent::set_orientation(Orientation orientation)
{
    if (orientation_ != orientation) {
        orientation_ = orientation;
        repaint();
    }
}

void OaeMeterComponent::set_scale(MeterScale scale)
{
    if (scale_ != scale) {
        scale_ = scale;
        repaint();
    }
}

void OaeMeterComponent::set_levels(float peak_linear, float rms_linear, float dt_seconds)
{
    meter_.update(peak_linear, rms_linear, dt_seconds);
    meterRight_.update(peak_linear, rms_linear, dt_seconds);
    repaint();
}

void OaeMeterComponent::set_stereo_levels(float peak_l, float peak_r, float rms_l, float rms_r, float dt_seconds)
{
    meter_.update(peak_l, rms_l, dt_seconds);
    meterRight_.update(peak_r, rms_r, dt_seconds);
    repaint();
}

void OaeMeterComponent::set_lufs_level(float momentary_lufs, float true_peak_dbtp) noexcept
{
    lufs_momentary_ = momentary_lufs;
    true_peak_dbtp_ = true_peak_dbtp;
    if (scale_ == MeterScale::EbuR128Lufs) repaint();
}

float OaeMeterComponent::get_display_rms() const noexcept
{
    if (scale_ == MeterScale::EbuR128Lufs) {
        return (lufs_momentary_ > -79.9f) ? lufs_momentary_ : meter_.rms_dbfs();
    }
    return dbfs_to_scale(meter_.rms_dbfs(), scale_);
}

float OaeMeterComponent::get_display_peak() const noexcept
{
    if (scale_ == MeterScale::EbuR128Lufs) {
        return (true_peak_dbtp_ > -79.9f) ? true_peak_dbtp_ : meter_.peak_dbfs();
    }
    return dbfs_to_scale(meter_.peak_dbfs(), scale_);
}

float OaeMeterComponent::get_display_peak_hold() const noexcept
{
    return dbfs_to_scale(meter_.peak_hold_dbfs(), scale_);
}

void OaeMeterComponent::clear()
{
    meter_.reset();
    meterRight_.reset();
    lufs_momentary_ = OaeTheme::Animation::meterFloorDbfs;
    true_peak_dbtp_ = OaeTheme::Animation::meterFloorDbfs;
    repaint();
}

void OaeMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (orientation_ == Orientation::HorizontalStereo) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
        g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawRoundedRectangle(bounds.reduced(OaeTheme::Edges::borderStroke * 0.5f),
                               OaeTheme::Edges::cornerRadius,
                               OaeTheme::Edges::borderStroke);

        const auto inner = bounds.reduced(4.0f, 3.0f);
        if (inner.getWidth() <= 4.0f || inner.getHeight() <= 4.0f) return;

        float min_val = -60.0f;
        float max_val = 0.0f;
        meter_scale_range(scale_, min_val, max_val);
        if (scale_ == MeterScale::DbfsNominal) min_val = OaeTheme::Animation::meterFloorDbfs;

        const float gap = 2.0f;
        const float laneH = std::max(3.0f, (inner.getHeight() - gap) * 0.5f);
        const auto drawLane = [&](const BallisticMeter& m, juce::Rectangle<float> lane) {
            g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
            g.fillRoundedRectangle(lane, 2.0f);

            const float peakDisp = dbfs_to_scale(m.peak_dbfs(), scale_);
            const float peakNorm = value_to_norm(peakDisp, min_val, max_val);
            if (peakNorm > 0.005f) {
                juce::ColourGradient grad(juce::Colour(OaeTheme::Colors::accentGreen), lane.getX(), lane.getCentreY(),
                                          juce::Colour(OaeTheme::Colors::accentCoral), lane.getRight(), lane.getCentreY(), false);
                const float amberPos = (scale_ == MeterScale::DbfsNominal) ? 0.8f
                                     : value_to_norm(0.0f, min_val, max_val);
                grad.addColour(std::clamp(static_cast<double>(amberPos), 0.1, 0.95),
                               juce::Colour(OaeTheme::Colors::accentAmber));
                g.setGradientFill(grad);
                g.fillRoundedRectangle(lane.withWidth(lane.getWidth() * peakNorm), 2.0f);
            }

            const float holdDisp = dbfs_to_scale(m.peak_hold_dbfs(), scale_);
            const float holdNorm = value_to_norm(holdDisp, min_val, max_val);
            if (holdNorm > 0.01f) {
                const float holdX = juce::jlimit(lane.getX(), lane.getRight() - 1.5f,
                                                 lane.getX() + holdNorm * lane.getWidth());
                g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                g.fillRect(holdX, lane.getY(), 1.5f, lane.getHeight());
            }
        };

        drawLane(meter_, inner.withHeight(laneH));
        drawLane(meterRight_, inner.withY(inner.getY() + laneH + gap).withHeight(laneH));
        return;
    }

    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));

    const float h = bounds.getHeight();
    const float w = bounds.getWidth();
    if (h <= 4.0f || w <= 2.0f) return;

    float min_val = -60.0f;
    float max_val = 0.0f;
    meter_scale_range(scale_, min_val, max_val);
    if (scale_ == MeterScale::DbfsNominal) min_val = OaeTheme::Animation::meterFloorDbfs;

    const float peak_norm = value_to_norm(get_display_peak(), min_val, max_val);
    const float peak_h = peak_norm * h;

    juce::Rectangle<float> peakRect(bounds.getX() + 1.0f, bounds.getBottom() - peak_h, w - 2.0f, peak_h);

    juce::ColourGradient grad(juce::Colour(OaeTheme::Colors::accentGreen), bounds.getX(), bounds.getBottom(),
                              juce::Colour(OaeTheme::Colors::accentCoral), bounds.getX(), bounds.getY(), false);
    const float amber_pos = (scale_ == MeterScale::DbfsNominal) ? 0.8f
                          : (scale_ == MeterScale::EbuR128Lufs) ? value_to_norm(-23.0f, min_val, max_val)
                                                                : value_to_norm(0.0f, min_val, max_val);
    grad.addColour(std::clamp(static_cast<double>(amber_pos), 0.1, 0.95), juce::Colour(OaeTheme::Colors::accentAmber));

    g.setGradientFill(grad);
    g.fillRect(peakRect);

    const float rms_norm = value_to_norm(get_display_rms(), min_val, max_val);
    const float rms_h = rms_norm * h;
    const float inner_w = std::max(2.0f, w * 0.4f);
    juce::Rectangle<float> rmsRect(bounds.getCentreX() - inner_w * 0.5f, bounds.getBottom() - rms_h, inner_w, rms_h);
    g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.6f));
    g.fillRect(rmsRect);

    const float hold_norm = value_to_norm(get_display_peak_hold(), min_val, max_val);
    if (hold_norm > 0.01f) {
        const float hold_y = juce::jlimit(bounds.getY(), bounds.getBottom() - OaeTheme::Edges::borderStroke,
                                         bounds.getBottom() - hold_norm * h);
        g.setColour(juce::Colour(OaeTheme::Colors::indicator));
        g.fillRect(bounds.getX(), hold_y, w, OaeTheme::Edges::borderStroke);
    }

    if (scale_ != MeterScale::DbfsNominal) {
        const float ref_val = (scale_ == MeterScale::EbuR128Lufs) ? -23.0f : 0.0f;
        const float ref_norm = value_to_norm(ref_val, min_val, max_val);
        const float ref_y = bounds.getBottom() - (ref_norm * h);
        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber).withAlpha(0.65f));
        g.drawHorizontalLine(static_cast<int>(ref_y), bounds.getX(), bounds.getRight());
    }

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRect(bounds, OaeTheme::Edges::borderStroke);
}

} // namespace oae::studio
