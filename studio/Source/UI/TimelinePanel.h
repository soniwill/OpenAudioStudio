#pragma once

#include "OaePrimitives.h"
#include "OaeSlider.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Timeline/TimelineSnap.h"
#include "../Timeline/WaveformCache.h"
#include "TimelineRuler.h"
#include "TrackHeaderPanel.h"
#include "TimelineTrackLane.h"
#include "TimelineOverviewMinimap.h"
#include "TimelineParameterStrip.h"
#include "LayerStackView.h"
#include <vector>
#include <memory>
#include <string>

namespace oae::studio {

class AuditionTransport;

/** Complete multi-track timeline panel featuring ruler, snap grid, track lanes, waveform cache, overview minimap, contextual parameter strip, and clip editing. */
class TimelinePanel : public juce::Component, public AuthoringModel::Listener, public juce::ScrollBar::Listener, public juce::ChangeListener {
public:
    explicit TimelinePanel(AuthoringModel& model, AuditionTransport* transport = nullptr);
    ~TimelinePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return event_path_; }

    /// Retorna a trilha atual de navegacao hierarquica entre eventos (pai -> filho).
    const std::vector<std::string>& get_breadcrumb_trail() const noexcept { return breadcrumb_trail_; }

    /// Navega diretamente para o indice especificado na trilha de breadcrumb.
    void navigate_breadcrumb_to(size_t index);

    /// Retorna ao evento pai imediato na trilha de breadcrumb, quando houver.
    void navigate_to_parent_event();

    void set_zoom(double pixels_per_second);
    double get_zoom() const noexcept { return pixels_per_second_; }

    void set_horizontal_offset(int offset_px);
    int get_horizontal_offset() const noexcept { return horizontal_offset_px_; }

    void set_playhead_time_ms(double time_ms);
    double get_playhead_time_ms() const noexcept { return playhead_time_ms_; }

    TimelineSnap& snap() noexcept { return snap_; }
    const TimelineSnap& snap() const noexcept { return snap_; }
    WaveformCache& waveform_cache() noexcept { return waveform_cache_; }
    TimelineOverviewMinimap& minimap() noexcept { return minimap_; }
    TimelineParameterStrip& parameter_strip() noexcept { return parameterStrip_; }
    const TimelineParameterStrip& parameter_strip() const noexcept { return parameterStrip_; }
    LayerStackView& layer_stack_view() noexcept { return layerStackView_; }
    const LayerStackView& layer_stack_view() const noexcept { return layerStackView_; }
    void set_transport(AuditionTransport* transport)
    {
        parameterStrip_.set_transport(transport);
        layerStackView_.set_transport(transport);
    }

    /** Stage display modes selectable in the TimelinePanel top bar. */
    enum class StageViewMode { Timeline, Layers };

    /** Switches the stage between the linear Timeline and the stacked Layers view. */
    void set_view_mode(StageViewMode mode);

    /** Returns the active stage view mode. */
    StageViewMode get_view_mode() const noexcept { return stage_view_mode_; }

    const std::vector<std::unique_ptr<TimelineTrackLane>>& get_lanes() const noexcept { return lanes_; }
    TrackHeaderPanel& track_headers() noexcept { return trackHeaders_; }

    // AuthoringModel::Listener overrides
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

    // juce::ScrollBar::Listener overrides
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    // juce::ChangeListener overrides
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::function<void(double)> onPlayheadScrubbed;
    std::function<void(StageViewMode)> onViewModeChanged;

private:
    void rebuild_track_lanes();
    void update_time_converters();
    void handle_clip_drag(ClipComponent* clip, int delta_x, int delta_y, bool shift_down);
    void handle_clip_drag_ended(ClipComponent* clip);
    void rebuild_breadcrumb_buttons();
    bool is_direct_child_event(const std::string& parent_path, const std::string& child_path) const;

    AuthoringModel& model_;
    std::string event_path_;
    std::vector<std::string> breadcrumb_trail_;
    TimelineSnap snap_;
    WaveformCache waveform_cache_;

    double pixels_per_second_{ 100.0 };
    int horizontal_offset_px_{ 0 };
    int vertical_scroll_y_{ 0 };
    double playhead_time_ms_{ 0.0 };

    // Toolbar controls
    juce::TextButton breadcrumbBackButton_{ "<" };
    std::vector<std::unique_ptr<juce::TextButton>> breadcrumbButtons_;
    OaeSegmentedControl viewModeSwitcher_;
    OaeToolbarButton addStemLayerButton_{ "+ Stem Layer", OaeToolbarButton::Variant::AccentCyan };
    juce::Label snapLabel{ "snapLabel", "Snap:" };
    juce::ComboBox snapComboBox;
    juce::TextButton timeFormatButton{ "Time / Bars" };
    juce::TextButton zoomOutButton{ "-" };
    OaeSlider zoomSlider;
    juce::TextButton zoomInButton{ "+" };
    juce::Label playheadLabel{ "playhead", "00:00.000" };

    // Layout components
    TimelineOverviewMinimap minimap_{ model_ };
    TimelineParameterStrip parameterStrip_{ model_, nullptr };
    LayerStackView layerStackView_{ model_, waveform_cache_, nullptr };
    StageViewMode stage_view_mode_{ StageViewMode::Timeline };
    TrackHeaderPanel trackHeaders_{ model_ };
    TimelineRuler ruler_{ model_ };
    juce::Component trackLanesContainer_;
    std::vector<std::unique_ptr<TimelineTrackLane>> lanes_;

    juce::ScrollBar horizontalScrollBar{ false };
    juce::ScrollBar verticalScrollBar{ true };

    bool is_dragging_clip_{ false };
    ClipComponent* dragging_clip_{ nullptr };
    size_t drag_target_track_{ 0 };
    double drag_target_offset_ms_{ 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelinePanel)
};

} // namespace oae::studio
