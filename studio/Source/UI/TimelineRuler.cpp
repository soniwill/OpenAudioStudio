#include "TimelineRuler.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <iomanip>
#include <sstream>

namespace oae::studio {

TimelineRuler::TimelineRuler(AuthoringModel& model)
    : model_(model)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

void TimelineRuler::set_event_path(const std::string& path)
{
    event_path_ = path;
    repaint();
}

void TimelineRuler::set_zoom(double pixels_per_second)
{
    pixels_per_second_ = std::clamp(pixels_per_second, 10.0, 5000.0);
    repaint();
}

void TimelineRuler::set_horizontal_offset(int offset_px)
{
    horizontal_offset_px_ = std::max(0, offset_px);
    repaint();
}

void TimelineRuler::set_playhead_time_ms(double time_ms)
{
    playhead_time_ms_ = std::max(0.0, time_ms);
    repaint();
}

void TimelineRuler::set_ghost_guide_ms(double raw_ms, double snapped_ms)
{
    has_ghost_guide_ = true;
    ghost_raw_ms_ = std::max(0.0, raw_ms);
    ghost_snapped_ms_ = std::max(0.0, snapped_ms);
    repaint();
}

void TimelineRuler::clear_ghost_guide()
{
    if (has_ghost_guide_) {
        has_ghost_guide_ = false;
        repaint();
    }
}

double TimelineRuler::time_to_pixel(double time_ms) const noexcept
{
    return (time_ms / 1000.0) * pixels_per_second_ - horizontal_offset_px_;
}

double TimelineRuler::pixel_to_time(float px) const noexcept
{
    return ((px + horizontal_offset_px_) / pixels_per_second_) * 1000.0;
}

void TimelineRuler::resized() {}

const nlohmann::json* TimelineRuler::get_logic_items() const
{
    const auto& root = std::as_const(model_).json();
    const auto events = root.find("events");
    if (events == root.end() || !events->is_array()) return nullptr;
    for (const auto& event : *events) {
        if (event.value("path", "") != event_path_) continue;
        const auto items = event.find("logic_items");
        if (items != event.end() && items->is_array()) return &*items;
    }
    return nullptr;
}

juce::Rectangle<float> TimelineRuler::logic_bounds(const nlohmann::json& item, int index) const
{
    const auto kind = item.value("kind", "");
    const bool previewing = (index >= 0 && index == dragged_logic_index_ && drag_mode_ != LogicDragMode::None);
    const auto position = previewing ? drag_preview_start_ms_ : item.value("position_ms", item.value("start_ms", 0.0));
    const auto x = static_cast<float>(time_to_pixel(position));
    if (kind == "loop_region" || kind == "transition_region") {
        const auto len = previewing ? drag_preview_length_ms_ : item.value("length_ms", 0.0);
        const auto end = static_cast<float>(time_to_pixel(position + len));
        return { x, 3.0f, std::max(8.0f, end - x), 12.0f };
    }
    return { x, 2.0f, kind == "tempo" ? 75.0f : 60.0f, 14.0f };
}

int TimelineRuler::logic_at(juce::Point<float> point) const
{
    const auto* items = get_logic_items();
    if (!items || point.y < 0.0f || point.y >= 18.0f) return -1;
    for (int index = static_cast<int>(items->size()) - 1; index >= 0; --index) {
        if (logic_bounds((*items)[static_cast<std::size_t>(index)], index).expanded(0.0f, 2.0f).contains(point)) {
            return index;
        }
    }
    return -1;
}

TimelineRuler::LogicDragMode TimelineRuler::hit_drag_mode(const nlohmann::json& item, juce::Point<float> point) const
{
    const auto kind = item.value("kind", "");
    if (kind == "loop_region" || kind == "transition_region") {
        const auto area = logic_bounds(item);
        if (area.getWidth() >= 16.0f) {
            if (point.x <= area.getX() + 6.0f) return LogicDragMode::ResizeLeft;
            if (point.x >= area.getRight() - 6.0f) return LogicDragMode::ResizeRight;
        }
    }
    return LogicDragMode::Move;
}

void TimelineRuler::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(OaeTheme::Colors::bgSurface));

    // Bottom border
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    const double visible_start_ms = pixel_to_time(0.0f);
    const double visible_end_ms = pixel_to_time(bounds.getWidth());

    // Access event logic items if available
    const auto* logic_items = get_logic_items();

    // Ruler graduations
    g.setFont(juce::FontOptions(11.0f));

    if (musical_mode_) {
        const auto segments = TimelineSnap::extract_tempo_segments(logic_items);
        const auto& tempo = TimelineSnap::get_active_tempo(visible_start_ms, segments);
        const double bar_ms = tempo.bar_duration_ms();
        const double beat_ms = tempo.beat_duration_ms();

        const int start_bar = static_cast<int>(visible_start_ms / bar_ms);
        const int end_bar = static_cast<int>(visible_end_ms / bar_ms) + 1;

        for (int b = start_bar; b <= end_bar; ++b) {
            const double bar_time = b * bar_ms;
            const float bar_x = static_cast<float>(time_to_pixel(bar_time));

            g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
            g.drawVerticalLine(static_cast<int>(bar_x), bounds.getHeight() - 14.0f, bounds.getHeight());

            g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
            g.drawText(juce::String(b + 1), juce::Rectangle<float>(bar_x + 3.0f, bounds.getHeight() - 16.0f, 40.0f, 14.0f),
                       juce::Justification::centredLeft, false);

            // Sub-beats
            if (pixels_per_second_ >= 50.0) {
                for (int beat = 1; beat < tempo.ts_upper; ++beat) {
                    const float beat_x = static_cast<float>(time_to_pixel(bar_time + beat * beat_ms));
                    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
                    g.drawVerticalLine(static_cast<int>(beat_x), bounds.getHeight() - 8.0f, bounds.getHeight());
                }
            }
        }
    } else {
        // Seconds mode
        double interval_sec = 1.0;
        if (pixels_per_second_ >= 300.0) interval_sec = 0.1;
        else if (pixels_per_second_ >= 100.0) interval_sec = 0.5;
        else if (pixels_per_second_ <= 30.0) interval_sec = 5.0;

        const double interval_ms = interval_sec * 1000.0;
        const int64_t start_idx = static_cast<int64_t>(visible_start_ms / interval_ms);
        const int64_t end_idx = static_cast<int64_t>(visible_end_ms / interval_ms) + 1;

        for (int64_t i = start_idx; i <= end_idx; ++i) {
            const double t_ms = i * interval_ms;
            const float x = static_cast<float>(time_to_pixel(t_ms));

            g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
            g.drawVerticalLine(static_cast<int>(x), bounds.getHeight() - 12.0f, bounds.getHeight());

            g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
            juce::String label;
            if (interval_sec < 1.0) {
                label = juce::String::formatted("%.2fs", t_ms / 1000.0);
            } else {
                const int sec = static_cast<int>(t_ms / 1000.0);
                const int mins = sec / 60;
                const int rem_sec = sec % 60;
                label = juce::String::formatted("%02d:%02d", mins, rem_sec);
            }
            g.drawText(label, juce::Rectangle<float>(x + 3.0f, bounds.getHeight() - 16.0f, 50.0f, 14.0f),
                       juce::Justification::centredLeft, false);
        }
    }

    // Render Logic Items in upper strip (height: 20px)
    if (logic_items && logic_items->is_array()) {
        const auto& selection = model_.get_selection();
        auto has_condition = [](const nlohmann::json& it) {
            if (!it.contains("condition") || !it["condition"].is_object()) return false;
            const auto& cond = it["condition"];
            const bool has_terms = cond.contains("terms") && cond["terms"].is_array() && !cond["terms"].empty();
            const bool has_prob = cond.value("probability_pct", 100.0) < 99.99;
            return has_terms || has_prob;
        };

        for (size_t i = 0; i < logic_items->size(); ++i) {
            const auto& item = (*logic_items)[i];
            const std::string kind = item.value("kind", "");
            const auto area = logic_bounds(item, static_cast<int>(i));
            const juce::String cond_badge = has_condition(item) ? " *" : "";
            g.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
            if (kind == "marker") {
                const std::string name = item.value("name", "Marker");
                g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
                g.fillRoundedRectangle(area, 3.0f);
                g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
                g.drawText(juce::String::fromUTF8(name.c_str()), area.reduced(2, 0), juce::Justification::centredLeft, true);
            } else if (kind == "tempo") {
                const double bpm = item.value("bpm", 120.0);
                const int upper = item.value("ts_upper", 4);
                const int lower = item.value("ts_lower", 4);
                g.setColour(juce::Colour(OaeTheme::Colors::accentGreen));
                g.fillRoundedRectangle(area, 3.0f);
                g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                const juce::String txt = juce::String::formatted("%.0f BPM %d/%d", bpm, upper, lower);
                g.drawText(txt, area.reduced(2, 0), juce::Justification::centred, false);
            } else if (kind == "loop_region") {
                g.setColour(juce::Colour(OaeTheme::Colors::accentAmber).withAlpha(0.33f));
                g.fillRect(area);
                g.setColour(juce::Colour(OaeTheme::Colors::accentAmber));
                g.drawRect(area, 1.0f);
                g.fillRect(area.getX(), area.getY(), 2.5f, area.getHeight());
                g.fillRect(area.getRight() - 2.5f, area.getY(), 2.5f, area.getHeight());
                juce::String loop_txt = "LOOP";
                if (item.value("until_key_off", false)) {
                    loop_txt += " [KeyOff]";
                } else {
                    const int iters = item.value("iterations", -1);
                    loop_txt += iters < 0 ? juce::String::fromUTF8(" [∞]") : (" [" + juce::String(iters) + "x]");
                }
                loop_txt += cond_badge;
                g.drawText(loop_txt, area.reduced(4, 0), juce::Justification::centred, true);
            } else if (kind == "sustain_point") {
                g.setColour(juce::Colour(OaeTheme::Colors::accentPurple));
                g.fillRoundedRectangle(area, 3.0f);
                g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                g.drawText("SUSTAIN", area, juce::Justification::centred, true);
            } else {
                const bool is_region = (kind == "transition_region");
                const auto target = item.value("target_marker", "");
                const auto quant = item.value("quantization", "immediate");
                const juce::String q_tag = (quant == "beat") ? " [Beat]" : (quant == "bar" ? " [Bar]" : "");
                const juce::String label = (target.empty() ? juce::String("TRANSITION")
                                                           : ("-> " + juce::String::fromUTF8(target.c_str())))
                                           + q_tag + cond_badge;
                if (is_region) {
                    g.setColour(juce::Colour(OaeTheme::Colors::accentCoral).withAlpha(0.35f));
                    g.fillRect(area);
                    g.setColour(juce::Colour(OaeTheme::Colors::accentCoral));
                    g.drawRect(area, 1.0f);
                    g.fillRect(area.getX(), area.getY(), 2.5f, area.getHeight());
                    g.fillRect(area.getRight() - 2.5f, area.getY(), 2.5f, area.getHeight());
                    g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                    g.drawText(label, area.reduced(4, 0), juce::Justification::centred, true);
                } else {
                    g.setColour(juce::Colour(OaeTheme::Colors::accentCoral));
                    g.fillRoundedRectangle(area, 3.0f);
                    g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                    g.drawText(label, area.reduced(2, 0), juce::Justification::centred, true);
                }
            }
            if (selection.type == ItemType::Event && selection.id_or_path == event_path_
                && selection.logic_index == static_cast<int>(i)) {
                g.setColour(juce::Colour(OaeTheme::Colors::indicator));
                g.drawRect(area, 1.0f);
            }
        }
    }

    // Playhead cursor on ruler
    const float playhead_x = static_cast<float>(time_to_pixel(playhead_time_ms_));
    if (playhead_x >= 0.0f && playhead_x <= bounds.getWidth()) {
        juce::Path cursorPath;
        cursorPath.startNewSubPath(playhead_x - 6.0f, 0.0f);
        cursorPath.lineTo(playhead_x + 6.0f, 0.0f);
        cursorPath.lineTo(playhead_x + 6.0f, bounds.getHeight() - 6.0f);
        cursorPath.lineTo(playhead_x, bounds.getHeight());
        cursorPath.lineTo(playhead_x - 6.0f, bounds.getHeight() - 6.0f);
        cursorPath.closeSubPath();

        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.fillPath(cursorPath);
        g.setColour(juce::Colour(OaeTheme::Colors::indicator));
        g.strokePath(cursorPath, juce::PathStrokeType(1.0f));
    }

    if (has_ghost_guide_) {
        const float raw_x = static_cast<float>(time_to_pixel(ghost_raw_ms_));
        const float snap_x = static_cast<float>(time_to_pixel(ghost_snapped_ms_));
        if (std::abs(raw_x - snap_x) > 1.0f) {
            g.setColour(juce::Colour(OaeTheme::Colors::textMuted).withAlpha(0.65f));
            g.drawVerticalLine(static_cast<int>(std::round(raw_x)), 0.0f, bounds.getHeight());
        }
        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber));
        g.drawVerticalLine(static_cast<int>(std::round(snap_x)), 0.0f, bounds.getHeight());
        juce::Path guideHead;
        guideHead.addTriangle(snap_x - 4.0f, bounds.getHeight() - 8.0f,
                              snap_x + 4.0f, bounds.getHeight() - 8.0f,
                              snap_x, bounds.getHeight());
        g.fillPath(guideHead);
    }
}

void TimelineRuler::mouseMove(const juce::MouseEvent& e)
{
    const int index = logic_at(e.position);
    if (index >= 0) {
        const auto* items = get_logic_items();
        if (items && static_cast<std::size_t>(index) < items->size()) {
            const auto mode = hit_drag_mode((*items)[static_cast<std::size_t>(index)], e.position);
            if (mode == LogicDragMode::ResizeLeft || mode == LogicDragMode::ResizeRight) {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void TimelineRuler::mouseDown(const juce::MouseEvent& e)
{
    dragged_logic_index_ = -1;
    drag_mode_ = LogicDragMode::None;
    const int hit_index = logic_at(e.position);

    if (e.mods.isPopupMenu()) {
        if (hit_index >= 0) {
            model_.select({ ItemType::Event, event_path_, -1, -1, hit_index });
            repaint();
        }
        const double clicked_time_ms = pixel_to_time(static_cast<float>(e.x));
        show_context_menu(e.getScreenPosition(), clicked_time_ms, hit_index);
        return;
    }

    if (hit_index >= 0) {
        const auto& item = (*get_logic_items())[static_cast<std::size_t>(hit_index)];
        dragged_logic_index_ = hit_index;
        drag_mode_ = hit_drag_mode(item, e.position);
        dragged_logic_start_ = item.value("position_ms", item.value("start_ms", 0.0));
        dragged_logic_length_ = item.value("length_ms", 0.0);
        drag_preview_start_ms_ = dragged_logic_start_;
        drag_preview_length_ms_ = dragged_logic_length_;
        dragged_mouse_x_ = e.position.x;
        model_.select({ ItemType::Event, event_path_, -1, -1, hit_index });
        grabKeyboardFocus();
        repaint();
        return;
    }
    handle_scrub(e);
}

void TimelineRuler::update_drag_preview(const juce::MouseEvent& e)
{
    const auto* items = get_logic_items();
    if (!items || dragged_logic_index_ < 0 || static_cast<std::size_t>(dragged_logic_index_) >= items->size()) return;
    const double delta_ms = (e.position.x - dragged_mouse_x_) * 1000.0 / pixels_per_second_;

    if (drag_mode_ == LogicDragMode::Move) {
        double pos = dragged_logic_start_ + delta_ms;
        if (snap_ && !e.mods.isShiftDown()) pos = snap_->snap(pos, items);
        drag_preview_start_ms_ = std::clamp(pos, 0.0, 86400000.0 - dragged_logic_length_);
        drag_preview_length_ms_ = dragged_logic_length_;
    } else if (drag_mode_ == LogicDragMode::ResizeLeft) {
        const double orig_end = dragged_logic_start_ + dragged_logic_length_;
        double new_start = dragged_logic_start_ + delta_ms;
        if (snap_ && !e.mods.isShiftDown()) new_start = snap_->snap(new_start, items);
        new_start = std::clamp(new_start, 0.0, std::max(0.0, orig_end - 1.0));
        drag_preview_start_ms_ = new_start;
        drag_preview_length_ms_ = std::max(1.0, orig_end - new_start);
    } else if (drag_mode_ == LogicDragMode::ResizeRight) {
        double new_end = dragged_logic_start_ + dragged_logic_length_ + delta_ms;
        if (snap_ && !e.mods.isShiftDown()) new_end = snap_->snap(new_end, items);
        new_end = std::clamp(new_end, dragged_logic_start_ + 1.0, 86400000.0);
        drag_preview_start_ms_ = dragged_logic_start_;
        drag_preview_length_ms_ = new_end - dragged_logic_start_;
    }
}

void TimelineRuler::mouseDrag(const juce::MouseEvent& e)
{
    if (dragged_logic_index_ >= 0 && drag_mode_ != LogicDragMode::None) {
        update_drag_preview(e);
        repaint();
        return;
    }
    handle_scrub(e);
}

void TimelineRuler::mouseUp(const juce::MouseEvent& e)
{
    const int index = std::exchange(dragged_logic_index_, -1);
    const auto mode = std::exchange(drag_mode_, LogicDragMode::None);
    if (index < 0 || mode == LogicDragMode::None || !e.mouseWasDraggedSinceMouseDown()) {
        repaint();
        return;
    }
    update_drag_preview(e);
    const auto& selection = model_.get_selection();
    if (selection.id_or_path != event_path_ || selection.logic_index != index) {
        repaint();
        return;
    }
    const auto* items = get_logic_items();
    if (!items || static_cast<std::size_t>(index) >= items->size()) {
        repaint();
        return;
    }
    const auto& item = (*items)[static_cast<std::size_t>(index)];
    const bool has_len = item.value("kind", "") == "loop_region" || item.value("kind", "") == "transition_region";
    if (drag_preview_start_ms_ == dragged_logic_start_ && (!has_len || drag_preview_length_ms_ == dragged_logic_length_)) {
        repaint();
        return;
    }
    model_.modify([&](nlohmann::json& root) {
        for (auto& event : root["events"]) {
            if (event.value("path", "") != event_path_) continue;
            auto& moved = event["logic_items"][static_cast<std::size_t>(index)];
            moved[moved.contains("start_ms") ? "start_ms" : "position_ms"] = drag_preview_start_ms_;
            if (has_len) {
                moved["length_ms"] = drag_preview_length_ms_;
            }
            break;
        }
    });
}

bool TimelineRuler::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        const auto& sel = model_.get_selection();
        if (sel.type == ItemType::Event && sel.id_or_path == event_path_ && sel.logic_index >= 0) {
            const size_t idx = static_cast<size_t>(sel.logic_index);
            if (model_.remove_logic_item(event_path_, idx)) {
                model_.select({ ItemType::Event, event_path_, -1, -1, -1 });
                repaint();
                return true;
            }
        }
    }
    return Component::keyPressed(key);
}

void TimelineRuler::handle_scrub(const juce::MouseEvent& e)
{
    double target_time_ms = pixel_to_time(static_cast<float>(e.x));
    if (snap_ && !e.mods.isShiftDown()) {
        target_time_ms = snap_->snap(target_time_ms, get_logic_items());
    }

    set_playhead_time_ms(target_time_ms);
    if (onPlayheadChanged) {
        onPlayheadChanged(playhead_time_ms_);
    }
}

void TimelineRuler::show_context_menu(juce::Point<int> screen_pos, double clicked_time_ms, int hit_logic_index)
{
    bool has_marker = false;
    if (const auto* items = get_logic_items()) {
        for (const auto& item : *items) {
            if (item.value("kind", "") == "marker" && !item.value("name", "").empty()) {
                has_marker = true;
                break;
            }
        }
    }

    juce::PopupMenu menu;
    menu.addItem(1, "Add Marker Here");
    menu.addItem(2, "Add Tempo Marker Here (120 BPM)");
    menu.addItem(3, "Add Loop Region Here (2s)");
    menu.addItem(4, "Add Sustain Point Here");
    menu.addItem(5, "Add Transition Marker Here", has_marker);
    menu.addItem(6, "Add Transition Region Here (2s)", has_marker);
    if (hit_logic_index >= 0) {
        menu.addSeparator();
        menu.addItem(7, "Delete Logic Item");
    }
    menu.addSeparator();
    menu.addItem(8, musical_mode_ ? "Switch to Timecode (Seconds)" : "Switch to Musical (Bars/Beats)");

    juce::Component::SafePointer<TimelineRuler> safe_this(this);
    UiTestDialogSeam::show_popup_menu("timeline_ruler_context_menu", menu,
        juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screen_pos.x, screen_pos.y, 1, 1)),
        [safe_this, clicked_time_ms, hit_logic_index](int result) {
            if (!safe_this) return;
            if (result == 1) {
                safe_this->model_.add_marker(safe_this->event_path_, "Marker", clicked_time_ms);
            } else if (result == 2) {
                safe_this->model_.add_tempo_marker(safe_this->event_path_, 120.0, 4, 4, clicked_time_ms);
            } else if (result == 3) {
                safe_this->model_.add_loop_region(safe_this->event_path_, clicked_time_ms, 2000.0, -1);
            } else if (result == 4) {
                safe_this->model_.add_sustain_point(safe_this->event_path_, clicked_time_ms);
            } else if (result == 5) {
                safe_this->model_.add_transition_marker(safe_this->event_path_, clicked_time_ms, "");
            } else if (result == 6) {
                safe_this->model_.add_transition_region(safe_this->event_path_, clicked_time_ms, 2000.0, "");
            } else if (result == 7 && hit_logic_index >= 0) {
                if (safe_this->model_.remove_logic_item(safe_this->event_path_, static_cast<size_t>(hit_logic_index))) {
                    safe_this->model_.select({ ItemType::Event, safe_this->event_path_, -1, -1, -1 });
                }
            } else if (result == 8) {
                safe_this->musical_mode_ = !safe_this->musical_mode_;
            }
            safe_this->repaint();
        });
}

std::vector<TestableTarget> TimelineRuler::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};
    std::vector<TestableTarget> out;
    const int scrubTop = std::min(18, bounds.getHeight() / 2);
    const auto scrubRect = juce::Rectangle<int>(0, scrubTop, bounds.getWidth(), std::max(1, bounds.getHeight() - scrubTop)).getIntersection(bounds);
    if (!scrubRect.isEmpty()) {
        out.push_back({ "scrub", "drag", scrubRect, isEnabled() });
    }
    if (const auto* items = get_logic_items()) {
        for (size_t i = 0; i < items->size(); ++i) {
            const auto& item = (*items)[i];
            const auto fb = logic_bounds(item, static_cast<int>(i));
            const auto ib = fb.toNearestInt().getIntersection(bounds);
            if (ib.isEmpty()) continue;
            const std::string baseId = "logic_" + std::to_string(i);
            const std::string kind = item.value("kind", "");
            const bool isRegion = (kind == "loop_region" || kind == "transition_region");
            if (isRegion && ib.getWidth() >= 18) {
                const auto leftR = juce::Rectangle<int>(ib.getX(), ib.getY(), 5, ib.getHeight()).getIntersection(bounds);
                const auto rightR = juce::Rectangle<int>(ib.getRight() - 5, ib.getY(), 5, ib.getHeight()).getIntersection(bounds);
                const auto bodyR = juce::Rectangle<int>(ib.getX() + 5, ib.getY(), ib.getWidth() - 10, ib.getHeight()).getIntersection(bounds);
                if (!bodyR.isEmpty()) out.push_back({ baseId, "drag", bodyR, isEnabled() });
                if (!leftR.isEmpty()) out.push_back({ baseId + ".resize_left", "drag", leftR, isEnabled() });
                if (!rightR.isEmpty()) out.push_back({ baseId + ".resize_right", "drag", rightR, isEnabled() });
            } else {
                out.push_back({ baseId, "drag", ib, isEnabled() });
            }
        }
    }
    return out;
}

} // namespace oae::studio
