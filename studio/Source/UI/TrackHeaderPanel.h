#pragma once

#include "OaeSlider.h"
#include "Testing/UiTestHooks.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include <vector>
#include <string>

namespace oae::studio {

/** Single track header strip with Volume fader, Pan, Mute, Solo, and Automation buttons. */
class TrackHeaderItem : public juce::Component, public ITestableCustomControl {
public:
    TrackHeaderItem(AuthoringModel& model, const std::string& event_path, size_t track_index);
    ~TrackHeaderItem() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void update_from_model();
    void set_track_index(size_t track_index);
    std::vector<TestableTarget> get_testable_targets() const override;

    size_t get_track_index() const noexcept { return track_index_; }
    bool is_automation_expanded() const noexcept { return automation_expanded_; }
    bool is_editing() const noexcept { return is_dragging_ || is_adjusting_slider_; }

    std::function<void(size_t track_index, bool expanded)> onAutomationToggled;
    std::function<void()> onTrackStructureChanged;
    std::function<void(size_t src_track, int delta_y)> onTrackDragged;
    std::function<void(size_t src_track, int delta_y)> onTrackDragEnded;

private:
    void update_component_ids();
    void show_context_menu(juce::Point<int> screen_pos);
    void show_color_menu();

    AuthoringModel& model_;
    std::string event_path_;
    size_t track_index_;
    bool automation_expanded_{ false };
    bool is_dragging_{ false };
    bool is_adjusting_slider_{ false };
    bool is_updating_from_model_{ false };
    int drag_start_y_{ 0 };

    juce::TextButton colorSwatchButton_;
    juce::Label titleLabel;
    OaeSlider volumeSlider;
    OaeSlider panSlider;
    juce::TextButton muteButton{ "M" };
    juce::TextButton soloButton{ "S" };
    juce::TextButton autoButton{ "A" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeaderItem)
};

/** Left panel hosting track headers aligned vertically with timeline track lanes. */
class TrackHeaderPanel : public juce::Component {
public:
    explicit TrackHeaderPanel(AuthoringModel& model);
    ~TrackHeaderPanel() override = default;

    static constexpr int header_height = 32;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return event_path_; }

    void refresh_tracks();
    void repaint_selection();
    void set_vertical_scroll(int scroll_y);

    int get_track_y(size_t track_index) const;
    int get_track_height(size_t track_index) const;
    /** Returns whether the track header is showing its automation lane. */
    bool is_automation_expanded(size_t track_index) const noexcept;

    bool is_dragging_track() const noexcept {
        if (drag_src_track_ >= 0 || is_reordering_track_) return true;
        for (const auto& it : items_) {
            if (it && it->is_editing()) return true;
        }
        return false;
    }
    void handle_track_drag(size_t src_track, int delta_y);
    void handle_track_drag_ended(size_t src_track, int delta_y);

    std::function<void()> onTracksChanged;

private:
    AuthoringModel& model_;
    std::string event_path_;
    int vertical_scroll_y_{ 0 };
    int drag_src_track_{ -1 };
    int drag_dest_track_{ -1 };
    bool is_reordering_track_{ false };

    juce::Label headerLabel{ "header", "TRACKS" };
    juce::TextButton addTrackButton{ "+" };

    std::vector<std::unique_ptr<TrackHeaderItem>> items_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeaderPanel)
};

} // namespace oae::studio
