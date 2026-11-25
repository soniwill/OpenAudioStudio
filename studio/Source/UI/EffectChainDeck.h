#pragma once

#include "OaeSlider.h"
#include "Testing/UiTestHooks.h"

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../StudioEngine.h"

namespace oae::studio {

/** Edits the authored chain of effects on a Master/child bus or an event track. */
class EffectChainDeck : public juce::Component, public AuthoringModel::Listener {
public:
    EffectChainDeck(StudioEngine& engine, AuthoringModel& model);
    ~EffectChainDeck() override;

    void resized() override;
    void set_target_bus(const std::string& bus_path);
    const std::string& get_target_bus() const noexcept { return target_bus_; }

    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

private:
    class EqPlot : public juce::Component, public ITestableCustomControl {
    public:
        explicit EqPlot(EffectChainDeck& owner);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        std::vector<TestableTarget> get_testable_targets() const override;

    private:
        EffectChainDeck& owner_;
        int drag_index_{ -1 };
        bool owns_gesture_{ false };
    };

    const nlohmann::json* selected_effect() const;
    void refresh();
    void refresh_parameters();
    void set_parameter(size_t index, double value);
    void bind_slider(juce::Slider& slider, const juce::String& id);

    AuthoringModel& model_;
    ItemSelection target_{ ItemType::Bus, "bus:/" };
    std::string target_bus_{ "bus:/" };
    std::vector<std::string> buses_;
    int selected_index_{ 0 };
    double sample_rate_{ 48000.0 };
    bool track_selected_{ false };
    bool owns_slider_gesture_{ false };
    bool refreshing_{ false };

    juce::Component controls_;
    juce::Viewport viewport_;
    juce::Label heading_{ "heading", "Bus effects" };
    juce::Label status_;
    juce::ComboBox busSelector_;
    juce::ComboBox addType_;
    juce::ComboBox addPosition_;
    juce::TextButton addButton_{ "+ Add effect" };
    juce::ComboBox effectSelector_;
    juce::TextButton removeButton_{ "Remove" };
    juce::ToggleButton bypassButton_{ "Bypass" };
    juce::Label wetLabel_{ "wet", "Wet" };
    juce::Label dryLabel_{ "dry", "Dry" };
    OaeSlider wetSlider_;
    OaeSlider drySlider_;
    std::array<juce::Label, 5> paramLabels_;
    std::array<OaeSlider, 5> paramSliders_;
    EqPlot eqPlot_{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectChainDeck)
};

} // namespace oae::studio

