#include "TransportBar.h"
#include "../Timeline/TimelineSnap.h"
#include <cmath>

namespace oae::studio {

namespace {
constexpr int transportGap = OaeTheme::Spacing::unit;
constexpr int transportButtonSize = 24;
constexpr int transportRowHeight = TransportBar::kBarHeight;
} // namespace

TransportBar::TransportBar(AuthoringModel& model, AuditionTransport& transport)
    : model_(model), transport_(transport)
{
    model_.add_listener(this);
    transport_.add_listener(this);

    gripHandle_.onDoubleClick = [this] { set_minimized(!minimized_); };
    gripHandle_.onToggleMinimized = [this] { set_minimized(!minimized_); };
    addAndMakeVisible(gripHandle_);

    addAndMakeVisible(playButton);
    playButton.set_button_style(VectorIconButton::Style::OutlineTint);
    playButton.set_always_tinted(true);
    pauseButton.set_button_style(VectorIconButton::Style::OutlineTint);
    pauseButton.set_always_tinted(true);
    stopButton.set_button_style(VectorIconButton::Style::OutlineTint);
    loopButton.set_button_style(VectorIconButton::Style::OutlineTint);
    keyOffButton.set_button_style(VectorIconButton::Style::OutlineTint);
    addAndMakeVisible(pauseButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(keyOffButton);

    playButton.setComponentID("btn_play");
    playButton.setTooltip("Play (Space)");

    pauseButton.setComponentID("btn_pause");
    pauseButton.setTooltip("Pause (Space)");

    stopButton.setComponentID("btn_stop");
    stopButton.setTooltip("Stop");

    loopButton.setClickingTogglesState(false);
    loopButton.setComponentID("btn_loop");
    loopButton.setTooltip("Toggle Loop");

    keyOffButton.setComponentID("btn_key_off");
    keyOffButton.setTooltip("Key Off");

    addAndMakeVisible(timecodeLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(paramsButton);
    addAndMakeVisible(kSystemMeter);

    timecodeLabel.setComponentID("timecode_display");
    timecodeLabel.setFont(juce::FontOptions("Consolas", OaeTheme::Typography::compact, juce::Font::bold));
    timecodeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    timecodeLabel.setColour(juce::Label::outlineColourId, juce::Colour(OaeTheme::Colors::borderSubtle));
    timecodeLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    timecodeLabel.setJustificationType(juce::Justification::centred);

    statusLabel.setComponentID("status");
    statusLabel.setFont(juce::FontOptions(OaeTheme::Typography::caption));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    paramsButton.setComponentID("btn_transport_params");
    paramsButton.setInteractive(true);
    paramsButton.setTooltip("Open Global Parameters (Bottom Deck)");
    paramsButton.onClick = [this] {
        if (onOpenGlobalParameters) {
            onOpenGlobalParameters();
        }
    };

    kSystemMeter.setComponentID("transport_ksystem_meter");
    kSystemMeter.set_orientation(OaeMeterComponent::Orientation::HorizontalStereo);
    kSystemMeter.set_scale(MeterScale::K14);

    playButton.onClick = [this]() {
        if (!transport_.play()) {
            statusLabel.setText("Play failed (check bank or event)", juce::dontSendNotification);
        }
    };
    pauseButton.onClick = [this]() { transport_.pause(); };
    stopButton.onClick = [this]() { transport_.stop(); };

    loopButton.onClick = [this]() {
        const bool next_loop = !transport_.is_loop_enabled();
        transport_.set_loop(next_loop);
        loopButton.set_semantic_active(next_loop);
    };

    keyOffButton.onClick = [this]() { transport_.key_off(); };

    update_params_button();
}

TransportBar::~TransportBar()
{
    transport_.remove_listener(this);
    model_.remove_listener(this);
}

void TransportBar::update_params_button()
{
    int count = 0;
    const auto& root = model_.document().get_root_json();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        count = static_cast<int>(root["global_parameters"].size());
    }
    paramsButton.setButtonText("Params (" + juce::String(count) + ")");
}

void TransportBar::update_meter(float peak_l, float peak_r, float rms_l, float rms_r, float dt_seconds)
{
    kSystemMeter.set_stereo_levels(peak_l, peak_r, rms_l, rms_r, dt_seconds);
}

void TransportBar::set_minimized(bool minimized)
{
    if (minimized_ == minimized) {
        return;
    }
    minimized_ = minimized;
    gripHandle_.set_minimized(minimized_);
    keyOffButton.setVisible(!minimized_);
    statusLabel.setVisible(!minimized_);
    paramsButton.setVisible(!minimized_);
    kSystemMeter.setVisible(!minimized_);
    resized();
    if (onLayoutChanged) {
        onLayoutChanged();
    }
}

void TransportBar::update_timecode(double time_ms)
{
    const double clamped = std::max(0.0, time_ms);
    const int sec = static_cast<int>(clamped / 1000.0);
    const int mins = sec / 60;
    const int rem_sec = sec % 60;
    const int ms = static_cast<int>(std::fmod(clamped, 1000.0));
    juce::String text = juce::String::formatted("%02d:%02d.%03d", mins, rem_sec, ms);

    const std::string activePath = transport_.get_event_path();
    const auto& root = model_.document().get_root_json();
    if (!activePath.empty() && root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == activePath && ev.contains("logic_items") && ev["logic_items"].is_array()) {
                bool hasTempo = false;
                for (const auto& item : ev["logic_items"]) {
                    if (item.value("kind", "") == "tempo") {
                        hasTempo = true;
                        break;
                    }
                }
                if (hasTempo) {
                    const auto segments = TimelineSnap::extract_tempo_segments(&ev["logic_items"]);
                    const auto seg = TimelineSnap::get_active_tempo(clamped, segments);
                    text += juce::String::formatted("  |  %.0f BPM  |  %d/%d",
                                                    seg.bpm, seg.ts_upper, seg.ts_lower);
                }
                break;
            }
        }
    }

    timecodeLabel.setText(text, juce::dontSendNotification);
}

void TransportBar::update_status(const std::string& status)
{
    statusLabel.setText(juce::String::fromUTF8(status.c_str()), juce::dontSendNotification);
}

void TransportBar::on_transport_state_changed(TransportState new_state)
{
    playButton.set_semantic_active(new_state == TransportState::Playing);
    pauseButton.set_semantic_active(new_state == TransportState::Paused);
}

void TransportBar::on_parameter_changed(const std::string& /*name*/, float /*val*/, bool /*is_global*/)
{
}

void TransportBar::on_project_changed()
{
    const auto paths = model_.get_event_paths();
    if (!paths.empty()) {
        const auto& cur_sel = model_.get_selection();
        if (cur_sel.type == ItemType::Event && !cur_sel.id_or_path.empty()) {
            transport_.set_event_path(cur_sel.id_or_path);
        } else if (transport_.get_event_path().empty() || std::find(paths.begin(), paths.end(), transport_.get_event_path()) == paths.end()) {
            transport_.set_event_path(paths[0]);
        }
    } else {
        transport_.set_event_path("");
    }
    update_params_button();
}

void TransportBar::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && !selection.id_or_path.empty()) {
        transport_.set_event_path(selection.id_or_path);
    }
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgSurface));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

juce::FlexBox TransportBar::make_layout()
{
    juce::FlexBox layout;
    layout.flexWrap = juce::FlexBox::Wrap::wrap;
    layout.alignContent = juce::FlexBox::AlignContent::flexStart;
    layout.alignItems = juce::FlexBox::AlignItems::center;
    const auto add = [&layout](juce::Component& component, int width, int height, float grow = 0.0f) {
        const float verticalGap = (transportRowHeight - height) * 0.5f;
        layout.items.add(juce::FlexItem(component).withMinWidth(static_cast<float>(width))
            .withHeight(static_cast<float>(height)).withFlex(grow)
            .withMargin({ verticalGap, transportGap, verticalGap, 0 }));
    };

    for (auto* button : { &playButton, &pauseButton, &stopButton, &loopButton }) {
        add(*button, transportButtonSize, transportButtonSize);
    }
    if (!minimized_) {
        add(keyOffButton, transportButtonSize, transportButtonSize);
    }
    const bool hasTempo = timecodeLabel.getText().contains("BPM");
    add(timecodeLabel, hasTempo ? 176 : 92, transportButtonSize);
    if (!minimized_) {
        add(statusLabel, 80, transportButtonSize, 1.0f);
        add(paramsButton, paramsButton.preferredWidth(78), 20);
        add(kSystemMeter, 96, 20);
    }
    return layout;
}

int TransportBar::preferred_height(int width)
{
    const int available = std::max(1, width - GripHandle::kDefaultWidth - 2 * transportGap);
    int rows = 1;
    int used = 0;
    const auto layout = make_layout();
    for (const auto& item : layout.items) {
        const int footprint = static_cast<int>(item.minWidth + item.margin.right);
        if (used > 0 && used + footprint > available) {
            ++rows;
            used = 0;
        }
        used += footprint;
    }
    return rows * transportRowHeight;
}

void TransportBar::resized()
{
    auto area = getLocalBounds();
    gripHandle_.setBounds(area.removeFromLeft(GripHandle::kDefaultWidth));

    auto layout = make_layout();
    layout.performLayout(area.reduced(transportGap, 0));
}

} // namespace oae::studio
