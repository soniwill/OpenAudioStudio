#include "UiControlTestRunner.h"
#include "UiControlContracts.h"
#include <chrono>
#include <fstream>
#include <iostream>

namespace oae::studio::testing {

namespace {

bool contract_matches_filter(const UiControlContract& contract, const std::string& filter)
{
    if (filter.empty()) {
        return true;
    }
    if (UiControlAuditor::wildcard_match(filter, contract.id) ||
        contract.id.find(filter) != std::string::npos) {
        return true;
    }
    for (const auto& pat : contract.coveredPatterns) {
        if (UiControlAuditor::wildcard_match(filter, pat) ||
            UiControlAuditor::wildcard_match(pat, filter) ||
            pat.find(filter) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

int run_ui_controls_suite(const UiControlTestOptions& opts)
{
    const auto suiteStart = std::chrono::steady_clock::now();
    std::cout << "[OAE Studio] Starting --test-ui-controls suite"
              << (opts.filter.empty() ? "" : (" (filter='" + opts.filter + "')"))
              << (opts.auditOnly ? " [audit-only]" : "") << "..." << std::endl;

    UiTestDialogSeam::ScopedActivation seamGuard;

    UiControlTestFixture fixture;
    std::string initErr;
    if (!fixture.initialize(initErr)) {
        std::cerr << "[OAE Studio] Fixture initialization failed: " << initErr << std::endl;
        return 1;
    }

    MainComponentOptions mainOpts;
    mainOpts.headlessAudio = true;
    mainOpts.layoutFile = fixture.layout_file_path();
    mainOpts.autoLoadExample = false;
    mainOpts.enableLiveBridge = false;
    mainOpts.enableTimers = false;

    MainComponent mainComp(mainOpts);
    mainComp.setVisible(true);
    mainComp.setSize(1280, 800);

    if (!fixture.load_into(mainComp, initErr)) {
        std::cerr << "[OAE Studio] Fixture load_into failed: " << initErr << std::endl;
        return 1;
    }

    std::string negErr;
    if (!UiInteractionDriver::run_preflight_negative_self_test(mainComp, negErr)) {
        std::cerr << "[OAE Studio] Pre-flight negative self-test failed: " << negErr << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Pre-flight negative self-test passed (invisible/disabled/occluded blocked)." << std::endl;

    const auto allContracts = build_all_ui_control_contracts();
    const auto allCoveredPatterns = collect_all_covered_patterns(allContracts);

    const AuditReport audit = UiControlAuditor::audit_all_contexts(mainComp, allCoveredPatterns, opts.filter);
    std::cout << "[OAE Studio] Audit completed across " << audit.contextsAudited << " contexts: "
              << audit.controlsDiscovered << " visible control instances ("
              << audit.uniqueControlIds.size() << " unique ComponentIDs), "
              << audit.violations.size() << " violations." << std::endl;

    for (const auto& v : audit.violations) {
        std::cerr << "[UI-AUDIT-FAIL] [" << v.code << "] context=" << v.context
                  << " id=" << v.componentId
                  << " path=" << v.componentPath
                  << " detail=" << v.detail << std::endl;
    }

    UiTestContext ctx{ mainComp, fixture };
    ctx.goldenRevision_ = mainComp.authoring_model().get_content_revision();
    ctx.reset_to_baseline();
    int contractsPassed = 0;
    int contractsFailed = 0;
    nlohmann::json contractsJson = nlohmann::json::array();

    if (!opts.auditOnly) {
        for (const auto& contract : allContracts) {
            if (!contract_matches_filter(contract, opts.filter)) {
                continue;
            }
            const auto cStart = std::chrono::steady_clock::now();
            ContractResult res = contract.execute(ctx);
            const auto cEnd = std::chrono::steady_clock::now();
            res.durationMs = std::chrono::duration<double, std::milli>(cEnd - cStart).count();

            if (res.passed) {
                ++contractsPassed;
                std::cout << "[UI-CONTRACT-PASS] " << contract.id
                          << " (" << static_cast<int>(res.durationMs) << " ms)" << std::endl;
            } else {
                ++contractsFailed;
                std::cerr << "[UI-CONTRACT-FAIL] " << contract.id
                          << " [" << res.code << "] " << res.message << std::endl;
            }

            contractsJson.push_back({
                { "id", contract.id },
                { "passed", res.passed },
                { "code", res.code },
                { "message", res.message },
                { "duration_ms", res.durationMs },
                { "covered_patterns", contract.coveredPatterns }
            });
        }
        ctx.reset_to_baseline();
    }

    const auto suiteEnd = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(suiteEnd - suiteStart).count();

    if (!opts.reportPath.empty()) {
        std::error_code ec;
        if (opts.reportPath.has_parent_path()) {
            std::filesystem::create_directories(opts.reportPath.parent_path(), ec);
        }
        nlohmann::json violationsJson = nlohmann::json::array();
        for (const auto& v : audit.violations) {
            violationsJson.push_back({
                { "context", v.context },
                { "code", v.code },
                { "component_id", v.componentId },
                { "component_path", v.componentPath },
                { "detail", v.detail }
            });
        }
        nlohmann::json reportObj = {
            { "filter", opts.filter },
            { "audit_only", opts.auditOnly },
            { "duration_ms", totalMs },
            { "contexts_audited", audit.contextsAudited },
            { "controls_discovered", audit.controlsDiscovered },
            { "unique_controls", audit.uniqueControlIds.size() },
            { "uncovered_controls", audit.uncoveredIds },
            { "audit_violations_count", audit.violations.size() },
            { "violations", violationsJson },
            { "contracts_passed", contractsPassed },
            { "contracts_failed", contractsFailed },
            { "contracts", contractsJson }
        };
        std::ofstream rptOut(opts.reportPath, std::ios::binary);
        if (rptOut.is_open()) {
            rptOut << reportObj.dump(2);
        }
    }

    const bool allOk = audit.violations.empty() && (contractsFailed == 0) &&
                       (opts.auditOnly || (contractsPassed > 0));
    if (allOk) {
        std::cout << "[OAE Studio] --test-ui-controls PASSED ("
                  << contractsPassed << " contracts, "
                  << audit.uniqueControlIds.size() << " unique controls audited in "
                  << static_cast<int>(totalMs) << " ms)." << std::endl;
        return 0;
    }

    std::cerr << "[OAE Studio] --test-ui-controls FAILED (audit_violations="
              << audit.violations.size() << ", contracts_failed=" << contractsFailed
              << ", contracts_passed=" << contractsPassed << ")." << std::endl;
    return 1;
}

} // namespace oae::studio::testing
