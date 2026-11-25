#pragma once

#include "OaeMeterComponent.h"
#include "OaeSlider.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <oae_audio.h>
#include "../StudioEngine.h"
#include "../Audio/AcousticMeterProcessor.h"
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace oae::studio {

/** Single vertical mixer channel strip for Master, Buses, or VCAs. */
class MixerStripItem : public juce::Component {
public:
    enum class StripKind {
        Bus,
        Vca
    };

    MixerStripItem(StudioEngine& engine,
                   AuthoringModel& model,
                   AuditionTransport& transport,
                   const std::string& path,
                   StripKind kind,
                   std::function<void()> on_solo_changed);
    ~MixerStripItem() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    const std::string& get_path() const noexcept { return path_; }
    StripKind get_kind() const noexcept { return kind_; }

    bool is_muted() const noexcept { return is_muted_; }
    bool is_soloed() const noexcept { return is_soloed_; }
    bool is_effective_muted() const noexcept { return effective_muted_; }
    void set_muted(bool mute);
    void set_soloed(bool solo);
    void set_effective_mute(bool mute);

    void set_volume_db(float db);
    float get_volume_db() const;
    void set_pan(float pan);
    float get_pan() const;

    const OaeMeterComponent& get_vu_meter() const noexcept { return vuMeter_; }
    void set_meter_scale(MeterScale scale) { vuMeter_.set_scale(scale); }

    void update_meter(float elapsedSeconds = 1.0f / OaeTheme::Animation::meterRefreshHz);

private:
    StudioEngine& engine_;
    AuthoringModel& model_;
    AuditionTransport& transport_;
    std::string path_;
    StripKind kind_;
    std::function<void()> on_solo_changed_;

    bool is_muted_{ false };
    bool is_soloed_{ false };
    bool effective_muted_{ false };

    juce::Label nameLabel;
    OaeSlider volumeSlider;
    OaeSlider panSlider;
    juce::Component buttonRow_;
    juce::TextButton muteButton{ "M" };
    juce::TextButton soloButton{ "S" };
    OaeMeterComponent vuMeter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerStripItem)
};

/** Mixer console hosting strips for Master, child buses, and VCAs, plus snapshot toggles. */
class MixerDeckPanel : public juce::Component, public juce::Timer, public AuthoringModel::Listener {
public:
    MixerDeckPanel(StudioEngine& engine, AuthoringModel& model, AuditionTransport& transport);
    ~MixerDeckPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    /** Enables or disables periodic VU meter refresh timers. */
    void set_timers_enabled(bool enabled);

    void rebuild_strips();
    void update_hierarchical_solo();
    void set_meter_scale(MeterScale scale);
    MeterScale get_meter_scale() const noexcept { return current_scale_; }

    const std::vector<std::unique_ptr<MixerStripItem>>& get_strips() const noexcept { return strips_; }
    MixerStripItem* find_strip(const std::string& path) const {
        for (const auto& s : strips_) {
            if (s->get_path() == path) return s.get();
        }
        return nullptr;
    }

    // AuthoringModel::Listener
    void on_project_changed() override;

private:
    void rebuild_snapshot_toggles();

    StudioEngine& engine_;
    AuthoringModel& model_;
    AuditionTransport& transport_;

    juce::Label headerLabel{ "header", "MIXER DECK" };
    juce::ComboBox scaleSelector_;
    double lastMeterUpdateMs_{ 0.0 };
    MeterScale current_scale_{ MeterScale::DbfsNominal };
    juce::Label snapshotsLabel{ "snapshots", "SNAPSHOTS:" };
    juce::Component snapshotsContainer_;
    juce::Viewport snapshotsViewport_;
    std::vector<std::unique_ptr<juce::TextButton>> snapshotButtons_;

    juce::Component stripsContainer_;
    juce::Viewport stripsViewport_;
    std::vector<std::unique_ptr<MixerStripItem>> strips_;
    std::unordered_set<std::string> soloed_buses_;
    bool timersEnabled_{ true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerDeckPanel)
};

} // namespace oae::studio
