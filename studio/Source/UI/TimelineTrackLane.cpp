#include "TimelineTrackLane.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace oae::studio {

namespace {

fs::path resolve_sample_file(const AuthoringModel& model, const std::string& sample_id)
{
    if (sample_id.empty()) return {};
    const auto& root = model.document().get_root_json();
    if (root.contains("samples") && root["samples"].is_array()) {
        for (const auto& s : root["samples"]) {
            if (s.value("id", "") == sample_id) {
                const std::string rel_file = s.value("file", "");
                return model.document().get_project_dir() / fs::u8path(rel_file);
            }
        }
    }
    return {};
}

std::string lookup_audio_table_sample(const AuthoringModel& model, const std::string& key)
{
    if (key.empty()) return {};
    for (const auto& entry : model.get_audio_table_entries()) {
        if (entry.first == key && !entry.second.empty()) {
            return entry.second;
        }
    }
    const auto& root = model.document().get_root_json();
    if (root.contains("audio_table") && root["audio_table"].is_object()) {
        const auto& tbl = root["audio_table"];
        if (tbl.contains(key) && tbl[key].is_string()) {
            return tbl[key].get<std::string>();
        }
    }
    return {};
}

const nlohmann::json* find_event_json(const nlohmann::json& root, const std::string& event_path)
{
    if (!root.contains("events") || !root["events"].is_array()) return nullptr;
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == event_path) {
            return &ev;
        }
    }
    return nullptr;
}

const nlohmann::json* find_instrument_json(const AuthoringModel& model,
                                           const std::string& event_path,
                                           size_t track_index,
                                           size_t instrument_index)
{
    const auto* ev = find_event_json(model.document().get_root_json(), event_path);
    if (!ev || !ev->contains("tracks") || !(*ev)["tracks"].is_array()) return nullptr;
    const auto& trks = (*ev)["tracks"];
    if (track_index >= trks.size()) return nullptr;
    if (!trks[track_index].contains("instruments") || !trks[track_index]["instruments"].is_array()) return nullptr;
    const auto& insts = trks[track_index]["instruments"];
    if (instrument_index >= insts.size()) return nullptr;
    return &insts[instrument_index];
}

void push_unique_sample(std::vector<std::string>& out, const std::string& sample_id)
{
    if (sample_id.empty()) return;
    if (std::find(out.begin(), out.end(), sample_id) == out.end()) {
        out.push_back(sample_id);
    }
}

void collect_event_sample_ids_recursive(const AuthoringModel& model,
                                        const std::string& target_event_path,
                                        std::unordered_set<std::string>& visited_events,
                                        std::vector<std::string>& out_sample_ids)
{
    if (target_event_path.empty() || !visited_events.insert(target_event_path).second) {
        return;
    }
    const auto* ev = find_event_json(model.document().get_root_json(), target_event_path);
    if (!ev || !ev->contains("tracks") || !(*ev)["tracks"].is_array()) {
        return;
    }
    for (const auto& trk : (*ev)["tracks"]) {
        if (!trk.contains("instruments") || !trk["instruments"].is_array()) continue;
        for (const auto& inst : trk["instruments"]) {
            collect_instrument_sample_ids(model, inst, target_event_path, visited_events, out_sample_ids);
        }
    }
}

std::string extract_leaf_name(const std::string& path)
{
    const auto pos = path.find_last_of('/');
    return (pos != std::string::npos && pos + 1 < path.size()) ? path.substr(pos + 1) : path;
}

bool event_exists_in_model(const AuthoringModel& model, const std::string& event_path)
{
    if (event_path.empty()) return false;
    const auto& root = model.document().get_root_json();
    if (!root.contains("events") || !root["events"].is_array()) return false;
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == event_path && ev.value("kind", "event") == "event") {
            return true;
        }
    }
    return false;
}

} // namespace

void collect_instrument_sample_ids(const AuthoringModel& model,
                                   const nlohmann::json& inst,
                                   const std::string& current_event_path,
                                   std::unordered_set<std::string>& visited_events,
                                   std::vector<std::string>& out_sample_ids)
{
    if (!inst.is_object()) return;
    if (!current_event_path.empty()) {
        visited_events.insert(current_event_path);
    }

    const std::string kind = inst.value("kind", "single");
    if (kind == "single") {
        push_unique_sample(out_sample_ids, inst.value("sample_id", ""));
    } else if (kind == "multi" || kind == "scatterer") {
        if (inst.contains("playlist") && inst["playlist"].is_array()) {
            for (const auto& item : inst["playlist"]) {
                if (!item.is_object()) continue;
                const std::string sid = item.value("sample_id", "");
                if (!sid.empty()) {
                    push_unique_sample(out_sample_ids, sid);
                }
                const std::string nested_ev = item.value("target_event", "");
                if (!nested_ev.empty()) {
                    collect_event_sample_ids_recursive(model, nested_ev, visited_events, out_sample_ids);
                }
            }
        }
        push_unique_sample(out_sample_ids, inst.value("sample_id", ""));
    } else if (kind == "programmer") {
        const std::string key = inst.value("programmer_key", "");
        push_unique_sample(out_sample_ids, lookup_audio_table_sample(model, key));
    } else if (kind == "event") {
        const std::string target_ev = inst.value("target_event", "");
        if (!target_ev.empty()) {
            collect_event_sample_ids_recursive(model, target_ev, visited_events, out_sample_ids);
        }
    }
}

ClipComponent::ClipComponent(AuthoringModel& model,
                             WaveformCache& waveform_cache,
                             const std::string& event_path,
                             size_t track_index,
                             size_t instrument_index)
    : model_(model), waveform_cache_(waveform_cache),
      event_path_(event_path), track_index_(track_index), instrument_index_(instrument_index)
{
    setWantsKeyboardFocus(true);
    update_from_model();
}

void ClipComponent::update_from_model()
{
    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                const auto& trks = ev["tracks"];
                if (track_index_ < trks.size() && trks[track_index_].contains("instruments")) {
                    const auto& insts = trks[track_index_]["instruments"];
                    if (insts.is_array() && instrument_index_ < insts.size()) {
                        const auto& inst = insts[instrument_index_];
                        kind_ = inst.value("kind", "single");
                        sample_id_ = inst.value("sample_id", "");
                        target_path_ = inst.value(kind_ == "command" ? "command_target" : (kind_ == "programmer" ? "programmer_key" : "target_event"), "");
                        command_type_ = (kind_ == "command") ? inst.value("command_type", "start_event") : "start_event";
                        offset_ms_ = inst.value("offset_ms", 0.0);
                        length_ms_ = inst.value("length_ms", 500.0);
                        start_offset_ms_ = inst.value("start_offset_ms", 0.0);
                        pitch_semitones_ = inst.value("pitch_semitones_base", 0.0);
                        loop_ = inst.value("loop", false);
                        fade_in_ms_ = inst.value("fade_in_ms", 0.0);
                        fade_out_ms_ = inst.value("fade_out_ms", 0.0);
                    }
                }
                break;
            }
        }
    }

    const auto& sel = model_.get_selection();
    is_selected_ = (sel.type == ItemType::Event && sel.id_or_path == event_path_
                    && sel.track_index == static_cast<int>(track_index_)
                    && sel.instrument_index == static_cast<int>(instrument_index_));
    repaint();
}

ClipComponent::DragMode ClipComponent::hit_test_drag_mode(juce::Point<int> pos) const
{
    if (kind_ == "command") return DragMode::Move;
    if (pos.x <= 8) return DragMode::ResizeLeft;
    if (pos.x >= getWidth() - 8) return DragMode::ResizeRight;
    if (pos.y <= 12 && pos.x <= 20) return DragMode::FadeIn;
    if (pos.y <= 12 && pos.x >= getWidth() - 20) return DragMode::FadeOut;
    return DragMode::Move;
}

void ClipComponent::mouseMove(const juce::MouseEvent& e)
{
    const auto mode = hit_test_drag_mode(e.getPosition());
    if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight) {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    } else if (mode == DragMode::FadeIn || mode == DragMode::FadeOut) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void ClipComponent::mouseDown(const juce::MouseEvent& e)
{
    if (isShowing() || isOnDesktop()) {
        grabKeyboardFocus();
    }

    if (e.mods.isPopupMenu()) {
        show_context_menu(e.getScreenPosition());
        return;
    }

    current_drag_mode_ = hit_test_drag_mode(e.getPosition());
    drag_start_mouse_ = e.getPosition();
    drag_start_offset_ms_ = offset_ms_;
    drag_start_length_ms_ = length_ms_;
    drag_start_fade_in_ms_ = fade_in_ms_;
    drag_start_fade_out_ms_ = fade_out_ms_;

    model_.begin_gesture();
    model_.select(ItemSelection{ ItemType::Event, event_path_, static_cast<int>(track_index_), static_cast<int>(instrument_index_) });
    repaint();
}

void ClipComponent::mouseDrag(const juce::MouseEvent& e)
{
    const int delta_x = e.getDistanceFromDragStartX();
    const int delta_y = e.getDistanceFromDragStartY();

    if (current_drag_mode_ == DragMode::Move) {
        if (onClipDragged) onClipDragged(this, delta_x, delta_y, e.mods.isShiftDown());
    } else if (current_drag_mode_ == DragMode::ResizeRight) {
        const double px_per_ms = static_cast<double>(getWidth()) / std::max(1.0, length_ms_);
        const double delta_ms = static_cast<double>(delta_x) / std::max(0.001, px_per_ms);
        const double new_len = std::max(10.0, drag_start_length_ms_ + delta_ms);
        length_ms_ = new_len;
        if (auto* parent = getParentComponent()) {
            parent->resized();
        }
    } else if (current_drag_mode_ == DragMode::ResizeLeft) {
        const double px_per_ms = static_cast<double>(getWidth()) / std::max(1.0, length_ms_);
        const double delta_ms = static_cast<double>(delta_x) / std::max(0.001, px_per_ms);
        const double new_off = std::max(0.0, drag_start_offset_ms_ + delta_ms);
        const double new_len = std::max(10.0, drag_start_length_ms_ - (new_off - drag_start_offset_ms_));
        offset_ms_ = new_off;
        length_ms_ = new_len;
        if (auto* parent = getParentComponent()) {
            parent->resized();
        }
    } else if (current_drag_mode_ == DragMode::FadeIn) {
        const double delta_ms = (static_cast<double>(e.getPosition().x) / std::max(1, getWidth())) * length_ms_;
        fade_in_ms_ = std::clamp(delta_ms, 0.0, length_ms_ * 0.9);
        repaint();
    } else if (current_drag_mode_ == DragMode::FadeOut) {
        const double delta_ms = (static_cast<double>(getWidth() - e.getPosition().x) / std::max(1, getWidth())) * length_ms_;
        fade_out_ms_ = std::clamp(delta_ms, 0.0, length_ms_ * 0.9);
        repaint();
    }
}

void ClipComponent::mouseUp(const juce::MouseEvent&)
{
    const auto mode = current_drag_mode_;
    current_drag_mode_ = DragMode::None;

    if (mode == DragMode::Move) {
        if (onClipDragEnded) onClipDragEnded(this);
    } else if (mode == DragMode::ResizeRight || mode == DragMode::ResizeLeft) {
        const std::string path = event_path_;
        const size_t trk = track_index_;
        const size_t inst = instrument_index_;
        const double off = offset_ms_;
        const double len = length_ms_;
        const double start_off = start_offset_ms_;
        auto on_mod = onClipModified;
        auto& model = model_;

        juce::MessageManager::callAsync([&model, path, trk, inst, off, len, start_off, on_mod]() {
            model.resize_instrument(path, trk, inst, off, len, start_off);
            model.end_gesture();
            if (on_mod) on_mod();
        });
    } else if (mode == DragMode::FadeIn || mode == DragMode::FadeOut) {
        const std::string path = event_path_;
        const size_t trk = track_index_;
        const size_t inst = instrument_index_;
        const double fin = fade_in_ms_;
        const double fout = fade_out_ms_;
        auto& model = model_;

        juce::MessageManager::callAsync([&model, path, trk, inst, fin, fout]() {
            model.set_instrument_fades(path, trk, inst, fin, fout);
            model.end_gesture();
        });
    } else {
        model_.end_gesture();
    }
}

void ClipComponent::open_target_event()
{
    if (kind_ == "event" && event_exists_in_model(model_, target_path_)) {
        model_.select({ ItemType::Event, target_path_, -1, -1 });
    }
}

void ClipComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    open_target_event();
}

std::vector<TestableTarget> ClipComponent::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();
    std::vector<TestableTarget> out;
    if (kind_ == "command" || w < 20) {
        out.push_back({ "body", "drag", bounds, isEnabled() });
        return out;
    }
    const int bodyY = (h > 20) ? 13 : 0;
    const int bodyH = std::max(1, h - bodyY);
    const auto bodyRect = juce::Rectangle<int>(9, bodyY, std::max(2, w - 18), bodyH).getIntersection(bounds);
    if (!bodyRect.isEmpty()) {
        out.push_back({ "body", "drag", bodyRect, isEnabled() });
    }
    const auto leftRect = juce::Rectangle<int>(0, 0, 8, h).getIntersection(bounds);
    const auto rightRect = juce::Rectangle<int>(w - 8, 0, 8, h).getIntersection(bounds);
    if (!leftRect.isEmpty()) out.push_back({ "resize_left", "drag", leftRect, isEnabled() });
    if (!rightRect.isEmpty()) out.push_back({ "resize_right", "drag", rightRect, isEnabled() });
    if (w >= 44 && h >= 14) {
        const auto fadeInRect = juce::Rectangle<int>(9, 0, 11, 12).getIntersection(bounds);
        const auto fadeOutRect = juce::Rectangle<int>(w - 20, 0, 11, 12).getIntersection(bounds);
        if (!fadeInRect.isEmpty()) out.push_back({ "fade_in", "drag", fadeInRect, isEnabled() });
        if (!fadeOutRect.isEmpty()) out.push_back({ "fade_out", "drag", fadeOutRect, isEnabled() });
    }
    return out;
}

juce::String ClipComponent::get_display_title() const
{
    if (kind_ == "single") {
        return "[SGL] " + (sample_id_.empty() ? juce::String("Single") : juce::String::fromUTF8(sample_id_.c_str()));
    }
    if (kind_ == "multi") {
        return "[MLT] " + (sample_id_.empty() ? juce::String("Multi") : juce::String::fromUTF8(sample_id_.c_str()));
    }
    if (kind_ == "scatterer") {
        return "[SCT] " + (sample_id_.empty() ? juce::String("Scatterer") : juce::String::fromUTF8(sample_id_.c_str()));
    }
    if (kind_ == "programmer") {
        return "[PRG] " + (target_path_.empty() ? juce::String("(Key)") : juce::String::fromUTF8(target_path_.c_str()));
    }
    if (kind_ == "event") {
        const auto leaf = target_path_.empty() ? std::string("(No target)") : extract_leaf_name(target_path_);
        return "[EVT] " + juce::String::fromUTF8(leaf.c_str());
    }
    if (kind_ == "command") {
        const auto leaf = target_path_.empty() ? std::string("(No target)") : extract_leaf_name(target_path_);
        return "[CMD] " + juce::String::fromUTF8(command_type_.c_str()) + " " + juce::String::fromUTF8(leaf.c_str());
    }
    return "[" + juce::String::fromUTF8(kind_.c_str()).toUpperCase() + "]";
}

std::shared_ptr<const WaveformEnvelope> ClipComponent::resolve_waveform_envelope(bool synchronous) const
{
    std::vector<std::string> sample_ids;
    std::unordered_set<std::string> visited;
    if (const auto* inst = find_instrument_json(model_, event_path_, track_index_, instrument_index_)) {
        collect_instrument_sample_ids(model_, *inst, event_path_, visited, sample_ids);
    }
    if (sample_ids.empty() && !sample_id_.empty()) {
        sample_ids.push_back(sample_id_);
    }

    std::shared_ptr<const WaveformEnvelope> env;
    if (!sample_ids.empty()) {
        std::vector<fs::path> file_paths;
        file_paths.reserve(sample_ids.size());
        for (const auto& sid : sample_ids) {
            auto path = resolve_sample_file(model_, sid);
            if (!path.empty()) {
                file_paths.push_back(std::move(path));
            }
        }
        if (file_paths.size() == 1) {
            env = synchronous
                ? waveform_cache_.load_synchronously(file_paths.front())
                : waveform_cache_.get_or_load(file_paths.front());
        } else if (file_paths.size() > 1) {
            std::string cache_key = kind_;
            for (const auto& sid : sample_ids) {
                cache_key += "|";
                cache_key += sid;
            }
            env = synchronous
                ? waveform_cache_.load_composite_synchronously(cache_key, file_paths)
                : waveform_cache_.get_or_load_composite(cache_key, file_paths);
        }
    } else if (kind_ == "programmer") {
        const std::string key = !target_path_.empty() ? target_path_ : std::string("programmer");
        env = waveform_cache_.get_or_build_synthetic(key);
    }

    if (env && !env->is_empty()) {
        cached_envelope_ = env;
        return env;
    }
    return cached_envelope_;
}

void ClipComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 2.0f || bounds.getHeight() <= 2.0f) return;

    const auto track_hex = model_.get_track_color(event_path_, track_index_);
    const juce::Colour trackColour = OaeTheme::TrackColors::parseHexColor(track_hex, track_index_);
    const float bodyAlpha = is_being_dragged_ ? 0.30f : 0.94f;

    // Clip body always follows the parent track color for unified track visual identity
    const juce::Colour bodyColour = trackColour.darker(0.45f).withAlpha(bodyAlpha);
    g.setColour(bodyColour);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Dedicated header bar with type badge and high-contrast illuminated selection inversion
    const float titleH = std::min(18.0f, std::max(12.0f, bounds.getHeight() * 0.38f));
    auto titleArea = bounds.withHeight(titleH);
    {
        juce::Graphics::ScopedSaveState clipScope(g);
        juce::Path clipMask;
        clipMask.addRoundedRectangle(bounds, 4.0f);
        g.reduceClipRegion(clipMask);

        const juce::Colour headerBg = is_selected_
            ? trackColour.brighter(0.55f).withAlpha(bodyAlpha)
            : trackColour.darker(0.15f).withAlpha(bodyAlpha);
        g.setColour(headerBg);
        g.fillRect(titleArea);

        g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(0.45f));
        g.drawHorizontalLine(static_cast<int>(titleArea.getBottom()), bounds.getX(), bounds.getRight());
    }

    // Universal waveform rendering for all instrument types
    auto waveArea = bounds.withTrimmedTop(titleH + 1.0f).reduced(2.0f, 2.0f);
    if (waveArea.getHeight() >= 4.0f && waveArea.getWidth() >= 4.0f) {
        auto env = resolve_waveform_envelope(false);
        if (env && !env->is_empty()) {
            const auto peakCol = trackColour.brighter(0.60f).withAlpha(is_being_dragged_ ? 0.25f : 0.92f);
            const auto rmsCol = trackColour.brighter(0.20f).withAlpha(is_being_dragged_ ? 0.20f : 0.68f);
            env->render_clip(g, waveArea, offset_ms_, length_ms_,
                             start_offset_ms_, pitch_semitones_, loop_,
                             fade_in_ms_, fade_out_ms_,
                             peakCol, rmsCol);
        } else {
            const float centerY = waveArea.getCentreY();
            g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(is_being_dragged_ ? 0.12f : 0.24f));
            g.drawHorizontalLine(static_cast<int>(centerY), waveArea.getX(), waveArea.getRight());
        }
    }

    // Fade In overlay
    if (fade_in_ms_ > 0.0 && length_ms_ > 0.0) {
        const float fadeWidth = static_cast<float>((fade_in_ms_ / length_ms_) * bounds.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(bounds.getX(), bounds.getY());
        fadePath.lineTo(bounds.getX() + fadeWidth, bounds.getY());
        fadePath.lineTo(bounds.getX(), bounds.getBottom());
        fadePath.closeSubPath();
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(0.55f));
        g.fillPath(fadePath);
        g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.7f));
        g.fillEllipse(bounds.getX() + fadeWidth - 3.0f, bounds.getY() + 1.0f, 6.0f, 6.0f);
    }

    // Fade Out overlay
    if (fade_out_ms_ > 0.0 && length_ms_ > 0.0) {
        const float fadeWidth = static_cast<float>((fade_out_ms_ / length_ms_) * bounds.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(bounds.getRight(), bounds.getY());
        fadePath.lineTo(bounds.getRight() - fadeWidth, bounds.getY());
        fadePath.lineTo(bounds.getRight(), bounds.getBottom());
        fadePath.closeSubPath();
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(0.55f));
        g.fillPath(fadePath);
        g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.7f));
        g.fillEllipse(bounds.getRight() - fadeWidth - 3.0f, bounds.getY() + 1.0f, 6.0f, 6.0f);
    }

    // Title bar text with type badge
    g.setColour((is_selected_
        ? juce::Colour(OaeTheme::Colors::clipTitleTextDark)
        : juce::Colour(OaeTheme::Colors::indicator)).withAlpha(is_being_dragged_ ? 0.55f : 1.0f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(get_display_title(), titleArea.reduced(5.0f, 1.0f), juce::Justification::centredLeft, true);

    // High-contrast selection frame or origin drag outline
    if (is_being_dragged_) {
        const float dashPattern[2] = { 4.0f, 3.0f };
        juce::Path borderPath;
        borderPath.addRoundedRectangle(bounds.reduced(1.0f), 4.0f);
        juce::Path dashed;
        juce::PathStrokeType(1.5f).createDashedStroke(dashed, borderPath, dashPattern, 2);
        g.setColour(juce::Colour(OaeTheme::Colors::textSecondary).withAlpha(0.7f));
        g.fillPath(dashed);
    } else if (is_selected_) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(0.85f));
        g.drawRoundedRectangle(bounds.reduced(1.8f), 3.0f, 1.5f);
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.drawRoundedRectangle(bounds.reduced(0.8f), 4.0f, OaeTheme::States::selectedClipBorderThickness);

        if (bounds.getWidth() >= 20.0f && bounds.getHeight() >= 22.0f) {
            const float gripH = std::min(12.0f, bounds.getHeight() * 0.35f);
            const float gripY = bounds.getCentreY() - gripH * 0.5f;
            g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.9f));
            g.fillRoundedRectangle(bounds.getX() + 3.0f, gripY, 2.0f, gripH, 1.0f);
            g.fillRoundedRectangle(bounds.getRight() - 5.0f, gripY, 2.0f, gripH, 1.0f);
        }
    } else {
        g.setColour(trackColour.brighter(0.15f).withAlpha(0.75f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }
}

void ClipComponent::resized() {}

void ClipComponent::show_context_menu(juce::Point<int> screen_pos)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Duplicate Clip");
    menu.addItem(2, "Delete Clip");

    juce::Component::SafePointer<ClipComponent> clip(this);
    UiTestDialogSeam::show_popup_menu("clip_context_menu", menu,
        juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screen_pos.x, screen_pos.y, 1, 1)),
        [clip](int res) {
            if (!clip) return;
            if (res == 1) {
                clip->model_.duplicate_instrument(clip->event_path_, clip->track_index_, clip->instrument_index_);
            } else if (res == 2) {
                clip->model_.remove_instrument(clip->event_path_, clip->track_index_, clip->instrument_index_);
            }
        });
}

// ---------------- AutomationLaneComponent ----------------

AutomationLaneComponent::AutomationLaneComponent(AuthoringModel& model, const std::string& event_path, size_t track_index)
    : model_(model), event_path_(event_path), track_index_(track_index)
{
    setWantsKeyboardFocus(true);
}

void AutomationLaneComponent::set_time_converter(std::function<double(double)> time_to_px,
                                                std::function<double(float)> px_to_time)
{
    time_to_px_ = std::move(time_to_px);
    px_to_time_ = std::move(px_to_time);
    repaint();
}

namespace {

void append_curve_segment(juce::Path& path, float x0, float y0, float x1, float y1, const std::string& shape)
{
    if (shape == "hold") {
        path.lineTo(x1, y0);
        path.lineTo(x1, y1);
    } else if (shape == "ease_in") {
        const float dx = x1 - x0;
        path.cubicTo(x0 + dx * 0.45f, y0, x0 + dx * 0.85f, y0, x1, y1);
    } else if (shape == "ease_out") {
        const float dx = x1 - x0;
        path.cubicTo(x0 + dx * 0.15f, y1, x0 + dx * 0.55f, y1, x1, y1);
    } else if (shape == "s_curve") {
        const float dx = x1 - x0;
        path.cubicTo(x0 + dx * 0.5f, y0, x0 + dx * 0.5f, y1, x1, y1);
    } else {
        path.lineTo(x1, y1);
    }
}

} // namespace

nlohmann::json AutomationLaneComponent::read_points() const
{
    return model_.get_track_timeline_volume_points(event_path_, static_cast<int>(track_index_));
}

juce::Point<float> AutomationLaneComponent::point_to_screen(double x_ms, double y_val, juce::Rectangle<float> bounds) const
{
    const float px = time_to_px_ ? static_cast<float>(time_to_px_(x_ms)) : bounds.getX();
    const double clamped = std::clamp(y_val, -80.0, 10.0);
    const float py = bounds.getY() + bounds.getHeight() * (1.0f - static_cast<float>((clamped + 80.0) / 90.0));
    return { px, py };
}

int AutomationLaneComponent::hit_test_point(juce::Point<float> pos, const nlohmann::json& pts) const
{
    if (!pts.is_array() || !time_to_px_) return -1;
    const auto bounds = getLocalBounds().toFloat();
    int best = -1;
    float bestDist = 8.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const auto screenPt = point_to_screen(pts[i].value("x", 0.0), pts[i].value("y", 0.0), bounds);
        const float dist = pos.getDistanceFrom(screenPt);
        if (dist <= bestDist) {
            bestDist = dist;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void AutomationLaneComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(0, 0.0f, bounds.getWidth());

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle).withAlpha(OaeTheme::States::curveGridOpacity));
    for (float frac : { 0.25f, 0.50f, 0.75f }) {
        g.drawHorizontalLine(static_cast<int>(bounds.getHeight() * frac), 0.0f, bounds.getWidth());
    }

    g.setFont(juce::FontOptions(OaeTheme::Typography::caption));
    g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
    g.drawText("Volume Automation", bounds.reduced(6, 2), juce::Justification::topLeft, false);

    const auto pts = read_points();
    if (!pts.is_array() || pts.empty() || !time_to_px_) return;

    if (pts.size() >= 2) {
        juce::Path curvePath;
        juce::Point<float> prevPt;
        float firstX = 0.0f;
        float lastX = 0.0f;
        for (size_t i = 0; i < pts.size(); ++i) {
            const auto pt = point_to_screen(pts[i].value("x", 0.0), pts[i].value("y", 0.0), bounds);
            if (i == 0) {
                curvePath.startNewSubPath(pt);
                firstX = pt.x;
            } else {
                const auto shape = pts[i - 1].value("shape", "linear");
                append_curve_segment(curvePath, prevPt.x, prevPt.y, pt.x, pt.y, shape);
            }
            prevPt = pt;
            lastX = pt.x;
        }

        juce::Path ribbonPath = curvePath;
        ribbonPath.lineTo(lastX, bounds.getBottom());
        ribbonPath.lineTo(firstX, bounds.getBottom());
        ribbonPath.closeSubPath();

        const auto purple = juce::Colour(OaeTheme::Colors::accentPurple);
        juce::ColourGradient grad(purple.withAlpha(OaeTheme::States::curveRibbonTopOpacity), 0.0f, bounds.getY(),
                                  purple.withAlpha(OaeTheme::States::curveRibbonBottomOpacity), 0.0f, bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(ribbonPath);

        g.setColour(purple);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));
    }

    for (size_t i = 0; i < pts.size(); ++i) {
        const auto pt = point_to_screen(pts[i].value("x", 0.0), pts[i].value("y", 0.0), bounds);
        const bool isSelected = selected_point_index_ >= 0 &&
                                static_cast<size_t>(selected_point_index_) < pts.size() &&
                                static_cast<size_t>(selected_point_index_) == i;
        const bool isActive = static_cast<int>(i) == dragged_point_index_ ||
                              static_cast<int>(i) == hovered_point_index_ || isSelected;
        const float haloAlpha = isActive ? OaeTheme::States::nodeHoverHaloOpacity : 0.15f;
        g.setColour(juce::Colour(OaeTheme::Colors::accentPurple).withAlpha(haloAlpha));
        g.fillEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f);

        g.setColour(juce::Colour(isActive ? OaeTheme::Colors::textPrimary : OaeTheme::Colors::indicator));
        g.fillEllipse(pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
    }

    if (dragged_point_index_ >= 0 && static_cast<size_t>(dragged_point_index_) < pts.size()) {
        const double val = pts[static_cast<size_t>(dragged_point_index_)].value("y", 0.0);
        const auto nodePt = point_to_screen(pts[static_cast<size_t>(dragged_point_index_)].value("x", 0.0), val, bounds);
        const float badgeW = 54.0f;
        const float badgeH = 16.0f;
        const float bx = std::clamp(nodePt.x + 8.0f, 2.0f, std::max(2.0f, bounds.getWidth() - badgeW - 2.0f));
        const float by = std::clamp(nodePt.y - 18.0f, 2.0f, std::max(2.0f, bounds.getHeight() - badgeH - 2.0f));
        const juce::Rectangle<float> badge(bx, by, badgeW, badgeH);
        g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
        g.drawRoundedRectangle(badge, 3.0f, 1.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
        g.setFont(juce::FontOptions(OaeTheme::Typography::caption));
        g.drawText(juce::String(val, 1) + " dB", badge, juce::Justification::centred, false);
    }
}

void AutomationLaneComponent::mouseDown(const juce::MouseEvent& e)
{
    if (isShowing() || isOnDesktop()) {
        grabKeyboardFocus();
    }

    if (e.mods.isPopupMenu()) {
        const auto pts = read_points();
        const int hit = hit_test_point(e.position, pts);
        if (hit >= 0) {
            selected_point_index_ = hit;
            repaint();
            show_context_menu(e.getScreenPosition(), hit);
        }
        return;
    }

    if (e.getNumberOfClicks() >= 2 && px_to_time_) {
        dragged_point_index_ = -1;
        selected_point_index_ = -1;
        const double t_ms = std::max(0.0, px_to_time_(e.position.x));
        const float norm_y = std::clamp(1.0f - (e.position.y / static_cast<float>(std::max(1, getHeight()))), 0.0f, 1.0f);
        const double val = std::clamp(norm_y * 90.0 - 80.0, -80.0, 10.0);

        nlohmann::json pts = read_points();
        if (std::any_of(pts.begin(), pts.end(), [t_ms](const auto& point) {
                return point.value("x", 0.0) == t_ms;
            })) return;
        pts.push_back({ { "x", t_ms }, { "y", val }, { "shape", "linear" } });
        std::sort(pts.begin(), pts.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            return a.value("x", 0.0) < b.value("x", 0.0);
        });

        model_.begin_gesture();
        model_.set_track_timeline_volume_points(event_path_, static_cast<int>(track_index_), pts, true);
        model_.end_gesture();
        repaint();
        return;
    }

    const auto pts = read_points();
    const int hit = hit_test_point(e.position, pts);
    if (hit >= 0) {
        dragged_point_index_ = hit;
        selected_point_index_ = hit;
        model_.begin_gesture();
        repaint();
    } else if (selected_point_index_ != -1) {
        selected_point_index_ = -1;
        repaint();
    }
}

void AutomationLaneComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragged_point_index_ < 0 || !px_to_time_) return;

    nlohmann::json pts = read_points();
    const size_t idx = static_cast<size_t>(dragged_point_index_);
    if (!pts.is_array() || idx >= pts.size()) return;

    double t_ms = std::max(0.0, px_to_time_(e.position.x));
    if (idx > 0) {
        t_ms = std::max(t_ms, pts[idx - 1].value("x", 0.0) + 1.0);
    }
    if (idx + 1 < pts.size()) {
        t_ms = std::min(t_ms, pts[idx + 1].value("x", 0.0) - 1.0);
    }

    const float norm_y = std::clamp(1.0f - (e.position.y / static_cast<float>(std::max(1, getHeight()))), 0.0f, 1.0f);
    const double val = std::clamp(norm_y * 90.0 - 80.0, -80.0, 10.0);
    pts[idx]["x"] = t_ms;
    pts[idx]["y"] = val;

    model_.set_track_timeline_volume_points(event_path_, static_cast<int>(track_index_), pts);
    repaint();
}

void AutomationLaneComponent::mouseUp(const juce::MouseEvent&)
{
    if (dragged_point_index_ >= 0) {
        dragged_point_index_ = -1;
        model_.end_gesture();
        repaint();
    }
}

void AutomationLaneComponent::mouseMove(const juce::MouseEvent& e)
{
    const auto pts = read_points();
    if (selected_point_index_ >= 0 &&
        (!pts.is_array() || static_cast<size_t>(selected_point_index_) >= pts.size())) {
        selected_point_index_ = -1;
    }
    const int hit = hit_test_point(e.position, pts);
    if (hit != hovered_point_index_) {
        hovered_point_index_ = hit;
        repaint();
    }
}

void AutomationLaneComponent::mouseExit(const juce::MouseEvent&)
{
    if (hovered_point_index_ != -1) {
        hovered_point_index_ = -1;
        repaint();
    }
}

void AutomationLaneComponent::focusLost(FocusChangeType)
{
    if (selected_point_index_ != -1) {
        selected_point_index_ = -1;
        repaint();
    }
}

int AutomationLaneComponent::point_index_at(juce::Point<float> pos) const
{
    return hit_test_point(pos, read_points());
}

int AutomationLaneComponent::resolve_selected_point() const
{
    const auto pts = read_points();
    if (!pts.is_array()) return -1;
    if (selected_point_index_ >= 0 && static_cast<size_t>(selected_point_index_) < pts.size()) {
        return selected_point_index_;
    }
    if (hovered_point_index_ >= 0 && static_cast<size_t>(hovered_point_index_) < pts.size()) {
        return hovered_point_index_;
    }
    return -1;
}

bool AutomationLaneComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        delete_point_at(resolve_selected_point());
        return true;
    }
    return Component::keyPressed(key);
}

bool AutomationLaneComponent::delete_point_at(int index)
{
    const auto pts = read_points();
    if (!pts.is_array() || index < 0 || static_cast<size_t>(index) >= pts.size()) {
        if (selected_point_index_ != -1) {
            selected_point_index_ = -1;
            repaint();
        }
        return false;
    }

    model_.begin_gesture();
    bool removed = false;
    if (pts.size() <= 1) {
        removed = model_.clear_track_timeline_volume_points(event_path_, static_cast<int>(track_index_));
    } else {
        nlohmann::json remaining = nlohmann::json::array();
        for (size_t i = 0; i < pts.size(); ++i) {
            if (static_cast<int>(i) == index) continue;
            remaining.push_back(pts[i]);
        }
        removed = model_.set_track_timeline_volume_points(event_path_, static_cast<int>(track_index_), remaining);
    }
    model_.end_gesture();

    dragged_point_index_ = -1;
    hovered_point_index_ = -1;
    selected_point_index_ = -1;
    repaint();
    return removed;
}

void AutomationLaneComponent::show_context_menu(juce::Point<int> screen_pos, int point_index)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Delete Point");

    juce::Component::SafePointer<AutomationLaneComponent> lane(this);
    UiTestDialogSeam::show_popup_menu("automation_context_menu", menu,
        juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screen_pos.x, screen_pos.y, 1, 1)),
        [lane, point_index](int result) {
            if (!lane || result != 1) return;
            lane->delete_point_at(point_index);
        });
}

std::vector<TestableTarget> AutomationLaneComponent::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};
    std::vector<TestableTarget> out;
    const auto canvasRect = bounds.reduced(2).getIntersection(bounds);
    if (!canvasRect.isEmpty()) {
        out.push_back({ "canvas", "double_click", canvasRect, isEnabled() });
    }
    const auto pts = read_points();
    if (pts.is_array()) {
        const auto fBounds = bounds.toFloat().reduced(0.0f, 6.0f);
        for (size_t i = 0; i < pts.size(); ++i) {
            const double xMs = pts[i].value("x", 0.0);
            const double yVal = pts[i].value("y", 0.0);
            const auto pt = point_to_screen(xMs, yVal, fBounds);
            const auto r = juce::Rectangle<int>(
                static_cast<int>(std::round(pt.x)) - 6,
                static_cast<int>(std::round(pt.y)) - 6,
                12, 12).getIntersection(bounds);
            if (!r.isEmpty()) {
                out.push_back({ "point_" + std::to_string(i), "drag", r, isEnabled() });
            }
        }
    }
    return out;
}

// ---------------- TimelineTrackLane ----------------

TimelineTrackLane::TimelineTrackLane(AuthoringModel& model,
                                     WaveformCache& waveform_cache,
                                     const std::string& event_path,
                                     size_t track_index)
    : model_(model), waveform_cache_(waveform_cache),
      event_path_(event_path), track_index_(track_index)
{
    addAndMakeVisible(automationLane_);
    automationLane_.setComponentID("track_lane_" + juce::String(track_index_) + ".automation");
    automationLane_.setVisible(false);
    update_clips();
}

void TimelineTrackLane::set_time_converter(std::function<double(double)> time_to_px,
                                          std::function<double(float)> px_to_time)
{
    time_to_px_ = time_to_px;
    px_to_time_ = px_to_time;
    automationLane_.set_time_converter(time_to_px, px_to_time);
    resized();
}

void TimelineTrackLane::set_automation_expanded(bool expanded)
{
    automation_expanded_ = expanded;
    automationLane_.setVisible(expanded);
    resized();
}

void TimelineTrackLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        model_.select({ ItemType::Event, event_path_, static_cast<int>(track_index_), -1 });
        const double t_ms = px_to_time_ ? std::max(0.0, px_to_time_(e.position.x)) : 0.0;

        juce::PopupMenu addInstMenu;
        addInstMenu.addItem(1, "Single");
        addInstMenu.addItem(2, "Multi");
        addInstMenu.addItem(3, "Scatterer");
        addInstMenu.addItem(4, "Programmer");

        const auto targets = model_.get_instrument_targets(event_path_);
        juce::PopupMenu eventMenu;
        juce::PopupMenu commandMenu;
        for (size_t i = 0; i < targets.size(); ++i) {
            eventMenu.addItem(static_cast<int>(6 + i), targets[i]);
            commandMenu.addItem(static_cast<int>(6 + targets.size() + i), targets[i]);
        }
        addInstMenu.addSubMenu("Event", eventMenu, !targets.empty());
        addInstMenu.addSubMenu("Command", commandMenu, !targets.empty());
        addInstMenu.addItem(5, "Silence");

        juce::PopupMenu rootMenu;
        rootMenu.addSubMenu("Add Instrument", addInstMenu);

        juce::Component::SafePointer<TimelineTrackLane> safeLane(this);
        const auto eventPath = event_path_;
        const size_t trackIndex = track_index_;
        UiTestDialogSeam::show_popup_menu("track_lane_context_menu", rootMenu,
            juce::PopupMenu::Options(),
            [safeLane, eventPath, trackIndex, targets, t_ms](int result) {
                if (!safeLane || result == 0) return;
                bool created = false;
                if (result == 1) created = safeLane->model_.add_instrument(eventPath, trackIndex, "single", "", t_ms);
                else if (result == 2) created = safeLane->model_.add_instrument(eventPath, trackIndex, "multi", "", t_ms);
                else if (result == 3) created = safeLane->model_.add_instrument(eventPath, trackIndex, "scatterer", "", t_ms);
                else if (result == 4) created = safeLane->model_.add_instrument(eventPath, trackIndex, "programmer", "", t_ms);
                else if (result == 5) created = safeLane->model_.add_instrument(eventPath, trackIndex, "silence", "", t_ms);
                else if (result >= 6) {
                    const auto index = static_cast<size_t>(result - 6);
                    if (index < targets.size()) {
                        created = safeLane->model_.add_instrument(eventPath, trackIndex, "event", targets[index], t_ms);
                    } else if (index - targets.size() < targets.size()) {
                        created = safeLane->model_.add_instrument(eventPath, trackIndex, "command", targets[index - targets.size()], t_ms);
                    }
                }
                if (created && safeLane->onTrackModified) {
                    safeLane->onTrackModified();
                }
            });
        return;
    }

    is_dragging_track_ = false;
    model_.select({ ItemType::Event, event_path_, static_cast<int>(track_index_), -1 });
}

void TimelineTrackLane::mouseDrag(const juce::MouseEvent& e)
{
    const int delta_y = e.getDistanceFromDragStartY();
    if (!is_dragging_track_ && std::abs(delta_y) > 4) {
        is_dragging_track_ = true;
    }
    if (is_dragging_track_ && onTrackLaneDragged) {
        onTrackLaneDragged(track_index_, delta_y);
    }
}

void TimelineTrackLane::mouseUp(const juce::MouseEvent& e)
{
    if (is_dragging_track_) {
        is_dragging_track_ = false;
        if (onTrackLaneDragEnded) {
            onTrackLaneDragEnded(track_index_, e.getDistanceFromDragStartY());
        }
    }
}

void TimelineTrackLane::set_track_index(size_t track_index)
{
    track_index_ = track_index;
    automationLane_.set_track_index(track_index);
    update_clips();
}

void TimelineTrackLane::set_event_context(const std::string& event_path, size_t track_index)
{
    event_path_ = event_path;
    track_index_ = track_index;
    automationLane_.set_event_path(event_path_);
    automationLane_.set_track_index(track_index_);
    automationLane_.setComponentID("track_lane_" + juce::String(track_index_) + ".automation");
    update_clips();
}

void TimelineTrackLane::update_clips()
{
    const size_t num_insts = [this]() -> size_t {
        const auto& root = model_.document().get_root_json();
        if (root.contains("events") && root["events"].is_array()) {
            for (const auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                    const auto& trks = ev["tracks"];
                    if (track_index_ < trks.size() && trks[track_index_].contains("instruments")) {
                        const auto& insts = trks[track_index_]["instruments"];
                        if (insts.is_array()) return insts.size();
                    }
                    break;
                }
            }
        }
        return 0;
    }();

    if (clips_.size() == num_insts) {
        for (size_t i = 0; i < clips_.size(); ++i) {
            if (clips_[i]) {
                clips_[i]->setComponentID("track_lane_" + juce::String(track_index_) + ".clip_" + juce::String(i));
                clips_[i]->set_event_path(event_path_);
                clips_[i]->set_indices(track_index_, i);
                clips_[i]->update_from_model();
            }
        }
        resized();
        return;
    }

    clips_.clear();

    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                const auto& trks = ev["tracks"];
                if (track_index_ < trks.size() && trks[track_index_].contains("instruments")) {
                    const auto& insts = trks[track_index_]["instruments"];
                    if (insts.is_array()) {
                        for (size_t i = 0; i < insts.size(); ++i) {
                            auto clip = std::make_unique<ClipComponent>(model_, waveform_cache_, event_path_, track_index_, i);
                            clip->setComponentID("track_lane_" + juce::String(track_index_) + ".clip_" + juce::String(i));
                            clip->onClipDragged = [this](ClipComponent* c, int dx, int dy, bool shift_down) {
                                if (onClipDragged) onClipDragged(c, dx, dy, shift_down);
                            };
                            clip->onClipDragEnded = [this](ClipComponent* c) {
                                if (onClipDragEnded) onClipDragEnded(c);
                            };
                            clip->onClipModified = [this]() {
                                update_clips();
                                if (onTrackModified) onTrackModified();
                            };
                            addAndMakeVisible(*clip);
                            clips_.push_back(std::move(clip));
                        }
                    }
                }
                break;
            }
        }
    }
    resized();
}

void TimelineTrackLane::refresh_selection()
{
    for (auto& clip : clips_) {
        if (clip) {
            clip->update_from_model();
        }
    }
    repaint();
}

void TimelineTrackLane::set_drop_target(double start_ms, double length_ms)
{
    GhostPreviewState preview;
    preview.active = true;
    preview.raw_time_ms = start_ms;
    preview.snapped_time_ms = start_ms;
    preview.delta_ms = 0.0;
    preview.length_ms = length_ms;
    preview.colour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);
    set_drop_target(preview);
}

void TimelineTrackLane::set_drop_target(const GhostPreviewState& preview)
{
    ghost_preview_ = preview;
    ghost_preview_.active = true;
    repaint();
}

void TimelineTrackLane::clear_drop_target()
{
    if (ghost_preview_.active) {
        ghost_preview_.active = false;
        ghost_preview_.envelope.reset();
        repaint();
    }
}

void TimelineTrackLane::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark zebra lane background
    g.fillAll((track_index_ % 2 == 0) ? juce::Colour(OaeTheme::Colors::bgDark)
                                      : juce::Colour(OaeTheme::Colors::bgSurface));

    const auto* ev = find_event_json(model_.document().get_root_json(), event_path_);
    const size_t num_tracks = (ev && ev->contains("tracks") && (*ev)["tracks"].is_array())
        ? (*ev)["tracks"].size()
        : 0;
    const auto& sel = model_.get_selection();
    const bool is_selected_track = (num_tracks >= 2)
        && (sel.type == ItemType::Event && sel.id_or_path == event_path_
            && sel.track_index == static_cast<int>(track_index_));

    const juce::Colour trackColour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);

    if (is_selected_track) {
        g.setColour(trackColour.withAlpha(OaeTheme::States::selectedTrackTintOpacity));
        g.fillRect(bounds);
    }

    // Subtle track color strip at the left lane boundary
    g.setColour(trackColour.withAlpha(is_selected_track ? 0.95f : 0.45f));
    g.fillRect(0.0f, 0.0f, 3.0f, bounds.getHeight());

    if (is_selected_track) {
        g.setColour(trackColour.brighter(0.25f));
        g.fillRect(0.0f, 0.0f, bounds.getWidth(), 1.5f);
        g.fillRect(0.0f, bounds.getHeight() - 1.5f, bounds.getWidth(), 1.5f);
    } else {
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());
    }
}

void TimelineTrackLane::paintOverChildren(juce::Graphics& g)
{
    if (!ghost_preview_.active || !time_to_px_) return;

    const auto bounds = getLocalBounds().toFloat();
    const float snappedX = static_cast<float>(time_to_px_(ghost_preview_.snapped_time_ms));
    const float endX = static_cast<float>(time_to_px_(ghost_preview_.snapped_time_ms + ghost_preview_.length_ms));
    const float rawX = static_cast<float>(time_to_px_(ghost_preview_.raw_time_ms));
    const float w = std::max(8.0f, endX - snappedX);
    const auto ghostRect = juce::Rectangle<float>(snappedX, 6.0f, w, 68.0f);

    // Continuous vertical snap alignment guides
    if (std::abs(rawX - snappedX) > 1.5f) {
        g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(std::round(rawX)), 0.0f, bounds.getHeight());
    }
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(0.85f));
    g.drawVerticalLine(static_cast<int>(std::round(snappedX)), 0.0f, bounds.getHeight());

    // Translucent floating ghost clip container
    const juce::Colour ghostFill = ghost_preview_.colour.withAlpha(OaeTheme::States::ghostClipOpacity);
    g.setColour(ghostFill);
    g.fillRoundedRectangle(ghostRect, 4.0f);

    if (ghost_preview_.envelope && !ghost_preview_.envelope->is_empty()) {
        auto waveArea = ghostRect.withTrimmedTop(18.0f).reduced(2.0f, 2.0f);
        if (waveArea.getHeight() >= 4.0f && waveArea.getWidth() >= 4.0f) {
            ghost_preview_.envelope->render_clip(
                g, waveArea,
                ghost_preview_.snapped_time_ms, ghost_preview_.length_ms,
                ghost_preview_.start_offset_ms, ghost_preview_.pitch_semitones,
                ghost_preview_.loop, ghost_preview_.fade_in_ms, ghost_preview_.fade_out_ms,
                juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.70f),
                ghost_preview_.colour.brighter(0.35f).withAlpha(0.55f));
        }
    }

    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(0.95f));
    g.drawRoundedRectangle(ghostRect, 4.0f, 1.5f);

    if (ghost_preview_.title.isNotEmpty()) {
        g.setColour(juce::Colour(OaeTheme::Colors::indicator).withAlpha(0.95f));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(ghost_preview_.title,
                   ghostRect.withHeight(18.0f).reduced(5.0f, 1.0f),
                   juce::Justification::centredLeft, true);
    }

    // Numeric ms offset & delta pill badge above children
    const int snappedMs = static_cast<int>(std::round(ghost_preview_.snapped_time_ms));
    const int deltaMs = static_cast<int>(std::round(ghost_preview_.delta_ms));
    const juce::String pillText = juce::String(snappedMs) + " ms ("
        + (deltaMs >= 0 ? "+" : "") + juce::String(deltaMs) + " ms)";
    const float pillW = 112.0f;
    const float pillH = 18.0f;
    const float pillX = std::clamp(snappedX + 4.0f, 4.0f, std::max(4.0f, bounds.getWidth() - pillW - 4.0f));
    const float pillY = 4.0f;
    const auto pillRect = juce::Rectangle<float>(pillX, pillY, pillW, pillH);

    g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(0.92f));
    g.fillRoundedRectangle(pillRect, 3.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
    g.drawRoundedRectangle(pillRect, 3.0f, 1.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(pillText, pillRect.reduced(4.0f, 0.0f), juce::Justification::centred, false);
}

void TimelineTrackLane::resized()
{
    const int main_lane_height = 80;
    const int clip_margin_y = 6;
    const int usable_height = main_lane_height - (clip_margin_y * 2);

    if (!clips_.empty()) {
        std::vector<size_t> order(clips_.size());
        for (size_t i = 0; i < clips_.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [this](size_t a, size_t b) {
            const double oa = clips_[a]->get_offset_ms();
            const double ob = clips_[b]->get_offset_ms();
            return (oa != ob) ? (oa < ob) : (a < b);
        });

        std::vector<double> lane_ends;
        std::vector<int> clip_lanes(clips_.size(), 0);
        for (const size_t idx : order) {
            const double start_ms = clips_[idx]->get_offset_ms();
            const double end_ms = start_ms + std::max(clips_[idx]->get_length_ms(), 50.0);
            int assigned = -1;
            for (size_t li = 0; li < lane_ends.size(); ++li) {
                if (lane_ends[li] <= start_ms + 1e-6) {
                    assigned = static_cast<int>(li);
                    lane_ends[li] = end_ms;
                    break;
                }
            }
            if (assigned < 0) {
                assigned = static_cast<int>(lane_ends.size());
                lane_ends.push_back(end_ms);
            }
            clip_lanes[idx] = assigned;
        }

        const int total_lanes = std::max(1, static_cast<int>(lane_ends.size()));
        const int gap = (total_lanes > 1) ? 2 : 0;
        const int lane_h = std::max(14, (usable_height - gap * (total_lanes - 1)) / total_lanes);

        for (size_t i = 0; i < clips_.size(); ++i) {
            auto& clip = clips_[i];
            if (time_to_px_) {
                const int start_x = static_cast<int>(time_to_px_(clip->get_offset_ms()));
                const int end_x = static_cast<int>(time_to_px_(clip->get_offset_ms() + clip->get_length_ms()));
                const int clip_w = std::max(6, end_x - start_x);
                const int lane = clip_lanes[i];
                const int y = clip_margin_y + lane * (lane_h + gap);
                const int h = (lane == total_lanes - 1)
                    ? std::max(14, clip_margin_y + usable_height - y)
                    : lane_h;
                clip->setBounds(start_x, y, clip_w, h);
            }
        }
    }

    if (automation_expanded_) {
        automationLane_.setBounds(0, main_lane_height, getWidth(), 60);
    }
}

bool TimelineTrackLane::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files) {
        const juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            return true;
        }
    }
    return false;
}

void TimelineTrackLane::fileDragEnter(const juce::StringArray& files, int x, int /*y*/)
{
    double drop_time_ms = px_to_time_ ? px_to_time_(static_cast<float>(x)) : 0.0;
    if (drop_time_ms < 0.0) drop_time_ms = 0.0;

    ghost_preview_.active = true;
    ghost_preview_.snapped_time_ms = drop_time_ms;
    ghost_preview_.length_ms = 1000.0;
    ghost_preview_.colour = juce::Colour(OaeTheme::Colors::accentCyan);
    ghost_preview_.title = files.size() > 0 ? juce::File(files[0]).getFileNameWithoutExtension() : "Audio";
    repaint();
}

void TimelineTrackLane::fileDragMove(const juce::StringArray& /*files*/, int x, int /*y*/)
{
    double drop_time_ms = px_to_time_ ? px_to_time_(static_cast<float>(x)) : 0.0;
    if (drop_time_ms < 0.0) drop_time_ms = 0.0;

    ghost_preview_.snapped_time_ms = drop_time_ms;
    repaint();
}

void TimelineTrackLane::fileDragExit(const juce::StringArray& /*files*/)
{
    ghost_preview_.active = false;
    repaint();
}

void TimelineTrackLane::filesDropped(const juce::StringArray& files, int x, int /*y*/)
{
    ghost_preview_.active = false;
    repaint();

    double drop_time_ms = px_to_time_ ? px_to_time_(static_cast<float>(x)) : 0.0;
    if (drop_time_ms < 0.0) drop_time_ms = 0.0;

    model_.begin_gesture();
    double current_cursor = drop_time_ms;
    for (const auto& file : files) {
        const juce::File jFile(file);
        const juce::String ext = jFile.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            std::string sample_id;
            double duration_ms = 0.0;
            if (model_.add_single_instrument_from_audio_file(event_path_, track_index_,
                                                             fs::path(jFile.getFullPathName().toStdString()),
                                                             current_cursor,
                                                             &sample_id,
                                                             &duration_ms)) {
                current_cursor += (duration_ms > 0.0 ? duration_ms : 1000.0);
            }
        }
    }
    model_.end_gesture();

    update_clips();
    if (onTrackModified) onTrackModified();
}

} // namespace oae::studio
