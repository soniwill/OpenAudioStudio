#include "UiControlContracts.h"
#include <filesystem>

namespace oae::studio::testing {

void register_shell_contracts(std::vector<UiControlContract>& out)
{
    out.push_back({
        "shell.workspace_bar",
        {
            "btn_vis_transport",
            "btn_vis_browser",
            "btn_vis_timeline",
            "btn_vis_inspector",
            "btn_vis_bottom_deck",
            "combo_workspace_preset",
            "btn_preset_save",
            "btn_workspace_menu"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* trnBtn = ctx.find_as<juce::Button>("btn_vis_transport");
            if (trnBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_vis_transport not found");
            auto pf = UiInteractionDriver::click(*trnBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_vis_transport");
            if (!ctx.mainComp.transport_bar().isVisible()) {
                return ContractResult::fail(FailureCode::ViewMismatch, "TransportBar must remain visible after btn_vis_transport toggle");
            }
            UiInteractionDriver::click(*trnBtn);

            const struct { const char* btnId; DockPanelId pid; } visPanels[4] = {
                { "btn_vis_browser", DockPanelId::Browser },
                { "btn_vis_timeline", DockPanelId::Timeline },
                { "btn_vis_inspector", DockPanelId::Inspector },
                { "btn_vis_bottom_deck", DockPanelId::BottomDeck }
            };
            for (const auto& item : visPanels) {
                auto* btn = ctx.find_as<juce::Button>(item.btnId);
                if (btn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, std::string(item.btnId) + " not found");
                pf = UiInteractionDriver::click(*btn);
                if (!pf.ok) return ContractResult::from_preflight(pf, item.btnId);
                if (ctx.mainComp.docking_layout().is_panel_visible(item.pid)) {
                    return ContractResult::fail(FailureCode::ViewMismatch, std::string(item.btnId) + " did not hide panel");
                }
                pf = UiInteractionDriver::click(*btn);
                if (!pf.ok) return ContractResult::from_preflight(pf, item.btnId);
                if (!ctx.mainComp.docking_layout().is_panel_visible(item.pid)) {
                    return ContractResult::fail(FailureCode::ViewMismatch, std::string(item.btnId) + " did not restore panel");
                }
            }

            auto* presetCombo = ctx.find_as<juce::ComboBox>("combo_workspace_preset");
            if (presetCombo == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "combo_workspace_preset not found");
            pf = UiInteractionDriver::select_combo_index(*presetCombo, 1);
            if (!pf.ok) return ContractResult::from_preflight(pf, "combo_workspace_preset");
            pf = UiInteractionDriver::select_combo_index(*presetCombo, 0);
            if (!pf.ok) return ContractResult::from_preflight(pf, "combo_workspace_preset");

            auto* savePresetBtn = ctx.find_as<juce::Button>("btn_preset_save");
            if (savePresetBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_preset_save not found");
            UiTestDialogSeam::enqueue_alert_result(1, "QA_Isolated_Preset");
            pf = UiInteractionDriver::click(*savePresetBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_preset_save");
            if (ctx.mainComp.docking_layout().get_active_preset_name() != "QA_Isolated_Preset") {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_preset_save did not activate saved custom preset via seam");
            }

            auto* wsMenuBtn = ctx.find_as<juce::Button>("btn_workspace_menu");
            if (wsMenuBtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_workspace_menu not found");
            UiTestDialogSeam::enqueue_popup_result(3);
            pf = UiInteractionDriver::click(*wsMenuBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_workspace_menu");
            if (!ctx.mainComp.docking_layout().are_all_panels_hidden()) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_workspace_menu item 3 did not hide all panels");
            }
            UiTestDialogSeam::enqueue_popup_result(5);
            pf = UiInteractionDriver::click(*wsMenuBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_workspace_menu");
            if (ctx.mainComp.docking_layout().are_all_panels_hidden()) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_workspace_menu item 5 did not reset layout");
            }

            return ContractResult::ok();
        }
    });

    out.push_back({
        "shell.docking_and_popout",
        {
            "btn_slot_header_tab_*",
            "btn_slot_*_move",
            "btn_slot_*_popout",
            "btn_slot_*_close",
            "dock_inspector_lock_button",
            "dock_splitter_*",
            "btn_slot_tab_bottom_deck",
            "btn_dock_pop_out",
            "btn_dock_tabs_menu",
            "btn_tab_*",
            "btn_floating_dock_back",
            "grip_project_toolbar",
            "grip_transport_bar"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* insTab = ctx.find_as<juce::Button>("btn_slot_header_tab_inspector");
            if (insTab == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "btn_slot_header_tab_inspector not found");
            auto pf = UiInteractionDriver::click(*insTab);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_slot_header_tab_inspector");

            auto* dockLockBtn = ctx.find_as<juce::Button>("dock_inspector_lock_button");
            if (dockLockBtn != nullptr) {
                pf = UiInteractionDriver::click(*dockLockBtn);
                if (!pf.ok) return ContractResult::from_preflight(pf, "dock_inspector_lock_button");
                if (!ctx.mainComp.inspector_panel().is_locked()) {
                    return ContractResult::fail(FailureCode::ViewMismatch, "dock_inspector_lock_button did not lock Inspector");
                }
                UiInteractionDriver::click(*dockLockBtn);
            }

            auto splitters = UiControlAuditor::find_components_matching(ctx.mainComp, "dock_splitter_*");
            if (splitters.empty()) {
                return ContractResult::fail(FailureCode::ViewMismatch, "No dock_splitter_* found in default layout");
            }
            pf = UiInteractionDriver::drag(*splitters[0],
                                           splitters[0]->getLocalBounds().getCentre(),
                                           splitters[0]->getLocalBounds().getCentre() + juce::Point<int>(12, 12));
            if (!pf.ok) return ContractResult::from_preflight(pf, splitters[0]->getComponentID().toStdString());

            auto* prjGrip = ctx.find("grip_project_toolbar");
            if (prjGrip == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "grip_project_toolbar not found");
            const bool wasMin = ctx.mainComp.project_toolbar().is_minimized();
            pf = UiInteractionDriver::double_click_target(*prjGrip, "grip");
            if (!pf.ok) return ContractResult::from_preflight(pf, "grip_project_toolbar");
            if (ctx.mainComp.project_toolbar().is_minimized() == wasMin) {
                return ContractResult::fail(FailureCode::ViewMismatch, "Double-clicking grip_project_toolbar did not toggle minimized state");
            }
            UiInteractionDriver::double_click_target(*prjGrip, "grip");

            auto* trnGrip = ctx.find("grip_transport_bar");
            if (trnGrip == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "grip_transport_bar not found");
            UiTestDialogSeam::enqueue_popup_result(2);
            pf = UiInteractionDriver::click_target(*trnGrip, "grip", true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "grip_transport_bar");

            auto* floatingDockBack = ctx.find_as<juce::Button>("btn_floating_dock_back");
            if (floatingDockBack == nullptr) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_floating_dock_back not found in floating TopLevelWindow after floating TransportBar");
            }
            pf = UiInteractionDriver::click(*floatingDockBack);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_floating_dock_back");
            UiInteractionDriver::drain_messages(5);
            if (ctx.mainComp.docking_layout().get_panel_slot(DockPanelId::Transport) == DockSlotId::Floating) {
                return ContractResult::fail(FailureCode::ViewMismatch, "btn_floating_dock_back did not re-dock TransportBar");
            }

            const char* deckTabIds[7] = {
                "btn_tab_effects",
                "btn_tab_logic",
                "btn_tab_playlist",
                "btn_tab_spatializer",
                "btn_tab_parameters",
                "btn_tab_diagnostics",
                "btn_tab_mixer"
            };
            for (const char* tid : deckTabIds) {
                auto* tbtn = ctx.find_as<juce::Button>(tid);
                if (tbtn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, std::string(tid) + " not found");
                pf = UiInteractionDriver::click(*tbtn);
                if (!pf.ok) return ContractResult::from_preflight(pf, tid);
            }

            return ContractResult::ok();
        }
    });

    out.push_back({
        "shell.project_toolbar",
        {
            "btn_new_project",
            "btn_open_project",
            "btn_save_project",
            "btn_build_bank",
            "btn_help"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            auto* newBtn = ctx.find_as<juce::Button>("btn_new_project");
            auto* openBtn = ctx.find_as<juce::Button>("btn_open_project");
            auto* saveBtn = ctx.find_as<juce::Button>("btn_save_project");
            auto* buildBtn = ctx.find_as<juce::Button>("btn_build_bank");
            auto* helpBtn = ctx.find_as<juce::Button>("btn_help");
            if (!newBtn || !openBtn || !saveBtn || !buildBtn || !helpBtn) {
                return ContractResult::fail(FailureCode::ViewMismatch, "One or more ProjectToolbar buttons missing");
            }

            UiTestDialogSeam::clear_invocations();
            UiTestDialogSeam::enqueue_file_choice({});
            auto pf = UiInteractionDriver::click(*newBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_new_project");
            if (UiTestDialogSeam::invocations().empty() || UiTestDialogSeam::invocations().back().tag != "new_project") {
                return ContractResult::fail(FailureCode::DialogTimeout, "btn_new_project did not invoke FileChooser seam");
            }

            UiTestDialogSeam::enqueue_file_choice({});
            pf = UiInteractionDriver::click(*openBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_open_project");
            if (UiTestDialogSeam::invocations().back().tag != "open_project") {
                return ContractResult::fail(FailureCode::DialogTimeout, "btn_open_project did not invoke FileChooser seam");
            }

            pf = UiInteractionDriver::click(*saveBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_save_project");

            pf = UiInteractionDriver::click(*buildBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_build_bank");
            ctx.mainComp.publisher().wait_until_finished();
            UiInteractionDriver::drain_messages(20);

            pf = UiInteractionDriver::click(*helpBtn);
            if (!pf.ok) return ContractResult::from_preflight(pf, "btn_help");

            return ContractResult::ok();
        }
    });

    out.push_back({
        "shell.authoring_tree_browser",
        {
            "btn_browser_tab_events",
            "btn_browser_tab_snapshots",
            "btn_browser_tab_buses",
            "btn_browser_tab_assets",
            "browser_search_box",
            "browser_tree_view",
            "browser_params_list"
        },
        [](UiTestContext& ctx) -> ContractResult {
            ctx.reset_to_baseline();

            const char* tabs[4] = {
                "btn_browser_tab_snapshots",
                "btn_browser_tab_buses",
                "btn_browser_tab_assets",
                "btn_browser_tab_events"
            };
            for (const char* tabId : tabs) {
                auto* btn = ctx.find_as<juce::Button>(tabId);
                if (btn == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, std::string(tabId) + " not found");
                auto pf = UiInteractionDriver::click(*btn);
                if (!pf.ok) return ContractResult::from_preflight(pf, tabId);
            }

            auto* searchBox = ctx.find_as<juce::TextEditor>("browser_search_box");
            if (searchBox == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "browser_search_box not found");
            auto pf = UiInteractionDriver::type_text(*searchBox, "fx_probe", true, true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_search_box");
            if (searchBox->getText().toStdString() != "fx_probe") {
                return ContractResult::fail(FailureCode::ViewMismatch, "browser_search_box did not retain typed filter");
            }
            pf = UiInteractionDriver::type_text(*searchBox, "", true, true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_search_box");

            auto* treeView = ctx.find_as<juce::TreeView>("browser_tree_view");
            if (treeView == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "browser_tree_view not found");
            pf = UiInteractionDriver::click(*treeView, false, juce::Point<int>(40, 12));
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_tree_view");

            UiTestDialogSeam::enqueue_popup_result(1);
            const size_t eventsBefore = ctx.mainComp.authoring_model().get_event_paths().size();
            pf = UiInteractionDriver::click(*treeView, true, juce::Point<int>(40, treeView->getHeight() - 10));
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_tree_view");
            if (ctx.mainComp.authoring_model().get_event_paths().size() == eventsBefore + 1) {
                ctx.mainComp.authoring_model().undo();
            }

            auto* paramsList = ctx.find("browser_params_list");
            if (paramsList == nullptr) return ContractResult::fail(FailureCode::ViewMismatch, "browser_params_list not found");
            pf = UiInteractionDriver::click_target(*paramsList, "row_0", false);
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_params_list#row_0");
            if (ctx.mainComp.authoring_model().get_selection().type != ItemType::GlobalParameter) {
                return ContractResult::fail(FailureCode::ModelMismatch, "Clicking browser_params_list row_0 did not select GlobalParameter");
            }

            const size_t paramsBefore = ctx.mainComp.authoring_model().get_global_parameter_names().size();
            UiTestDialogSeam::enqueue_popup_result(1);
            pf = UiInteractionDriver::click_target(*paramsList, "row_0", true);
            if (!pf.ok) return ContractResult::from_preflight(pf, "browser_params_list#row_0(right)");
            if (ctx.mainComp.authoring_model().get_global_parameter_names().size() != paramsBefore + 1) {
                return ContractResult::fail(FailureCode::ModelMismatch, "browser_param_context menu via seam did not create global parameter");
            }
            if (!ctx.mainComp.authoring_model().undo() ||
                ctx.mainComp.authoring_model().get_global_parameter_names().size() != paramsBefore) {
                return ContractResult::fail(FailureCode::UndoMismatch, "Undo after browser_param_context creation failed");
            }

            return ContractResult::ok();
        }
    });
}

} // namespace oae::studio::testing
