#include "UiControlAuditor.h"
#include "../UI/OaeMeterComponent.h"
#include "../UI/OaePrimitives.h"
#include "../UI/TutorialHighlightOverlay.h"
#include <algorithm>
#include <typeinfo>

namespace oae::studio::testing {

namespace {

bool is_inside_atomic_juce_widget(juce::Component& comp) noexcept
{
    for (juce::Component* p = comp.getParentComponent(); p != nullptr; p = p->getParentComponent()) {
        if (dynamic_cast<juce::ComboBox*>(p) != nullptr ||
            dynamic_cast<juce::Slider*>(p) != nullptr ||
            dynamic_cast<juce::TreeView*>(p) != nullptr ||
            dynamic_cast<juce::ListBox*>(p) != nullptr ||
            dynamic_cast<juce::ScrollBar*>(p) != nullptr ||
            dynamic_cast<juce::TextEditor*>(p) != nullptr ||
            dynamic_cast<juce::Button*>(p) != nullptr) {
            return true;
        }
    }
    return false;
}

std::string build_component_path(juce::Component& comp)
{
    std::vector<std::string> parts;
    for (juce::Component* cur = &comp; cur != nullptr; cur = cur->getParentComponent()) {
        std::string seg = cur->getComponentID().toStdString();
        if (seg.empty()) {
            seg = cur->getName().toStdString();
        }
        if (seg.empty()) {
            seg = typeid(*cur).name();
        }
        parts.push_back(std::move(seg));
        if (auto* dw = dynamic_cast<juce::DocumentWindow*>(cur->getParentComponent())) {
            if (dw->getContentComponent() == cur) {
                break;
            }
        }
    }
    std::reverse(parts.begin(), parts.end());
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += " > ";
        out += parts[i];
    }
    return out;
}

void collect_visible_interactive_recursive(juce::Component& node, std::vector<juce::Component*>& out)
{
    if (!node.isVisible()) {
        return;
    }
    if (UiControlAuditor::is_interactive_component(node)) {
        out.push_back(&node);
    }
    if (dynamic_cast<juce::ComboBox*>(&node) != nullptr ||
        dynamic_cast<juce::Slider*>(&node) != nullptr ||
        dynamic_cast<juce::TreeView*>(&node) != nullptr ||
        dynamic_cast<juce::ListBox*>(&node) != nullptr ||
        dynamic_cast<juce::ScrollBar*>(&node) != nullptr ||
        dynamic_cast<juce::TextEditor*>(&node) != nullptr ||
        dynamic_cast<juce::Button*>(&node) != nullptr) {
        return;
    }
    for (int i = 0; i < node.getNumChildComponents(); ++i) {
        if (auto* child = node.getChildComponent(i)) {
            collect_visible_interactive_recursive(*child, out);
        }
    }
}

juce::Component* find_id_recursive(juce::Component& node, const std::string& id)
{
    if (!node.isVisible()) {
        return nullptr;
    }
    if (node.getComponentID().toStdString() == id) {
        return &node;
    }
    for (int i = 0; i < node.getNumChildComponents(); ++i) {
        if (auto* child = node.getChildComponent(i)) {
            if (auto* found = find_id_recursive(*child, id)) {
                return found;
            }
        }
    }
    return nullptr;
}

void find_matching_recursive(juce::Component& node, const std::string& pattern, std::vector<juce::Component*>& out)
{
    if (!node.isVisible()) {
        return;
    }
    const std::string cid = node.getComponentID().toStdString();
    if (!cid.empty() && UiControlAuditor::wildcard_match(pattern, cid)) {
        out.push_back(&node);
    }
    for (int i = 0; i < node.getNumChildComponents(); ++i) {
        if (auto* child = node.getChildComponent(i)) {
            find_matching_recursive(*child, pattern, out);
        }
    }
}

bool matches_any_pattern(const std::vector<std::string>& patterns, const std::string& id)
{
    for (const auto& pat : patterns) {
        if (UiControlAuditor::wildcard_match(pat, id)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool UiControlAuditor::wildcard_match(const std::string& pattern, const std::string& value) noexcept
{
    size_t p = 0;
    size_t v = 0;
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;

    while (v < value.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == value[v])) {
            ++p;
            ++v;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starIdx = p++;
            matchIdx = v;
        } else if (starIdx != std::string::npos) {
            p = starIdx + 1;
            v = ++matchIdx;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

bool UiControlAuditor::is_interactive_component(juce::Component& comp) noexcept
{
    if (is_inside_atomic_juce_widget(comp)) {
        return false;
    }
    if (dynamic_cast<TutorialHighlightOverlay*>(&comp) != nullptr ||
        dynamic_cast<juce::TooltipWindow*>(&comp) != nullptr ||
        dynamic_cast<OaeMeterComponent*>(&comp) != nullptr ||
        dynamic_cast<OaeSegmentedControl*>(&comp) != nullptr) {
        return false;
    }

    bool clicksOnThis = false;
    bool clicksOnChildren = false;
    comp.getInterceptsMouseClicks(clicksOnThis, clicksOnChildren);
    if (!clicksOnThis) {
        return false;
    }

    if (auto* lbl = dynamic_cast<juce::Label*>(&comp)) {
        return lbl->isEditable();
    }
    if (auto* ed = dynamic_cast<juce::TextEditor*>(&comp)) {
        return !ed->isReadOnly();
    }
    if (dynamic_cast<juce::ScrollBar*>(&comp) != nullptr) {
        return comp.getComponentID().isNotEmpty();
    }
    if (dynamic_cast<juce::Button*>(&comp) != nullptr ||
        dynamic_cast<juce::Slider*>(&comp) != nullptr ||
        dynamic_cast<juce::ComboBox*>(&comp) != nullptr ||
        dynamic_cast<juce::TreeView*>(&comp) != nullptr ||
        dynamic_cast<juce::ListBox*>(&comp) != nullptr ||
        dynamic_cast<ITestableCustomControl*>(&comp) != nullptr) {
        return true;
    }
    if (comp.getComponentID().startsWith("dock_splitter_")) {
        return true;
    }
    return false;
}

std::vector<juce::Component*> UiControlAuditor::collect_active_roots(MainComponent& mainComp)
{
    std::vector<juce::Component*> roots;
    roots.push_back(&mainComp);
    const int numTop = juce::TopLevelWindow::getNumTopLevelWindows();
    for (int i = 0; i < numTop; ++i) {
        auto* tlw = juce::TopLevelWindow::getTopLevelWindow(i);
        if (tlw == nullptr || !tlw->isVisible()) {
            continue;
        }
        if (auto* dw = dynamic_cast<juce::DocumentWindow*>(tlw)) {
            if (auto* content = dw->getContentComponent()) {
                if (content != &mainComp && !mainComp.isParentOf(content)) {
                    roots.push_back(content);
                }
            }
        }
    }
    return roots;
}

juce::Component* UiControlAuditor::find_component_by_id(MainComponent& mainComp, const std::string& id)
{
    for (auto* root : collect_active_roots(mainComp)) {
        if (auto* found = find_id_recursive(*root, id)) {
            return found;
        }
    }
    return nullptr;
}

std::vector<juce::Component*> UiControlAuditor::find_components_matching(MainComponent& mainComp, const std::string& pattern)
{
    std::vector<juce::Component*> out;
    for (auto* root : collect_active_roots(mainComp)) {
        find_matching_recursive(*root, pattern, out);
    }
    return out;
}

void UiControlAuditor::audit_current_context(MainComponent& mainComp,
                                             const std::string& contextName,
                                             const std::vector<std::string>& coveredPatterns,
                                             const std::string& activeFilter,
                                             AuditReport& ioReport)
{
    ++ioReport.contextsAudited;
    std::vector<juce::Component*> discovered;
    for (auto* root : collect_active_roots(mainComp)) {
        collect_visible_interactive_recursive(*root, discovered);
    }

    std::set<std::string> seenInContext;

    for (auto* comp : discovered) {
        if (comp == nullptr) continue;
        ++ioReport.controlsDiscovered;

        const std::string cid = comp->getComponentID().toStdString();
        const std::string cpath = build_component_path(*comp);

        if (cid.empty()) {
            ioReport.violations.push_back({
                contextName,
                FailureCode::MissingId,
                "<empty>",
                cpath,
                "Interactive control has empty ComponentID"
            });
            continue;
        }

        ioReport.uniqueControlIds.insert(cid);

        if (!seenInContext.insert(cid).second) {
            ioReport.violations.push_back({
                contextName,
                FailureCode::DuplicateId,
                cid,
                cpath,
                "Duplicate ComponentID visible simultaneously in context '" + contextName + "'"
            });
        }

        if (comp->getWidth() <= 0 || comp->getHeight() <= 0) {
            ioReport.violations.push_back({
                contextName,
                FailureCode::ZeroBounds,
                cid,
                cpath,
                "Interactive control has non-positive bounds (" + std::to_string(comp->getWidth()) + "x" + std::to_string(comp->getHeight()) + ")"
            });
            continue;
        }

        const auto savedViewports = UiInteractionDriver::scroll_into_view(*comp);
        if (const auto* custom = dynamic_cast<const ITestableCustomControl*>(comp)) {
            const auto targets = custom->get_testable_targets();
            if (targets.empty()) {
                ioReport.violations.push_back({
                    contextName,
                    FailureCode::ZeroBounds,
                    cid,
                    cpath,
                    "ITestableCustomControl returned zero testable targets"
                });
            }
            for (const auto& t : targets) {
                if (t.localBounds.getWidth() <= 0 || t.localBounds.getHeight() <= 0) {
                    ioReport.violations.push_back({
                        contextName,
                        FailureCode::ZeroBounds,
                        cid + "#" + t.id,
                        cpath,
                        "Custom target '" + t.id + "' has non-positive localBounds"
                    });
                    continue;
                }
                if (!t.enabled || !comp->isEnabled()) {
                    continue;
                }
                const auto pf = UiInteractionDriver::preflight(*comp, t.localBounds.getCentre(), true, t.localBounds);
                if (!pf.ok && (pf.code == FailureCode::Clipped || pf.code == FailureCode::OccludedHitTest || pf.code == FailureCode::ZeroBounds)) {
                    ioReport.violations.push_back({
                        contextName,
                        pf.code,
                        cid + "#" + t.id,
                        cpath,
                        pf.detail
                    });
                }
            }
        } else if (comp->isEnabled()) {
            const auto pf = UiInteractionDriver::preflight(*comp, std::nullopt, true);
            if (!pf.ok && (pf.code == FailureCode::Clipped || pf.code == FailureCode::OccludedHitTest || pf.code == FailureCode::ZeroBounds)) {
                ioReport.violations.push_back({
                    contextName,
                    pf.code,
                    cid,
                    cpath,
                    pf.detail
                });
            }
        }
        UiInteractionDriver::restore_viewports(savedViewports);

        const bool inFilterScope = activeFilter.empty() || wildcard_match(activeFilter, cid);
        if (inFilterScope && !matches_any_pattern(coveredPatterns, cid)) {
            if (ioReport.uncoveredIds.insert(cid).second) {
                ioReport.violations.push_back({
                    contextName,
                    FailureCode::Uncovered,
                    cid,
                    cpath,
                    "No behavioral/acoustic test contract registered covering '" + cid + "'"
                });
            }
        }
    }
}

AuditReport UiControlAuditor::audit_all_contexts(MainComponent& mainComp,
                                                 const std::vector<std::string>& coveredPatterns,
                                                 const std::string& activeFilter)
{
    AuditReport report;
    auto& model = mainComp.authoring_model();
    const std::string mainEv = "event:/test/main";

    mainComp.docking_layout().reset_layout();
    mainComp.sync_workspace_layout();
    mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Timeline);
    mainComp.contextual_deck().set_active_tab(DeckTab::Mixer);
    model.select({ ItemType::Event, mainEv, -1, -1, -1 });
    UiInteractionDriver::drain_messages(2);
    audit_current_context(mainComp, "1_root_default", coveredPatterns, activeFilter, report);

    const char* browserTabs[3] = {
        "btn_browser_tab_snapshots",
        "btn_browser_tab_buses",
        "btn_browser_tab_assets"
    };
    for (const char* tabId : browserTabs) {
        if (auto* btn = find_component_by_id(mainComp, tabId)) {
            UiInteractionDriver::click(*btn);
        }
    }
    audit_current_context(mainComp, "2_browser_tabs", coveredPatterns, activeFilter, report);
    if (auto* evTab = find_component_by_id(mainComp, "btn_browser_tab_events")) {
        UiInteractionDriver::click(*evTab);
    }

    model.select({ ItemType::Event, mainEv, 0, -1, -1 });
    if (auto* autoBtn = find_component_by_id(mainComp, "track_auto_0")) {
        UiInteractionDriver::click(*autoBtn);
    }
    UiInteractionDriver::drain_messages(2);
    audit_current_context(mainComp, "3_track_and_automation", coveredPatterns, activeFilter, report);
    if (auto* autoBtn = find_component_by_id(mainComp, "track_auto_0")) {
        UiInteractionDriver::click(*autoBtn);
    }

    const std::pair<int, int> clipSelections[6] = {
        { 0, 0 },
        { 1, 0 },
        { 2, 0 },
        { 2, 1 },
        { 2, 2 },
        { 2, 3 }
    };
    const DeckTab deckTabs[6] = {
        DeckTab::Effects,
        DeckTab::Playlist,
        DeckTab::Logic,
        DeckTab::Spatializer,
        DeckTab::Parameters,
        DeckTab::Diagnostics
    };
    for (size_t i = 0; i < 6; ++i) {
        model.select({ ItemType::Event, mainEv, clipSelections[i].first, clipSelections[i].second, -1 });
        mainComp.contextual_deck().set_active_tab(deckTabs[i]);
        UiInteractionDriver::drain_messages(2);
        audit_current_context(mainComp, "4_clips_and_decks_" + std::to_string(i), coveredPatterns, activeFilter, report);
    }

    mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Layers);
    model.select({ ItemType::Event, mainEv, 0, -1, -1 });
    UiInteractionDriver::drain_messages(2);
    audit_current_context(mainComp, "5_layers_mode", coveredPatterns, activeFilter, report);
    mainComp.timeline_panel().set_view_mode(TimelinePanel::StageViewMode::Timeline);

    const ItemSelection entities[6] = {
        { ItemType::Bus, "bus:/sfx", -1, -1, -1 },
        { ItemType::GlobalParameter, "Intensity", -1, -1, -1 },
        { ItemType::GlobalParameter, "Surface", -1, -1, -1 },
        { ItemType::Snapshot, "snapshot:/duck", -1, -1, -1 },
        { ItemType::Vca, "vca:/master_vca", -1, -1, -1 },
        { ItemType::Sample, "tone_440", -1, -1, -1 }
    };
    for (size_t i = 0; i < 6; ++i) {
        model.select(entities[i]);
        UiInteractionDriver::drain_messages(2);
        audit_current_context(mainComp, "6_inspector_entities_" + std::to_string(i), coveredPatterns, activeFilter, report);
    }

    mainComp.docking_layout().set_panel_slot(DockPanelId::Transport, DockSlotId::Floating);
    mainComp.sync_workspace_layout();
    mainComp.contextual_deck().set_active_tab(DeckTab::Parameters);
    UiInteractionDriver::drain_messages(2);
    if (auto* popBtn = find_component_by_id(mainComp, "btn_dock_pop_out")) {
        UiInteractionDriver::click(*popBtn);
    }
    UiInteractionDriver::drain_messages(20);
    audit_current_context(mainComp, "7_popout_window", coveredPatterns, activeFilter, report);

    if (auto* paramsDockBack = find_component_by_id(mainComp, "btn_params_pop_out")) {
        UiInteractionDriver::click(*paramsDockBack);
    }
    if (auto* trnDockBack = find_component_by_id(mainComp, "btn_floating_dock_back")) {
        UiInteractionDriver::click(*trnDockBack);
    }
    mainComp.docking_layout().reset_layout();
    mainComp.sync_workspace_layout();
    mainComp.contextual_deck().set_active_tab(DeckTab::Mixer);
    model.select({ ItemType::Event, mainEv, -1, -1, -1 });
    UiInteractionDriver::drain_messages(20);

    return report;
}

} // namespace oae::studio::testing
