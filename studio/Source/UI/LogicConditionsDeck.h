#pragma once

#include "OaeSlider.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"

namespace oae::studio {

/** Edits the selected event's timeline logic and the conditions supported by its bank. */
class LogicConditionsDeck : public juce::Component, public AuthoringModel::Listener {
public:
    LogicConditionsDeck(AuthoringModel& model, AuditionTransport& transport);
    ~LogicConditionsDeck() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

private:
    /** Identifies a parameter and the legal domain for a condition term. */
    struct ParameterOption {
        nlohmann::json ref;
        double minimum;
        double maximum;
        double interval;
    };

    void refresh();
    void select_item(int index);
    void add_item(const std::string& kind);
    void edit_item(const std::function<void(nlohmann::json&)>& edit);
    void edit_term(const std::function<void(nlohmann::json&)>& edit);
    void refresh_parameters(const nlohmann::json& event);
    void refresh_condition(const nlohmann::json* item);
    double insertion_time() const;

    AuthoringModel& model_;
    AuditionTransport& transport_;
    int term_index_{ 0 };
    std::vector<ParameterOption> parameters_;
    std::vector<std::string> markers_;

    juce::Component form_;
    juce::Viewport viewport_;
    juce::Label header_{ "logic_header", "Timeline Logic" };
    juce::ComboBox logic_items_;
    juce::TextButton add_marker_{ "+ Marker" };
    juce::TextButton add_loop_{ "+ Loop" };
    juce::TextButton add_transition_{ "+ Trans" };
    juce::TextButton add_transition_region_{ "+ Trans Reg" };
    juce::TextButton add_sustain_{ "+ Sustain" };
    juce::TextButton remove_item_{ "Delete" };
    juce::Label position_label_{ "logic_position_label", "Position (ms)" };
    OaeSlider position_;
    juce::Label length_label_{ "logic_length_label", "Region length (ms)" };
    OaeSlider length_;
    juce::Label repeats_label_{ "logic_repeat_label", "Repeats (-1 = infinite)" };
    OaeSlider repeats_;
    juce::ToggleButton until_key_off_{ "Until Key Off" };

    juce::Label condition_label_{ "logic_condition_label", "Conditions (all ranges must match)" };
    juce::ComboBox terms_;
    juce::TextButton add_term_{ "+ Condition" };
    juce::TextButton remove_term_{ "Remove" };
    juce::ComboBox parameter_;
    juce::Label minimum_label_{ "logic_minimum_label", "Minimum (inclusive)" };
    juce::Label maximum_label_{ "logic_maximum_label", "Maximum (inclusive)" };
    OaeSlider minimum_;
    OaeSlider maximum_;
    juce::Label probability_label_{ "logic_probability_label", "Chance (%)" };
    OaeSlider probability_;

    juce::Label target_label_{ "logic_target_label", "Destination marker" };
    juce::ComboBox target_;
    juce::Label quantization_label_{ "logic_quantization_label", "Quantization" };
    juce::ComboBox quantization_;
    juce::Label crossfade_label_{ "logic_crossfade_label", "Crossfade (ms)" };
    OaeSlider crossfade_;
    juce::Label marker_name_label_{ "logic_marker_name_label", "Marker name" };
    juce::TextEditor marker_name_;
    juce::Label bpm_label_{ "logic_bpm_label", "Tempo (BPM)" };
    OaeSlider bpm_;
    juce::Label timesig_label_{ "logic_timesig_label", "Time signature" };
    juce::ComboBox ts_upper_;
    juce::ComboBox ts_lower_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogicConditionsDeck)
};

} // namespace oae::studio
