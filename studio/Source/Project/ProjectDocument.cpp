#include "ProjectDocument.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <miniaudio.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace oae::studio {

ProjectDocument::ProjectDocument() = default;

bool ProjectDocument::write_atomic(const fs::path& target_path, const std::string& content)
{
    const fs::path temp_path = target_path.string() + ".tmp";

    // 1. Write content to temp file
    {
        std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        if (!ofs.good()) {
            std::error_code ec;
            fs::remove(temp_path, ec);
            return false;
        }
    }

    // 2. Atomically replace target file
#ifdef _WIN32
    const std::wstring target_w = target_path.wstring();
    const std::wstring temp_w = temp_path.wstring();

    if (fs::exists(target_path)) {
        if (!ReplaceFileW(target_w.c_str(), temp_w.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
            // Fallback to MoveFileExW
            if (!MoveFileExW(temp_w.c_str(), target_w.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::error_code ec;
                fs::remove(temp_path, ec);
                return false;
            }
        }
    } else {
        if (!MoveFileExW(temp_w.c_str(), target_w.c_str(), MOVEFILE_WRITE_THROUGH)) {
            std::error_code ec;
            fs::remove(temp_path, ec);
            return false;
        }
    }
    return true;
#else
    std::error_code ec;
    fs::rename(temp_path, target_path, ec);
    if (ec) {
        fs::remove(temp_path, ec);
        return false;
    }
    return true;
#endif
}

bool ProjectDocument::create_empty(const fs::path& directory, const std::string& name)
{
    std::error_code ec;
    fs::create_directories(directory / "assets", ec);
    fs::create_directories(directory / "build", ec);

    project_dir_ = fs::absolute(directory);
    project_file_ = project_dir_ / "project.json";
    project_name_ = name.empty() ? directory.filename().string() : name;

    root_ = nlohmann::json{
        { "version", 1 },
        { "name", project_name_ },
        { "buses", nlohmann::json::array({
            { { "path", "bus:/sfx" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } },
            { { "path", "bus:/music" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } }
        }) },
        { "global_parameters", nlohmann::json::array() },
        { "samples", nlohmann::json::array() },
        { "events", nlohmann::json::array() },
        { "effect_automations", nlohmann::json::array() },
        { "vcas", nlohmann::json::array() },
        { "mixer", {
            { "master", { { "volume", 1.0 }, { "mute", false } } }
        } }
    };

    loaded_ = true;
    return save();
}

bool ProjectDocument::open(const fs::path& project_file_path)
{
    if (!fs::exists(project_file_path)) {
        return false;
    }

    std::ifstream ifs(project_file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    nlohmann::json parsed_json;
    try {
        ifs >> parsed_json;
    } catch (...) {
        return false;
    }

    if (!parsed_json.contains("version") || parsed_json["version"].get<int>() != 1) {
        return false;
    }

    root_ = std::move(parsed_json);
    project_file_ = fs::absolute(project_file_path);
    project_dir_ = project_file_.parent_path();
    project_name_ = root_.value("name", project_dir_.filename().string());
    loaded_ = true;

    return true;
}

bool ProjectDocument::save()
{
    if (!loaded_ || project_file_.empty()) {
        return false;
    }
    return save_to(project_file_);
}

void ProjectDocument::close()
{
    loaded_ = false;
    project_file_.clear();
    project_dir_.clear();
    project_name_.clear();
    root_.clear();
}

bool ProjectDocument::save_to(const fs::path& target_file_path)
{
    const std::string serialized = root_.dump(2);
    if (!write_atomic(target_file_path, serialized)) {
        return false;
    }

    project_file_ = fs::absolute(target_file_path);
    project_dir_ = project_file_.parent_path();
    return true;
}

nlohmann::json ProjectDocument::create_immutable_snapshot() const
{
    return root_;
}

bool ProjectDocument::import_audio_file(const fs::path& source_path, const std::string& sample_id)
{
    if (!loaded_ || !fs::exists(source_path)) {
        return false;
    }

    const fs::path assets_dir = project_dir_ / "assets";
    std::error_code ec;
    fs::create_directories(assets_dir, ec);

    const fs::path target_file = assets_dir / source_path.filename();
    if (fs::absolute(source_path) != fs::absolute(target_file)) {
        fs::copy_file(source_path, target_file, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return false;
        }
    }

    const std::string rel_path_str = "assets/" + source_path.filename().string();

    // Check if sample_id already exists and update or append
    auto& samples = root_["samples"];
    bool found = false;
    for (auto& s : samples) {
        if (s.value("id", "") == sample_id) {
            s["file"] = rel_path_str;
            found = true;
            break;
        }
    }

    if (!found) {
        samples.push_back({
            { "id", sample_id },
            { "file", rel_path_str }
        });
    }

    return true;
}

AudioImportResult ProjectDocument::import_audio_file_safe(const fs::path& source_path, const std::string& preferred_sample_id)
{
    if (!loaded_ || !fs::exists(source_path)) {
        return { false, "", "", 0.0 };
    }

    std::string ext = source_path.extension().string();
    std::string ext_lower = ext;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext_lower != ".wav" && ext_lower != ".ogg" && ext_lower != ".mp3" && ext_lower != ".flac") {
        return { false, "", "", 0.0 };
    }

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#if defined(_WIN32)
    ma_result res = ma_decoder_init_file_w(source_path.c_str(), &config, &decoder);
#else
    ma_result res = ma_decoder_init_file(source_path.u8string().c_str(), &config, &decoder);
#endif
    if (res != MA_SUCCESS) {
        return { false, "", "", 0.0 };
    }

    ma_uint64 totalFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS || totalFrames == 0) {
        ma_uint64 framesCount = 0;
        ma_uint64 read = 0;
        std::vector<float> probeBuf(4096 * (decoder.outputChannels > 0 ? decoder.outputChannels : 2));
        while (ma_decoder_read_pcm_frames(&decoder, probeBuf.data(), 4096, &read) == MA_SUCCESS && read > 0) {
            framesCount += read;
        }
        totalFrames = framesCount;
    }

    double duration_ms = 0.0;
    if (decoder.outputSampleRate > 0 && totalFrames > 0) {
        duration_ms = (static_cast<double>(totalFrames) / decoder.outputSampleRate) * 1000.0;
    }
    ma_decoder_uninit(&decoder);

    const fs::path assets_dir = project_dir_ / "assets";
    std::error_code ec;
    fs::create_directories(assets_dir, ec);

    const std::string stem = source_path.stem().string();
    fs::path target_file = assets_dir / source_path.filename();

    bool is_same_file = false;
    if (fs::exists(target_file, ec)) {
        if (fs::equivalent(source_path, target_file, ec)) {
            is_same_file = true;
        } else if (fs::file_size(source_path, ec) == fs::file_size(target_file, ec)) {
            std::ifstream f1(source_path, std::ios::binary);
            std::ifstream f2(target_file, std::ios::binary);
            if (f1.is_open() && f2.is_open()) {
                bool identical = true;
                char b1[4096], b2[4096];
                while (f1 && f2) {
                    f1.read(b1, sizeof(b1));
                    f2.read(b2, sizeof(b2));
                    if (f1.gcount() != f2.gcount() || std::memcmp(b1, b2, static_cast<size_t>(f1.gcount())) != 0) {
                        identical = false;
                        break;
                    }
                }
                if (identical && f1.eof() && f2.eof()) {
                    is_same_file = true;
                }
            }
        }

        if (!is_same_file) {
            int suffix = 1;
            while (fs::exists(target_file, ec)) {
                target_file = assets_dir / (stem + "_" + std::to_string(suffix++) + ext);
            }
        }
    }

    if (!is_same_file) {
        fs::copy_file(source_path, target_file, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return { false, "", "", 0.0 };
        }
    }

    const std::string rel_path_str = "assets/" + target_file.filename().string();

    if (!root_.contains("samples") || !root_["samples"].is_array()) {
        root_["samples"] = nlohmann::json::array();
    }
    auto& samples = root_["samples"];

    for (const auto& s : samples) {
        if (s.value("file", "") == rel_path_str) {
            return { true, s.value("id", ""), rel_path_str, duration_ms };
        }
    }

    std::string base_id = preferred_sample_id.empty() ? target_file.stem().string() : preferred_sample_id;
    std::string final_id = base_id;
    int id_suffix = 1;
    bool conflict = true;
    while (conflict) {
        conflict = false;
        for (const auto& s : samples) {
            if (s.value("id", "") == final_id) {
                conflict = true;
                break;
            }
        }
        if (conflict) {
            final_id = base_id + "_" + std::to_string(id_suffix++);
        }
    }

    samples.push_back({
        { "id", final_id },
        { "file", rel_path_str }
    });

    return { true, final_id, rel_path_str, duration_ms };
}

std::vector<MissingAsset> ProjectDocument::check_missing_assets() const
{
    std::vector<MissingAsset> missing;
    if (!loaded_ || !root_.contains("samples") || !root_["samples"].is_array()) {
        return missing;
    }

    for (const auto& s : root_["samples"]) {
        const std::string id = s.value("id", "");
        const std::string file_str = s.value("file", "");
        if (file_str.empty()) continue;

        fs::path p(file_str);
        if (p.is_relative()) {
            p = project_dir_ / p;
        }

        if (!fs::exists(p)) {
            missing.push_back({ id, file_str });
        }
    }

    return missing;
}

bool ProjectDocument::relink_asset(const std::string& sample_id, const fs::path& new_source_path, bool copy_to_assets)
{
    if (!loaded_ || !fs::exists(new_source_path)) {
        return false;
    }

    std::string new_rel_path;
    if (copy_to_assets) {
        const fs::path assets_dir = project_dir_ / "assets";
        std::error_code ec;
        fs::create_directories(assets_dir, ec);

        const fs::path target_file = assets_dir / new_source_path.filename();
        if (fs::absolute(new_source_path) != fs::absolute(target_file)) {
            fs::copy_file(new_source_path, target_file, fs::copy_options::overwrite_existing, ec);
            if (ec) return false;
        }
        new_rel_path = "assets/" + new_source_path.filename().string();
    } else {
        std::error_code ec;
        const auto rel = fs::relative(new_source_path, project_dir_, ec);
        new_rel_path = ec ? new_source_path.string() : rel.generic_string();
    }

    if (root_.contains("samples") && root_["samples"].is_array()) {
        for (auto& s : root_["samples"]) {
            if (s.value("id", "") == sample_id) {
                s["file"] = new_rel_path;
                return true;
            }
        }
    }

    return false;
}

bool ProjectDocument::add_simple_event(const std::string& event_path, const std::string& sample_id, float length_seconds)
{
    if (!loaded_) {
        return false;
    }

    if (!root_.contains("events")) {
        root_["events"] = nlohmann::json::array();
    }

    nlohmann::json event_obj = {
        { "path", event_path },
        { "kind", "event" },
        { "bus_path", "bus:/sfx" },
        { "volume_db", 0.0f },
        { "tracks", nlohmann::json::array({
            {
                { "volume_db", 0.0f },
                { "pan", 0.0f },
                { "instruments", nlohmann::json::array({
                    {
                        { "kind", "single" },
                        { "sample_id", sample_id },
                        { "offset_ms", 0.0 },
                        { "length_ms", length_seconds * 1000.0 },
                        { "loop", false }
                    }
                }) }
            }
        }) }
    };

    root_["events"].push_back(std::move(event_obj));
    return true;
}

} // namespace oae::studio
