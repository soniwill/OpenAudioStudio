#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "Testing/UiTestHooks.h"
#include <string>

namespace oae::studio {

/** Condensed bird's-eye view minimap with interactive draggable and resizable viewport box (PRO-UI-04). */
class TimelineOverviewMinimap : public juce::Component,
                                public AuthoringModel::Listener,
                                public ITestableCustomControl {
public:
    explicit TimelineOverviewMinimap(AuthoringModel& model);
    ~TimelineOverviewMinimap() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    std::vector<TestableTarget> get_testable_targets() const override;

    void set_event_path(const std::string& path);
    void set_visible_range(double start_time_ms, double length_time_ms);
    void set_total_duration(double total_ms);
    void set_playhead_time_ms(double time_ms);

    // Callbacks when viewport is dragged or resized
    std::function<void(double new_start_ms, double new_length_ms)> onViewportChanged;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

private:
    AuthoringModel& model_;
    std::string event_path_;

    double total_duration_ms_{ 10000.0 };
    double viewport_start_ms_{ 0.0 };
    double viewport_length_ms_{ 3000.0 };
    double playhead_time_ms_{ 0.0 };

    enum class DragMode {
        None,
        Move,
        ResizeLeft,
        ResizeRight
    };

    DragMode drag_mode_{ DragMode::None };
    juce::Point<int> drag_start_mouse_;
    double drag_start_start_ms_{ 0.0 };
    double drag_start_length_ms_{ 3000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineOverviewMinimap)
};

} // namespace oae::studio
