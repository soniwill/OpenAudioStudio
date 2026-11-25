#include "TimelineOverviewMinimap.h"
#include "OaeDarkLookAndFeel.h"
#include <cmath>
#include <algorithm>

namespace oae::studio {

TimelineOverviewMinimap::TimelineOverviewMinimap(AuthoringModel& model)
    : model_(model)
{
    model_.add_listener(this);
}

TimelineOverviewMinimap::~TimelineOverviewMinimap()
{
    model_.remove_listener(this);
}

void TimelineOverviewMinimap::on_project_changed()
{
    repaint();
}

void TimelineOverviewMinimap::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && !selection.id_or_path.empty()) {
        set_event_path(selection.id_or_path);
    }
}

void TimelineOverviewMinimap::set_event_path(const std::string& path)
{
    event_path_ = path;
    repaint();
}

void TimelineOverviewMinimap::set_visible_range(double start_time_ms, double length_time_ms)
{
    viewport_start_ms_ = std::max(0.0, start_time_ms);
    viewport_length_ms_ = std::max(500.0, length_time_ms);
    repaint();
}

void TimelineOverviewMinimap::set_total_duration(double total_ms)
{
    total_duration_ms_ = std::max(1000.0, total_ms);
    repaint();
}

void TimelineOverviewMinimap::set_playhead_time_ms(double time_ms)
{
    playhead_time_ms_ = std::max(0.0, time_ms);
    repaint();
}

void TimelineOverviewMinimap::mouseDown(const juce::MouseEvent& e)
{
    if (total_duration_ms_ <= 0.0) return;

    const float ms_to_px = getWidth() / static_cast<float>(total_duration_ms_);
    const float vpX = static_cast<float>(viewport_start_ms_) * ms_to_px;
    const float vpW = static_cast<float>(viewport_length_ms_) * ms_to_px;

    drag_start_mouse_ = e.getPosition();
    drag_start_start_ms_ = viewport_start_ms_;
    drag_start_length_ms_ = viewport_length_ms_;

    const float mx = static_cast<float>(e.getPosition().x);
    if (std::abs(mx - vpX) <= 5.0f) {
        drag_mode_ = DragMode::ResizeLeft;
    } else if (std::abs(mx - (vpX + vpW)) <= 5.0f) {
        drag_mode_ = DragMode::ResizeRight;
    } else if (mx >= vpX && mx <= (vpX + vpW)) {
        drag_mode_ = DragMode::Move;
    } else {
        // Direct click jumps viewport center
        const double clicked_ms = (mx / getWidth()) * total_duration_ms_;
        viewport_start_ms_ = std::clamp(clicked_ms - (viewport_length_ms_ * 0.5), 0.0, total_duration_ms_ - viewport_length_ms_);
        drag_mode_ = DragMode::Move;
        drag_start_start_ms_ = viewport_start_ms_;
        if (onViewportChanged) onViewportChanged(viewport_start_ms_, viewport_length_ms_);
        repaint();
    }
}

void TimelineOverviewMinimap::mouseDrag(const juce::MouseEvent& e)
{
    if (drag_mode_ == DragMode::None || total_duration_ms_ <= 0.0) return;

    const int delta_x = e.getPosition().x - drag_start_mouse_.x;
    const double delta_ms = (static_cast<double>(delta_x) / getWidth()) * total_duration_ms_;

    if (drag_mode_ == DragMode::Move) {
        viewport_start_ms_ = std::clamp(drag_start_start_ms_ + delta_ms, 0.0, std::max(0.0, total_duration_ms_ - viewport_length_ms_));
    } else if (drag_mode_ == DragMode::ResizeRight) {
        viewport_length_ms_ = std::clamp(drag_start_length_ms_ + delta_ms, 500.0, total_duration_ms_ - viewport_start_ms_);
    } else if (drag_mode_ == DragMode::ResizeLeft) {
        const double new_start = std::clamp(drag_start_start_ms_ + delta_ms, 0.0, drag_start_start_ms_ + drag_start_length_ms_ - 500.0);
        viewport_length_ms_ = drag_start_length_ms_ - (new_start - drag_start_start_ms_);
        viewport_start_ms_ = new_start;
    }

    if (onViewportChanged) {
        onViewportChanged(viewport_start_ms_, viewport_length_ms_);
    }
    repaint();
}

void TimelineOverviewMinimap::mouseUp(const juce::MouseEvent&)
{
    drag_mode_ = DragMode::None;
}

void TimelineOverviewMinimap::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(OaePalette::bgDark));

    // Outer border
    g.setColour(juce::Colour(OaePalette::borderSubtle));
    g.drawRect(bounds, 1.0f);

    if (total_duration_ms_ <= 0.0) return;

    const float ms_to_px = bounds.getWidth() / static_cast<float>(total_duration_ms_);

    // Draw condensed track blocks
    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                const auto& trks = ev["tracks"];
                const float trkH = std::max(4.0f, (bounds.getHeight() - 4.0f) / std::max<size_t>(1, trks.size()));

                for (size_t t = 0; t < trks.size(); ++t) {
                    const float y = 2.0f + t * trkH;
                    if (trks[t].contains("instruments") && trks[t]["instruments"].is_array()) {
                        for (const auto& inst : trks[t]["instruments"]) {
                            const double off = inst.value("offset_ms", 0.0);
                            const double len = inst.value("length_ms", 500.0);

                            const float x = static_cast<float>(off) * ms_to_px;
                            const float w = std::max(2.0f, static_cast<float>(len) * ms_to_px);

                            g.setColour(juce::Colour(OaePalette::accentCyan).withAlpha(0.45f));
                            g.fillRoundedRectangle(x, y + 1.0f, w, trkH - 2.0f, 1.5f);
                        }
                    }
                }
                break;
            }
        }
    }

    // Viewport box
    const float vpX = static_cast<float>(viewport_start_ms_) * ms_to_px;
    const float vpW = static_cast<float>(viewport_length_ms_) * ms_to_px;
    auto vpBounds = juce::Rectangle<float>(vpX, 1.0f, vpW, bounds.getHeight() - 2.0f);

    g.setColour(juce::Colour(OaePalette::accentCyan).withAlpha(0.18f));
    g.fillRect(vpBounds);
    g.setColour(juce::Colour(OaePalette::accentCyan));
    g.drawRect(vpBounds, 1.5f);

    // Viewport resize handles
    g.setColour(juce::Colours::white);
    g.fillRect(vpBounds.getX() - 1.0f, vpBounds.getCentreY() - 4.0f, 2.0f, 8.0f);
    g.fillRect(vpBounds.getRight() - 1.0f, vpBounds.getCentreY() - 4.0f, 2.0f, 8.0f);

    // Playhead indicator in minimap
    const float phX = static_cast<float>(playhead_time_ms_) * ms_to_px;
    g.setColour(juce::Colour(OaePalette::accentCyan));
    g.drawVerticalLine(static_cast<int>(phX), 0.0f, bounds.getHeight());
}

void TimelineOverviewMinimap::resized()
{
}

std::vector<TestableTarget> TimelineOverviewMinimap::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty() || total_duration_ms_ <= 0.0) return {};
    const float ms_to_px = static_cast<float>(bounds.getWidth()) / static_cast<float>(total_duration_ms_);
    const int vpX = static_cast<int>(std::round(static_cast<float>(viewport_start_ms_) * ms_to_px));
    const int vpW = std::max(14, static_cast<int>(std::round(static_cast<float>(viewport_length_ms_) * ms_to_px)));
    const int h = std::max(1, bounds.getHeight() - 2);
    std::vector<TestableTarget> out;
    const auto leftHandle = juce::Rectangle<int>(vpX - 2, 1, 5, h).getIntersection(bounds);
    const auto rightHandle = juce::Rectangle<int>(vpX + vpW - 3, 1, 5, h).getIntersection(bounds);
    const auto innerVp = juce::Rectangle<int>(vpX + 3, 1, std::max(4, vpW - 6), h).getIntersection(bounds);
    if (!innerVp.isEmpty()) out.push_back({ "viewport", "drag", innerVp, isEnabled() });
    if (!leftHandle.isEmpty()) out.push_back({ "resize_left", "drag", leftHandle, isEnabled() });
    if (!rightHandle.isEmpty()) out.push_back({ "resize_right", "drag", rightHandle, isEnabled() });
    const auto rightTrack = juce::Rectangle<int>(vpX + vpW + 4, 1, std::max(1, bounds.getWidth() - (vpX + vpW + 4)), h).getIntersection(bounds);
    if (!rightTrack.isEmpty()) {
        out.push_back({ "track", "click", rightTrack, isEnabled() });
    } else if (vpX > 4) {
        const auto leftTrack = juce::Rectangle<int>(0, 1, vpX - 4, h).getIntersection(bounds);
        if (!leftTrack.isEmpty()) out.push_back({ "track", "click", leftTrack, isEnabled() });
    }
    return out;
}

} // namespace oae::studio
