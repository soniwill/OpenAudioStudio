#include "UiControlContracts.h"
#include <cmath>

namespace oae::studio::testing {

void register_timeline_layers_contracts(std::vector<UiControlContract>& out)
{
    out.push_back({
        "timeline.toolbar_ruler_minimap",
        {
            "timeline.snap_combo",
            "timeline.time_format",
            "timeline.zoom_out",
            "timeline.zoom_slider",
            "timeline.zoom_in",
            "timeline.h_scroll",
            "timeline.v_scroll",
            "timeline.mode.*",
            "timeline.breadcrumb_back",
            "timeline.breadcrumb.*",
            "minimap_view",
            "timeline.ruler"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* snapCombo = ctx.find_as<juce::ComboBox>("timeline.snap_combo");
            if (snapCombo == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.snap_combo not found");
            auto pf = UiInteractionDriver::select_combo_index(*snapCombo, 2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.snap_combo");
            pf = UiInteractionDriver::select_combo_index(*snapCombo, 7);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.snap_combo");

            auto* timeFmtBtn = ctx.find_as<juce::Button>("timeline.time_format");
            if (timeFmtBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.time_format not found");
            const auto fmtBefore = timeFmtBtn->getButtonText();
            pf = UiInteractionDriver::click(*timeFmtBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.time_format");
            if (timeFmtBtn->getButtonText() == fmtBefore) {
                return ContractResult::fail(FailureCode::ViewMismatch, "timeline.time_format did not toggle time format label");
            }
            UiInteractionDriver::click(*timeFmtBtn);

            auto* zoomSlider = ctx.find_as<juce::Slider>("timeline.zoom_slider");
            auto* zoomIn = ctx.find_as<juce::Button>("timeline.zoom_in");
            auto* zoomOut = ctx.find_as<juce::Button>("timeline.zoom_out");
            if (!zoomSlider || !zoomIn || !zoomOut) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Timeline zoom controls missing");
            }
            const double zoomBase = zoomSlider->getValue();
            pf = UiInteractionDriver::click(*zoomIn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.zoom_in");
            if (zoomSlider->getValue() <= zoomBase) {
                return ContractResult::fail(FailureCode::ViewMismatch, "timeline.zoom_in did not increase zoom");
            }
            pf = UiInteractionDriver::click(*zoomOut);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.zoom_out");
            pf = UiInteractionDriver::drag_slider(*zoomSlider, 0.15);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.zoom_slider");
            zoomSlider->setValue(zoomBase, juce::sendNotificationSync);
            UiInteractionDriver::drain_messages(2);

            auto* minimap = ctx.find("minimap_view");
            if (minimap == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "minimap_view not found");
            pf = UiInteractionDriver::drag_target(*minimap, "viewport", juce::Point<int>(20, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "minimap_view#viewport");
            pf = UiInteractionDriver::drag_target(*minimap, "viewport", juce::Point<int>(-20, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "minimap_view#viewport(back)");

            auto* ruler = ctx.find("timeline.ruler");
            if (ruler == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.ruler not found");
            pf = UiInteractionDriver::drag_target(*ruler, "scrub", juce::Point<int>(45, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.ruler#scrub");

            const auto* logicBefore = ctx.get_event_logic_items("event:/test/main");
            if (!logicBefore || logicBefore->size() <= 2) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Expected logic items on event:/test/main");
            }
            const double sustainPosBefore = (*logicBefore)[2].value("position_ms", 2100.0);
            pf = UiInteractionDriver::drag_target(*ruler, "logic_2", juce::Point<int>(35, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.ruler#logic_2");
            const double sustainPosAfter = (*ctx.get_event_logic_items("event:/test/main"))[2].value("position_ms", 2100.0);
            if (std::abs(sustainPosAfter - sustainPosBefore) < 1.0) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging timeline.ruler#logic_2 did not move sustain_point");
            }
            if (!ctx.mainComp.authoring_model().undo() ||
                std::abs((*ctx.get_event_logic_items("event:/test/main"))[2].value("position_ms", 0.0) - sustainPosBefore) > 0.5) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after timeline.ruler#logic_2 drag failed");
            }

            return ContractResult::ok();
        }
    });

    out.push_back({
        "timeline.clips_and_automation_lane",
        {
            "track_lane_*.clip_*",
            "track_lane_*.automation"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* clip0 = ctx.find("track_lane_0.clip_0");
            if (clip0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_lane_0.clip_0 not found");

            const auto* tracks0 = ctx.get_event_tracks("event:/test/main");
            const double lenBefore = (*tracks0)[0]["instruments"][0].value("length_ms", 2000.0);
            auto pf = UiInteractionDriver::drag_target(*clip0, "resize_right", juce::Point<int>(-40, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_lane_0.clip_0#resize_right");
            const double lenAfter = (*ctx.get_event_tracks("event:/test/main"))[0]["instruments"][0].value("length_ms", 2000.0);
            if (std::abs(lenAfter - lenBefore) < 1.0) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging clip resize_right did not change length_ms");
            }
            if (!ctx.mainComp.authoring_model().undo() ||
                std::abs((*ctx.get_event_tracks("event:/test/main"))[0]["instruments"][0].value("length_ms", 0.0) - lenBefore) > 0.5) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after clip resize_right failed");
            }
            if (!ctx.mainComp.authoring_model().redo()) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Redo after clip resize_right failed");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            clip0 = ctx.find("track_lane_0.clip_0");
            if (clip0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_lane_0.clip_0 missing after undo");
            pf = UiInteractionDriver::drag_target(*clip0, "fade_in", juce::Point<int>(25, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_lane_0.clip_0#fade_in");
            const double fadeInAfter = (*ctx.get_event_tracks("event:/test/main"))[0]["instruments"][0].value("fade_in_ms", 0.0);
            if (fadeInAfter <= 0.5) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging clip fade_in did not increase fade_in_ms");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* eventClip = ctx.find("track_lane_2.clip_1");
            if (eventClip == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_lane_2.clip_1 (event instrument) not found");
            pf = UiInteractionDriver::double_click_target(*eventClip, "body");
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_lane_2.clip_1#body(double_click)");
            eventClip = nullptr;
            auto* backBtn = ctx.find_as<juce::Button>("timeline.breadcrumb_back");
            if (backBtn == nullptr || !backBtn->isEnabled()) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Double-clicking event instrument clip did not enable timeline.breadcrumb_back");
            }
            pf = UiInteractionDriver::click(*backBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.breadcrumb_back");

            auto* autoBtn0 = ctx.find_as<juce::Button>("track_auto_0");
            if (autoBtn0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_auto_0 not found");
            pf = UiInteractionDriver::click(*autoBtn0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_auto_0");

            auto* autoLane = ctx.find("track_lane_0.automation");
            if (autoLane == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_lane_0.automation not found after expanding");
            const size_t ptsBefore = ctx.mainComp.authoring_model().get_track_timeline_volume_points("event:/test/main", 0).size();
            pf = UiInteractionDriver::double_click_target(*autoLane, "canvas");
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_lane_0.automation#canvas");
            const size_t ptsAfterAdd = ctx.mainComp.authoring_model().get_track_timeline_volume_points("event:/test/main", 0).size();
            if (ptsAfterAdd != ptsBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Double-clicking automation lane canvas did not insert point");
            }
            if (!ctx.mainComp.authoring_model().undo() ||
                ctx.mainComp.authoring_model().get_track_timeline_volume_points("event:/test/main", 0).size() != ptsBefore) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after automation point insertion failed");
            }

            autoLane = ctx.find("track_lane_0.automation");
            const double pt1YBefore = ctx.mainComp.authoring_model().get_track_timeline_volume_points("event:/test/main", 0)[1].value("y", -1.0);
            pf = UiInteractionDriver::drag_target(*autoLane, "point_1", juce::Point<int>(0, 14));
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_lane_0.automation#point_1");
            const double pt1YAfter = ctx.mainComp.authoring_model().get_track_timeline_volume_points("event:/test/main", 0)[1].value("y", -1.0);
            if (std::abs(pt1YAfter - pt1YBefore) < 0.1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging automation point_1 did not change y value");
            }
            ctx.mainComp.authoring_model().undo();

            UiInteractionDriver::click(*autoBtn0);
            return ContractResult::ok();
        }
    });

    out.push_back({
        "timeline.parameter_strip",
        {
            "btn_timeline_params_toggle",
            "timeline_param.*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            auto pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play");

            const auto baseAudio = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            auto* intensitySlider = ctx.find_as<juce::Slider>("timeline_param.global.Intensity.slider");
            if (intensitySlider == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "timeline_param.global.Intensity.slider not found");
            }
            pf = UiInteractionDriver::drag_slider(*intensitySlider, -0.55);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline_param.global.Intensity.slider");
            const auto lowIntensityAudio = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (lowIntensityAudio.rmsDb > baseAudio.rmsDb - 5.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging timeline_param.global.Intensity.slider down did not attenuate automated event volume (base=" +
                    std::to_string(baseAudio.rmsDb) + ", low=" + std::to_string(lowIntensityAudio.rmsDb) + ")");
            }
            pf = UiInteractionDriver::double_click(*intensitySlider);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline_param.global.Intensity.slider(reset)");
            ctx.mainComp.transport().stop(true);

            auto* localModeBadge = ctx.find_as<juce::Button>("timeline_param.local.LocalMode.badge");
            if (localModeBadge == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "timeline_param.local.LocalMode.badge not found");
            }
            const auto badgeTextBefore = localModeBadge->getButtonText();
            pf = UiInteractionDriver::click(*localModeBadge);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline_param.local.LocalMode.badge");
            if (localModeBadge->getButtonText() == badgeTextBefore) {
                return ContractResult::fail(FailureCode::ViewMismatch, "timeline_param.local.LocalMode.badge did not toggle label");
            }

            auto* toggleBtn = ctx.find_as<juce::Button>("btn_timeline_params_toggle");
            if (toggleBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_timeline_params_toggle not found");
            pf = UiInteractionDriver::click(*toggleBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_timeline_params_toggle(collapse)");
            pf = UiInteractionDriver::click(*toggleBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_timeline_params_toggle(expand)");

            return ContractResult::ok();
        }
    });

    out.push_back({
        "timeline.layer_stack_view",
        {
            "timeline.layers.add_stem",
            "layers.card.*",
            "layers.adjustment.*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* layersModeBtn = ctx.find_as<juce::Button>("timeline.mode.layers");
            if (layersModeBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.mode.layers not found");
            auto pf = UiInteractionDriver::click(*layersModeBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.mode.layers");

            auto* addStemBtn = ctx.find_as<juce::Button>("timeline.layers.add_stem");
            if (addStemBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.layers.add_stem not found");
            const size_t tracksBefore = ctx.get_event_tracks("event:/test/main")->size();
            pf = UiInteractionDriver::click(*addStemBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.layers.add_stem");
            if (ctx.get_event_tracks("event:/test/main")->size() != tracksBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "timeline.layers.add_stem did not add a stem layer track");
            }
            if (!ctx.mainComp.authoring_model().undo()) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after timeline.layers.add_stem failed");
            }
            UiInteractionDriver::drain_messages(2);

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play");
            const auto baseSpec = HeadlessAudioProbe::capture(ctx.mainComp.engine());

            auto* eye0 = ctx.find_as<juce::Button>("layers.card.0.eye");
            if (eye0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.card.0.eye not found");
            pf = UiInteractionDriver::click(*eye0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.eye(mute)");
            const auto muted0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (muted0Spec.energy440 > baseSpec.energy440 * 0.05) {
                return ContractResult::fail(FailureCode::AudioMismatch, "layers.card.0.eye did not mute Track 0 (440Hz)");
            }
            pf = UiInteractionDriver::click(*eye0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.eye(unmute)");

            auto* solo0 = ctx.find_as<juce::Button>("layers.card.0.solo");
            if (solo0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.card.0.solo not found");
            pf = UiInteractionDriver::click(*solo0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.solo");
            const auto solo0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (solo0Spec.energy880 > baseSpec.energy880 * 0.05) {
                return ContractResult::fail(FailureCode::AudioMismatch, "layers.card.0.solo did not isolate Track 0");
            }
            pf = UiInteractionDriver::click(*solo0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.solo(unsolo)");
            ctx.mainComp.transport().stop(true);

            auto* opacity0 = ctx.find_as<juce::Slider>("layers.card.0.opacity");
            if (opacity0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.card.0.opacity not found");
            pf = UiInteractionDriver::drag_slider(*opacity0, -0.35);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.opacity");
            if (std::abs((*ctx.get_event_tracks("event:/test/main"))[0].value("volume_db", 0.0)) < 0.5) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging layers.card.0.opacity did not update track volume_db");
            }
            ctx.mainComp.authoring_model().undo();

            auto* fade0 = ctx.find_as<juce::Slider>("layers.card.0.fade");
            if (fade0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.card.0.fade not found");
            pf = UiInteractionDriver::drag_slider(*fade0, 0.25);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.fade");
            ctx.mainComp.authoring_model().undo();

            auto* maskCanvas0 = ctx.find("layers.card.0.mask_canvas");
            if (maskCanvas0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.card.0.mask_canvas not found");
            const double minBefore = ctx.mainComp.authoring_model().get_track_layer_mask("event:/test/main", 0).min_norm;
            pf = UiInteractionDriver::drag_target(*maskCanvas0, "handle_min", juce::Point<int>(35, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.card.0.mask_canvas#handle_min");
            const double minAfter = ctx.mainComp.authoring_model().get_track_layer_mask("event:/test/main", 0).min_norm;
            if (minAfter <= minBefore + 0.02) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging layers.card.0.mask_canvas#handle_min did not increase min_norm");
            }
            if (!ctx.mainComp.authoring_model().undo()) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after mask_canvas handle_min drag failed");
            }

            auto* busFxBypass = ctx.find_as<juce::Button>("layers.adjustment.bus.0.1.bypass");
            if (busFxBypass == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.adjustment.bus.0.1.bypass not found");
            pf = UiInteractionDriver::click(*busFxBypass);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.adjustment.bus.0.1.bypass");
            const auto* busFx = ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" });
            if (!busFx || (*busFx).size() < 2 || !(*busFx)[1].value("bypass", false)) {
                return ContractResult::fail(FailureCode::ModelMismatch, "layers.adjustment.bus.0.1.bypass did not toggle bus effect bypass");
            }
            ctx.mainComp.authoring_model().undo();

            auto* trkFxBypass = ctx.find_as<juce::Button>("layers.adjustment.track.0.0.bypass");
            if (trkFxBypass == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "layers.adjustment.track.0.0.bypass not found");
            pf = UiInteractionDriver::click(*trkFxBypass);
            if (!pf.ok) return ContractResult::from_preflight(pf, "layers.adjustment.track.0.0.bypass");
            const auto* trkFx = ctx.mainComp.authoring_model().get_effects({ ItemType::Event, "event:/test/main", 0 });
            if (!trkFx || !(*trkFx)[0].value("bypass", false)) {
                return ContractResult::fail(FailureCode::ModelMismatch, "layers.adjustment.track.0.0.bypass did not toggle track effect bypass");
            }
            ctx.mainComp.authoring_model().undo();

            auto* timelineModeBtn = ctx.find_as<juce::Button>("timeline.mode.timeline");
            if (timelineModeBtn != nullptr) {
                UiInteractionDriver::click(*timelineModeBtn);
            }
            return ContractResult::ok();
        }
    });
}

} // namespace oae::studio::testing
