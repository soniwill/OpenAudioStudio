#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Timeline/TimelineSnap.h"
#include "../Timeline/WaveformCache.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_set>

namespace oae::studio {

class TimelineTrackLane;

/** Collects referenced audio sample IDs recursively for any instrument type with cycle protection. */
void collect_instrument_sample_ids(const AuthoringModel& model,
                                   const nlohmann::json& inst,
                                   const std::string& current_event_path,
                                   std::unordered_set<std::string>& visited_events,
                                   std::vector<std::string>& out_sample_ids);

/** Interactive visual clip component supporting move, resize, fades, and waveform rendering. */
class ClipComponent : public juce::Component, public ITestableCustomControl {
public:
    enum class DragMode {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        FadeIn,
        FadeOut
    };

    ClipComponent(AuthoringModel& model,
                  WaveformCache& waveform_cache,
                  const std::string& event_path,
                  size_t track_index,
                  size_t instrument_index);
    ~ClipComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    std::vector<TestableTarget> get_testable_targets() const override;

    size_t get_track_index() const noexcept { return track_index_; }
    size_t get_instrument_index() const noexcept { return instrument_index_; }
    void set_indices(size_t track_index, size_t instrument_index) noexcept {
        track_index_ = track_index;
        instrument_index_ = instrument_index;
    }
    void set_event_path(const std::string& event_path) { event_path_ = event_path; }

    double get_offset_ms() const noexcept { return offset_ms_; }
    double get_length_ms() const noexcept { return length_ms_; }
    double get_start_offset_ms() const noexcept { return start_offset_ms_; }
    double get_pitch_semitones() const noexcept { return pitch_semitones_; }
    bool is_loop() const noexcept { return loop_; }
    double get_fade_in_ms() const noexcept { return fade_in_ms_; }
    double get_fade_out_ms() const noexcept { return fade_out_ms_; }
    double get_drag_start_offset_ms() const noexcept { return drag_start_offset_ms_; }
    double get_drag_start_length_ms() const noexcept { return drag_start_length_ms_; }
    void set_offset_ms(double off) noexcept { offset_ms_ = std::max(0.0, off); }
    void set_length_ms(double len) noexcept { length_ms_ = std::max(10.0, len); }
    bool is_selected() const noexcept { return is_selected_; }
    void set_selected(bool sel) noexcept { is_selected_ = sel; repaint(); }
    bool is_being_dragged() const noexcept { return is_being_dragged_; }
    void set_being_dragged(bool dragging) noexcept { is_being_dragged_ = dragging; repaint(); }
    const std::string& get_kind() const noexcept { return kind_; }
    juce::String get_display_title() const;
    std::shared_ptr<const WaveformEnvelope> resolve_waveform_envelope(bool synchronous = false) const;

    void update_from_model();
    void open_target_event();

    std::function<void(ClipComponent*, int delta_x, int delta_y, bool shift_down)> onClipDragged;
    std::function<void(ClipComponent*)> onClipDragEnded;
    std::function<void()> onClipModified;

private:
    void show_context_menu(juce::Point<int> screen_pos);
    DragMode hit_test_drag_mode(juce::Point<int> pos) const;

    AuthoringModel& model_;
    WaveformCache& waveform_cache_;
    std::string event_path_;
    size_t track_index_;
    size_t instrument_index_;

    std::string kind_{ "single" };
    std::string sample_id_;
    std::string target_path_;
    std::string command_type_{ "start_event" };
    double offset_ms_{ 0.0 };
    double length_ms_{ 500.0 };
    double start_offset_ms_{ 0.0 };
    double pitch_semitones_{ 0.0 };
    bool loop_{ false };
    double fade_in_ms_{ 0.0 };
    double fade_out_ms_{ 0.0 };
    bool is_selected_{ false };
    bool is_being_dragged_{ false };

    DragMode current_drag_mode_{ DragMode::None };
    juce::Point<int> drag_start_mouse_;
    double drag_start_offset_ms_{ 0.0 };
    double drag_start_length_ms_{ 500.0 };
    double drag_start_fade_in_ms_{ 0.0 };
    double drag_start_fade_out_ms_{ 0.0 };
    mutable std::shared_ptr<const WaveformEnvelope> cached_envelope_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipComponent)
};

/** Visual automation lane displaying and editing curve points for track parameters. */
class AutomationLaneComponent : public juce::Component, public ITestableCustomControl {
public:
    AutomationLaneComponent(AuthoringModel& model, const std::string& event_path, size_t track_index);
    ~AutomationLaneComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void focusLost(FocusChangeType cause) override;
    bool keyPressed(const juce::KeyPress& key) override;
    std::vector<TestableTarget> get_testable_targets() const override;

    void set_time_converter(std::function<double(double)> time_to_px,
                            std::function<double(float)> px_to_time);
    void set_track_index(size_t track_index) noexcept { track_index_ = track_index; }
    void set_event_path(const std::string& event_path) { event_path_ = event_path; }

    /** Index of the point currently selected for keyboard deletion, or -1. */
    int get_selected_point_index() const noexcept { return selected_point_index_; }
    void set_selected_point_index(int index) noexcept { selected_point_index_ = index; }

    /** Removes the point at the given index, clearing the selection. Returns false when nothing was removed. */
    bool delete_point_at(int index);

    /** Index of the point under a local position, as resolved by the context menu. */
    int point_index_at(juce::Point<float> pos) const;

private:
    nlohmann::json read_points() const;
    int hit_test_point(juce::Point<float> pos, const nlohmann::json& pts) const;
    juce::Point<float> point_to_screen(double x_ms, double y_val, juce::Rectangle<float> bounds) const;
    int resolve_selected_point() const;
    void show_context_menu(juce::Point<int> screen_pos, int point_index);

    AuthoringModel& model_;
    std::string event_path_;
    size_t track_index_;
    int dragged_point_index_{ -1 };
    int hovered_point_index_{ -1 };
    int selected_point_index_{ -1 };

    std::function<double(double)> time_to_px_;
    std::function<double(float)> px_to_time_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLaneComponent)
};

/** Track lane hosting clips and optional expanded automation sub-lane. */
class TimelineTrackLane : public juce::Component,
                          public juce::FileDragAndDropTarget {
public:
    TimelineTrackLane(AuthoringModel& model,
                      WaveformCache& waveform_cache,
                      const std::string& event_path,
                      size_t track_index);
    ~TimelineTrackLane() override = default;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void update_clips();
    void set_track_index(size_t track_index);
    void set_event_context(const std::string& event_path, size_t track_index);
    void set_automation_expanded(bool expanded);
    bool is_automation_expanded() const noexcept { return automation_expanded_; }

    /** Visual state describing a floating ghost clip preview during a move drag. */
    struct GhostPreviewState {
        bool active{ false };
        double raw_time_ms{ 0.0 };
        double snapped_time_ms{ 0.0 };
        double delta_ms{ 0.0 };
        double length_ms{ 0.0 };
        double start_offset_ms{ 0.0 };
        double pitch_semitones{ 0.0 };
        bool loop{ false };
        double fade_in_ms{ 0.0 };
        double fade_out_ms{ 0.0 };
        juce::Colour colour{ juce::Colour(OaeTheme::Colors::accentCyan) };
        juce::String title;
        std::shared_ptr<const WaveformEnvelope> envelope;
    };

    void set_time_converter(std::function<double(double)> time_to_px,
                            std::function<double(float)> px_to_time);

    void set_drop_target(double start_ms, double length_ms);
    void set_drop_target(const GhostPreviewState& preview);
    void clear_drop_target();
    void refresh_selection();
    const GhostPreviewState& get_ghost_preview() const noexcept { return ghost_preview_; }

    size_t get_track_index() const noexcept { return track_index_; }
    const std::vector<std::unique_ptr<ClipComponent>>& get_clips() const noexcept { return clips_; }
    AutomationLaneComponent& automation_lane() noexcept { return automationLane_; }
    const AutomationLaneComponent& automation_lane() const noexcept { return automationLane_; }

    std::function<void(ClipComponent*, int delta_x, int delta_y, bool shift_down)> onClipDragged;
    std::function<void(ClipComponent*)> onClipDragEnded;
    std::function<void()> onTrackModified;
    std::function<void(size_t src_track, int delta_y)> onTrackLaneDragged;
    std::function<void(size_t src_track, int delta_y)> onTrackLaneDragEnded;

private:
    AuthoringModel& model_;
    WaveformCache& waveform_cache_;
    std::string event_path_;
    size_t track_index_;
    bool automation_expanded_{ false };
    GhostPreviewState ghost_preview_;
    bool is_dragging_track_{ false };

    std::function<double(double)> time_to_px_;
    std::function<double(float)> px_to_time_;

    std::vector<std::unique_ptr<ClipComponent>> clips_;
    AutomationLaneComponent automationLane_{ model_, event_path_, track_index_ };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineTrackLane)
};

} // namespace oae::studio
