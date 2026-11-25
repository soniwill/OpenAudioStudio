#pragma once

#include "OaePrimitives.h"
#include "OaeSlider.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include "../Model/AuthoringModel.h"
#include "../Timeline/WaveformCache.h"
#include "../Transport/AuditionTransport.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oae::studio {

/** Photoshop-inspired stacked layer view for adaptive stems, parameter masks, and adjustment layers. */
class LayerStackView : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       public AuthoringModel::Listener,
                       public AuditionTransport::Listener,
                       public juce::ChangeListener {
public:
    static constexpr int kLeftColumnWidth = 350;
    static constexpr int kColumnHeaderHeight = 22;
    static constexpr int kStemCardHeight = 92;
    static constexpr int kAdjustmentCardHeight = 46;
    static constexpr int kCardGap = 6;
    static constexpr int kRightColPadX = 6;
    static constexpr int kCanvasPadX = 6;
    static constexpr int kScrollbarThickness = 8;

    /** Visual canvas for editing a track's normalized [0..1] parameter activation mask and live cursor. */
    class MaskRangeCanvas : public juce::Component, public ITestableCustomControl {
    public:
        MaskRangeCanvas(LayerStackView& owner,
                        AuthoringModel& model,
                        const std::string& event_path,
                        size_t track_index,
                        std::function<void()> on_switch_to_timeline);

        void update_state(const LayerMaskSpec& mask,
                          juce::Colour track_colour,
                          double live_param_norm,
                          bool has_live_param);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        std::vector<TestableTarget> get_testable_targets() const override;

        std::function<void()> onMaskEdited;
        std::function<void()> onSelectRequested;

    private:
        enum class DragMode { None, LeftHandle, RightHandle, MoveRange };

        float norm_to_x(double norm) const noexcept;
        double x_to_norm(float x) const noexcept;

        LayerStackView& owner_;
        AuthoringModel& model_;
        std::string event_path_;
        size_t track_index_{ 0 };
        std::function<void()> on_switch_to_timeline_;

        LayerMaskSpec mask_;
        juce::Colour track_colour_{ OaeTheme::Colors::accentCyan };
        double live_param_norm_{ 0.0 };
        bool has_live_param_{ false };

        DragMode drag_mode_{ DragMode::None };
        double drag_start_min_{ 0.0 };
        double drag_start_max_{ 1.0 };
        float drag_start_x_{ 0.0f };

        juce::TextButton editTimelineButton_{ "Editar na Timeline" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaskRangeCanvas)
    };

    /** Bipartite horizontal card representing one audio track stem and its parameter activation mask. */
    class StemLayerCard : public juce::Component {
    public:
        StemLayerCard(LayerStackView& owner,
                      AuthoringModel& model,
                      WaveformCache& waveform_cache,
                      const std::string& event_path,
                      size_t track_index,
                      std::function<void()> on_switch_to_timeline);

        void refresh_from_model();
        void refresh_live_state();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;

        size_t get_track_index() const noexcept { return track_index_; }
        const LayerMaskSpec& get_mask_spec() const noexcept { return mask_; }

    private:
        /** Vector eye toggle button mapping layer visibility to track mute state. */
        class EyeToggleButton : public juce::Button {
        public:
            explicit EyeToggleButton(const juce::String& name);
            void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        };

        struct ParamChoice {
            std::string scope;
            std::string name;
        };

        void select_layer();
        void populate_parameter_combo();
        std::shared_ptr<const WaveformEnvelope> resolve_primary_waveform() const;

        LayerStackView& owner_;
        AuthoringModel& model_;
        WaveformCache& waveform_cache_;
        std::string event_path_;
        size_t track_index_{ 0 };
        int primary_inst_index_{ -1 };
        bool primary_loop_{ false };
        double primary_offset_ms_{ 0.0 };
        double primary_length_ms_{ 1000.0 };
        double primary_start_offset_ms_{ 0.0 };
        double primary_pitch_semitones_{ 0.0 };
        double primary_fade_in_ms_{ 0.0 };
        double primary_fade_out_ms_{ 0.0 };
        std::string primary_kind_;
        std::string primary_sample_id_;
        std::string primary_target_path_;

        LayerMaskSpec mask_;
        std::vector<ParamChoice> param_choices_;
        bool updating_controls_{ false };

        EyeToggleButton eyeButton_{ "Visibility" };
        juce::TextButton soloButton_{ "S" };
        juce::Label titleLabel_;
        OaeStatusBadge syncBadge_{ "1-SHOT", OaeStatusBadge::Variant::Neutral, false };
        juce::Label opacityLabel_{ "opacity", "Opacidade 100%" };
        OaeSlider opacitySlider_;

        juce::ComboBox paramCombo_;
        juce::Label fadeLabel_{ "fade", "Fade: 10%" };
        OaeSlider fadeSlider_;
        OaeStatusBadge statusBadge_{ "AGUARDANDO", OaeStatusBadge::Variant::Neutral, false };
        MaskRangeCanvas maskCanvas_;

        juce::Rectangle<float> waveformBounds_;
        mutable std::shared_ptr<const WaveformEnvelope> cachedEnvelope_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemLayerCard)
    };

    /** Top adjustment card representing parameter-conditioned FX on a track or event bus. */
    class AdjustmentLayerCard : public juce::Component {
    public:
        struct Info {
            ItemSelection target;
            std::string title;
            std::string summary;
            std::string param_scope;
            std::string param_name;
            size_t bus_index{ 0 };
            size_t effect_index{ 0 };
            bool bypassed{ false };
        };

        AdjustmentLayerCard(LayerStackView& owner, AuthoringModel& model, Info info);

        void refresh_live_state();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;

    private:
        LayerStackView& owner_;
        AuthoringModel& model_;
        Info info_;
        double live_param_norm_{ 0.0 };
        bool has_live_param_{ false };

        juce::TextButton bypassButton_{ "FX" };
        juce::Label titleLabel_;
        juce::Label statusLabel_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdjustmentLayerCard)
    };

    LayerStackView(AuthoringModel& model, WaveformCache& waveform_cache, AuditionTransport* transport = nullptr);
    ~LayerStackView() override;

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return event_path_; }

    void set_transport(AuditionTransport* transport);
    void refresh();

    void set_interacting(bool interacting) noexcept { is_interacting_ = interacting; }
    bool is_interacting() const noexcept { return is_interacting_; }

    /** Reads the normalized [0..1] live parameter value from AuditionTransport or model defaults. */
    double get_normalized_parameter_value(const std::string& scope, const std::string& name) const;

    size_t get_stem_card_count() const noexcept { return stem_cards_.size(); }
    size_t get_adjustment_card_count() const noexcept { return adjustment_cards_.size(); }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

    // AuditionTransport::Listener
    void on_parameter_changed(const std::string& name, float val, bool is_global) override;

    // juce::ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::function<void()> onSwitchToTimelineRequested;

private:
    void rebuild_cards();
    void layout_cards();
    void update_live_states();
    void collect_adjustment_infos(const nlohmann::json& root,
                                  const nlohmann::json& event_json,
                                  std::vector<AdjustmentLayerCard::Info>& out) const;

    AuthoringModel& model_;
    WaveformCache& waveform_cache_;
    AuditionTransport* transport_{ nullptr };
    std::string event_path_;
    bool is_interacting_{ false };
    bool is_file_drag_over_{ false };

    juce::Viewport viewport_;
    juce::Component contentContainer_;
    std::vector<std::unique_ptr<AdjustmentLayerCard>> adjustment_cards_;
    std::vector<std::unique_ptr<StemLayerCard>> stem_cards_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerStackView)
};

} // namespace oae::studio
