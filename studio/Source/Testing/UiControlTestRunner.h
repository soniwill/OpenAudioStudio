#pragma once

#include <filesystem>
#include <string>

namespace oae::studio::testing {

/** CLI options for the headless Studio UI control verification runner. */
struct UiControlTestOptions {
    std::string filter;
    bool auditOnly{ false };
    std::filesystem::path reportPath;
};

/** Executes the headless Studio UI control auditor and behavioral/acoustic contract suite. */
int run_ui_controls_suite(const UiControlTestOptions& opts);

} // namespace oae::studio::testing
