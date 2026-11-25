#include "UiControlContracts.h"
#include <cmath>

namespace oae::studio::testing {

void register_decks_contracts(std::vector<UiControlContract>& out)
{
    out.push_back({
        "decks.effects_chain_and_eq_plot",
        {
            "effect_eq_plot",
            "effect_bus_selector",
            "effect_add_type",
            "effect_add_position",
            "effect_add",
            "effect_selector",
            "effect_remove",
            "effect_bypass",
            "effect_wet",
            "effect_dry",
            "effect_param_*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/fx_probe", -1, -1, -1 });
            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Effects);
            UiInteractionDriver::drain_messages(2);

            auto* busSelector = ctx.find_as<juce::ComboBox>("effect_bus_selector");
            if (busSelector == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "effect_bus_selector not found");
            int sfxIdx = -1;
            for (int i = 0; i < busSelector->getNumItems(); ++i) {
                if (busSelector->getItemText(i).toStdString() == "bus:/sfx") {
                    sfxIdx = i;
                    break;
                }
            }
            if (sfxIdx < 0) return ContractResult::fail(FailureCode::ViewMismatch, "bus:/sfx not listed in effect_bus_selector");
            auto pf = UiInteractionDriver::select_combo_index(*busSelector, sfxIdx);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_bus_selector");

            auto* fxSelector = ctx.find_as<juce::ComboBox>("effect_selector");
            if (fxSelector == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "effect_selector not found");
            pf = UiInteractionDriver::select_combo_index(*fxSelector, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_selector(lowpass)");

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(fx_probe)");
            const auto baselineDual = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (baselineDual.energy220 < 1e-4 || baselineDual.energy4000 < 1e-4) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "event:/test/fx_probe baseline missing 220Hz/4000Hz dual-tone energy");
            }

            auto* eqPlot = ctx.find("effect_eq_plot");
            if (eqPlot == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "effect_eq_plot not found");
            pf = UiInteractionDriver::drag_target(*eqPlot, "cutoff", juce::Point<int>(-75, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_eq_plot#cutoff");

            ctx.mainComp.transport().stop(true);
            ctx.mainComp.preview_rebuilder().rebuild_synchronously();
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(lowpass)");
            const auto lowpassDual = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (lowpassDual.hfToLfRatio > baselineDual.hfToLfRatio * 0.35) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Lowering lowpass cutoff via effect_eq_plot did not attenuate 4kHz vs 220Hz (base ratio=" +
                    std::to_string(baselineDual.hfToLfRatio) + ", lp ratio=" + std::to_string(lowpassDual.hfToLfRatio) + ")");
            }

            auto* bypassBtn = ctx.find_as<juce::Button>("effect_bypass");
            if (bypassBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "effect_bypass not found");
            pf = UiInteractionDriver::click(*bypassBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_bypass(on)");
            ctx.mainComp.transport().stop(true);
            ctx.mainComp.preview_rebuilder().rebuild_synchronously();
            pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(bypass)");
            const auto bypassedDual = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (bypassedDual.hfToLfRatio < baselineDual.hfToLfRatio * 0.65) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Bypassing lowpass via effect_bypass did not restore 4kHz energy (bypass ratio=" +
                    std::to_string(bypassedDual.hfToLfRatio) + ")");
            }
            ctx.mainComp.transport().stop(true);

            ctx.mainComp.authoring_model().undo();
            ctx.mainComp.authoring_model().undo();
            ctx.mainComp.preview_rebuilder().rebuild_synchronously();
            UiInteractionDriver::drain_messages(2);

            fxSelector = ctx.find_as<juce::ComboBox>("effect_selector");
            pf = UiInteractionDriver::select_combo_index(*fxSelector, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_selector(eq3)");
            eqPlot = ctx.find("effect_eq_plot");
            pf = UiInteractionDriver::drag_target(*eqPlot, "band_high", juce::Point<int>(0, 22));
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_eq_plot#band_high");
            const auto* sfxEffects = ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" });
            if (!sfxEffects || (*sfxEffects)[1]["params"][2].get<double>() >= -1.0) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Dragging effect_eq_plot#band_high downward did not reduce eq3 high gain");
            }
            ctx.mainComp.authoring_model().undo();

            auto* wetSlider = ctx.find_as<juce::Slider>("effect_wet");
            auto* drySlider = ctx.find_as<juce::Slider>("effect_dry");
            auto* param0Slider = ctx.find_as<juce::Slider>("effect_param_0");
            if (!wetSlider || !drySlider || !param0Slider) {
                return ContractResult::fail(FailureCode::ViewMismatch, "EffectChainDeck wet/dry/param sliders missing");
            }
            pf = UiInteractionDriver::drag_slider(*wetSlider, -0.3);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_wet");
            ctx.mainComp.authoring_model().undo();

            auto* addType = ctx.find_as<juce::ComboBox>("effect_add_type");
            auto* addPos = ctx.find_as<juce::ComboBox>("effect_add_position");
            auto* addBtn = ctx.find_as<juce::Button>("effect_add");
            auto* rmBtn = ctx.find_as<juce::Button>("effect_remove");
            if (!addType || !addPos || !addBtn || !rmBtn) {
                return ContractResult::fail(FailureCode::ViewMismatch, "EffectChainDeck add/remove controls missing");
            }
            pf = UiInteractionDriver::select_combo_index(*addType, 6);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_add_type");
            pf = UiInteractionDriver::select_combo_index(*addPos, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_add_position");
            const size_t countBefore = ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" })->size();
            pf = UiInteractionDriver::click(*addBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_add");
            if (ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" })->size() != countBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "effect_add did not append effect to bus:/sfx");
            }
            pf = UiInteractionDriver::click(*rmBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "effect_remove");
            if (ctx.mainComp.authoring_model().get_effects({ ItemType::Bus, "bus:/sfx" })->size() != countBefore) {
                return ContractResult::fail(FailureCode::ModelMismatch, "effect_remove did not remove selected effect");
            }

            return ContractResult::ok();
        }
    });

    out.push_back({
        "decks.spatializer_3d",
        {
            "deck_spatializer",
            "spatial_preview_*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Spatializer);
            UiInteractionDriver::drain_messages(2);

            auto* spatialDeck = ctx.find("deck_spatializer");
            if (spatialDeck == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "deck_spatializer not found");

            auto* minDist = ctx.find_as<juce::Slider>("spatial_preview_min_distance");
            auto* maxDist = ctx.find_as<juce::Slider>("spatial_preview_max_distance");
            auto* soundSize = ctx.find_as<juce::Slider>("spatial_preview_sound_size");
            auto* minExtent = ctx.find_as<juce::Slider>("spatial_preview_min_extent");
            auto* rolloff = ctx.find_as<juce::ComboBox>("spatial_preview_rolloff");
            if (!minDist || !maxDist || !soundSize || !minExtent || !rolloff) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Spatializer3DDeck controls missing");
            }

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            auto pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(spatial)");

            pf = UiInteractionDriver::double_click_target(*spatialDeck, "radar_center");
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_center");
            const auto centerMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (centerMetrics.rmsDb < -35.0) {
                return ContractResult::fail(FailureCode::AudioMismatch, "Spatializer center playback is silent");
            }

            pf = UiInteractionDriver::drag_target(*spatialDeck, "radar_emitter", juce::Point<int>(55, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_emitter(right)");
            const auto rightFarMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (rightFarMetrics.rmsDb > centerMetrics.rmsDb - 3.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging 3D emitter outward did not attenuate distance gain (center=" +
                    std::to_string(centerMetrics.rmsDb) + ", far=" + std::to_string(rightFarMetrics.rmsDb) + ")");
            }
            if (rightFarMetrics.lrRatio > 0.78) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging 3D emitter to +X did not pan audio right (lrRatio=" + std::to_string(rightFarMetrics.lrRatio) + ")");
            }

            pf = UiInteractionDriver::double_click_target(*spatialDeck, "radar_center");
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_center(reset)");
            pf = UiInteractionDriver::drag_target(*spatialDeck, "radar_emitter", juce::Point<int>(-55, 0));
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_emitter(left)");
            const auto leftFarMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (leftFarMetrics.lrRatio < 1.30) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging 3D emitter to -X did not pan audio left (lrRatio=" + std::to_string(leftFarMetrics.lrRatio) + ")");
            }

            pf = UiInteractionDriver::wheel_target(*spatialDeck, "radar_area", 0.5f);
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_area(wheel)");
            pf = UiInteractionDriver::double_click_target(*spatialDeck, "radar_center");
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck_spatializer#radar_center(final)");

            pf = UiInteractionDriver::select_combo_index(*rolloff, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_rolloff");
            pf = UiInteractionDriver::select_combo_index(*rolloff, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_rolloff(reset)");

            pf = UiInteractionDriver::drag_slider(*minDist, 0.1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_min_distance");
            pf = UiInteractionDriver::drag_slider(*maxDist, -0.1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_max_distance");
            pf = UiInteractionDriver::drag_slider(*soundSize, 0.15);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_sound_size");
            pf = UiInteractionDriver::drag_slider(*minExtent, 0.15);
            if (!pf.ok) return ContractResult::from_preflight(pf, "spatial_preview_min_extent");

            ctx.mainComp.transport().stop(true);
            return ContractResult::ok();
        }
    });

    out.push_back({
        "decks.global_parameters",
        {
            "ed_params_search",
            "btn_filter_all",
            "btn_filter_used",
            "btn_filter_modified",
            "btn_reset_all_params",
            "btn_params_pop_out",
            "deck.param.*"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Parameters);
            UiInteractionDriver::drain_messages(2);

            auto* playBtn = ctx.find_as<juce::Button>("btn_play");
            if (playBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_play not found");
            auto pf = UiInteractionDriver::click(*playBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_play(params)");
            const auto baseMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());

            auto* intensitySlider = ctx.find_as<juce::Slider>("deck.param.Intensity.slider");
            if (intensitySlider == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "deck.param.Intensity.slider not found");
            pf = UiInteractionDriver::drag_slider(*intensitySlider, -0.55);
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck.param.Intensity.slider");
            const auto lowMetrics = HeadlessAudioProbe::capture(ctx.mainComp.engine());
            if (lowMetrics.rmsDb > baseMetrics.rmsDb - 5.0) {
                return ContractResult::fail(FailureCode::AudioMismatch,
                    "Dragging deck.param.Intensity.slider down did not attenuate live audio (base=" +
                    std::to_string(baseMetrics.rmsDb) + ", low=" + std::to_string(lowMetrics.rmsDb) + ")");
            }
            ctx.mainComp.transport().stop(true);

            auto* surfaceOpt2 = ctx.find_as<juce::Button>("deck.param.Surface.opt.2");
            if (surfaceOpt2 == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "deck.param.Surface.opt.2 not found");
            pf = UiInteractionDriver::click(*surfaceOpt2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "deck.param.Surface.opt.2");
            if (std::abs(ctx.mainComp.transport().get_global_parameter("Surface") - 2.0f) > 0.01f) {
                return ContractResult::fail(FailureCode::ModelMismatch, "deck.param.Surface.opt.2 did not set Surface to 2.0");
            }

            auto* resetAllBtn = ctx.find_as<juce::Button>("btn_reset_all_params");
            if (resetAllBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_reset_all_params not found");
            pf = UiInteractionDriver::click(*resetAllBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_reset_all_params");
            if (std::abs(ctx.mainComp.transport().get_global_parameter("Intensity") - 0.5f) > 0.01f ||
                std::abs(ctx.mainComp.transport().get_global_parameter("Surface") - 0.0f) > 0.01f) {
                return ContractResult::fail(FailureCode::ModelMismatch, "btn_reset_all_params did not restore default parameter values");
            }

            auto* filterUsed = ctx.find_as<juce::Button>("btn_filter_used");
            auto* filterMod = ctx.find_as<juce::Button>("btn_filter_modified");
            auto* filterAll = ctx.find_as<juce::Button>("btn_filter_all");
            auto* searchEd = ctx.find_as<juce::TextEditor>("ed_params_search");
            if (!filterUsed || !filterMod || !filterAll || !searchEd) {
                return ContractResult::fail(FailureCode::ViewMismatch, "GlobalParametersDeck filter/search controls missing");
            }
            pf = UiInteractionDriver::click(*filterUsed);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_filter_used");
            pf = UiInteractionDriver::click(*filterMod);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_filter_modified");
            pf = UiInteractionDriver::click(*filterAll);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_filter_all");

            pf = UiInteractionDriver::type_text(*searchEd, "Intensity", true, true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "ed_params_search");
            pf = UiInteractionDriver::type_text(*searchEd, "", true, true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "ed_params_search(clear)");

            auto* dockPopOut = ctx.find_as<juce::Button>("btn_dock_pop_out");
            if (dockPopOut == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_dock_pop_out not found");
            pf = UiInteractionDriver::click(*dockPopOut);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_dock_pop_out");
            UiInteractionDriver::drain_messages(25);

            auto* paramsPopOut = ctx.find_as<juce::Button>("btn_params_pop_out");
            if (paramsPopOut == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_params_pop_out not visible in floating Parameters deck window");
            }
            pf = UiInteractionDriver::click(*paramsPopOut);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_params_pop_out");
            UiInteractionDriver::drain_messages(25);
            if (ctx.mainComp.docking_layout().get_panel_slot(DockPanelId::BottomDeck) == DockSlotId::Floating) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_params_pop_out did not re-dock BottomDeck");
            }

            return ContractResult::ok();
        }
    });

    out.push_back({
        "decks.playlist_logic_and_diagnostics",
        {
            "playlist_mode",
            "playlist_scope",
            "playlist_entries",
            "playlist_add",
            "playlist_remove",
            "playlist_move_up",
            "playlist_move_down",
            "playlist_weight",
            "playlist_pitch_range",
            "playlist_volume_range",
            "playlist_delay_range",
            "logic_*",
            "diagnostics_log_editor"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", 1, 0, -1 });
            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Playlist);
            UiInteractionDriver::drain_messages(2);

            auto* plMode = ctx.find_as<juce::ComboBox>("playlist_mode");
            auto* plScope = ctx.find_as<juce::ComboBox>("playlist_scope");
            auto* plEntries = ctx.find_as<juce::ListBox>("playlist_entries");
            auto* plAdd = ctx.find_as<juce::Button>("playlist_add");
            auto* plRemove = ctx.find_as<juce::Button>("playlist_remove");
            auto* plUp = ctx.find_as<juce::Button>("playlist_move_up");
            auto* plDown = ctx.find_as<juce::Button>("playlist_move_down");
            auto* plWeight = ctx.find_as<juce::Slider>("playlist_weight");
            auto* plPitch = ctx.find_as<juce::Slider>("playlist_pitch_range");
            auto* plVol = ctx.find_as<juce::Slider>("playlist_volume_range");
            auto* plDelay = ctx.find_as<juce::Slider>("playlist_delay_range");
            if (!plMode || !plScope || !plEntries || !plAdd || !plRemove || !plUp || !plDown ||
                !plWeight || !plPitch || !plVol || !plDelay) {
                return ContractResult::fail(FailureCode::ViewMismatch, "PlaylistDeck controls missing");
            }

            auto pf = UiInteractionDriver::select_combo_index(*plMode, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_mode");
            pf = UiInteractionDriver::select_combo_index(*plScope, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_scope");
            pf = UiInteractionDriver::click(*plEntries, false, juce::Point<int>(30, 14));
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_entries");

            pf = UiInteractionDriver::click(*plDown);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_move_down");
            pf = UiInteractionDriver::click(*plUp);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_move_up");

            pf = UiInteractionDriver::drag_slider(*plWeight, 0.15);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_weight");
            pf = UiInteractionDriver::drag_slider(*plPitch, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_pitch_range");
            pf = UiInteractionDriver::drag_slider(*plVol, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_volume_range");
            pf = UiInteractionDriver::drag_slider(*plDelay, 0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_delay_range");

            UiTestDialogSeam::enqueue_popup_result(1);
            pf = UiInteractionDriver::click(*plAdd);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_add");
            pf = UiInteractionDriver::click(*plRemove);
            if (!pf.ok) return ContractResult::from_preflight(pf, "playlist_remove");

            ctx.mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", -1, -1, 1 });
            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Logic);
            UiInteractionDriver::drain_messages(2);

            auto* logicItems = ctx.find_as<juce::ComboBox>("logic_items");
            auto* logicPos = ctx.find_as<juce::Slider>("logic_position");
            auto* logicLen = ctx.find_as<juce::Slider>("logic_length");
            auto* logicReps = ctx.find_as<juce::Slider>("logic_repeats");
            auto* logicKeyOff = ctx.find_as<juce::Button>("logic_until_key_off");
            auto* logicProb = ctx.find_as<juce::Slider>("logic_probability");
            auto* logicParam = ctx.find_as<juce::ComboBox>("logic_parameter");
            auto* logicMin = ctx.find_as<juce::Slider>("logic_minimum");
            auto* logicMax = ctx.find_as<juce::Slider>("logic_maximum");
            auto* logicAddTerm = ctx.find_as<juce::Button>("logic_add_term");
            auto* logicRmTerm = ctx.find_as<juce::Button>("logic_remove_term");
            if (!logicItems || !logicPos || !logicLen || !logicReps || !logicKeyOff ||
                !logicProb || !logicParam || !logicMin || !logicMax || !logicAddTerm || !logicRmTerm) {
                return ContractResult::fail(FailureCode::ViewMismatch, "LogicConditionsDeck loop_region controls missing");
            }

            pf = UiInteractionDriver::click(*logicKeyOff);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_until_key_off");
            ctx.mainComp.authoring_model().undo();

            pf = UiInteractionDriver::drag_slider(*logicProb, -0.2);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_probability");
            ctx.mainComp.authoring_model().undo();

            pf = UiInteractionDriver::click(*logicAddTerm);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_add_term");
            pf = UiInteractionDriver::click(*logicRmTerm);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_remove_term");

            pf = UiInteractionDriver::select_combo_index(*logicItems, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_items(marker)");
            auto* markerName = ctx.find_as<juce::TextEditor>("logic_marker_name");
            if (markerName == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "logic_marker_name not visible for marker");
            pf = UiInteractionDriver::type_text(*markerName, "IntroEdited", true, true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_marker_name");
            ctx.mainComp.authoring_model().undo();
            UiInteractionDriver::drain_messages(2);

            logicItems = ctx.find_as<juce::ComboBox>("logic_items");
            pf = UiInteractionDriver::select_combo_index(*logicItems, 3);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_items(transition)");
            auto* logicQuant = ctx.find_as<juce::ComboBox>("logic_quantization");
            auto* logicXfade = ctx.find_as<juce::Slider>("logic_crossfade");
            if (!logicQuant || !logicXfade) return ContractResult::fail(FailureCode::ViewMismatch, "Transition controls missing in LogicConditionsDeck");
            pf = UiInteractionDriver::select_combo_index(*logicQuant, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_quantization");
            ctx.mainComp.authoring_model().undo();

            pf = UiInteractionDriver::select_combo_index(*logicItems, 4);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_items(tempo)");
            auto* logicBpm = ctx.find_as<juce::Slider>("logic_bpm");
            auto* logicTsUp = ctx.find_as<juce::ComboBox>("logic_ts_upper");
            auto* logicTsLow = ctx.find_as<juce::ComboBox>("logic_ts_lower");
            if (!logicBpm || !logicTsUp || !logicTsLow) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Tempo controls missing in LogicConditionsDeck");
            }
            pf = UiInteractionDriver::drag_slider(*logicBpm, 0.15);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_bpm");
            ctx.mainComp.authoring_model().undo();

            auto* addSustain = ctx.find_as<juce::Button>("logic_add_sustain");
            auto* rmItem = ctx.find_as<juce::Button>("logic_remove_item");
            if (!addSustain || !rmItem) return ContractResult::fail(FailureCode::ViewMismatch, "LogicConditionsDeck add/remove buttons missing");
            const size_t logicCountBefore = ctx.get_event_logic_items("event:/test/main")->size();
            pf = UiInteractionDriver::click(*addSustain);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_add_sustain");
            if (ctx.get_event_logic_items("event:/test/main")->size() != logicCountBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "logic_add_sustain did not add sustain point");
            }
            pf = UiInteractionDriver::click(*rmItem);
            if (!pf.ok) return ContractResult::from_preflight(pf, "logic_remove_item");

            ctx.mainComp.contextual_deck().set_active_tab(DeckTab::Diagnostics);
            UiInteractionDriver::drain_messages(2);
            if (ctx.mainComp.contextual_deck().get_active_tab() != DeckTab::Diagnostics) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Failed to activate Diagnostics deck tab");
            }

            return ContractResult::ok();
        }
    });
}

} // namespace oae::studio::testing
