#include "MixerDeckPanel.h"
#include "Theme.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace oae::studio {

MixerStripItem::MixerStripItem(StudioEngine& engine,
                               AuthoringModel& model,
                               AuditionTransport& transport,
                               const std::string& path,
                               StripKind kind,
                               std::function<void()> on_solo_changed)
    : engine_(engine), model_(model), transport_(transport),
      path_(path), kind_(kind), on_solo_changed_(std::move(on_solo_changed))
{
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(panSlider);
    addAndMakeVisible(buttonRow_);
    buttonRow_.addAndMakeVisible(muteButton);
    buttonRow_.addAndMakeVisible(soloButton);
    addAndMakeVisible(vuMeter_);

    std::string displayName = path_;
    if (displayName.rfind("bus:/", 0) == 0) displayName = displayName.substr(5);
    else if (displayName.rfind("vca:/", 0) == 0) displayName = displayName.substr(5);
    if (displayName.empty()) displayName = "Master";

    std::string safeName = displayName;
    for (auto& c : safeName) {
        if (c == '/' || c == ' ') c = '_';
    }

    if (path_ == "bus:/" || displayName == "Master") {
        volumeSlider.setComponentID("mixer_fader_master");
        panSlider.setComponentID("mixer_pan_master");
        muteButton.setComponentID("mixer_mute_master");
        soloButton.setComponentID("mixer_solo_master");
    } else if (kind_ == StripKind::Bus) {
        volumeSlider.setComponentID("mixer_fader_bus_" + safeName);
        panSlider.setComponentID("mixer_pan_bus_" + safeName);
        muteButton.setComponentID("mixer_mute_bus_" + safeName);
        soloButton.setComponentID("mixer_solo_bus_" + safeName);
    } else {
        volumeSlider.setComponentID("mixer_fader_vca_" + safeName);
        panSlider.setComponentID("mixer_pan_vca_" + safeName);
        muteButton.setComponentID("mixer_mute_vca_" + safeName);
        soloButton.setComponentID("mixer_solo_vca_" + safeName);
    }

    nameLabel.setText(juce::String::fromUTF8(displayName.c_str()), juce::dontSendNotification);
    nameLabel.setFont(juce::FontOptions(OaeTheme::Typography::compact, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(kind_ == StripKind::Vca ? OaeTheme::Colors::accentAmber : OaeTheme::Colors::textSecondary));
    nameLabel.setJustificationType(juce::Justification::centred);

    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    volumeSlider.setRange(-80.0, 10.0, 0.1);
    volumeSlider.setTextValueSuffix(" dB");
    volumeSlider.setValue(0.0, juce::dontSendNotification);
    volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::trackFaderAccent));

    volumeSlider.onValueChange = [this]() {
        const float val = static_cast<float>(volumeSlider.getValue());
        if (kind_ == StripKind::Bus) {
            oae_bus b = 0;
            if (engine_.get_bus(path_, &b) == OAE_OK) {
                engine_.set_bus_volume_db(b, val);
            }
            if (auto* live = transport_.get_live_bridge(); live != nullptr && live->is_client_connected()) {
                live->send_bus_volume(path_, val);
            }
        } else {
            oae_vca v = 0;
            if (engine_.get_vca(path_, &v) == OAE_OK) {
                engine_.set_vca_volume_db(v, val);
            }
            if (auto* live = transport_.get_live_bridge(); live != nullptr && live->is_client_connected()) {
                live->send_vca_volume(path_, val);
            }
        }
    };

    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::trackPanAccent));

    panSlider.onValueChange = [this]() {
        const float val = static_cast<float>(panSlider.getValue());
        if (kind_ == StripKind::Bus) {
            oae_bus b = 0;
            if (engine_.get_bus(path_, &b) == OAE_OK) {
                engine_.set_bus_pan(b, val);
            }
            if (auto* live = transport_.get_live_bridge(); live != nullptr && live->is_client_connected()) {
                live->send_bus_pan(path_, val);
            }
        }
    };

    muteButton.setClickingTogglesState(false);
    muteButton.onClick = [this]() {
        is_muted_ = !is_muted_;
        muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(is_muted_ ? OaeTheme::Colors::accentAmber : OaeTheme::Colors::bgSurfaceRaised));
        muteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(is_muted_ ? OaeTheme::Colors::clipTitleTextDark : OaeTheme::Colors::textSecondary));
        if (on_solo_changed_) on_solo_changed_();
    };

    soloButton.setClickingTogglesState(false);
    soloButton.onClick = [this]() {
        is_soloed_ = !is_soloed_;
        soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(is_soloed_ ? OaeTheme::Colors::accentCyan : OaeTheme::Colors::bgSurfaceRaised));
        soloButton.setColour(juce::TextButton::textColourOffId, juce::Colour(is_soloed_ ? OaeTheme::Colors::clipTitleTextDark : OaeTheme::Colors::textSecondary));
        if (on_solo_changed_) on_solo_changed_();
    };
}

void MixerStripItem::set_muted(bool mute)
{
    is_muted_ = mute;
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(is_muted_ ? OaeTheme::Colors::accentAmber : OaeTheme::Colors::bgSurfaceRaised));
    muteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(is_muted_ ? OaeTheme::Colors::clipTitleTextDark : OaeTheme::Colors::textSecondary));
    if (on_solo_changed_) on_solo_changed_();
}

void MixerStripItem::set_soloed(bool solo)
{
    is_soloed_ = solo;
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(is_soloed_ ? OaeTheme::Colors::accentCyan : OaeTheme::Colors::bgSurfaceRaised));
    soloButton.setColour(juce::TextButton::textColourOffId, juce::Colour(is_soloed_ ? OaeTheme::Colors::clipTitleTextDark : OaeTheme::Colors::textSecondary));
    if (on_solo_changed_) on_solo_changed_();
}

void MixerStripItem::set_effective_mute(bool mute)
{
    effective_muted_ = mute;
    if (kind_ == StripKind::Bus) {
        oae_bus b = 0;
        if (engine_.get_bus(path_, &b) == OAE_OK) {
            engine_.set_bus_mute(b, effective_muted_);
        }
        if (auto* live = transport_.get_live_bridge(); live != nullptr && live->is_client_connected()) {
            live->send_bus_mute(path_, effective_muted_);
        }
    }
}

void MixerStripItem::set_volume_db(float db)
{
    volumeSlider.setValue(db, juce::sendNotificationSync);
}

float MixerStripItem::get_volume_db() const
{
    return static_cast<float>(volumeSlider.getValue());
}

void MixerStripItem::set_pan(float pan)
{
    panSlider.setValue(pan, juce::sendNotificationSync);
}

float MixerStripItem::get_pan() const
{
    return static_cast<float>(panSlider.getValue());
}

void MixerStripItem::update_meter(float elapsedSeconds)
{
    if (kind_ == StripKind::Bus) {
        oae_bus b = 0;
        if (engine_.get_bus(path_, &b) == OAE_OK) {
            oae_meter m{};
            if (engine_.get_bus_meter(b, &m) == OAE_OK) {
                vuMeter_.set_levels(m.peak[0], m.rms[0], elapsedSeconds);
                return;
            }
        }
    }
    vuMeter_.set_levels(0.0f, 0.0f, elapsedSeconds);
}

void MixerStripItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(kind_ == StripKind::Vca ? OaeTheme::Colors::cardBodyBg : OaeTheme::Colors::bgSurface));

    // Right border separator
    g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
    g.drawVerticalLine(getWidth() - 1, 0.0f, bounds.getHeight());
}

void MixerStripItem::resized()
{
    juce::FlexBox faderRow;
    faderRow.flexDirection = juce::FlexBox::Direction::row;
    faderRow.items.add(juce::FlexItem(volumeSlider).withFlex(1.0f).withMinWidth(32.0f));
    faderRow.items.add(juce::FlexItem(vuMeter_).withFlex(0.0f, 0.0f, 12.0f));

    juce::FlexBox buttons;
    buttons.flexDirection = juce::FlexBox::Direction::row;
    buttons.justifyContent = juce::FlexBox::JustifyContent::center;
    buttons.items.add(juce::FlexItem(muteButton).withFlex(0.0f, 0.0f, 24.0f));
    buttons.items.add(juce::FlexItem(soloButton).withFlex(0.0f, 0.0f, 24.0f));

    juce::FlexBox column;
    column.flexDirection = juce::FlexBox::Direction::column;
    column.items.add(juce::FlexItem(nameLabel).withFlex(0.0f, 0.0f, 18.0f));
    column.items.add(juce::FlexItem(panSlider).withFlex(0.0f, 0.0f, 42.0f));
    column.items.add(juce::FlexItem(buttonRow_).withFlex(0.0f, 0.0f, 20.0f));
    column.items.add(juce::FlexItem(faderRow).withFlex(1.0f));
    column.performLayout(getLocalBounds().reduced(2));
    buttons.performLayout(buttonRow_.getLocalBounds());
}

// ---------------- MixerDeckPanel ----------------

MixerDeckPanel::MixerDeckPanel(StudioEngine& engine, AuthoringModel& model, AuditionTransport& transport)
    : engine_(engine), model_(model), transport_(transport)
{
    model_.add_listener(this);

    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    addAndMakeVisible(scaleSelector_);
    scaleSelector_.setComponentID("mixer_scale_selector");
    scaleSelector_.addItem("dBFS Nominal", 1);
    scaleSelector_.addItem("K-12", 2);
    scaleSelector_.addItem("K-14", 3);
    scaleSelector_.addItem("K-20", 4);
    scaleSelector_.addItem("EBU R128 LUFS", 5);
    scaleSelector_.setSelectedId(1, juce::dontSendNotification);
    scaleSelector_.onChange = [this]() {
        const int id = scaleSelector_.getSelectedId();
        if (id >= 1 && id <= 5) {
            set_meter_scale(static_cast<MeterScale>(id - 1));
        }
    };

    addAndMakeVisible(snapshotsLabel);
    snapshotsLabel.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
    snapshotsLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentPurple));

    addAndMakeVisible(snapshotsViewport_);
    snapshotsViewport_.setViewedComponent(&snapshotsContainer_, false);
    snapshotsViewport_.setScrollBarsShown(false, true);
    snapshotsViewport_.setScrollBarThickness(OaeTheme::Spacing::unit);
    addAndMakeVisible(stripsViewport_);
    stripsViewport_.setViewedComponent(&stripsContainer_, false);
    stripsViewport_.setScrollBarsShown(false, true);
    stripsViewport_.setScrollBarThickness(OaeTheme::Spacing::small);

    rebuild_strips();
    lastMeterUpdateMs_ = juce::Time::getMillisecondCounterHiRes();
    if (timersEnabled_) {
        startTimerHz(OaeTheme::Animation::meterRefreshHz);
    }
}

MixerDeckPanel::~MixerDeckPanel()
{
    stopTimer();
    model_.remove_listener(this);
}

void MixerDeckPanel::set_timers_enabled(bool enabled)
{
    timersEnabled_ = enabled;
    if (timersEnabled_) {
        lastMeterUpdateMs_ = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(OaeTheme::Animation::meterRefreshHz);
    } else {
        stopTimer();
    }
}

void MixerDeckPanel::on_project_changed()
{
    rebuild_strips();
}

void MixerDeckPanel::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto elapsedSeconds = static_cast<float>((nowMs - lastMeterUpdateMs_) / 1000.0);
    lastMeterUpdateMs_ = nowMs;
    for (auto& strip : strips_) {
        strip->update_meter(elapsedSeconds);
    }
}

void MixerDeckPanel::update_hierarchical_solo()
{
    soloed_buses_.clear();
    for (const auto& strip : strips_) {
        if (strip->get_kind() == MixerStripItem::StripKind::Bus && strip->is_soloed()) {
            soloed_buses_.insert(strip->get_path());
        }
    }

    for (auto& strip : strips_) {
        if (strip->get_kind() != MixerStripItem::StripKind::Bus) continue;

        if (soloed_buses_.empty()) {
            strip->set_effective_mute(strip->is_muted());
        } else {
            const std::string& p = strip->get_path();
            bool keep_audible = false;

            // Master bus is common ancestor of all
            if (p == "bus:/") {
                keep_audible = true;
            } else {
                for (const auto& soloed : soloed_buses_) {
                    if (p == soloed) {
                        keep_audible = true;
                        break;
                    }
                    // p is ancestor of soloed
                    if (soloed.rfind(p + "/", 0) == 0) {
                        keep_audible = true;
                        break;
                    }
                    // p is descendant of soloed
                    if (p.rfind(soloed + "/", 0) == 0) {
                        keep_audible = true;
                        break;
                    }
                }
            }

            strip->set_effective_mute(!keep_audible || strip->is_muted());
        }
    }
}

void MixerDeckPanel::rebuild_snapshot_toggles()
{
    snapshotButtons_.clear();
    const auto snapshot_paths = model_.get_snapshot_paths();

    for (const auto& path : snapshot_paths) {
        std::string short_name = path;
        if (short_name.rfind("snapshot:/", 0) == 0) short_name = short_name.substr(10);

        std::string safeName;
        safeName.reserve(short_name.size());
        for (unsigned char ch : short_name) {
            safeName.push_back((std::isalnum(ch) || ch == '_') ? static_cast<char>(ch) : '_');
        }
        if (safeName.empty()) safeName = "unnamed";

        auto btn = std::make_unique<juce::TextButton>(juce::String::fromUTF8(short_name.c_str()));
        btn->setComponentID("mixer_snapshot_" + safeName);
        btn->setClickingTogglesState(true);

        // Invariant: synchronize with IsSnapshotActive using setToggleState(..., dontSendNotification)
        const bool active = transport_.is_snapshot_active(path);
        btn->setToggleState(active, juce::dontSendNotification);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(active ? OaeTheme::Colors::accentPurple : OaeTheme::Colors::borderSubtle));
        btn->setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::textPrimary));

        btn->onClick = [this, path, b = btn.get()]() {
            const bool is_on = b->getToggleState();
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(is_on ? OaeTheme::Colors::accentPurple : OaeTheme::Colors::borderSubtle));
            transport_.toggle_snapshot(path, is_on);
        };

        snapshotsContainer_.addAndMakeVisible(*btn);
        snapshotButtons_.push_back(std::move(btn));
    }
}

void MixerDeckPanel::rebuild_strips()
{
    strips_.clear();

    auto on_solo_change = [this]() {
        update_hierarchical_solo();
    };

    // Master bus strip
    strips_.push_back(std::make_unique<MixerStripItem>(engine_, model_, transport_, "bus:/", MixerStripItem::StripKind::Bus, on_solo_change));

    // Child buses
    const auto bus_paths = model_.get_bus_paths();
    for (const auto& b : bus_paths) {
        if (b != "bus:/") {
            strips_.push_back(std::make_unique<MixerStripItem>(engine_, model_, transport_, b, MixerStripItem::StripKind::Bus, on_solo_change));
        }
    }

    // VCAs
    const auto vca_paths = model_.get_vca_paths();
    for (const auto& v : vca_paths) {
        strips_.push_back(std::make_unique<MixerStripItem>(engine_, model_, transport_, v, MixerStripItem::StripKind::Vca, on_solo_change));
    }

    for (auto& s : strips_) {
        s->set_meter_scale(current_scale_);
        stripsContainer_.addAndMakeVisible(*s);
    }

    rebuild_snapshot_toggles();
    update_hierarchical_solo();
    resized();
}

void MixerDeckPanel::set_meter_scale(MeterScale scale)
{
    current_scale_ = scale;
    const int target_id = static_cast<int>(scale) + 1;
    if (scaleSelector_.getSelectedId() != target_id) {
        scaleSelector_.setSelectedId(target_id, juce::dontSendNotification);
    }
    for (auto& strip : strips_) {
        strip->set_meter_scale(scale);
    }
}

void MixerDeckPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRect(getLocalBounds(), 1);
}

void MixerDeckPanel::resized()
{
    juce::FlexBox header;
    header.flexDirection = juce::FlexBox::Direction::row;
    header.items.add(juce::FlexItem(headerLabel).withFlex(0.0f, 0.0f, 85.0f));
    header.items.add(juce::FlexItem(scaleSelector_).withFlex(0.0f, 0.0f, 125.0f));
    header.items.add(juce::FlexItem(snapshotsLabel).withFlex(0.0f, 0.0f, 75.0f));
    header.items.add(juce::FlexItem(snapshotsViewport_).withFlex(1.0f));

    juce::FlexBox column;
    column.flexDirection = juce::FlexBox::Direction::column;
    column.items.add(juce::FlexItem(header).withHeight(28.0f));
    column.items.add(juce::FlexItem(stripsViewport_).withFlex(1.0f));
    column.performLayout(getLocalBounds().reduced(2));

    constexpr int snapshotWidth = 72;
    const int snapshotContentWidth = static_cast<int>(snapshotButtons_.size()) * snapshotWidth;
    snapshotsContainer_.setSize(juce::jmax(snapshotsViewport_.getWidth(), snapshotContentWidth), 24);
    juce::FlexBox snapshotRow;
    snapshotRow.flexDirection = juce::FlexBox::Direction::row;
    snapshotRow.items.ensureStorageAllocated(static_cast<int>(snapshotButtons_.size()));
    for (auto& btn : snapshotButtons_) {
        snapshotRow.items.add(juce::FlexItem(*btn).withWidth(snapshotWidth));
    }
    snapshotRow.performLayout(snapshotsContainer_.getLocalBounds().reduced(0, 2));

    constexpr int stripWidth = 72;
    const int stripContentWidth = static_cast<int>(strips_.size()) * stripWidth;
    const int stripViewportWidth = stripsViewport_.getWidth();
    const int scrollbarHeight = stripContentWidth > stripViewportWidth ? stripsViewport_.getScrollBarThickness() : 0;
    stripsContainer_.setSize(juce::jmax(stripViewportWidth, stripContentWidth),
                             juce::jmax(0, stripsViewport_.getHeight() - scrollbarHeight));

    juce::FlexBox stripRow;
    stripRow.flexDirection = juce::FlexBox::Direction::row;
    stripRow.items.ensureStorageAllocated(static_cast<int>(strips_.size()));
    for (auto& strip : strips_) {
        stripRow.items.add(juce::FlexItem(*strip).withFlex(0.0f, 0.0f,
                                                            static_cast<float>(stripWidth)));
    }
    stripRow.performLayout(stripsContainer_.getLocalBounds());
}

} // namespace oae::studio
