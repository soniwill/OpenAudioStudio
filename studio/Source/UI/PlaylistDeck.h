#pragma once

#include "OaeSlider.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Timeline/WaveformCache.h"
#include <map>

namespace oae::studio {

/** Edits the selected multi-instrument's ordered, weighted entries and variations. */
class PlaylistDeck : public juce::Component,
                     public AuthoringModel::Listener,
                     private juce::ListBoxModel,
                     private juce::ChangeListener {
public:
    explicit PlaylistDeck(AuthoringModel& model);
    ~PlaylistDeck() override;

    void resized() override;
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;
    bool has_selected_instrument(const ItemSelection& selection) const;
    juce::String duration(const std::string& sample);

private:
    const nlohmann::json* instrument(const nlohmann::json& root, const ItemSelection& selection) const;
    void edit(const std::function<void(nlohmann::json&)>& change);
    void refresh(int selected_row);
    void show_add_sample_menu();
    void move_entry(int delta);
    void change_weight(double percent);
    void configure_slider(juce::Slider& slider, double max, const char* suffix);
    fs::path resolve_sample_path(const std::string& sample) const;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    AuthoringModel& model_;
    ItemSelection selection_;
    nlohmann::json entries_ = nlohmann::json::array();
    juce::AudioFormatManager formats_;
    WaveformCache waveform_cache_;
    std::map<fs::path, std::pair<juce::Time, juce::String>> durations_;
    std::vector<juce::String> entry_durations_;
    bool refreshing_{ false };

    juce::Label status_{ {}, "Select a multi-instrument in the timeline." };
    juce::ComboBox mode_;
    juce::ComboBox scope_;
    juce::ListBox entriesList_{ "Playlist entries", this };
    juce::TextButton add_{ "+ Sample" };
    juce::TextButton remove_{ "Remove" };
    juce::TextButton up_{ "Move up" };
    juce::TextButton down_{ "Move down" };
    juce::Label weightLabel_{ {}, "Weight" };
    juce::Label pitchLabel_{ {}, "Pitch variation" };
    juce::Label volumeLabel_{ {}, "Volume variation" };
    juce::Label delayLabel_{ {}, "Delay variation" };
    OaeSlider weight_;
    OaeSlider pitch_;
    OaeSlider volume_;
    OaeSlider delay_;
};

} // namespace oae::studio
