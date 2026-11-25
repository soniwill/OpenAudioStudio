#include "TrackHeaderPanel.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

TrackHeaderItem::TrackHeaderItem(AuthoringModel& model, const std::string& event_path, size_t track_index)
    : model_(model), event_path_(event_path), track_index_(track_index)
{
    addAndMakeVisible(colorSwatchButton_);
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(panSlider);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(autoButton);

    colorSwatchButton_.setButtonText("");
    colorSwatchButton_.setTooltip("Track color");
    colorSwatchButton_.onClick = [this]() { show_color_menu(); };

    titleLabel.setText(juce::String::fromUTF8(model_.get_track_name(event_path_, track_index_).c_str()), juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(OaeTheme::Typography::control, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    titleLabel.setInterceptsMouseClicks(false, false);
    titleLabel.setEditable(false, true, false);
    titleLabel.onTextChange = [this]() {
        if (is_updating_from_model_) return;
        const auto text = titleLabel.getText().trim().toStdString();
        if (!text.empty()) {
            model_.set_track_name(event_path_, track_index_, text);
        }
    };

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 16);
    volumeSlider.setRange(-80.0, 10.0, 0.1);
    volumeSlider.setTextValueSuffix(" dB");
    volumeSlider.setValue(0.0, juce::dontSendNotification);
    volumeSlider.setDoubleClickReturnValue(true, 0.0, {});
    volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::trackFaderAccent));

    volumeSlider.onDragStart = [this]() {
        is_adjusting_slider_ = true;
        model_.begin_gesture();
    };
    volumeSlider.onValueChange = [this]() {
        if (is_updating_from_model_) return;
        model_.set_track_volume(event_path_, track_index_, volumeSlider.getValue());
    };
    volumeSlider.onDragEnd = [this]() {
        is_adjusting_slider_ = false;
        model_.end_gesture();
    };

    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setDoubleClickReturnValue(true, 0.0, {});
    panSlider.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::trackPanAccent));

    panSlider.onDragStart = [this]() {
        is_adjusting_slider_ = true;
        model_.begin_gesture();
    };
    panSlider.onValueChange = [this]() {
        if (is_updating_from_model_) return;
        model_.set_track_pan(event_path_, track_index_, panSlider.getValue());
    };
    panSlider.onDragEnd = [this]() {
        is_adjusting_slider_ = false;
        model_.end_gesture();
    };

    muteButton.setClickingTogglesState(false);
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(OaeTheme::Colors::accentAmber));
    muteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::textSecondary));
    muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::bgDark));
    muteButton.onClick = [this]() {
        const bool current = model_.is_track_muted(event_path_, track_index_);
        model_.set_track_muted(event_path_, track_index_, !current);
    };

    soloButton.setClickingTogglesState(false);
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    soloButton.setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::textSecondary));
    soloButton.setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::bgDark));
    soloButton.onClick = [this]() {
        const bool current = model_.is_track_solo(event_path_, track_index_);
        model_.set_track_solo(event_path_, track_index_, !current);
    };

    autoButton.setClickingTogglesState(false);
    autoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    autoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(OaeTheme::Colors::accentPurple));
    autoButton.setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::textSecondary));
    autoButton.setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::textPrimary));
    autoButton.onClick = [this]() {
        automation_expanded_ = !automation_expanded_;
        autoButton.setToggleState(automation_expanded_, juce::dontSendNotification);
        if (onAutomationToggled) {
            onAutomationToggled(track_index_, automation_expanded_);
        }
    };

    update_component_ids();
    update_from_model();
}

void TrackHeaderItem::update_component_ids()
{
    const auto idx = juce::String(track_index_);
    setComponentID("track_header_" + idx);
    colorSwatchButton_.setComponentID("track_color_swatch_" + idx);
    titleLabel.setComponentID("track_title_" + idx);
    volumeSlider.setComponentID("track_volume_" + idx);
    panSlider.setComponentID("track_pan_" + idx);
    muteButton.setComponentID("track_mute_" + idx);
    soloButton.setComponentID("track_solo_" + idx);
    autoButton.setComponentID("track_auto_" + idx);
}

void TrackHeaderItem::update_from_model()
{
    is_updating_from_model_ = true;
    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                const auto& trks = ev["tracks"];
                if (track_index_ < trks.size()) {
                    const auto& trk = trks[track_index_];
                    volumeSlider.setValue(trk.value("volume_db", 0.0), juce::dontSendNotification);
                    panSlider.setValue(trk.value("pan", 0.0), juce::dontSendNotification);
                }
                break;
            }
        }
    }

    const bool muted = model_.is_track_muted(event_path_, track_index_);
    muteButton.setToggleState(muted, juce::dontSendNotification);

    const bool solo = model_.is_track_solo(event_path_, track_index_);
    soloButton.setToggleState(solo, juce::dontSendNotification);

    autoButton.setToggleState(automation_expanded_, juce::dontSendNotification);

    titleLabel.setText(juce::String::fromUTF8(model_.get_track_name(event_path_, track_index_).c_str()),
                       juce::dontSendNotification);
    const auto trackColour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);
    colorSwatchButton_.setColour(juce::TextButton::buttonColourId, trackColour);

    is_updating_from_model_ = false;
    repaint();
}

void TrackHeaderItem::set_track_index(size_t track_index)
{
    track_index_ = track_index;
    update_component_ids();
    update_from_model();
}

void TrackHeaderItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto sel = model_.get_selection();
    const bool is_selected = (sel.type == ItemType::Event && sel.id_or_path == event_path_
                              && sel.track_index == static_cast<int>(track_index_));
    const auto trackColour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);

    size_t num_tracks = 1;
    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                num_tracks = ev["tracks"].size();
                break;
            }
        }
    }

    auto bg = is_selected
        ? juce::Colour(OaeTheme::Colors::bgSurfaceRaised).interpolatedWith(trackColour, OaeTheme::States::selectedTrackTintOpacity)
        : juce::Colour(OaeTheme::Colors::bgSurface);
    g.fillAll(bg);

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    g.setColour(trackColour);
    g.fillRect(bounds.withWidth(is_selected ? 4.0f : 3.0f));

    if (is_selected && num_tracks >= 2) {
        const auto borderColour = trackColour.brighter(0.25f);
        g.setColour(borderColour);
        g.fillRect(0.0f, 0.0f, bounds.getWidth(), 1.5f);
        g.fillRect(0.0f, bounds.getHeight() - 1.5f, bounds.getWidth(), 1.5f);
    }
}

void TrackHeaderItem::resized()
{
    auto area = getLocalBounds().reduced(OaeTheme::Spacing::unit);
    area.removeFromLeft(OaeTheme::Spacing::unit);

    juce::FlexBox topRow;
    topRow.flexDirection = juce::FlexBox::Direction::row;
    topRow.alignItems = juce::FlexBox::AlignItems::center;
    topRow.items.add(juce::FlexItem(colorSwatchButton_).withFlex(0.0f, 0.0f, 14.0f).withHeight(14.0f).withMargin({ 0.0f, 5.0f, 0.0f, 0.0f }));
    topRow.items.add(juce::FlexItem(titleLabel).withFlex(1.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    topRow.items.add(juce::FlexItem(muteButton).withFlex(0.0f, 0.5f, 22.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    topRow.items.add(juce::FlexItem(soloButton).withFlex(0.0f, 0.5f, 22.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    topRow.items.add(juce::FlexItem(autoButton).withFlex(0.0f, 0.5f, 22.0f).withHeight(22.0f));

    juce::FlexBox column;
    column.flexDirection = juce::FlexBox::Direction::column;
    column.alignItems = juce::FlexBox::AlignItems::stretch;
    column.items.add(juce::FlexItem(topRow).withFlex(0.0f, 0.0f, 22.0f).withMargin({ 0.0f, 0.0f, 4.0f, 0.0f }));
    column.items.add(juce::FlexItem(volumeSlider).withFlex(0.0f, 0.0f, 22.0f).withMargin({ 0.0f, 0.0f, 2.0f, 0.0f }));
    column.items.add(juce::FlexItem(panSlider).withFlex(0.0f, 0.0f, 20.0f));

    column.performLayout(area.toFloat());
}

void TrackHeaderItem::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        show_context_menu(e.getScreenPosition());
        return;
    }
    is_dragging_ = false;
    drag_start_y_ = e.getPosition().y;
    model_.select(ItemSelection{ ItemType::Event, event_path_, static_cast<int>(track_index_), -1 });
}

void TrackHeaderItem::mouseDrag(const juce::MouseEvent& e)
{
    const int delta_y = e.getDistanceFromDragStartY();
    if (!is_dragging_ && std::abs(delta_y) > 4) {
        is_dragging_ = true;
    }
    if (is_dragging_ && onTrackDragged) {
        onTrackDragged(track_index_, delta_y);
    }
}

void TrackHeaderItem::mouseUp(const juce::MouseEvent& e)
{
    if (is_dragging_) {
        is_dragging_ = false;
        if (onTrackDragEnded) {
            onTrackDragEnded(track_index_, e.getDistanceFromDragStartY());
        }
    }
}

void TrackHeaderItem::mouseDoubleClick(const juce::MouseEvent&)
{
    titleLabel.showEditor();
}

std::vector<TestableTarget> TrackHeaderItem::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    const auto titleBounds = titleLabel.getBounds().getIntersection(bounds);
    if (titleBounds.isEmpty()) return {};
    return {
        { "title", "double_click", titleBounds, isEnabled() },
        { "body", "drag", titleBounds, isEnabled() }
    };
}

void TrackHeaderItem::show_color_menu()
{
    juce::PopupMenu menu;
    const auto currentHex = model_.get_track_color(event_path_, track_index_);
    for (size_t i = 0; i < OaeTheme::TrackColors::paletteSize; ++i) {
        const auto hex = OaeTheme::TrackColors::hexForIndex(i);
        const bool selected = (currentHex == hex);
        menu.addColouredItem(static_cast<int>(i + 1),
                             juce::String(OaeTheme::TrackColors::labelForIndex(i)) + " (" + juce::String(hex) + ")",
                             OaeTheme::TrackColors::colorForIndex(i),
                             true,
                             selected);
    }

    juce::Component::SafePointer<TrackHeaderItem> safe(this);
    UiTestDialogSeam::show_popup_menu("track_color_menu", menu,
        juce::PopupMenu::Options().withTargetComponent(colorSwatchButton_),
        [safe](int result) {
            if (!safe || result <= 0) return;
            const size_t idx = static_cast<size_t>(result - 1);
            if (idx < OaeTheme::TrackColors::paletteSize) {
                safe->model_.set_track_color(safe->event_path_, safe->track_index_, OaeTheme::TrackColors::hexForIndex(idx));
            }
        });
}

void TrackHeaderItem::show_context_menu(juce::Point<int> screen_pos)
{
    juce::PopupMenu menu;
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

    menu.addItem(1, "Move Track Up", track_index_ > 0);
    menu.addItem(2, "Move Track Down", track_index_ + 1 < num_tracks);
    menu.addSeparator();
    menu.addItem(3, "Add Track Below");
    menu.addItem(4, "Duplicate Track");
    menu.addItem(5, "Delete Track", num_tracks > 1);

    const std::string path = event_path_;
    const size_t track_idx = track_index_;
    auto on_struct_changed = onTrackStructureChanged;
    auto& model = model_;

    UiTestDialogSeam::show_popup_menu("track_context_menu", menu,
        juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screen_pos.x, screen_pos.y, 1, 1)),
        [&model, path, track_idx, on_struct_changed](int res) {
            if (res <= 0) return;
            juce::MessageManager::callAsync([&model, path, track_idx, on_struct_changed, res]() {
                if (res == 1 && track_idx > 0) {
                    model.reorder_track(path, track_idx, track_idx - 1);
                    if (on_struct_changed) on_struct_changed();
                } else if (res == 2) {
                    model.reorder_track(path, track_idx, track_idx + 1);
                    if (on_struct_changed) on_struct_changed();
                } else if (res == 3) {
                    model.add_track(path);
                    if (on_struct_changed) on_struct_changed();
                } else if (res == 4) {
                    model.duplicate_track(path, track_idx);
                    if (on_struct_changed) on_struct_changed();
                } else if (res == 5) {
                    model.remove_track(path, track_idx);
                    if (on_struct_changed) on_struct_changed();
                }
            });
        });
}

TrackHeaderPanel::TrackHeaderPanel(AuthoringModel& model)
    : model_(model)
{
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(addTrackButton);
    addTrackButton.setComponentID("timeline.add_track");

    headerLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    addTrackButton.onClick = [this]() {
        if (!event_path_.empty()) {
            model_.add_track(event_path_);
            refresh_tracks();
            if (onTracksChanged) onTracksChanged();
        }
    };
}

void TrackHeaderPanel::set_event_path(const std::string& path)
{
    if (event_path_ != path) {
        event_path_ = path;
        refresh_tracks();
    }
}

void TrackHeaderPanel::repaint_selection()
{
    for (auto& item : items_) {
        if (item) item->repaint();
    }
    repaint();
}

void TrackHeaderPanel::set_vertical_scroll(int scroll_y)
{
    vertical_scroll_y_ = scroll_y;
    resized();
}

int TrackHeaderPanel::get_track_y(size_t track_index) const
{
    int y = header_height - vertical_scroll_y_;
    for (size_t i = 0; i < track_index && i < items_.size(); ++i) {
        y += items_[i]->is_automation_expanded() ? 140 : 80;
    }
    return y;
}

int TrackHeaderPanel::get_track_height(size_t track_index) const
{
    return is_automation_expanded(track_index) ? 140 : 80;
}

bool TrackHeaderPanel::is_automation_expanded(size_t track_index) const noexcept
{
    return track_index < items_.size() && items_[track_index]->is_automation_expanded();
}

void TrackHeaderPanel::handle_track_drag(size_t src_track, int delta_y)
{
    if (items_.empty()) return;
    drag_src_track_ = static_cast<int>(src_track);

    const int current_y = get_track_y(src_track) + delta_y + (get_track_height(src_track) / 2);
    int best_idx = static_cast<int>(src_track);
    int min_dist = std::numeric_limits<int>::max();

    for (size_t i = 0; i < items_.size(); ++i) {
        const int mid_y = get_track_y(i) + (get_track_height(i) / 2);
        const int dist = std::abs(current_y - mid_y);
        if (dist < min_dist) {
            min_dist = dist;
            best_idx = static_cast<int>(i);
        }
    }

    drag_dest_track_ = best_idx;
    repaint();
}

void TrackHeaderPanel::handle_track_drag_ended(size_t src_track, int /*delta_y*/)
{
    const int dest = drag_dest_track_;
    drag_src_track_ = -1;
    drag_dest_track_ = -1;
    repaint();

    if (dest >= 0 && static_cast<size_t>(dest) != src_track && static_cast<size_t>(dest) < items_.size()) {
        is_reordering_track_ = true;
        const std::string path = event_path_;
        const size_t target_dest = static_cast<size_t>(dest);

        juce::MessageManager::callAsync([safe_this = juce::Component::SafePointer<TrackHeaderPanel>(this), path, src_track, target_dest]() {
            if (!safe_this) return;
            safe_this->model_.reorder_track(path, src_track, target_dest);
            safe_this->refresh_tracks();
            if (safe_this->onTracksChanged) {
                safe_this->onTracksChanged();
            }
            safe_this->is_reordering_track_ = false;
        });
    }
}

void TrackHeaderPanel::refresh_tracks()
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

    if (items_.size() == num_tracks) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i]) {
                items_[i]->set_track_index(i);
            }
        }
        resized();
        return;
    }

    items_.clear();

    const auto& root = model_.document().get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path_ && ev.contains("tracks") && ev["tracks"].is_array()) {
                const size_t total_tracks = ev["tracks"].size();
                for (size_t i = 0; i < total_tracks; ++i) {
                    auto item = std::make_unique<TrackHeaderItem>(model_, event_path_, i);
                    item->onAutomationToggled = [this](size_t, bool) {
                        resized();
                        if (onTracksChanged) onTracksChanged();
                    };
                    item->onTrackStructureChanged = [this]() {
                        refresh_tracks();
                        if (onTracksChanged) onTracksChanged();
                    };
                    item->onTrackDragged = [this](size_t src_track, int delta_y) {
                        handle_track_drag(src_track, delta_y);
                    };
                    item->onTrackDragEnded = [this](size_t src_track, int delta_y) {
                        handle_track_drag_ended(src_track, delta_y);
                    };
                    addAndMakeVisible(*item);
                    items_.push_back(std::move(item));
                }
                break;
            }
        }
    }
    resized();
}

void TrackHeaderPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));

    // Draw drag insertion indicator line
    if (drag_dest_track_ >= 0 && drag_dest_track_ < static_cast<int>(items_.size()) && drag_src_track_ != drag_dest_track_) {
        const int target_y = (drag_dest_track_ > drag_src_track_)
            ? get_track_y(static_cast<size_t>(drag_dest_track_)) + get_track_height(static_cast<size_t>(drag_dest_track_))
            : get_track_y(static_cast<size_t>(drag_dest_track_));
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.fillRect(0, target_y - 2, getWidth(), 4);
    }
}

void TrackHeaderPanel::resized()
{
    auto area = getLocalBounds();
    auto topArea = area.removeFromTop(header_height).reduced(2);
    addTrackButton.setBounds(topArea.removeFromRight(24).reduced(2));
    headerLabel.setBounds(topArea);

    int cur_y = header_height - vertical_scroll_y_;
    for (auto& item : items_) {
        const int h = item->is_automation_expanded() ? 140 : 80;
        item->setBounds(0, cur_y, getWidth() - 1, h);
        cur_y += h;
    }
}

} // namespace oae::studio
