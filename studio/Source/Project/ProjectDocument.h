#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace oae::studio {

struct MissingAsset {
    std::string sample_id;
    std::string relative_path;
};

struct AudioImportResult {
    bool success{ false };
    std::string sample_id;
    std::string relative_path;
    double duration_ms{ 0.0 };
};

class ProjectDocument {
public:
    ProjectDocument();
    ~ProjectDocument() = default;

    // Creation and File I/O
    bool create_empty(const fs::path& directory, const std::string& name);
    bool open(const fs::path& project_file_path);
    bool save();
    bool save_to(const fs::path& target_file_path);
    void close();

    // State inspection
    bool is_loaded() const { return loaded_; }
    const fs::path& get_project_file() const { return project_file_; }
    const fs::path& get_project_dir() const { return project_dir_; }
    const std::string& get_project_name() const { return project_name_; }

    // JSON Manipulation & Snapshots
    nlohmann::json& get_root_json() { return root_; }
    const nlohmann::json& get_root_json() const { return root_; }
    nlohmann::json create_immutable_snapshot() const;

    // Asset Management
    bool import_audio_file(const fs::path& source_path, const std::string& sample_id);
    AudioImportResult import_audio_file_safe(const fs::path& source_path, const std::string& preferred_sample_id = "");
    std::vector<MissingAsset> check_missing_assets() const;
    bool relink_asset(const std::string& sample_id, const fs::path& new_source_path, bool copy_to_assets = true);

    // Helpers to populate basic events
    bool add_simple_event(const std::string& event_path, const std::string& sample_id, float length_seconds = 1.0f);

private:
    bool write_atomic(const fs::path& target_path, const std::string& content);

    bool loaded_{ false };
    fs::path project_file_;
    fs::path project_dir_;
    std::string project_name_;
    nlohmann::json root_;
};

} // namespace oae::studio
