#include "TimelinePanel.h"
#include "Theme.h"

namespace oae::studio {

TimelinePanel::TimelinePanel(AuthoringModel& model, AuditionTransport* transport)
    : model_(model)
{
    setComponentID("timeline_view");
    minimap_.setComponentID("minimap_view");

    model_.add_listener(this);

    addChildComponent(parameterStrip_);
    parameterStrip_.set_transport(transport);
    parameterStrip_.onHeightChanged = [this] { resized(); };

    addChildComponent(layerStackView_);
    layerStackView_.set_transport(transport);
    layerStackView_.onSwitchToTimelineRequested = [this]() { set_view_mode(StageViewMode::Timeline); };

    addAndMakeVisible(viewModeSwitcher_);
    viewModeSwitcher_.setComponentID("timeline.mode.switcher");
    viewModeSwitcher_.addOption("Timeline", "timeline.mode.timeline");
    viewModeSwitcher_.addOption("Layers", "timeline.mode.layers");
    viewModeSwitcher_.onSelectionChanged = [this](int index) {
        set_view_mode(index == 1 ? StageViewMode::Layers : StageViewMode::Timeline);
    };

    addChildComponent(addStemLayerButton_);
    addStemLayerButton_.setComponentID("timeline.layers.add_stem");
    addStemLayerButton_.setTooltip("Adicionar nova camada de stem em loop");
    addStemLayerButton_.onClick = [this]() {
        if (!event_path_.empty()) {
            model_.add_stem_layer(event_path_);
        }
    };

    // Toolbar
    addAndMakeVisible(snapLabel);
    snapLabel.setFont(juce::FontOptions(11.0f));
    snapLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    addAndMakeVisible(snapComboBox);
    snapComboBox.setComponentID("timeline.snap_combo");
    for (int i = 0; i < 12; ++i) {
        snapComboBox.addItem(TimelineSnap::get_mode_name(TimelineSnap::mode_from_index(i)), i + 1);
    }
    snapComboBox.setSelectedId(8, juce::dontSendNotification); // Default: 1 Beat
    snapComboBox.onChange = [this]() {
        snap_.set_mode(TimelineSnap::mode_from_index(snapComboBox.getSelectedId() - 1));
    };

    addAndMakeVisible(timeFormatButton);
    timeFormatButton.setComponentID("timeline.time_format");
    timeFormatButton.onClick = [this]() {
        const bool next = !ruler_.is_musical_mode();
        ruler_.set_musical_mode(next);
        timeFormatButton.setButtonText(next ? "Bars / Beats" : "Time (Sec)");
    };

    addAndMakeVisible(zoomOutButton);
    zoomOutButton.setComponentID("timeline.zoom_out");
    zoomOutButton.onClick = [this]() {
        zoomSlider.setValue(zoomSlider.getValue() * 0.8);
    };

    addAndMakeVisible(zoomSlider);
    zoomSlider.setComponentID("timeline.zoom_slider");
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    zoomSlider.setRange(10.0, 2000.0, 1.0);
    zoomSlider.setValue(100.0, juce::dontSendNotification);
    zoomSlider.onValueChange = [this]() {
        set_zoom(zoomSlider.getValue());
    };

    addAndMakeVisible(zoomInButton);
    zoomInButton.setComponentID("timeline.zoom_in");
    zoomInButton.onClick = [this]() {
        zoomSlider.setValue(zoomSlider.getValue() * 1.25);
    };

    addAndMakeVisible(playheadLabel);
    playheadLabel.setFont(juce::FontOptions("Consolas", 12.0f, juce::Font::bold));
    playheadLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    playheadLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(breadcrumbBackButton_);
    breadcrumbBackButton_.setComponentID("timeline.breadcrumb_back");
    breadcrumbBackButton_.setTooltip("Back to parent event");
    breadcrumbBackButton_.setEnabled(false);
    breadcrumbBackButton_.onClick = [this] { navigate_to_parent_event(); };

    // Layout
    addAndMakeVisible(minimap_);
    addAndMakeVisible(trackHeaders_);
    trackHeaders_.setComponentID("timeline.track_headers");
    addAndMakeVisible(ruler_);
    ruler_.setComponentID("timeline.ruler");
    addAndMakeVisible(trackLanesContainer_);
    addAndMakeVisible(horizontalScrollBar);
    horizontalScrollBar.setComponentID("timeline.h_scroll");
    addAndMakeVisible(verticalScrollBar);
    verticalScrollBar.setComponentID("timeline.v_scroll");

    minimap_.onViewportChanged = [this](double new_start_ms, double new_len_ms) {
        const double visible_w = trackLanesContainer_.getWidth();
        if (visible_w > 0.0 && new_len_ms > 0.0) {
            const double new_zoom = (visible_w / (new_len_ms / 1000.0));
            set_zoom(new_zoom);
            set_horizontal_offset(static_cast<int>((new_start_ms / 1000.0) * new_zoom));
        }
    };

    ruler_.set_snap(&snap_);
    ruler_.onPlayheadChanged = [this](double t_ms) {
        set_playhead_time_ms(t_ms);
        if (onPlayheadScrubbed) onPlayheadScrubbed(playhead_time_ms_);
    };

    trackHeaders_.onTracksChanged = [this]() {
        rebuild_track_lanes();
    };

    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.setRangeLimits(0.0, 60000.0); // 60s default domain
    horizontalScrollBar.setCurrentRange(0.0, 10000.0);
    horizontalScrollBar.addListener(this);

    verticalScrollBar.setAutoHide(false);
    verticalScrollBar.setRangeLimits(0.0, 2000.0);
    verticalScrollBar.setCurrentRange(0.0, 500.0);
    verticalScrollBar.addListener(this);

    waveform_cache_.addChangeListener(this);

    set_view_mode(StageViewMode::Timeline);

    // If project already has events, select first event
    const auto paths = model_.get_event_paths();
    if (!paths.empty()) {
        set_event_path(paths[0]);
    }
}

TimelinePanel::~TimelinePanel()
{
    waveform_cache_.removeChangeListener(this);
    horizontalScrollBar.removeListener(this);
    verticalScrollBar.removeListener(this);
    model_.remove_listener(this);
}

void TimelinePanel::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &horizontalScrollBar) {
        const double current_sec = newRangeStart / 1000.0;
        set_horizontal_offset(static_cast<int>(current_sec * pixels_per_second_));
    } else if (scrollBarThatHasMoved == &verticalScrollBar) {
        vertical_scroll_y_ = static_cast<int>(newRangeStart);
        trackHeaders_.set_vertical_scroll(vertical_scroll_y_);
        resized();
    }
}

void TimelinePanel::set_event_path(const std::string& path)
{
    if (path.empty()) {
        const bool changed = !event_path_.empty() || !breadcrumb_trail_.empty();
        event_path_.clear();
        breadcrumb_trail_.clear();
        if (changed) {
            minimap_.set_event_path("");
            parameterStrip_.set_event_path("");
            layerStackView_.set_event_path("");
            trackHeaders_.set_event_path("");
            ruler_.set_event_path("");
            rebuild_track_lanes();
            rebuild_breadcrumb_buttons();
        }
        return;
    }

    const auto existing_it = std::find(breadcrumb_trail_.begin(), breadcrumb_trail_.end(), path);
    if (existing_it != breadcrumb_trail_.end()) {
        breadcrumb_trail_.erase(existing_it + 1, breadcrumb_trail_.end());
    } else if (!breadcrumb_trail_.empty() && is_direct_child_event(breadcrumb_trail_.back(), path)) {
        breadcrumb_trail_.push_back(path);
    } else {
        breadcrumb_trail_.assign(1, path);
    }

    if (event_path_ != path) {
        event_path_ = path;
        minimap_.set_event_path(path);
        parameterStrip_.set_event_path(path);
        layerStackView_.set_event_path(path);
        trackHeaders_.set_event_path(path);
        ruler_.set_event_path(path);
        rebuild_track_lanes();
    }
    rebuild_breadcrumb_buttons();
}

void TimelinePanel::navigate_breadcrumb_to(size_t index)
{
    if (index >= breadcrumb_trail_.size()) {
        return;
    }

    const std::string target_path = breadcrumb_trail_[index];
    breadcrumb_trail_.resize(index + 1);
    if (event_path_ != target_path) {
        event_path_ = target_path;
        minimap_.set_event_path(target_path);
        parameterStrip_.set_event_path(target_path);
        layerStackView_.set_event_path(target_path);
        trackHeaders_.set_event_path(target_path);
        ruler_.set_event_path(target_path);
        rebuild_track_lanes();
    }
    rebuild_breadcrumb_buttons();
    model_.select({ ItemType::Event, target_path, -1, -1 });
}

void TimelinePanel::navigate_to_parent_event()
{
    if (breadcrumb_trail_.size() >= 2) {
        navigate_breadcrumb_to(breadcrumb_trail_.size() - 2);
    }
}

void TimelinePanel::rebuild_breadcrumb_buttons()
{
    for (auto& btn : breadcrumbButtons_) {
        removeChildComponent(btn.get());
    }
    breadcrumbButtons_.clear();

    for (size_t i = 0; i < breadcrumb_trail_.size(); ++i) {
        const auto& path = breadcrumb_trail_[i];
        const auto slash = path.find_last_of('/');
        const std::string leaf = (slash != std::string::npos && slash + 1 < path.size())
                                     ? path.substr(slash + 1)
                                     : path;
        auto btn = std::make_unique<juce::TextButton>(i == 0 ? leaf : ("> " + leaf));
        btn->setComponentID("timeline.breadcrumb." + std::to_string(i));
        btn->setTooltip(path);
        btn->setClickingTogglesState(false);
        btn->setToggleState(i + 1 == breadcrumb_trail_.size(), juce::dontSendNotification);
        btn->onClick = [this, i] { navigate_breadcrumb_to(i); };
        addAndMakeVisible(*btn);
        breadcrumbButtons_.push_back(std::move(btn));
    }

    breadcrumbBackButton_.setEnabled(breadcrumb_trail_.size() > 1);
    resized();
    repaint();
}

bool TimelinePanel::is_direct_child_event(const std::string& parent_path, const std::string& child_path) const
{
    if (parent_path.empty() || child_path.empty() || parent_path == child_path) {
        return false;
    }
    const auto& root = model_.json();
    const auto events_it = root.find("events");
    if (events_it == root.end() || !events_it->is_array()) {
        return false;
    }
    for (const auto& ev : *events_it) {
        if (ev.value("path", "") != parent_path) {
            continue;
        }
        const auto tracks_it = ev.find("tracks");
        if (tracks_it == ev.end() || !tracks_it->is_array()) {
            return false;
        }
        for (const auto& track : *tracks_it) {
            const auto insts_it = track.find("instruments");
            if (insts_it == track.end() || !insts_it->is_array()) {
                continue;
            }
            for (const auto& inst : *insts_it) {
                const auto kind = inst.value("kind", "single");
                if (kind == "event" && inst.value("target_event", "") == child_path) {
                    return true;
                }
                if (kind == "command" && inst.value("command_type", "") == "start_event"
                    && inst.value("command_target", "") == child_path) {
                    return true;
                }
                if (kind == "multi" || kind == "scatterer") {
                    const auto playlist_it = inst.find("playlist");
                    if (playlist_it != inst.end() && playlist_it->is_array()) {
                        for (const auto& entry : *playlist_it) {
                            if (entry.value("target_event", "") == child_path) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }
    return false;
}

void TimelinePanel::set_zoom(double pixels_per_second)
{
    pixels_per_second_ = std::clamp(pixels_per_second, 10.0, 2000.0);
    ruler_.set_zoom(pixels_per_second_);
    update_time_converters();
    resized();
}

void TimelinePanel::set_horizontal_offset(int offset_px)
{
    horizontal_offset_px_ = std::max(0, offset_px);
    ruler_.set_horizontal_offset(horizontal_offset_px_);
    update_time_converters();
    resized();
}

void TimelinePanel::set_playhead_time_ms(double time_ms)
{
    playhead_time_ms_ = std::max(0.0, time_ms);
    const int seconds = static_cast<int>(playhead_time_ms_ / 1000.0);
    const int milliseconds = static_cast<int>(std::fmod(playhead_time_ms_, 1000.0));
    playheadLabel.setText(juce::String::formatted("%02d:%02d.%03d", seconds / 60, seconds % 60, milliseconds),
                          juce::dontSendNotification);
    ruler_.set_playhead_time_ms(playhead_time_ms_);
    minimap_.set_playhead_time_ms(playhead_time_ms_);
    trackLanesContainer_.repaint();
}

void TimelinePanel::update_time_converters()
{
    auto time_to_px = [this](double t_ms) -> double {
        return (t_ms / 1000.0) * pixels_per_second_ - horizontal_offset_px_;
    };
    auto px_to_time = [this](float px) -> double {
        return ((px + horizontal_offset_px_) / pixels_per_second_) * 1000.0;
    };

    for (auto& lane : lanes_) {
        lane->set_time_converter(time_to_px, px_to_time);
    }
}

void TimelinePanel::rebuild_track_lanes()
{
    const size_t num_tracks = [this]() -> size_t {
        const auto& root = model_.document().get_root_json();
        if (root.contains("events") && root["events"].is_array()) {
            for (const auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                    return ev["tracks"].size();
                }
            }
        }
        return 0;
    }();

    if (lanes_.size() == num_tracks) {
        for (size_t i = 0; i < lanes_.size(); ++i) {
            if (lanes_[i]) {
                lanes_[i]->set_event_context(event_path_, i);
                lanes_[i]->repaint();
            }
        }
        update_time_converters();
        resized();
        return;
    }

    lanes_.clear();

    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                for (size_t i = 0; i < num_tracks; ++i) {
                    auto lane = std::make_unique<TimelineTrackLane>(model_, waveform_cache_, event_path_, i);
                    lane->setComponentID("track_lane_" + std::to_string(i));
                    lane->onClipDragged = [this](ClipComponent* c, int dx, int dy, bool shift_down) {
                        handle_clip_drag(c, dx, dy, shift_down);
                    };
                    lane->onClipDragEnded = [this](ClipComponent* c) {
                        handle_clip_drag_ended(c);
                    };
                    lane->onTrackModified = [this]() {
                        rebuild_track_lanes();
                    };
                    lane->onTrackLaneDragged = [this](size_t src_track, int dy) {
                        trackHeaders_.handle_track_drag(src_track, dy);
                    };
                    lane->onTrackLaneDragEnded = [this](size_t src_track, int dy) {
                        trackHeaders_.handle_track_drag_ended(src_track, dy);
                    };
                    trackLanesContainer_.addAndMakeVisible(*lane);
                    lanes_.push_back(std::move(lane));
                }
                break;
            }
        }
    }

    update_time_converters();
    resized();
}

void TimelinePanel::handle_clip_drag(ClipComponent* clip, int delta_x, int delta_y, bool shift_down)
{
    if (!clip || lanes_.empty()) return;
    is_dragging_clip_ = true;
    dragging_clip_ = clip;
    clip->set_being_dragged(true);

    const double sensitivity = shift_down ? 0.1 : 1.0;
    const double delta_ms = ((static_cast<double>(delta_x) * sensitivity) / std::max(1.0, pixels_per_second_)) * 1000.0;
    const double raw_time_ms = std::max(0.0, clip->get_drag_start_offset_ms() + delta_ms);

    const auto& root = model_.document().get_root_json();
    const nlohmann::json* logic_items = nullptr;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("logic_items")) {
                logic_items = &ev["logic_items"];
                break;
            }
        }
    }
    const double target_time_ms = shift_down ? raw_time_ms : snap_.snap(raw_time_ms, logic_items);

    const size_t current_track = clip->get_track_index();
    const int lane_h = 80;
    const int track_delta = (delta_y + (delta_y > 0 ? lane_h / 2 : -lane_h / 2)) / lane_h;
    const int dest_track = std::clamp(static_cast<int>(current_track) + track_delta, 0, static_cast<int>(lanes_.size()) - 1);

    drag_target_track_ = static_cast<size_t>(dest_track);
    drag_target_offset_ms_ = target_time_ms;

    TimelineTrackLane::GhostPreviewState preview;
    preview.active = true;
    preview.raw_time_ms = raw_time_ms;
    preview.snapped_time_ms = drag_target_offset_ms_;
    preview.delta_ms = drag_target_offset_ms_ - clip->get_drag_start_offset_ms();
    preview.length_ms = clip->get_length_ms();
    preview.start_offset_ms = clip->get_start_offset_ms();
    preview.pitch_semitones = clip->get_pitch_semitones();
    preview.loop = clip->is_loop();
    preview.fade_in_ms = clip->get_fade_in_ms();
    preview.fade_out_ms = clip->get_fade_out_ms();
    preview.colour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, drag_target_track_), drag_target_track_);
    preview.title = clip->get_display_title();
    preview.envelope = clip->resolve_waveform_envelope(false);

    for (size_t i = 0; i < lanes_.size(); ++i) {
        if (i == drag_target_track_) {
            lanes_[i]->set_drop_target(preview);
        } else {
            lanes_[i]->clear_drop_target();
        }
    }
    ruler_.set_ghost_guide_ms(raw_time_ms, drag_target_offset_ms_);
}

void TimelinePanel::handle_clip_drag_ended(ClipComponent* clip)
{
    if (clip) {
        clip->set_being_dragged(false);
    }
    ruler_.clear_ghost_guide();

    for (auto& lane : lanes_) {
        lane->clear_drop_target();
    }

    if (!clip || !is_dragging_clip_) {
        is_dragging_clip_ = false;
        dragging_clip_ = nullptr;
        model_.end_gesture();
        return;
    }

    const std::string path = event_path_;
    const size_t current_track = clip->get_track_index();
    const size_t inst_idx = clip->get_instrument_index();
    const size_t target_track = drag_target_track_;
    const double target_offset = drag_target_offset_ms_;
    const double start_offset = clip->get_drag_start_offset_ms();
    const double length = clip->get_length_ms();

    is_dragging_clip_ = false;
    dragging_clip_ = nullptr;

    juce::MessageManager::callAsync([safe_this = juce::Component::SafePointer<TimelinePanel>(this),
                                     path, current_track, inst_idx, target_track, target_offset, start_offset, length]() {
        if (!safe_this) return;
        if (target_track != current_track) {
            safe_this->model_.move_instrument(path, current_track, inst_idx, target_track, target_offset);
        } else if (std::abs(target_offset - start_offset) > 0.001) {
            safe_this->model_.resize_instrument(path, current_track, inst_idx, target_offset, length, -1.0);
        }
        safe_this->model_.end_gesture();
        safe_this->rebuild_track_lanes();
    });
}

void TimelinePanel::on_project_changed()
{
    if (is_dragging_clip_ || trackHeaders_.is_dragging_track()) return;

    const auto paths = model_.get_event_paths();
    breadcrumb_trail_.erase(
        std::remove_if(breadcrumb_trail_.begin(), breadcrumb_trail_.end(),
                       [&paths](const std::string& p) {
                           return std::find(paths.begin(), paths.end(), p) == paths.end();
                       }),
        breadcrumb_trail_.end());

    if (!paths.empty()) {
        const auto& cur_sel = model_.get_selection();
        if (cur_sel.type == ItemType::Event && !cur_sel.id_or_path.empty()) {
            set_event_path(cur_sel.id_or_path);
        } else if (event_path_.empty() || std::find(paths.begin(), paths.end(), event_path_) == paths.end()) {
            set_event_path(paths[0]);
        } else {
            rebuild_breadcrumb_buttons();
        }
    } else {
        set_event_path("");
    }
    trackHeaders_.refresh_tracks();
    rebuild_track_lanes();
    ruler_.repaint();
}

void TimelinePanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveform_cache_) {
        for (auto& lane : lanes_) {
            for (const auto& clip : lane->get_clips()) {
                clip->repaint();
            }
            lane->repaint();
        }
        trackLanesContainer_.repaint();
    }
}

void TimelinePanel::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && !selection.id_or_path.empty() && selection.id_or_path != event_path_) {
        set_event_path(selection.id_or_path);
        return;
    }

    trackHeaders_.repaint_selection();
    for (auto& lane : lanes_) {
        if (lane) {
            lane->refresh_selection();
        }
    }
}

void TimelinePanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (juce::ModifierKeys::getCurrentModifiers().isCommandDown()) {
        const double factor = wheel.deltaY > 0.0f ? 1.25 : 0.8;
        zoomSlider.setValue(zoomSlider.getValue() * factor);
    } else if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
        set_horizontal_offset(horizontal_offset_px_ - static_cast<int>(wheel.deltaY * 100.0f));
    } else {
        vertical_scroll_y_ = std::max(0, vertical_scroll_y_ - static_cast<int>(wheel.deltaY * 50.0f));
        trackHeaders_.set_vertical_scroll(vertical_scroll_y_);
        resized();
    }
}

bool TimelinePanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        const auto& sel = model_.get_selection();
        if (sel.type == ItemType::Event && sel.track_index >= 0 && sel.instrument_index >= 0) {
            model_.begin_gesture();
            model_.remove_instrument(sel.id_or_path, static_cast<size_t>(sel.track_index), static_cast<size_t>(sel.instrument_index));
            model_.clear_selection();
            model_.end_gesture();
            rebuild_track_lanes();
            return true;
        }
        if (sel.type == ItemType::Event && sel.logic_index >= 0) {
            if (model_.remove_logic_item(sel.id_or_path, static_cast<size_t>(sel.logic_index))) {
                model_.select({ ItemType::Event, sel.id_or_path, -1, -1, -1 });
                ruler_.repaint();
                return true;
            }
        }
    }
    return Component::keyPressed(key);
}

void TimelinePanel::set_view_mode(StageViewMode mode)
{
    stage_view_mode_ = mode;
    const bool layersMode = (mode == StageViewMode::Layers);

    viewModeSwitcher_.setSelectedIndex(layersMode ? 1 : 0, juce::dontSendNotification);

    addStemLayerButton_.setVisible(layersMode);
    snapLabel.setVisible(!layersMode);
    snapComboBox.setVisible(!layersMode);
    timeFormatButton.setVisible(!layersMode);
    zoomOutButton.setVisible(!layersMode);
    zoomSlider.setVisible(!layersMode);
    zoomInButton.setVisible(!layersMode);
    playheadLabel.setVisible(!layersMode);

    if (layersMode) {
        layerStackView_.refresh();
    }

    resized();
    repaint();
    if (onViewModeChanged) {
        onViewModeChanged(stage_view_mode_);
    }
}

void TimelinePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRect(0, 0, getWidth(), 38);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(37, 0.0f, static_cast<float>(getWidth()));

    if (stage_view_mode_ == StageViewMode::Timeline) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
        g.fillRect(0, 38, getWidth(), 28);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawHorizontalLine(65, 0.0f, static_cast<float>(getWidth()));
    }
}

void TimelinePanel::resized()
{
    auto area = getLocalBounds();
    const bool layersMode = (stage_view_mode_ == StageViewMode::Layers);

    // StageModeHeader (38 px)
    auto headerArea = area.removeFromTop(38);
    const int switcherW = std::min(196, std::max(120, headerArea.getWidth() / 3));
    const auto switcherBounds = juce::Rectangle<int>(
        headerArea.getCentreX() - switcherW / 2,
        headerArea.getY() + 3,
        switcherW,
        32);
    viewModeSwitcher_.setBounds(switcherBounds);

    if (layersMode) {
        const int rightAvail = std::max(0, headerArea.getRight() - 12 - (switcherBounds.getRight() + 8));
        const int btnW = std::min(112, rightAvail);
        addStemLayerButton_.setBounds(headerArea.getRight() - 12 - btnW, headerArea.getY() + 7, btnW, 24);
    }

    auto leftArea = juce::Rectangle<int>(
        headerArea.getX() + 12,
        headerArea.getY() + 7,
        std::max(0, switcherBounds.getX() - 8 - (headerArea.getX() + 12)),
        24);
    breadcrumbBackButton_.setBounds(leftArea.removeFromLeft(std::min(30, leftArea.getWidth())));
    leftArea.removeFromLeft(4);

    const juce::Font crumbFont{ juce::FontOptions(OaeTheme::Typography::compact) };
    juce::GlyphArrangement glyphs;
    for (const auto& btn : breadcrumbButtons_) {
        if (leftArea.getWidth() < 24) {
            btn->setBounds({});
            continue;
        }
        glyphs.clear();
        glyphs.addLineOfText(crumbFont, btn->getButtonText(), 0.0f, 0.0f);
        const int preferredW = static_cast<int>(std::clamp(glyphs.getBoundingBox(0, -1, false).getWidth() + 16.0f, 44.0f, 180.0f));
        const int w = std::min(preferredW, leftArea.getWidth());
        btn->setBounds(leftArea.removeFromLeft(w));
        leftArea.removeFromLeft(4);
    }

    // Sub-barra exclusiva do modo Timeline (28 px)
    if (!layersMode) {
        auto subBar = area.removeFromTop(28).reduced(12, 3);
        juce::FlexBox subBox;
        subBox.flexDirection = juce::FlexBox::Direction::row;
        subBox.alignItems = juce::FlexBox::AlignItems::center;
        subBox.items.add(juce::FlexItem(snapLabel).withWidth(36.0f).withHeight(22.0f));
        subBox.items.add(juce::FlexItem(snapComboBox).withWidth(112.0f).withHeight(22.0f).withMargin({ 0, 8, 0, 2 }));
        subBox.items.add(juce::FlexItem(timeFormatButton).withWidth(84.0f).withHeight(22.0f).withMargin({ 0, 8, 0, 0 }));
        subBox.items.add(juce::FlexItem(zoomOutButton).withWidth(22.0f).withHeight(22.0f).withMargin({ 0, 2, 0, 0 }));
        subBox.items.add(juce::FlexItem(zoomSlider).withWidth(84.0f).withHeight(22.0f));
        subBox.items.add(juce::FlexItem(zoomInButton).withWidth(22.0f).withHeight(22.0f).withMargin({ 0, 8, 0, 2 }));
        subBox.items.add(juce::FlexItem().withFlex(1.0f));
        subBox.items.add(juce::FlexItem(playheadLabel).withWidth(92.0f).withHeight(22.0f));
        subBox.performLayout(subBar.toFloat());
    }

    minimap_.setVisible(!layersMode);
    horizontalScrollBar.setVisible(!layersMode);
    verticalScrollBar.setVisible(!layersMode);
    trackHeaders_.setVisible(!layersMode);
    ruler_.setVisible(!layersMode);
    trackLanesContainer_.setVisible(!layersMode);
    layerStackView_.setVisible(layersMode);

    if (!layersMode) {
        // Overview Minimap (height: 22)
        minimap_.setBounds(area.removeFromTop(22).reduced(2, 1));
    }

    // Contextual Parameter Strip
    const int stripHeight = parameterStrip_.preferred_height();
    parameterStrip_.setVisible(stripHeight > 0);
    if (stripHeight > 0) {
        parameterStrip_.setBounds(area.removeFromTop(stripHeight));
    }

    if (layersMode) {
        layerStackView_.setBounds(area);
        return;
    }

    // Scrollbars
    auto bottomBar = area.removeFromBottom(14);
    auto rightBar = area.removeFromRight(14);

    horizontalScrollBar.setBounds(bottomBar);
    verticalScrollBar.setBounds(rightBar);

    // Left Track Headers column
    const int header_width = 200;
    trackHeaders_.setBounds(area.removeFromLeft(header_width));

    // Right Area: Ruler + Track Lanes Container
    ruler_.setBounds(area.removeFromTop(TrackHeaderPanel::header_height));
    trackLanesContainer_.setBounds(area);

    // Position lanes within container
    int cur_y = -vertical_scroll_y_;
    for (size_t i = 0; i < lanes_.size(); ++i) {
        auto& lane = *lanes_[i];
        const int h = trackHeaders_.get_track_height(i);
        lane.setBounds(0, cur_y, area.getWidth(), h);
        const bool expanded = trackHeaders_.is_automation_expanded(i);
        if (lane.is_automation_expanded() != expanded) {
            lane.set_automation_expanded(expanded);
        }
        cur_y += h;
    }
}

} // namespace oae::studio
