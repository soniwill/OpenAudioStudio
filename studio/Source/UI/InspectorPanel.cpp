#include "InspectorPanel.h"
#include "Inspector/EventHeaderCard.h"
#include "Inspector/TrackCard.h"
#include "Inspector/ClipCard.h"
#include "Inspector/MaskTransitionCard.h"
#include "Inspector/BusCard.h"
#include "Inspector/ParameterCard.h"
#include "Inspector/SnapshotCard.h"
#include "Inspector/VcaCard.h"
#include "Inspector/SampleCard.h"

namespace oae::studio {

InspectorPanel::InspectorPanel(AuthoringModel& model)
    : model_(model)
{
    model_.add_listener(this);

    addAndMakeVisible(headerLabel_);
    headerLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    headerLabel_.setColour(juce::Label::textColourId, juce::Colours::darkgrey);

    lockButton_.setTooltip("Lock Inspector");
    lockButton_.setComponentID("inspector_lock_button");
    lockButton_.onClick = [this]() { set_locked(!is_locked_); };
    addAndMakeVisible(lockButton_);

    addAndMakeVisible(viewport_);
    viewport_.setScrollBarsShown(true, false);

    update_view();
}

InspectorPanel::~InspectorPanel()
{
    model_.remove_listener(this);
}

void InspectorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    if (!embeddedHeaderMode_) {
        g.setColour(juce::Colour(OaeTheme::Colors::dockHeaderBg));
        g.fillRect(0, 0, getWidth(), 24);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawHorizontalLine(23, 0.0f, static_cast<float>(getWidth()));
    }
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRect(getLocalBounds(), 1);
}

void InspectorPanel::resized()
{
    const int toolbarH = embeddedHeaderMode_ ? 0 : 24;
    headerLabel_.setVisible(!embeddedHeaderMode_);
    lockButton_.setVisible(!embeddedHeaderMode_);
    if (!embeddedHeaderMode_) {
        headerLabel_.setBounds(8, 2, std::max(40, getWidth() - 36), toolbarH - 4);
        lockButton_.setBounds(getWidth() - 25, 2, 20, 20);
    }
    viewport_.setBounds(2, toolbarH + 2, std::max(0, getWidth() - 4), std::max(0, getHeight() - toolbarH - 4));

    if (auto* viewed = viewport_.getViewedComponent())
        resize_viewport_content(viewport_, *viewed);
}

void InspectorPanel::set_embedded_header_mode(bool embedded)
{
    if (embeddedHeaderMode_ == embedded) return;
    embeddedHeaderMode_ = embedded;
    resized();
    repaint();
}

void InspectorPanel::set_layers_mode(bool layersMode)
{
    if (layersMode_ == layersMode) return;
    layersMode_ = layersMode;
    update_view();
}

void InspectorPanel::set_locked(bool locked)
{
    if (is_locked_ == locked) return;
    is_locked_ = locked;
    lockButton_.set_icon_type(is_locked_ ? VectorIconType::LockClosed : VectorIconType::LockOpen);
    lockButton_.setToggleState(is_locked_, juce::dontSendNotification);
    lockButton_.set_semantic_active(is_locked_);
    lockButton_.setTooltip(is_locked_ ? "Unlock Inspector" : "Lock Inspector");
    if (onLockChanged) {
        onLockChanged(is_locked_);
    }

    if (!is_locked_) {
        const auto& current_sel = model_.get_selection();
        if (current_sel.type != last_selection_.type ||
            current_sel.id_or_path != last_selection_.id_or_path ||
            current_sel.track_index != last_selection_.track_index ||
            current_sel.instrument_index != last_selection_.instrument_index) {
            update_view();
        }
    }
}

void InspectorPanel::schedule_refresh()
{
    refresh_pending_ = false;
    juce::Component::SafePointer<InspectorPanel> safe_this(this);
    juce::MessageManager::callAsync([safe_this]() {
        if (safe_this && safe_this->edit_depth_ == 0) {
            safe_this->update_view();
        }
    });
}

void InspectorPanel::on_project_changed()
{
    if (edit_depth_ > 0) {
        refresh_pending_ = true;
        return;
    }
    schedule_refresh();
}

void InspectorPanel::on_selection_changed(const ItemSelection&)
{
    if (is_locked_) {
        return;
    }
    update_view();
}

void InspectorPanel::on_item_renamed(ItemType, const std::string&, const std::string&)
{
    update_view();
}

void InspectorPanel::on_item_deleted(ItemType type, const std::string& id)
{
    if (last_selection_.type == type && last_selection_.id_or_path == id) {
        if (is_locked_) {
            set_locked(false);
        }
    }
    update_view();
}

void InspectorPanel::update_view()
{
    const auto sel = model_.get_selection();
    const bool same_selection = currentEditor_ != nullptr
        && sel.type == last_selection_.type
        && sel.id_or_path == last_selection_.id_or_path
        && sel.track_index == last_selection_.track_index
        && sel.instrument_index == last_selection_.instrument_index;
    const auto prev_scroll = same_selection ? viewport_.getViewPosition() : juce::Point<int>(0, 0);
    if (!same_selection && sel.type == ItemType::Event) {
        if (sel.track_index < 0) {
            event_card_expanded_ = true;
        } else if (sel.instrument_index < 0) {
            track_card_expanded_ = true;
            event_card_expanded_ = false;
        } else {
            clip_card_expanded_ = true;
            track_card_expanded_ = false;
            event_card_expanded_ = false;
        }
    }
    last_selection_ = sel;

    if (!sel.is_valid()) {
        currentEditor_ = create_empty_editor();
    } else {
        switch (sel.type) {
            case ItemType::Event:
                currentEditor_ = create_event_editor(sel.id_or_path);
                break;
            case ItemType::Snapshot:
                currentEditor_ = create_snapshot_editor(sel.id_or_path);
                break;
            case ItemType::Bus:
                currentEditor_ = create_bus_editor(sel.id_or_path);
                break;
            case ItemType::Vca:
                currentEditor_ = create_vca_editor(sel.id_or_path);
                break;
            case ItemType::GlobalParameter:
                currentEditor_ = create_param_editor(sel.id_or_path);
                break;
            case ItemType::Sample:
                currentEditor_ = create_sample_editor(sel.id_or_path);
                break;
            default:
                currentEditor_ = create_empty_editor();
                break;
        }
    }

    viewport_.setViewedComponent(currentEditor_.get(), false);
    resize_viewport_content(viewport_, *currentEditor_);
    if (same_selection) {
        viewport_.setViewPosition(prev_scroll);
    }
}

bool InspectorPanel::is_card_expanded(const std::string& card_id) const noexcept
{
    if (card_id == "inspector_card_clip") return clip_card_expanded_;
    if (card_id == "inspector_card_track") return track_card_expanded_;
    if (card_id == "inspector_card_mask") return mask_card_expanded_;
    if (card_id == "inspector_card_event") return event_card_expanded_;
    return true;
}

void InspectorPanel::set_card_expanded(const std::string& card_id, bool expanded)
{
    if (card_id == "inspector_card_clip") clip_card_expanded_ = expanded;
    else if (card_id == "inspector_card_track") track_card_expanded_ = expanded;
    else if (card_id == "inspector_card_mask") mask_card_expanded_ = expanded;
    else if (card_id == "inspector_card_event") event_card_expanded_ = expanded;
    update_view();
}

std::unique_ptr<juce::Component> InspectorPanel::create_empty_editor()
{
    class EmptyComp : public juce::Component {
    public:
        void paint(juce::Graphics& g) override {
            g.setColour(juce::Colours::grey);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText("No item selected", getLocalBounds(), juce::Justification::centred);
        }
    };
    auto comp = std::make_unique<EmptyComp>();
    comp->setSize(260, 200);
    return comp;
}

std::unique_ptr<juce::Component> InspectorPanel::create_event_editor(const std::string& path)
{
    const auto& root = model_.json();
    const nlohmann::json* ev_ptr = nullptr;
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == path) {
                ev_ptr = &ev;
                break;
            }
        }
    }
    if (!ev_ptr) return create_empty_editor();
    const auto& ev = *ev_ptr;

    auto stack = std::make_unique<CardStackEditor>();
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto make_toggle_cb = [safe_panel = juce::Component::SafePointer<InspectorPanel>(this)](const std::string& cid) {
        return [safe_panel, cid](bool nextExpanded) {
            juce::MessageManager::callAsync([safe_panel, cid, nextExpanded]() {
                if (safe_panel) {
                    safe_panel->set_card_expanded(cid, nextExpanded);
                }
            });
        };
    };

    const auto& selection = model_.get_selection();
    const bool has_tracks = ev.contains("tracks") && ev["tracks"].is_array() && !ev["tracks"].empty();
    const auto& tracks = has_tracks ? ev["tracks"] : nlohmann::json::array();

    int active_track = -1;
    if (has_tracks && selection.track_index >= 0
        && static_cast<size_t>(selection.track_index) < tracks.size()) {
        active_track = selection.track_index;
    }

    bool has_selected_clip = false;
    size_t sel_track = 0;
    size_t sel_inst = 0;
    if (active_track >= 0 && selection.instrument_index >= 0) {
        sel_track = static_cast<size_t>(active_track);
        sel_inst = static_cast<size_t>(selection.instrument_index);
        if (tracks[sel_track].contains("instruments")
            && tracks[sel_track]["instruments"].is_array()
            && sel_inst < tracks[sel_track]["instruments"].size()) {
            has_selected_clip = true;
        }
    }

    if (has_selected_clip) {
        stack->addCard(std::make_unique<ClipCard>(
            path,
            sel_track,
            sel_inst,
            clip_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_clip")));
        stack->addCard(std::make_unique<TrackCard>(
            path,
            sel_track,
            track_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_track")));
        const auto maskSpec = model_.get_track_layer_mask(path, static_cast<int>(sel_track));
        if (layersMode_ || maskSpec.state != LayerMaskSpec::State::AlwaysActive) {
            stack->addCard(std::make_unique<MaskTransitionCard>(
                path,
                sel_track,
                mask_card_expanded_,
                model_,
                edit_depth_,
                on_end,
                make_toggle_cb("inspector_card_mask"),
                onSwitchToTimelineRequested));
        }
        stack->addCard(std::make_unique<EventHeaderCard>(
            path,
            event_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_event")));
    } else if (active_track >= 0) {
        stack->addCard(std::make_unique<TrackCard>(
            path,
            static_cast<size_t>(active_track),
            track_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_track")));
        const auto maskSpec = model_.get_track_layer_mask(path, active_track);
        if (layersMode_ || maskSpec.state != LayerMaskSpec::State::AlwaysActive) {
            stack->addCard(std::make_unique<MaskTransitionCard>(
                path,
                static_cast<size_t>(active_track),
                mask_card_expanded_,
                model_,
                edit_depth_,
                on_end,
                make_toggle_cb("inspector_card_mask"),
                onSwitchToTimelineRequested));
        }
        stack->addCard(std::make_unique<EventHeaderCard>(
            path,
            event_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_event")));
    } else {
        stack->addCard(std::make_unique<EventHeaderCard>(
            path,
            event_card_expanded_,
            model_,
            edit_depth_,
            on_end,
            make_toggle_cb("inspector_card_event")));
    }

    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

std::unique_ptr<juce::Component> InspectorPanel::create_snapshot_editor(const std::string& path)
{
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto card = std::make_unique<SnapshotCard>(path, true, model_, edit_depth_, on_end);
    auto stack = std::make_unique<CardStackEditor>();
    stack->addCard(std::move(card));
    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

std::unique_ptr<juce::Component> InspectorPanel::create_bus_editor(const std::string& path)
{
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto card = std::make_unique<BusCard>(path, true, model_, edit_depth_, on_end);
    auto stack = std::make_unique<CardStackEditor>();
    stack->addCard(std::move(card));
    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

std::unique_ptr<juce::Component> InspectorPanel::create_vca_editor(const std::string& path)
{
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto card = std::make_unique<VcaCard>(path, true, model_, edit_depth_, on_end);
    auto stack = std::make_unique<CardStackEditor>();
    stack->addCard(std::move(card));
    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

std::unique_ptr<juce::Component> InspectorPanel::create_param_editor(const std::string& name)
{
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto card = std::make_unique<ParameterCard>(name, true, model_, edit_depth_, on_end);
    auto stack = std::make_unique<CardStackEditor>();
    stack->addCard(std::move(card));
    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

std::unique_ptr<juce::Component> InspectorPanel::create_sample_editor(const std::string& id)
{
    auto on_end = [this]() { if (refresh_pending_) schedule_refresh(); };
    auto card = std::make_unique<SampleCard>(id, true, model_, edit_depth_, on_end);
    auto stack = std::make_unique<CardStackEditor>();
    stack->addCard(std::move(card));
    stack->setSize(270, stack->contentHeightForWidth(270));
    return stack;
}

} // namespace oae::studio
