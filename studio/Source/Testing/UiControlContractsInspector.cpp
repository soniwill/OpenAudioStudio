#include "UiControlContracts.h"
#include <cmath>

namespace oae::studio::testing {

void register_inspector_contracts(std::vector<UiControlContract>& out)
{
    out.push_back({
        "inspector.headers_and_event_card",
        {
            "inspector_lock_button",
            "inspector_card_*.header",
            "inspector_card_*.more_actions",
            "inspector_volume",
            "inspector_pitch",
            "inspector_pan",
            "inspector_max_inst",
            "inspector.event.*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* cardHeader = ctx.find("inspector_card_event.header");
            if (cardHeader == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_card_event.header not found");
            auto pf = UiInteractionDriver::click_target(*cardHeader, "collapse");
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_card_event.header#collapse");
            if (ctx.find("inspector_volume") != nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Collapsing inspector_card_event.header did not hide card body");
            }
            cardHeader = ctx.find("inspector_card_event.header");
            if (cardHeader == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_card_event.header not found after collapse");
            pf = UiInteractionDriver::click_target(*cardHeader, "collapse");
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_card_event.header#collapse(expand)");
            if (ctx.find("inspector_volume") == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Expanding inspector_card_event.header did not show card body");
            }

            auto* moreBtn = ctx.find_as<juce::Button>("inspector_card_event.more_actions");
            if (moreBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_card_event.more_actions not found");
            UiTestDialogSeam::enqueue_popup_result(2);
            pf = UiInteractionDriver::click(*moreBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_card_event.more_actions");

            auto* volSlider = ctx.find_as<juce::Slider>("inspector_volume");
            auto* pitchSlider = ctx.find_as<juce::Slider>("inspector_pitch");
            auto* panSlider = ctx.find_as<juce::Slider>("inspector_pan");
            auto* maxInstSlider = ctx.find_as<juce::Slider>("inspector_max_inst");
            if (!volSlider || !pitchSlider || !panSlider || !maxInstSlider) {
                return ContractResult::fail(FailureCode::ViewMismatch, "EventHeaderCard core sliders missing");
            }
            pf = UiInteractionDriver::drag_slider(*volSlider, -0.25);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_volume");
            if (!ctx.mainComp.authoring_model().undo() || !ctx.mainComp.authoring_model().redo()) {
                return ContractResult::fail(FailureCode::UndoMismatch, "inspector_volume undo/redo failed");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            pitchSlider = ctx.find_as<juce::Slider>("inspector_pitch");
            pf = UiInteractionDriver::drag_slider(*pitchSlider, 0.25);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_pitch");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            panSlider = ctx.find_as<juce::Slider>("inspector_pan");
            pf = UiInteractionDriver::drag_slider(*panSlider, -0.3);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_pan");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            maxInstSlider = ctx.find_as<juce::Slider>("inspector_max_inst");
            pf = UiInteractionDriver::drag_slider(*maxInstSlider, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_max_inst");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* busCombo = ctx.find_as<juce::ComboBox>("inspector.event.bus");
            auto* stealCombo = ctx.find_as<juce::ComboBox>("inspector.event.stealing");
            if (!busCombo || !stealCombo) return ContractResult::fail(FailureCode::ViewMismatch, "EventHeaderCard combos missing");
            pf = UiInteractionDriver::select_combo_index(*stealCombo, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.stealing");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* notesEd = ctx.find_as<juce::TextEditor>("inspector.event.notes");
            if (notesEd == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.notes not found");
            pf = UiInteractionDriver::type_text(*notesEd, "Updated QA notes", true, false);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.notes");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* newKey = ctx.find_as<juce::TextEditor>("inspector.event.user_prop.new_key");
            if (!newKey) {
                return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.user_prop.new_key missing");
            }
            pf = UiInteractionDriver::type_text(*newKey, "qa_key", false, false);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.user_prop.new_key");
            auto* newVal = ctx.find_as<juce::TextEditor>("inspector.event.user_prop.new_value");
            if (!newVal) {
                return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.user_prop.new_value missing");
            }
            pf = UiInteractionDriver::type_text(*newVal, "qa_val", false, false);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.user_prop.new_value");
            auto* addProp = ctx.find_as<juce::Button>("inspector.event.user_prop.add");
            if (!addProp) {
                return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.user_prop.add missing");
            }
            pf = UiInteractionDriver::click(*addProp);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.user_prop.add");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* ahdsrEd = ctx.find("inspector.event.mod.1.ahdsr");
            if (ahdsrEd == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.mod.1.ahdsr not found");
            pf = UiInteractionDriver::drag_target(*ahdsrEd, "node_decay", juce::Point<int>(18, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.mod.1.ahdsr#node_decay");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* autoCurve = ctx.find("inspector.event.auto.0.curve");
            if (autoCurve == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.event.auto.0.curve not found");
            pf = UiInteractionDriver::drag_target(*autoCurve, "point_1", juce::Point<int>(0, 14));
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.event.auto.0.curve#point_1");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            return ContractResult::ok();
        }
    });

    out.push_back({
        "inspector.track_and_clip_cards",
        {
            "inspector_track_*",
            "inspector.track.*",
            "inspector_instrument_*",
            "inspector_command_type",
            "inspector.clip.*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 0, 0, -1 });
            UiInteractionDriver::drain_messages(2);

            auto* trkHeader = ctx.find("inspector_card_track.header");
            if (trkHeader != nullptr && ctx.find("inspector_track_name") == nullptr) {
                auto pfExpand = UiInteractionDriver::click_target(*trkHeader, "collapse");
                if (!pfExpand.ok) return ContractResult::from_preflight(pfExpand, "inspector_card_track.header#collapse");
            }

            auto* trkName = ctx.find_as<juce::TextEditor>("inspector_track_name");
            if (trkName == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_track_name not found");
            auto pf = UiInteractionDriver::type_text(*trkName, "Renamed Track 0", true, false);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_track_name");
            if (ctx.mainComp.authoring_model().get_track_name("event:/test/main", 0) != "Renamed Track 0") {
                return ContractResult::fail(FailureCode::ModelMismatch, "inspector_track_name did not update track name");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* colorSwatch2 = ctx.find_as<juce::Button>("inspector_track_color_2");
            if (colorSwatch2 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_track_color_2 not found");
            pf = UiInteractionDriver::click(*colorSwatch2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_track_color_2");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* trkVol = ctx.find_as<juce::Slider>("inspector_track_volume");
            auto* trkPan = ctx.find_as<juce::Slider>("inspector_track_pan");
            auto* trkMute = ctx.find_as<juce::Button>("inspector_track_mute");
            auto* trkSolo = ctx.find_as<juce::Button>("inspector_track_solo");
            if (!trkVol || !trkPan || !trkMute || !trkSolo) {
                return ContractResult::fail(FailureCode::ViewMismatch, "TrackCard controls missing");
            }
            pf = UiInteractionDriver::drag_slider(*trkVol, -0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_track_volume");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            trkMute = ctx.find_as<juce::Button>("inspector_track_mute");
            pf = UiInteractionDriver::click(*trkMute);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_track_mute");
            if (!ctx.mainComp.authoring_model().is_track_muted("event:/test/main", 0)) {
                return ContractResult::fail(FailureCode::ModelMismatch, "inspector_track_mute did not mute track 0");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            auto* clipSample = ctx.find_as<juce::ComboBox>("inspector.clip.sample");
            auto* clipPitch = ctx.find_as<juce::Slider>("inspector.clip.pitch");
            auto* clipLoop = ctx.find_as<juce::Button>("inspector.clip.loop");
            auto* clipAsync = ctx.find_as<juce::Button>("inspector_instrument_async");
            if (!clipSample || !clipPitch || !clipLoop || !clipAsync) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Single ClipCard controls missing");
            }
            pf = UiInteractionDriver::drag_slider(*clipPitch, 0.25);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.clip.pitch");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            clipLoop = ctx.find_as<juce::Button>("inspector.clip.loop");
            pf = UiInteractionDriver::click(*clipLoop);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.clip.loop");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 1, 0, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* playMode = ctx.find_as<juce::ComboBox>("inspector.clip.play_mode");
            if (playMode == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.clip.play_mode not found on Multi clip");
            pf = UiInteractionDriver::select_combo_index(*playMode, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.clip.play_mode");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 2, 0, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* scatterPoly = ctx.find_as<juce::Slider>("inspector.clip.scatter_poly");
            if (scatterPoly == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.clip.scatter_poly not found on Scatterer clip");
            pf = UiInteractionDriver::drag_slider(*scatterPoly, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.clip.scatter_poly");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 2, 1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* instTarget = ctx.find_as<juce::ComboBox>("inspector_instrument_target");
            if (instTarget == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_instrument_target not found on Event clip");
            pf = UiInteractionDriver::select_combo_index(*instTarget, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_instrument_target(event)");

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 2, 2, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* cmdType = ctx.find_as<juce::ComboBox>("inspector_command_type");
            if (cmdType == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_command_type not found on Command clip");
            pf = UiInteractionDriver::select_combo_index(*cmdType, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_command_type");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 2, 3, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* progSample = ctx.find_as<juce::ComboBox>("inspector.clip.prog_sample");
            if (progSample == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.clip.prog_sample not found on Programmer clip");
            pf = UiInteractionDriver::select_combo_index(*progSample, 2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.clip.prog_sample");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            return ContractResult::ok();
        }
    });

    out.push_back({
        "inspector.mask_bus_param_snapshot_vca_sample",
        {
            "inspector_mask_*",
            "inspector.bus.*",
            "inspector.param.*",
            "inspector.snapshot.*",
            "inspector_sample_stream"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Layers);
            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 0, -1, -1 });
            UiInteractionDriver::drain_messages(2);

            auto* maskMin = ctx.find_as<juce::Slider>("inspector_mask_min");
            auto* maskMax = ctx.find_as<juce::Slider>("inspector_mask_max");
            auto* maskFade = ctx.find_as<juce::Slider>("inspector_mask_fade");
            auto* maskParam = ctx.find_as<juce::ComboBox>("inspector_mask_param");
            if (!maskMin || !maskMax || !maskFade || !maskParam) {
                return ContractResult::fail(FailureCode::ViewMismatch, "MaskTransitionCard controls missing in Layers mode");
            }
            auto pf = UiInteractionDriver::drag_slider(*maskMin, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_mask_min");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);
            ctx.mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Timeline);

            ctx.mainComp.authoring_model().select({ ItemType::Bus, "bus:/sfx", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* busAddFx = ctx.find_as<juce::Button>("inspector.bus.add_fx");
            auto* busFxBypass0 = ctx.find_as<juce::Button>("inspector.bus.fx.0.bypass");
            auto* busFxWet0 = ctx.find_as<juce::Slider>("inspector.bus.fx.0.wet");
            auto* busFxCurve0 = ctx.find("inspector.bus.fx_auto.0.curve");
            if (!busAddFx || !busFxBypass0 || !busFxWet0 || !busFxCurve0) {
                return ContractResult::fail(FailureCode::ViewMismatch, "BusCard controls missing on bus:/sfx");
            }
            pf = UiInteractionDriver::click(*busFxBypass0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.bus.fx.0.bypass");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            busAddFx = ctx.find_as<juce::Button>("inspector.bus.add_fx");
            const size_t fxCountBefore = ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" })->size();
            UiTestDialogSeam::enqueue_popup_result(7);
            pf = UiInteractionDriver::click(*busAddFx);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.bus.add_fx");
            if (ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" })->size() != fxCountBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "inspector.bus.add_fx did not add effect via seam");
            }
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::GlobalParameter, "Intensity", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* paramDefault = ctx.find_as<juce::Slider>("inspector.param.default");
            auto* paramHold = ctx.find_as<juce::Button>("inspector.param.hold");
            if (!paramDefault || !paramHold) {
                return ContractResult::fail(FailureCode::ViewMismatch, "ParameterCard controls missing on Intensity");
            }
            pf = UiInteractionDriver::drag_slider(*paramDefault, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.param.default");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::GlobalParameter, "Surface", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* paramDefLbl = ctx.find_as<juce::ComboBox>("inspector.param.default_label");
            if (paramDefLbl == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector.param.default_label missing on Surface");
            pf = UiInteractionDriver::select_combo_index(*paramDefLbl, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.param.default_label");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Snapshot, "snapshot:/duck", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* snapMode = ctx.find_as<juce::ComboBox>("inspector.snapshot.mode");
            auto* snapFade = ctx.find_as<juce::Slider>("inspector.snapshot.fade");
            if (!snapMode || !snapFade) {
                return ContractResult::fail(FailureCode::ViewMismatch, "SnapshotCard controls missing on snapshot:/duck");
            }
            pf = UiInteractionDriver::select_combo_index(*snapMode, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector.snapshot.mode");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Vca, "vca:/master_vca", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* vcaVol = ctx.find_as<juce::Slider>("inspector_volume");
            if (vcaVol == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "VcaCard inspector_volume not found");
            pf = UiInteractionDriver::drag_slider(*vcaVol, -0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_volume(vca)");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            ctx.mainComp.authoring_model().select({ ItemType::Sample, "tone_440", -1, -1, -1 });
            UiInteractionDriver::drain_messages(2);
            auto* sampleStream = ctx.find_as<juce::Button>("inspector_sample_stream");
            if (sampleStream == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "inspector_sample_stream not found on tone_440");
            pf = UiInteractionDriver::click(*sampleStream);
            if (!pf.ok) return ContractResult::from_preflight(pf, "inspector_sample_stream");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            return ContractResult::ok();
        }
    });
}

} // namespace oae::studio::testing
