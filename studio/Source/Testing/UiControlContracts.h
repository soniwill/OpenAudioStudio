#pragma once

#include "../MainComponent.h"
#include "HeadlessAudioProbe.h"
#include "UiControlAuditor.h"
#include "UiControlTestFixture.h"
#include "UiInteractionDriver.h"
#include "UiTestHooks.h"
#include <functional>
#include <string>
#include <vector>

namespace oae::studio::testing {

/** Outcome of executing a single behavioral or acoustic UI control contract. */
struct ContractResult {
    bool passed{ true };
    std::string code;
    std::string message;
    double durationMs{ 0.0 };

    static ContractResult ok()
    {
        return { true, "OK", {}, 0.0 };
    }

    static ContractResult fail(const std::string& errCode, const std::string& msg)
    {
        return { false, errCode, msg, 0.0 };
    }

    static ContractResult from_preflight(const PreflightResult& pf, const std::string& controlId)
    {
        return { false, pf.code, "Pre-flight failed for '" + controlId + "': " + pf.detail, 0.0 };
    }
};

/** Shared execution context passed to each UI control contract. */
struct UiTestContext {
    MainComponent& mainComp;
    UiControlTestFixture& fixture;

    /** Finds a visible component by exact ComponentID across MainComponent and floating TopLevelWindows. */
    juce::Component* find(const std::string& id) const
    {
        return UiControlAuditor::find_component_by_id(mainComp, id);
    }

    /** Finds a visible component cast to T by exact ComponentID. */
    template <typename T>
    T* find_as(const std::string& id) const
    {
        return dynamic_cast<T*>(find(id));
    }

    /** Returns the JSON object for an event in AuthoringModel by path, or nullptr if not found. */
    const nlohmann::json* find_event_json(const std::string& eventPath) const
    {
        const auto& root = mainComp.authoring_model().json();
        if (!root.contains("events") || !root["events"].is_array()) {
            return nullptr;
        }
        for (const auto& ev : root["events"]) {
            if (ev.is_object() && ev.value("path", "") == eventPath) {
                return &ev;
            }
        }
        return nullptr;
    }

    /** Returns the tracks JSON array for an event in AuthoringModel, or nullptr if not found. */
    const nlohmann::json* get_event_tracks(const std::string& eventPath) const
    {
        const auto* ev = find_event_json(eventPath);
        if (ev != nullptr && ev->contains("tracks") && (*ev)["tracks"].is_array()) {
            return &(*ev)["tracks"];
        }
        return nullptr;
    }

    /** Returns the logic_items JSON array for an event in AuthoringModel, or nullptr if not found. */
    const nlohmann::json* get_event_logic_items(const std::string& eventPath) const
    {
        const auto* ev = find_event_json(eventPath);
        if (ev != nullptr && ev->contains("logic_items") && (*ev)["logic_items"].is_array()) {
            return &(*ev)["logic_items"];
        }
        return nullptr;
    }

    /** Resets workspace layout, stops transport, restores golden model, and selects event:/test/main. */
    void reset_to_baseline()
    {
        UiTestDialogSeam::clear_all();
        mainComp.transport().stop(true);
        mainComp.transport().stop_all_snapshots();
        if (mainComp.docking_layout().get_active_preset_name() != "Default" ||
            mainComp.docking_layout().are_all_panels_hidden() ||
            juce::TopLevelWindow::getNumTopLevelWindows() > 0) {
            mainComp.docking_layout().reset_layout();
            mainComp.sync_workspace_layout();
        }
        mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Timeline);
        mainComp.contextual_deck().set_active_tab(DeckTab::Mixer);
        const uint64_t curRev = mainComp.authoring_model().get_content_revision();
        fixture.restore_golden_model(mainComp);
        if (curRev != goldenRevision_ || mainComp.preview_rebuilder().get_loaded_bank() == 0) {
            mainComp.preview_rebuilder().rebuild_synchronously();
        }
        goldenRevision_ = mainComp.authoring_model().get_content_revision();
        const char* busPaths[3] = { "bus:/", "bus:/sfx", "bus:/music" };
        for (const char* bp : busPaths) {
            oae_bus busHandle = 0;
            if (mainComp.engine().get_bus(bp, &busHandle) == OAE_OK && busHandle != 0) {
                mainComp.engine().set_bus_volume_db(busHandle, 0.0f);
                mainComp.engine().set_bus_pan(busHandle, 0.0f);
                mainComp.engine().set_bus_mute(busHandle, false);
            }
        }
        oae_vca vcaHandle = 0;
        if (mainComp.engine().get_vca("vca:/master_vca", &vcaHandle) == OAE_OK && vcaHandle != 0) {
            mainComp.engine().set_vca_volume_db(vcaHandle, 0.0f);
        }
        mainComp.authoring_model().select({ ItemType::Event, "event:/test/main", -1, -1, -1 });
        mainComp.transport().set_global_parameter("Intensity", 0.5f);
        mainComp.transport().set_global_parameter("Stage", 1.0f);
        mainComp.transport().set_global_parameter("Surface", 0.0f);
        UiInteractionDriver::drain_messages(2);
    }

    uint64_t goldenRevision_{ 0 };
};

/** Behavioral and/or acoustic contract covering a declared set of ComponentID patterns. */
struct UiControlContract {
    std::string id;
    std::vector<std::string> coveredPatterns;
    std::function<ContractResult(UiTestContext&)> execute;
};

void register_shell_contracts(std::vector<UiControlContract>& out);
void register_audio_core_contracts(std::vector<UiControlContract>& out);
void register_timeline_layers_contracts(std::vector<UiControlContract>& out);
void register_inspector_contracts(std::vector<UiControlContract>& out);
void register_decks_contracts(std::vector<UiControlContract>& out);

inline std::vector<UiControlContract> build_all_ui_control_contracts()
{
    std::vector<UiControlContract> all;
    register_shell_contracts(all);
    register_audio_core_contracts(all);
    register_timeline_layers_contracts(all);
    register_inspector_contracts(all);
    register_decks_contracts(all);
    return all;
}

inline std::vector<std::string> collect_all_covered_patterns(const std::vector<UiControlContract>& contracts)
{
    std::vector<std::string> out;
    for (const auto& c : contracts) {
        for (const auto& p : c.coveredPatterns) {
            out.push_back(p);
        }
    }
    return out;
}

} // namespace oae::studio::testing
