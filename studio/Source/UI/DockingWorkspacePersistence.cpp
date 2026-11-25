#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "DockingLayoutManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <unordered_set>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace oae::studio {
namespace {

using Json = nlohmann::json;

const std::array<DockPanelId, dockPanelCount> allPanels = {
    DockPanelId::Transport, DockPanelId::Browser, DockPanelId::Timeline,
    DockPanelId::Inspector, DockPanelId::BottomDeck, DockPanelId::Diagnostics,
    DockPanelId::ProjectToolbar
};

std::string slot_to_string(DockSlotId slot)
{
    return DockingLayoutManager::slot_key(slot);
}

DockSlotId slot_from_string(const Json& value, DockSlotId fallback)
{
    if (!value.is_string()) return fallback;
    const auto name = value.get<std::string>();
    for (int i = static_cast<int>(DockSlotId::Top); i <= static_cast<int>(DockSlotId::Floating); ++i) {
        const auto slot = static_cast<DockSlotId>(i);
        if (name == DockingLayoutManager::slot_key(slot)) return slot;
    }
    return fallback;
}

Json node_to_json(const WorkspaceNode& node)
{
    Json result = Json::object();
    result["id"] = node.id;
    if (!node.children.empty()) {
        result["axis"] = node.vertical ? "vertical" : "horizontal";
        result["ratio"] = node.ratio;
        result["extent"] = node.extent;
        result["children"] = Json::array();
        for (const auto& child : node.children) result["children"].push_back(node_to_json(child));
    } else {
        result["tabs"] = Json::array();
        for (const auto panel : node.tabs) result["tabs"].push_back(DockingLayoutManager::panel_key(panel));
        result["active"] = DockingLayoutManager::panel_key(node.active);
    }
    return result;
}

bool panel_from_json(const Json& value, DockPanelId& panel)
{
    if (!value.is_string()) return false;
    const auto key = value.get<std::string>();
    for (const auto candidate : allPanels) {
        if (key == DockingLayoutManager::panel_key(candidate)) {
            panel = candidate;
            return true;
        }
    }
    return false;
}

bool node_from_json(const Json& value, WorkspaceNode& result,
                    std::unordered_set<int>& nodeIds,
                    std::unordered_set<int>& panelIds,
                    int depth = 0)
{
    if (depth > 32 || !value.is_object() || !value.contains("id") || !value["id"].is_number_integer())
        return false;
    result.id = value["id"].get<int>();
    if (result.id <= 0 || !nodeIds.insert(result.id).second) return false;

    const bool hasChildren = value.contains("children");
    if (hasChildren) {
        if (!value["children"].is_array() || value["children"].size() != 2
            || !value.contains("axis") || !value["axis"].is_string()
            || !value.contains("ratio") || !value["ratio"].is_number()
            || !value.contains("extent") || !value["extent"].is_number_integer())
            return false;
        const auto axis = value["axis"].get<std::string>();
        if (axis != "horizontal" && axis != "vertical") return false;
        result.vertical = axis == "vertical";
        result.ratio = value["ratio"].get<double>();
        if (!std::isfinite(result.ratio) || result.ratio <= 0.0 || result.ratio >= 1.0) return false;
        result.extent = value["extent"].get<int>();
        result.children.resize(2);
        return node_from_json(value["children"][0], result.children[0], nodeIds, panelIds, depth + 1)
            && node_from_json(value["children"][1], result.children[1], nodeIds, panelIds, depth + 1);
    }

    if (!value.contains("tabs") || !value["tabs"].is_array()) return false;
    for (const auto& item : value["tabs"]) {
        DockPanelId panel{};
        if (!panel_from_json(item, panel)) return false;
        const auto index = static_cast<int>(panel);
        if (!panelIds.insert(index).second) return false;
        result.tabs.push_back(panel);
    }
    if (value.contains("active")) {
        if (!panel_from_json(value["active"], result.active)) return false;
    }
    if (!result.tabs.empty() && std::find(result.tabs.begin(), result.tabs.end(), result.active) == result.tabs.end())
        result.active = result.tabs.front();
    return true;
}

Json dimensions_to_json(const DockingDimensions& dimensions)
{
    Json result;
    result["left_width"] = dimensions.left_width;
    result["right_width"] = dimensions.right_width;
    result["bottom_height"] = dimensions.bottom_height;
    result["top_height"] = dimensions.top_height;
    result["active_preset"] = dimensions.active_preset;
    result["hide_all_active"] = dimensions.hide_all_active;

    result["pre_hide_visible"] = Json::array();
    for (const bool visible : dimensions.pre_hide_visible) result["pre_hide_visible"].push_back(visible);
    result["active_tab_in_slot"] = Json::array();
    for (const int active : dimensions.active_tab_in_slot) result["active_tab_in_slot"].push_back(active);

    result["panels"] = Json::object();
    for (size_t i = 0; i < allPanels.size(); ++i) {
        result["panels"][DockingLayoutManager::panel_key(allPanels[i])] = {
            { "visible", dimensions.panels[i].visible },
            { "slot", slot_to_string(dimensions.panels[i].slot) }
        };
    }
    result["tree"] = node_to_json(dimensions.tree);
    return result;
}

bool dimensions_from_json(const Json& value, DockingDimensions& result, bool version3)
{
    if (!value.is_object()) return false;
    if (!version3) {
        if (!result.from_json(value.dump())) return false;
        result.panels[static_cast<size_t>(DockPanelId::Diagnostics)].visible = !result.hide_all_active;
        result.tree = DockingLayoutManager::workspace_from_legacy(result);
        return true;
    }

    // The flat fields keep v3 compatible with the legacy dimension parser.
    if (!result.from_json(value.dump())) return false;
    if (value.contains("pre_hide_visible")) {
        if (!value["pre_hide_visible"].is_array()) return false;
        const auto& values = value["pre_hide_visible"];
        for (size_t i = 0; i < std::min(values.size(), result.pre_hide_visible.size()); ++i) {
            if (!values[i].is_boolean()) return false;
            result.pre_hide_visible[i] = values[i].get<bool>();
        }
    }
    if (value.contains("active_tab_in_slot")) {
        if (!value["active_tab_in_slot"].is_array()) return false;
        const auto& values = value["active_tab_in_slot"];
        for (size_t i = 0; i < std::min(values.size(), result.active_tab_in_slot.size()); ++i) {
            if (!values[i].is_number_integer()) return false;
            const int active = values[i].get<int>();
            if (active < 0 || active >= static_cast<int>(DockPanelId::Count)) return false;
            result.active_tab_in_slot[i] = active;
        }
    }
    if (value.contains("panels")) {
        if (!value["panels"].is_object()) return false;
        const auto& panels = value["panels"];
        for (auto it = panels.begin(); it != panels.end(); ++it) {
            const bool known = std::any_of(allPanels.begin(), allPanels.end(), [&](DockPanelId panel) {
                return it.key() == DockingLayoutManager::panel_key(panel);
            });
            if (!known) return false;
        }
        for (size_t i = 0; i < allPanels.size(); ++i) {
            const auto key = DockingLayoutManager::panel_key(allPanels[i]);
            if (!panels.contains(key)) continue;
            const auto& panel = panels[key];
            if (!panel.is_object()) return false;
            if (panel.contains("visible")) {
                if (!panel["visible"].is_boolean()) return false;
                result.panels[i].visible = panel["visible"].get<bool>();
            }
            if (panel.contains("slot")) {
                if (!panel["slot"].is_string()) return false;
                const auto parsed = slot_from_string(panel["slot"], result.panels[i].slot);
                if (panel["slot"].get<std::string>() != slot_to_string(parsed)) return false;
                result.panels[i].slot = parsed;
            }
        }
    }
    if (!value.contains("tree")) return false;
    std::unordered_set<int> nodeIds;
    std::unordered_set<int> panelIds;
    WorkspaceNode tree;
    if (!node_from_json(value["tree"], tree, nodeIds, panelIds)) return false;
    for (const auto id : { DockPanelId::Transport, DockPanelId::Browser, DockPanelId::Timeline,
                           DockPanelId::Inspector, DockPanelId::BottomDeck }) {
        if (panelIds.find(static_cast<int>(id)) == panelIds.end()) return false;
    }

    if (panelIds.find(static_cast<int>(DockPanelId::ProjectToolbar)) == panelIds.end()) {
        int nextId = 1;
        while (nodeIds.find(nextId) != nodeIds.end()) ++nextId;
        const int projLeafId = nextId++;
        while (nodeIds.find(nextId) != nodeIds.end()) ++nextId;
        const int splitNodeId = nextId++;

        std::function<bool(WorkspaceNode&)> injectProjectToolbar = [&](WorkspaceNode& node) -> bool {
            if (node.children.empty()) {
                if (std::find(node.tabs.begin(), node.tabs.end(), DockPanelId::Transport) != node.tabs.end()) {
                    WorkspaceNode projLeaf;
                    projLeaf.id = projLeafId;
                    projLeaf.active = DockPanelId::ProjectToolbar;
                    projLeaf.tabs.push_back(DockPanelId::ProjectToolbar);

                    WorkspaceNode oldLeaf = std::move(node);
                    node = WorkspaceNode{};
                    node.id = splitNodeId;
                    node.vertical = false;
                    node.ratio = 0.25;
                    node.extent = 192;
                    node.children.push_back(std::move(projLeaf));
                    node.children.push_back(std::move(oldLeaf));
                    return true;
                }
                return false;
            }
            return injectProjectToolbar(node.children[0]) || injectProjectToolbar(node.children[1]);
        };
        injectProjectToolbar(tree);
        result.panels[static_cast<size_t>(DockPanelId::ProjectToolbar)] = { true, DockSlotId::Top };
        result.pre_hide_visible[static_cast<size_t>(DockPanelId::ProjectToolbar)] = true;
    }

    // Ensure permanent toolbar docks are never persisted as closed.
    result.panels[static_cast<size_t>(DockPanelId::Transport)].visible = true;
    result.panels[static_cast<size_t>(DockPanelId::ProjectToolbar)].visible = true;

    result.tree = std::move(tree);
    return true;
}

const Json* current_state(const Json& root)
{
    if (root.contains("state")) return &root["state"];
    if (root.contains("current")) return &root["current"];
    if (root.contains("layout")) return &root["layout"];
    return &root;
}

bool read_state_entry(const Json& entry, DockingDimensions& state, bool version3)
{
    if (!entry.is_object()) return false;
    if (entry.contains("state")) return dimensions_from_json(entry["state"], state, version3);
    return dimensions_from_json(entry, state, version3);
}

bool read_file(const fs::path& path, Json& value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    try {
        input >> value;
        return input.good() || input.eof();
    } catch (...) {
        return false;
    }
}

bool replace_file(const fs::path& temporary, const fs::path& destination)
{
#if defined(_WIN32)
    return MoveFileExW(temporary.c_str(), destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    fs::rename(temporary, destination, error);
    return !error;
#endif
}

} // namespace

std::string DockingLayoutManager::serialize_state_json() const
{
    Json root = dimensions_to_json(dims_);
    root["version"] = 3;
    root["custom_presets"] = Json::array();
    for (const auto& preset : presets_) {
        if (preset.builtin) continue;
        root["custom_presets"].push_back({
            { "name", preset.name },
            { "state", dimensions_to_json(preset.state) }
        });
    }
    return root.dump(2);
}

bool DockingLayoutManager::deserialize_state_json(const std::string& json_str)
{
    Json root;
    try {
        root = Json::parse(json_str);
    } catch (...) {
        return false;
    }
    if (!root.is_object()) return false;

    if (root.contains("version") && !root["version"].is_number_integer()) return false;
    const int version = root.value("version", 1);
    if (version < 1 || version > 3) return false;
    const bool version3 = version == 3;

    auto savedBuiltins = std::vector<DockWorkspacePreset>{};
    for (const auto& preset : presets_) if (preset.builtin) savedBuiltins.push_back(preset);
    presets_ = std::move(savedBuiltins);

    const char* presetKey = root.contains("custom_presets") ? "custom_presets" : "presets";
    if (root.contains(presetKey)) {
        const auto& entries = root[presetKey];
        if (entries.is_array()) {
            std::set<std::string> names;
            for (const auto& entry : entries) {
                if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()) continue;
                const auto name = entry["name"].get<std::string>();
                if (name.empty() || is_builtin_preset(name) || !names.insert(name).second) continue;
                DockWorkspacePreset preset;
                preset.name = name;
                preset.builtin = false;
                if (!read_state_entry(entry, preset.state, version3)) continue;
                preset.state.active_preset = name;
                presets_.push_back(std::move(preset));
            }
        } else if (entries.is_object()) {
            // Accept the early v1 map representation as well as the v2 array.
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                if (it.key().empty() || is_builtin_preset(it.key())) continue;
                DockWorkspacePreset preset;
                preset.name = it.key();
                if (!read_state_entry(it.value(), preset.state, false)) continue;
                preset.state.active_preset = preset.name;
                presets_.push_back(std::move(preset));
            }
        }
    }

    DockingDimensions candidate;
    const auto* stateJson = current_state(root);
    if (!stateJson || !dimensions_from_json(*stateJson, candidate, version3)) {
        dims_ = DockingDimensions{};
        dims_.tree = workspace_from_legacy(dims_);
        dims_.active_preset = "Default (Authoring)";
        return true;
    }

    const auto active = candidate.active_preset;
    if (active.empty() || (!is_builtin_preset(active)
        && std::none_of(presets_.begin(), presets_.end(), [&](const auto& preset) { return preset.name == active; }))) {
        dims_ = DockingDimensions{};
        dims_.tree = workspace_from_legacy(dims_);
        dims_.active_preset = "Default (Authoring)";
        return true;
    }
    dims_ = std::move(candidate);
    return true;
}

void DockingLayoutManager::load(const fs::path& layout_file_path)
{
    init_builtin_presets();
    Json root;
    if (!read_file(layout_file_path, root)) return;
    (void)deserialize_state_json(root.dump());
}

void DockingLayoutManager::save(const fs::path& layout_file_path) const
{
    std::error_code error;
    if (!layout_file_path.parent_path().empty())
        fs::create_directories(layout_file_path.parent_path(), error);
    if (error) return;

    auto temporary = layout_file_path;
    temporary += std::string(".tmp.") + juce::Uuid().toString().toStdString();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return;
        const auto contents = serialize_state_json();
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output) {
            output.close();
            fs::remove(temporary, error);
            return;
        }
    }

    Json existing;
    if (read_file(layout_file_path, existing) && existing.is_object()) {
        int version = 1;
        if (existing.contains("version")) {
            if (!existing["version"].is_number_integer()) {
                version = -1;
            } else {
                try {
                    version = existing["version"].get<int>();
                } catch (...) {
                    version = -1;
                }
            }
        }
        const auto backup = fs::path(layout_file_path.wstring() + L".legacy.bak");
        const bool backupExists = fs::exists(backup, error);
        if (error) {
            error.clear();
            fs::remove(temporary, error);
            return;
        }
        if (version != 3 && !backupExists) {
            error.clear();
            fs::copy_file(layout_file_path, backup, fs::copy_options::none, error);
            if (error) {
                error.clear();
                const bool backupCreatedByPeer = fs::exists(backup, error);
                if (!error && backupCreatedByPeer) error.clear();
                else {
                    error.clear();
                    fs::remove(temporary, error);
                    return;
                }
            }
            if (error) {
                error.clear();
                fs::remove(temporary, error);
                return;
            }
        }
    }

    if (!replace_file(temporary, layout_file_path)) {
        error.clear();
        fs::remove(temporary, error);
    }
}

} // namespace oae::studio
