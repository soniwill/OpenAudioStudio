#pragma once

#include "OaePrimitives.h"
#include "OaeSlider.h"
#include "Theme.h"
#include "VectorIcons.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>

namespace oae::studio {

/** Rich interactive card representing a single global parameter with specialized controls and telemetry. */
class GlobalParamCard : public juce::Component {
public:
    GlobalParamCard(const nlohmann::json& spec,
                    AuditionTransport& transport,
                    const juce::String& subtitle = {},
                    const juce::String& footerText = {},
                    juce::Colour accentColour = juce::Colour(OaeTheme::Colors::accentCyan));
    ~GlobalParamCard() override = default;

    const std::string& get_name() const noexcept { return name_; }
    const std::string& get_kind() const noexcept { return kind_; }
    double get_default_value() const noexcept { return defaultValue_; }
    double get_current_value() const noexcept { return currentValue_; }

    bool is_modified() const noexcept;
    void reset_to_default();
    void set_selected(bool selected);
    bool is_selected() const noexcept { return selected_; }

    void update_telemetry(float currentVal, bool isPlaying);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    OaeSlider* get_slider() noexcept { return slider_.get(); }
    const std::vector<juce::TextButton*>& get_option_buttons() const noexcept { return optionButtonPtrs_; }

    bool has_velocity() const noexcept { return velocity_ != 0.0; }
    bool has_seek_speed() const noexcept { return seekSpeed_ > 0.0 || seekSpeedDown_ > 0.0; }
    bool has_hold() const noexcept { return holdDuringPlayback_; }

    std::function<void(GlobalParamCard&)> onCardSelected;

    juce::Label nameLabel;
    juce::Label subtitleLabel;
    juce::Label valueLabel;
    juce::Label footerLabel;
    OaeStatusBadge statusBadge{ "DEFAULT", OaeStatusBadge::Variant::Amber, false };
    juce::Label kindBadge;
    juce::Label velBadge;
    juce::Label seekBadge;
    juce::Label holdBadge;
    juce::TextButton resetBtn{ "Reset" };

private:
    void refresh_status_and_value();

    AuditionTransport& transport_;
    std::string name_;
    std::string kind_{ "continuous" };
    juce::Colour accentColour_{ OaeTheme::Colors::accentCyan };
    bool selected_{ false };
    double minValue_{ 0.0 };
    double maxValue_{ 1.0 };
    double defaultValue_{ 0.0 };
    double currentValue_{ 0.0 };
    double velocity_{ 0.0 };
    double seekSpeed_{ 0.0 };
    double seekSpeedDown_{ 0.0 };
    bool holdDuringPlayback_{ false };
    std::vector<std::string> labels_;

    std::unique_ptr<OaeSlider> slider_;
    std::unique_ptr<OaeSegmentedControl> segmented_;
    std::unique_ptr<juce::TextButton> decBtn_;
    std::unique_ptr<juce::TextButton> incBtn_;
    std::vector<juce::TextButton*> optionButtonPtrs_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalParamCard)
};

/** Dedicated Global Parameters simulation deck featuring card grid, search, filters, and telemetry. */
class GlobalParametersDeck : public juce::Component,
                             public juce::Timer,
                             public AuditionTransport::Listener,
                             public AuthoringModel::Listener {
public:
    enum class FilterMode {
        All,
        UsedInEvent,
        Modified
    };

    GlobalParametersDeck(AuthoringModel& model, AuditionTransport& transport);
    ~GlobalParametersDeck() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

    void timerCallback() override;

    /** Enables or disables periodic parameter telemetry polling timers. */
    void set_timers_enabled(bool enabled);

    // AuditionTransport::Listener
    void on_parameter_changed(const std::string& name, float val, bool is_global) override;
    void on_transport_state_changed(TransportState state) override;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

    void refresh();
    void apply_filters();
    void reset_all_to_default();

    void set_floating(bool floating);
    bool is_floating() const noexcept { return isFloating_; }

    size_t get_card_count() const noexcept { return cards_.size(); }
    GlobalParamCard* find_card(const std::string& name) const;
    bool is_card_visible(const std::string& name) const;

    std::function<void()> onPopOutRequested;

    juce::Label deckTitleLabel{ "deck_title", "GLOBAL PARAMETERS DECK" };
    juce::TextEditor searchEditor;
    OaeToolbarButton filterAllBtn{ "All (0)", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton filterUsedBtn{ "Used in Event (0)", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton filterModifiedBtn{ "Modified (0)", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton resetAllBtn{ "Reset All", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton popOutBtn{ "Dock Back", OaeToolbarButton::Variant::TintCyan };

private:
    std::unordered_set<std::string> get_event_used_global_parameters() const;

    AuthoringModel& model_;
    AuditionTransport& transport_;
    FilterMode filterMode_{ FilterMode::All };
    bool isFloating_{ false };
    bool timersEnabled_{ true };

    juce::Viewport viewport_;
    juce::Component cardsContainer_;
    std::vector<std::unique_ptr<GlobalParamCard>> cards_;
    std::vector<GlobalParamCard*> visibleCards_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalParametersDeck)
};

} // namespace oae::studio
