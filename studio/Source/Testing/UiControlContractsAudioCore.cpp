#include "UiControlContracts.h"
#include <cmath>

namespace oae::studio::testing {

void register_audio_core_contracts(std::vector<UiControlContract>& out)
{
    out.push_back({
        "audio_core.transport_bar",
        {
            "btn_play",
            "btn_pause",
            "btn_stop",
            "btn_loop",
            "btn_key_off",
            "btn_transport_params"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            auto* pauseBtn = ctx.find_as<juce::Button>("btn_pause");
            auto* stopBtn = ctx.find_as<juce::Button>("btn_stop");
            auto* loopBtn = ctx.find_as<juce::Button>("btn_loop");
            auto* keyOffBtn = ctx.find_as<juce::Button>("btn_key_off");
            auto* paramsBtn = ctx.find_as<juce::Button>("btn_transport_params");
            if (!playBtn || !pauseBtn || !stopBtn || !loopBtn || !keyOffBtn || !paramsBtn) {
                return ContractResult::fail(FailureCode::ViewMismatch, "One or more TransportBar buttons missing");
            }

            const bool loopBefore = ctx.mainComp.transport().is_loop_enabled();
            auto pf = UiInteractionDriver::click(*loopBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_loop");
            if (ctx.mainComp.transport().is_loop_enabled() == loopBefore) {
                return ContractResult::fail(FailureCode::ModelMismatch, "btn_loop did not toggle loop enabled state");
            }
            UiInteractionDriver::click(*loopBtn);

            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play");
            if (ctx.mainComp.transport().get_state() != TransportState::Playing ||
                ctx.mainComp.transport().get_active_instance() == 0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "btn_play did not start active playback instance");
            }
            const auto playMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (playMetrics.rmsDb < -35.0 || playMetrics.busMeterRms < 0.005f) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "btn_play produced silent output (rmsDb=" + std::to_string(playMetrics.rmsDb) + ")");
            }

            pf = UiInteractionDriver::click(*pauseBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_pause");
            if (ctx.mainComp.transport().get_state() != TransportState::Paused) {
                return ContractResult::fail(FailureCode::AudioMismatch, "btn_pause did not transition transport to Paused");
            }
            const auto pauseMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (pauseMetrics.rmsDb > -70.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "btn_pause did not silence audio (rmsDb=" + std::to_string(pauseMetrics.rmsDb) + ")");
            }

            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(resume)");
            if (ctx.mainComp.transport().get_state() != TransportState::Playing) {
                return ContractResult::fail(FailureCode::AudioMismatch, "btn_play did not resume from Paused");
            }
            const auto resumeMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (resumeMetrics.rmsDb < -35.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "Resumed playback produced silent output");
            }

            pf = UiInteractionDriver::click(*keyOffBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_key_off");

            pf = UiInteractionDriver::click(*stopBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_stop");
            if (ctx.mainComp.transport().get_state() != TransportState::Stopped) {
                return ContractResult::fail(FailureCode::AudioMismatch, "btn_stop did not transition transport to Stopped");
            }
            const auto stopMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (stopMetrics.rmsDb > -70.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "btn_stop did not silence audio (rmsDb=" + std::to_string(stopMetrics.rmsDb) + ")");
            }

            pf = UiInteractionDriver::click(*paramsBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_transport_params");
            if (ctx.mainComp.contextual_deck().get_active_tab() != DeckTab::Parameters) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_transport_params did not switch Bottom Deck to Parameters");
            }
            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Mixer);
            UiInteractionDriver::drain_messages(2);

            return ContractResult::ok();
        }
    });

    out.push_back({
        "audio_core.mixer_deck",
        {
            "mixer_fader_*",
            "mixer_pan_*",
            "mixer_mute_*",
            "mixer_solo_*",
            "mixer_scale_selector",
            "mixer_snapshot_*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* scaleCombo = ctx.find_as<juce::ComboBox>("mixer_scale_selector");
            if (scaleCombo == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_scale_selector not found");
            auto pf = UiInteractionDriver::select_combo_index(*scaleCombo, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_scale_selector");
            pf = UiInteractionDriver::select_combo_index(*scaleCombo, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_scale_selector");

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play");

            const auto baseline = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (baseline.rmsDb < -35.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "Baseline playback silent before mixer tests");
            }

            auto* sfxFader = ctx.find_as<juce::Slider>("mixer_fader_bus_sfx");
            if (sfxFader == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_fader_bus_sfx not found");
            pf = UiInteractionDriver::drag_slider(*sfxFader, -0.45);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_fader_bus_sfx");
            const auto attenuated = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (attenuated.rmsDb > baseline.rmsDb - 3.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging mixer_fader_bus_sfx down did not attenuate output (baseline=" +
                    std::to_string(baseline.rmsDb) + ", attenuated=" + std::to_string(attenuated.rmsDb) + ")");
            }
            pf = UiInteractionDriver::double_click(*sfxFader);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_fader_bus_sfx(double_click)");

            auto* sfxPan = ctx.find_as<juce::Slider>("mixer_pan_bus_sfx");
            if (sfxPan == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_pan_bus_sfx not found");
            pf = UiInteractionDriver::drag_slider(*sfxPan, -0.6);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_pan_bus_sfx(left)");
            const auto panLeft = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (panLeft.lrRatio < 1.35) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Panning mixer_pan_bus_sfx left did not shift L/R energy (lrRatio=" + std::to_string(panLeft.lrRatio) + ")");
            }
            pf = UiInteractionDriver::drag_slider(*sfxPan, 1.0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_pan_bus_sfx(right)");
            const auto panRight = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (panRight.lrRatio > 0.75) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Panning mixer_pan_bus_sfx right did not shift L/R energy (lrRatio=" + std::to_string(panRight.lrRatio) + ")");
            }
            pf = UiInteractionDriver::double_click(*sfxPan);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_pan_bus_sfx(reset)");

            auto* sfxMute = ctx.find_as<juce::Button>("mixer_mute_bus_sfx");
            if (sfxMute == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_mute_bus_sfx not found");
            pf = UiInteractionDriver::click(*sfxMute);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_mute_bus_sfx");
            const auto mutedMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (mutedMetrics.rmsDb > -70.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "mixer_mute_bus_sfx did not silence bus:/sfx output (rmsDb=" + std::to_string(mutedMetrics.rmsDb) + ")");
            }
            pf = UiInteractionDriver::click(*sfxMute);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_mute_bus_sfx(unmute)");

            auto* musicSolo = ctx.find_as<juce::Button>("mixer_solo_bus_music");
            if (musicSolo == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_solo_bus_music not found");
            pf = UiInteractionDriver::click(*musicSolo);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_solo_bus_music");
            const auto otherSoloMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (otherSoloMetrics.rmsDb > -70.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "mixer_solo_bus_music did not silence non-soloed bus:/sfx event (rmsDb=" +
                    std::to_string(otherSoloMetrics.rmsDb) + ")");
            }
            pf = UiInteractionDriver::click(*musicSolo);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_solo_bus_music(unsolo)");

            ctx.mainComp.transport().stop(true);
            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/nested", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(nested)");
            const auto musicBaseline = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (musicBaseline.rmsDb < -35.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "event:/test/nested baseline playback silent");
            }

            auto* snapBtn = ctx.find_as<juce::Button>("mixer_snapshot_duck");
            if (snapBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "mixer_snapshot_duck not found");
            pf = UiInteractionDriver::click(*snapBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_snapshot_duck(on)");
            const auto duckedMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (duckedMetrics.rmsDb > musicBaseline.rmsDb - 6.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "mixer_snapshot_duck did not attenuate bus:/music (baseline=" +
                    std::to_string(musicBaseline.rmsDb) + ", ducked=" + std::to_string(duckedMetrics.rmsDb) + ")");
            }
            pf = UiInteractionDriver::click(*snapBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "mixer_snapshot_duck(off)");
            const auto unduckedMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (unduckedMetrics.rmsDb < musicBaseline.rmsDb - 2.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "Disabling mixer_snapshot_duck did not restore bus:/music level");
            }

            ctx.mainComp.transport().stop(true);
            return ContractResult::ok();
        }
    });

    out.push_back({
        "audio_core.track_headers",
        {
            "timeline.track_headers",
            "timeline.add_track",
            "track_header_*",
            "track_color_swatch_*",
            "track_volume_*",
            "track_pan_*",
            "track_mute_*",
            "track_solo_*",
            "track_auto_*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            auto pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play");

            const auto baseSpec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (baseSpec.energy440 < 1e-4 || baseSpec.energy880 < 1e-4) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Baseline event:/test/main missing expected 440Hz/880Hz spectral energy");
            }

            auto* mute0 = ctx.find_as<juce::Button>("track_mute_0");
            if (mute0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_mute_0 not found");
            pf = UiInteractionDriver::click(*mute0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_mute_0");
            const auto muted0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (muted0Spec.energy440 > baseSpec.energy440 * 0.05 || muted0Spec.energy880 < baseSpec.energy880 * 0.5) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "track_mute_0 did not isolate 440Hz track attenuation while keeping 880Hz track active");
            }
            pf = UiInteractionDriver::click(*mute0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_mute_0(unmute)");

            auto* solo0 = ctx.find_as<juce::Button>("track_solo_0");
            if (solo0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_solo_0 not found");
            pf = UiInteractionDriver::click(*solo0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_solo_0");
            const auto solo0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (solo0Spec.energy880 > baseSpec.energy880 * 0.05 || solo0Spec.energy440 < baseSpec.energy440 * 0.5) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "track_solo_0 did not silence 880Hz track while preserving 440Hz track");
            }

            auto* vol0 = ctx.find_as<juce::Slider>("track_volume_0");
            if (vol0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_volume_0 not found");
            pf = UiInteractionDriver::drag_slider(*vol0, -0.4);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_volume_0");
            const auto atten0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (atten0Spec.rmsDb > solo0Spec.rmsDb - 3.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "Dragging track_volume_0 down did not attenuate track 0");
            }
            pf = UiInteractionDriver::double_click(*vol0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_volume_0(reset)");

            auto* pan0 = ctx.find_as<juce::Slider>("track_pan_0");
            if (pan0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_pan_0 not found");
            pf = UiInteractionDriver::drag_slider(*pan0, -0.6);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_pan_0");
            const auto pan0Spec = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (pan0Spec.lrRatio < 1.35) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging track_pan_0 left did not shift stereo balance (lrRatio=" + std::to_string(pan0Spec.lrRatio) + ")");
            }
            pf = UiInteractionDriver::double_click(*pan0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_pan_0(reset)");

            pf = UiInteractionDriver::click(*solo0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_solo_0(unsolo)");
            ctx.mainComp.transport().stop(true);

            auto* auto0 = ctx.find_as<juce::Button>("track_auto_0");
            if (auto0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_auto_0 not found");
            pf = UiInteractionDriver::click(*auto0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_auto_0(expand)");
            if (ctx.find("track_lane_0.automation") == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "track_auto_0 did not reveal track_lane_0.automation");
            }
            pf = UiInteractionDriver::click(*auto0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_auto_0(collapse)");

            auto* swatch0 = ctx.find_as<juce::Button>("track_color_swatch_0");
            if (swatch0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_color_swatch_0 not found");
            UiTestDialogSeam::enqueue_popup_result(2);
            pf = UiInteractionDriver::click(*swatch0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_color_swatch_0");
            if (ctx.mainComp.authoring_model().get_track_color("event:/test/main", 0).empty()) {
                return ContractResult::fail(FailureCode::ModelMismatch, "track_color_swatch_0 did not set track color via seam");
            }
            if (!ctx.mainComp.authoring_model().undo() || !ctx.mainComp.authoring_model().redo()) {
                return ContractResult::fail(FailureCode::UndoMismatch, "track_color_swatch_0 undo/redo failed");
            }

            auto* header0 = ctx.find("track_header_0");
            if (header0 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "track_header_0 not found");
            pf = UiInteractionDriver::click_target(*header0, "body");
            if (!pf.ok) return ContractResult::from_preflight(pf, "track_header_0#body");
            if (ctx.mainComp.authoring_model().get_selection().track_index != 0) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Clicking track_header_0#body did not select track 0");
            }

            auto* addTrackBtn = ctx.find_as<juce::Button>("timeline.add_track");
            if (addTrackBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "timeline.add_track not found");
            const size_t countBefore = ctx.mainComp.authoring_model().get_track_count("event:/test/main");
            pf = UiInteractionDriver::click(*addTrackBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "timeline.add_track");
            if (ctx.mainComp.authoring_model().get_track_count("event:/test/main") != countBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "timeline.add_track did not append a track");
            }
            if (!ctx.mainComp.authoring_model().undo() ||
                ctx.mainComp.authoring_model().get_track_count("event:/test/main") != countBefore) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after timeline.add_track failed");
            }

            return ContractResult::ok();
        }
    });
}

} // namespace oae::studio::testing
