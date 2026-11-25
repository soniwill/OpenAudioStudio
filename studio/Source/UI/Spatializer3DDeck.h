#pragma once

#include "OaeSlider.h"
#include "Testing/UiTestHooks.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Transport/AuditionTransport.h"

namespace oae::studio {

/** Auditions distance attenuation and stereo pan on the local event instance while the deck is visible. */
class Spatializer3DDeck : public juce::Component,
                          private AuditionTransport::Listener,
                          public ITestableCustomControl {
public:
    explicit Spatializer3DDeck(AuditionTransport& transport);
    ~Spatializer3DDeck() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    std::vector<TestableTarget> get_testable_targets() const override;

private:
    void on_transport_state_changed(TransportState state) override;
    void update_preview();
    void restore_instance();
    float gain_at_distance(float distance) const;
    void draw_radar(juce::Graphics& g, juce::Rectangle<float> area);
    void draw_attenuation_graph(juce::Graphics& g, juce::Rectangle<float> area);

    AuditionTransport& transport_;
    oae_event_instance preview_instance_{ 0 };
    float instance_volume_{ 1.0f };
    float instance_pan_{ 0.0f };

    juce::Label headerLabel_{ "header", "SPATIAL PREVIEW (LOCAL PLAYBACK ONLY)" };

    float emitter_x_{ 0.0f };
    float emitter_y_{ 0.0f };
    float emitter_z_{ 0.0f };
    juce::Rectangle<float> radar_area_;
    juce::Rectangle<float> graph_area_;

    OaeSlider minDistanceSlider_;
    juce::Label minDistanceLabel_{ "minDist", "Min Distance (m)" };
    OaeSlider maxDistanceSlider_;
    juce::Label maxDistanceLabel_{ "maxDist", "Max Distance (m)" };
    juce::ComboBox rolloffCombo_;
    juce::Label rolloffLabel_{ "rolloff", "Rolloff Curve" };
    OaeSlider soundSizeSlider_;
    juce::Label soundSizeLabel_{ "soundSize", "Sound Size (m)" };
    OaeSlider minExtentSlider_;
    juce::Label minExtentLabel_{ "minExtent", "Min Extent (deg)" };

    juce::Label coordsLabel_{ "coords", "" };

    bool is_dragging_emitter_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spatializer3DDeck)
};

} // namespace oae::studio
