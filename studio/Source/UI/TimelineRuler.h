#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Timeline/TimelineSnap.h"
#include "Testing/UiTestHooks.h"
#include <functional>

namespace oae::studio {

/** Renders the timeline time graduation, musical meter ruler, logic markers, and playhead cursor. */
class TimelineRuler : public juce::Component, public ITestableCustomControl {
public:
    explicit TimelineRuler(AuthoringModel& model);
    ~TimelineRuler() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    std::vector<TestableTarget> get_testable_targets() const override;

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return event_path_; }

    void set_zoom(double pixels_per_second);
    double get_zoom() const noexcept { return pixels_per_second_; }

    void set_horizontal_offset(int offset_px);
    int get_horizontal_offset() const noexcept { return horizontal_offset_px_; }

    void set_playhead_time_ms(double time_ms);
    double get_playhead_time_ms() const noexcept { return playhead_time_ms_; }

    void set_snap(const TimelineSnap* snap) noexcept { snap_ = snap; }
    void set_musical_mode(bool musical) noexcept { musical_mode_ = musical; repaint(); }
    bool is_musical_mode() const noexcept { return musical_mode_; }

    /** Displays continuous vertical guide indicators on the ruler while dragging a clip. */
    void set_ghost_guide_ms(double raw_ms, double snapped_ms);
    /** Clears any active drag guide indicators on the ruler. */
    void clear_ghost_guide();

    double time_to_pixel(double time_ms) const noexcept;
    double pixel_to_time(float px) const noexcept;

    std::function<void(double)> onPlayheadChanged;

private:
    enum class LogicDragMode { None, Move, ResizeLeft, ResizeRight };

    const nlohmann::json* get_logic_items() const;
    juce::Rectangle<float> logic_bounds(const nlohmann::json& item, int index = -1) const;
    int logic_at(juce::Point<float> point) const;
    LogicDragMode hit_drag_mode(const nlohmann::json& item, juce::Point<float> point) const;
    void update_drag_preview(const juce::MouseEvent& e);
    void handle_scrub(const juce::MouseEvent& e);
    void show_context_menu(juce::Point<int> screen_pos, double clicked_time_ms, int hit_logic_index);

    AuthoringModel& model_;
    std::string event_path_;
    const TimelineSnap* snap_{ nullptr };

    double pixels_per_second_{ 100.0 };
    int horizontal_offset_px_{ 0 };
    double playhead_time_ms_{ 0.0 };
    bool musical_mode_{ false };
    int dragged_logic_index_{ -1 };
    LogicDragMode drag_mode_{ LogicDragMode::None };
    double dragged_logic_start_{ 0.0 };
    double dragged_logic_length_{ 0.0 };
    double drag_preview_start_ms_{ 0.0 };
    double drag_preview_length_ms_{ 0.0 };
    float dragged_mouse_x_{ 0.0f };
    bool has_ghost_guide_{ false };
    double ghost_raw_ms_{ 0.0 };
    double ghost_snapped_ms_{ 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineRuler)
};

} // namespace oae::studio
