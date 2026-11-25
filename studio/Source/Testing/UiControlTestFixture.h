#pragma once

#include "../MainComponent.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace oae::studio::testing {

/** Deterministic temporary project fixture with synthetic WAV tones and full authoring entities for UI control tests. */
class UiControlTestFixture {
public:
    UiControlTestFixture();
    ~UiControlTestFixture();

    UiControlTestFixture(const UiControlTestFixture&) = delete;
    UiControlTestFixture& operator=(const UiControlTestFixture&) = delete;

    /** Generates synthetic WAV assets and project.json in the temporary directory. */
    bool initialize(std::string& outError);

    /** Loads the fixture project into MainComponent, populates canonical authoring structures, and compiles preview bank once. */
    bool load_into(MainComponent& mainComp, std::string& outError);

    /** Restores the AuthoringModel to the canonical golden JSON snapshot and clears Undo/Redo history. */
    void restore_golden_model(MainComponent& mainComp);

    const std::filesystem::path& temp_dir() const noexcept { return tempDir_; }
    const std::filesystem::path& project_path() const noexcept { return projectPath_; }
    const std::filesystem::path& layout_file_path() const noexcept { return layoutFilePath_; }
    const std::filesystem::path& sample_440_path() const noexcept { return sample440Path_; }

private:
    std::filesystem::path tempDir_;
    std::filesystem::path projectPath_;
    std::filesystem::path layoutFilePath_;
    std::filesystem::path sample440Path_;
    std::filesystem::path sample880Path_;
    std::filesystem::path sampleDualPath_;
    nlohmann::json goldenJson_;
};

} // namespace oae::studio::testing
