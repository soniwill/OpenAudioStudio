#pragma once

#include "../MainComponent.h"
#include "UiInteractionDriver.h"
#include "UiTestHooks.h"
#include <set>
#include <string>
#include <vector>

namespace oae::studio::testing {

/** Canonical failure codes emitted by the UI control auditor and contract runner. */
namespace FailureCode {
constexpr const char* MissingId = "MISSING_ID";
constexpr const char* DuplicateId = "DUPLICATE_ID";
constexpr const char* ZeroBounds = "ZERO_BOUNDS";
constexpr const char* Clipped = "CLIPPED";
constexpr const char* OccludedHitTest = "OCCLUDED_HITTEST";
constexpr const char* Uncovered = "UNCOVERED";
constexpr const char* ModelMismatch = "MODEL_MISMATCH";
constexpr const char* UndoMismatch = "UNDO_MISMATCH";
constexpr const char* ViewMismatch = "VIEW_MISMATCH";
constexpr const char* AudioMismatch = "AUDIO_MISMATCH";
constexpr const char* DialogTimeout = "DIALOG_TIMEOUT";
} // namespace FailureCode

/** Single structural, geometric, or coverage violation discovered during UI auditing. */
struct AuditViolation {
    std::string context;
    std::string code;
    std::string componentId;
    std::string componentPath;
    std::string detail;
};

/** Aggregated result of running UiControlAuditor across the canonical context matrix. */
struct AuditReport {
    int contextsAudited{ 0 };
    int controlsDiscovered{ 0 };
    std::set<std::string> uniqueControlIds;
    std::set<std::string> uncoveredIds;
    std::vector<AuditViolation> violations;
};

/** Walks MainComponent and floating TopLevelWindows across 7 canonical contexts enforcing Zero Orphan Controls. */
class UiControlAuditor {
public:
    /** Glob wildcard matcher supporting '*' and '?'. */
    static bool wildcard_match(const std::string& pattern, const std::string& value) noexcept;

    /** Returns true when a component is an interactive control subject to ID, reachability, and contract auditing. */
    static bool is_interactive_component(juce::Component& comp) noexcept;

    /** Enumerates MainComponent plus all active floating OaeFloatingPanelWindow content components. */
    static std::vector<juce::Component*> collect_active_roots(MainComponent& mainComp);

    /** Finds a visible component by exact ComponentID across MainComponent and all active floating windows. */
    static juce::Component* find_component_by_id(MainComponent& mainComp, const std::string& id);

    /** Finds all visible components matching a glob pattern across MainComponent and all active floating windows. */
    static std::vector<juce::Component*> find_components_matching(MainComponent& mainComp, const std::string& pattern);

    /** Audits the currently visible hierarchy across all active roots for a single named context. */
    static void audit_current_context(MainComponent& mainComp,
                                      const std::string& contextName,
                                      const std::vector<std::string>& coveredPatterns,
                                      const std::string& activeFilter,
                                      AuditReport& ioReport);

    /** Walks all 7 canonical contexts on the loaded fixture project and populates an AuditReport. */
    static AuditReport audit_all_contexts(MainComponent& mainComp,
                                          const std::vector<std::string>& coveredPatterns,
                                          const std::string& activeFilter = {});
};

} // namespace oae::studio::testing
