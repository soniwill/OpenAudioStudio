#include "AuthoringModel.h"
#include "../UI/Theme.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace oae::studio {

namespace {

void ensure_array(nlohmann::json& parent, const char* key)
{
    if (!parent.contains(key) || !parent[key].is_array()) {
        parent[key] = nlohmann::json::array();
    }
}

bool valid_non_null_string(const std::string& s, bool allow_empty = false)
{
    if (!allow_empty && s.empty()) return false;
    return s.find('\0') == std::string::npos;
}

std::pair<double, double> parameter_json_domain(const nlohmann::json& p)
{
    if (p.value("kind", "") == "labeled") {
        const size_t n = (p.contains("labels") && p["labels"].is_array()) ? p["labels"].size() : 0;
        return { 0.0, n > 0 ? static_cast<double>(n - 1) : 0.0 };
    }
    const double mn = p.value("min", 0.0);
    const double mx = p.value("max", 1.0);
    return { mn, std::max(mn, mx) };
}

bool build_normalized_parameter(const nlohmann::json& spec, nlohmann::json& out)
{
    if (!spec.is_object()) return false;
    const std::string name = spec.value("name", "");
    if (!valid_non_null_string(name)) return false;

    const std::string kind = spec.value("kind", "continuous");
    if (kind != "continuous" && kind != "discrete" && kind != "labeled") return false;

    const double seek_up = spec.value("seek_speed", 0.0);
    const double seek_down = spec.value("seek_speed_down", 0.0);
    const double velocity = spec.value("velocity", 0.0);
    if (!std::isfinite(seek_up) || seek_up < 0.0 ||
        !std::isfinite(seek_down) || seek_down < 0.0 ||
        !std::isfinite(velocity) || !spec.value("hold_during_playback", nlohmann::json(false)).is_boolean()) {
        return false;
    }

    nlohmann::json result = {
        { "name", name },
        { "kind", kind }
    };
    if (seek_up > 0.0) result["seek_speed"] = seek_up;
    if (seek_down > 0.0) result["seek_speed_down"] = seek_down;
    if (velocity != 0.0) result["velocity"] = velocity;
    if (spec.value("hold_during_playback", false)) result["hold_during_playback"] = true;

    if (kind == "labeled") {
        std::vector<std::string> labels;
        if (spec.contains("labels")) {
            if (!spec["labels"].is_array()) return false;
            std::unordered_set<std::string> seen;
            for (const auto& item : spec["labels"]) {
                if (!item.is_string()) return false;
                const std::string s = item.get<std::string>();
                if (!valid_non_null_string(s) || !seen.insert(s).second) return false;
                labels.push_back(s);
            }
        } else {
            labels = { "Option1", "Option2" };
        }
        if (labels.empty() || labels.size() > 64) return false;
        const int max_idx = static_cast<int>(labels.size() - 1);
        const int def_idx = std::clamp(static_cast<int>(std::round(spec.value("default_value", 0.0))), 0, max_idx);
        result["labels"] = std::move(labels);
        result["default_value"] = def_idx;
    } else if (kind == "discrete") {
        const int mn = static_cast<int>(std::round(spec.value("min", 0.0)));
        const int mx = static_cast<int>(std::round(spec.value("max", 10.0)));
        if (mn > mx) return false;
        const int def = std::clamp(static_cast<int>(std::round(spec.value("default_value", static_cast<double>(mn)))), mn, mx);
        result["min"] = mn;
        result["max"] = mx;
        result["default_value"] = def;
    } else {
        const double mn = spec.value("min", 0.0);
        const double mx = spec.value("max", 100.0);
        if (!std::isfinite(mn) || !std::isfinite(mx) || mn > mx) return false;
        double def = spec.value("default_value", spec.contains("default_value") ? mn : 50.0);
        if (!std::isfinite(def)) return false;
        def = std::clamp(def, mn, mx);
        result["min"] = mn;
        result["max"] = mx;
        result["default_value"] = def;
    }

    out = std::move(result);
    return true;
}

nlohmann::json* find_event(nlohmann::json& root, const std::string& path)
{
    const auto events = root.find("events");
    if (events == root.end() || !events->is_array()) return nullptr;
    for (auto& ev : *events) {
        if (ev.value("path", "") == path) return &ev;
    }
    return nullptr;
}

const nlohmann::json* find_parameter_in_array(const nlohmann::json& list, const std::string& name)
{
    if (!list.is_array()) return nullptr;
    for (const auto& p : list) {
        if (p.is_object() && p.value("name", "") == name) return &p;
    }
    return nullptr;
}

const nlohmann::json* find_parameter_json(const nlohmann::json& root, const std::string& event_path,
                                          const std::string& scope, const std::string& name)
{
    if (scope.empty() || scope == "local") {
        if (root.contains("events") && root["events"].is_array()) {
            for (const auto& ev : root["events"]) {
                if (ev.is_object() && ev.value("path", "") == event_path && ev.contains("parameters")) {
                    if (const auto* found = find_parameter_in_array(ev["parameters"], name)) {
                        return found;
                    }
                    break;
                }
            }
        }
        if (scope == "local") return nullptr;
    }
    if ((scope.empty() || scope == "global") && root.contains("global_parameters")) {
        return find_parameter_in_array(root["global_parameters"], name);
    }
    return nullptr;
}

/** Finds an existing instrument without inserting missing tracks or clips. */
nlohmann::json* find_instruments(nlohmann::json& root, const std::string& path, size_t track)
{
    const auto events = root.find("events");
    if (events == root.end() || !events->is_array()) return nullptr;
    for (auto& event : *events) {
        if (event.value("path", "") != path) continue;
        const auto tracks = event.find("tracks");
        if (tracks == event.end() || !tracks->is_array() || track >= tracks->size()) return nullptr;
        auto& selected_track = (*tracks)[track];
        const auto instruments = selected_track.find("instruments");
        return instruments != selected_track.end() && instruments->is_array() ? &*instruments : nullptr;
    }
    return nullptr;
}

} // namespace

AuthoringModel::AuthoringModel(ProjectDocument& doc)
    : doc_(doc)
{
}

void AuthoringModel::add_listener(Listener* l)
{
    if (l != nullptr && std::find(listeners_.begin(), listeners_.end(), l) == listeners_.end()) {
        listeners_.push_back(l);
    }
}

void AuthoringModel::remove_listener(Listener* l)
{
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
}

void AuthoringModel::select(const ItemSelection& sel)
{
    if (current_selection_ != sel) {
        current_selection_ = sel;
        for (auto* l : listeners_) {
            l->on_selection_changed(current_selection_);
        }
    }
}

void AuthoringModel::clear_selection()
{
    select(ItemSelection{});
}

void AuthoringModel::push_undo_state()
{
    undo_stack_.push_back(doc_.get_root_json());
    if (undo_stack_.size() > max_history_size_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    redo_stack_.clear();
}

void AuthoringModel::begin_gesture()
{
    if (!in_gesture_) {
        in_gesture_ = true;
        gesture_start_state_ = doc_.get_root_json();
    }
}

void AuthoringModel::end_gesture()
{
    if (in_gesture_) {
        in_gesture_ = false;
        if (gesture_start_state_ != doc_.get_root_json()) {
            undo_stack_.push_back(std::move(gesture_start_state_));
            if (undo_stack_.size() > max_history_size_) {
                undo_stack_.erase(undo_stack_.begin());
            }
            redo_stack_.clear();
            notify_project_changed();
        }
    }
}

void AuthoringModel::cancel_gesture()
{
    if (in_gesture_) {
        in_gesture_ = false;
        apply_snapshot(gesture_start_state_);
    }
}

bool AuthoringModel::undo()
{
    if (undo_stack_.empty()) {
        return false;
    }
    redo_stack_.push_back(doc_.get_root_json());
    const auto prev = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    apply_snapshot(prev);
    return true;
}

bool AuthoringModel::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }
    undo_stack_.push_back(doc_.get_root_json());
    const auto next = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    apply_snapshot(next);
    return true;
}

void AuthoringModel::clear_history()
{
    undo_stack_.clear();
    redo_stack_.clear();
    in_gesture_ = false;
}

void AuthoringModel::apply_snapshot(const nlohmann::json& snapshot)
{
    doc_.get_root_json() = snapshot;
    ++content_revision_;
    notify_project_changed();
}

void AuthoringModel::modify(const std::function<void(nlohmann::json&)>& mutator)
{
    if (!in_gesture_) {
        push_undo_state();
    }
    mutator(doc_.get_root_json());
    ++content_revision_;
    notify_project_changed();
}

void AuthoringModel::modify_audition(const std::function<void(nlohmann::json&)>& mutator)
{
    if (!in_gesture_) {
        push_undo_state();
    }
    mutator(doc_.get_root_json());
    notify_project_changed();
}

void AuthoringModel::notify_project_changed()
{
    for (auto* l : listeners_) {
        l->on_project_changed();
    }
}

bool AuthoringModel::create_event(const std::string& path, const std::string& bus_path)
{
    std::string valid_path = path;
    if (valid_path.rfind("event:/", 0) != 0) {
        valid_path = "event:/" + valid_path;
    }

    // Check duplicate
    for (const auto& ev : get_event_paths()) {
        if (ev == valid_path) return false;
    }

    modify([&](nlohmann::json& root) {
        ensure_array(root, "events");
        nlohmann::json new_ev = {
            { "kind", "event" },
            { "path", valid_path },
            { "bus_path", bus_path.empty() ? "bus:/" : bus_path },
            { "volume_db", 0.0 },
            { "pitch_semitones", 0.0 },
            { "pan", 0.0 },
            { "max_instances", 64 },
            { "steal_mode", "oldest" },
            { "cooldown_ms", 0 },
            { "priority", 128 },
            { "stop_fadeout_ms", 0 },
            { "parameters", nlohmann::json::array() },
            { "tracks", nlohmann::json::array({
                {
                    { "volume_db", 0.0 },
                    { "pan", 0.0 },
                    { "automations", nlohmann::json::array() },
                    { "instruments", nlohmann::json::array() }
                }
            }) },
            { "logic_items", nlohmann::json::array() },
            { "modulators", nlohmann::json::array() },
            { "automations", nlohmann::json::array() }
        };
        root["events"].push_back(std::move(new_ev));
    });

    select({ ItemType::Event, valid_path });
    return true;
}

bool AuthoringModel::create_snapshot(const std::string& path)
{
    std::string valid_path = path;
    if (valid_path.rfind("snapshot:/", 0) != 0) {
        valid_path = "snapshot:/" + valid_path;
    }

    for (const auto& sn : get_snapshot_paths()) {
        if (sn == valid_path) return false;
    }

    modify([&](nlohmann::json& root) {
        ensure_array(root, "events");
        nlohmann::json new_sn = {
            { "kind", "snapshot" },
            { "path", valid_path },
            { "snapshot_mode", "override" },
            { "fade_in_ms", 100.0 },
            { "priority", 128 },
            { "stop_fadeout_ms", 100 },
            { "parameters", nlohmann::json::array() },
            { "snapshot_targets", nlohmann::json::array() }
        };
        root["events"].push_back(std::move(new_sn));
    });

    select({ ItemType::Snapshot, valid_path });
    return true;
}

bool AuthoringModel::create_bus(const std::string& path, const std::string& parent)
{
    std::string valid_path = path;
    if (valid_path.rfind("bus:/", 0) != 0) {
        valid_path = "bus:/" + valid_path;
    }

    for (const auto& b : get_bus_paths()) {
        if (b == valid_path) return false;
    }

    modify([&](nlohmann::json& root) {
        ensure_array(root, "buses");
        nlohmann::json new_bus = {
            { "path", valid_path },
            { "parent", parent.empty() ? "bus:/" : parent },
            { "volume", 1.0 },
            { "effects", nlohmann::json::array() }
        };
        root["buses"].push_back(std::move(new_bus));
    });

    select({ ItemType::Bus, valid_path });
    return true;
}

bool AuthoringModel::create_vca(const std::string& path)
{
    std::string valid_path = path;
    if (valid_path.rfind("vca:/", 0) != 0) {
        valid_path = "vca:/" + valid_path;
    }

    for (const auto& v : get_vca_paths()) {
        if (v == valid_path) return false;
    }

    modify([&](nlohmann::json& root) {
        ensure_array(root, "vcas");
        nlohmann::json new_vca = {
            { "path", valid_path },
            { "buses", nlohmann::json::array() },
            { "events", nlohmann::json::array() }
        };
        root["vcas"].push_back(std::move(new_vca));
    });

    select({ ItemType::Vca, valid_path });
    return true;
}

bool AuthoringModel::create_global_parameter(const std::string& name, const std::string& kind)
{
    if (name.empty()) return false;
    for (const auto& gp : get_global_parameter_names()) {
        if (gp == name) return false;
    }

    nlohmann::json param;
    if (!build_normalized_parameter({ { "name", name }, { "kind", kind } }, param)) {
        return false;
    }

    modify([&](nlohmann::json& root) {
        ensure_array(root, "global_parameters");
        root["global_parameters"].push_back(std::move(param));
    });

    select({ ItemType::GlobalParameter, name });
    return true;
}

bool AuthoringModel::import_sample(const std::string& id, const std::string& relative_file)
{
    if (id.empty() || relative_file.empty()) return false;

    modify([&](nlohmann::json& root) {
        ensure_array(root, "samples");
        for (auto& s : root["samples"]) {
            if (s.value("id", "") == id) {
                s["file"] = relative_file;
                return;
            }
        }
        root["samples"].push_back({
            { "id", id },
            { "file", relative_file }
        });
    });

    select({ ItemType::Sample, id });
    return true;
}

bool AuthoringModel::import_audio_asset(const fs::path& source_path, std::string* out_sample_id, double* out_duration_ms)
{
    const bool was_in_gesture = in_gesture_;
    if (!was_in_gesture) {
        begin_gesture();
    }

    const auto res = doc_.import_audio_file_safe(source_path);
    if (!res.success) {
        if (!was_in_gesture) {
            cancel_gesture();
        }
        return false;
    }

    if (out_sample_id) *out_sample_id = res.sample_id;
    if (out_duration_ms) *out_duration_ms = res.duration_ms;

    select({ ItemType::Sample, res.sample_id });
    ++content_revision_;

    if (!was_in_gesture) {
        end_gesture();
    } else {
        notify_project_changed();
    }
    return true;
}

bool AuthoringModel::add_single_instrument_from_audio_file(const std::string& event_path, size_t track_index,
                                                           const fs::path& source_path, double cursor_time_ms,
                                                           std::string* out_sample_id, double* out_duration_ms)
{
    const bool was_in_gesture = in_gesture_;
    if (!was_in_gesture) {
        begin_gesture();
    }

    std::string sample_id;
    double duration_ms = 0.0;
    if (!import_audio_asset(source_path, &sample_id, &duration_ms)) {
        if (!was_in_gesture) {
            cancel_gesture();
        }
        return false;
    }

    if (out_sample_id) *out_sample_id = sample_id;
    if (out_duration_ms) *out_duration_ms = duration_ms;

    bool instrument_added = false;
    modify([&](nlohmann::json& root) {
        ensure_array(root, "events");
        for (auto& evt : root["events"]) {
            if (evt.value("path", "") == event_path) {
                ensure_array(evt, "tracks");
                if (track_index < evt["tracks"].size()) {
                    auto& track = evt["tracks"][track_index];
                    ensure_array(track, "instruments");

                    nlohmann::json inst = {
                        { "kind", "single" },
                        { "sample_id", sample_id },
                        { "offset_ms", std::max(0.0, cursor_time_ms) },
                        { "length_ms", duration_ms > 0.0 ? duration_ms : 1000.0 },
                        { "start_offset_ms", 0.0 },
                        { "volume", 1.0 },
                        { "pitch", 0.0 },
                        { "loop", false },
                        { "stream", false }
                    };
                    track["instruments"].push_back(inst);
                    const int inst_idx = static_cast<int>(track["instruments"].size() - 1);
                    select({ ItemType::Event, event_path, static_cast<int>(track_index), inst_idx });
                    instrument_added = true;
                }
                break;
            }
        }
    });

    if (!instrument_added) {
        if (!was_in_gesture) {
            cancel_gesture();
        }
        return false;
    }

    if (!was_in_gesture) {
        end_gesture();
    }
    return true;
}

bool AuthoringModel::add_stem_layer(const std::string& event_path, const std::string& sample_id)
{
    bool added = false;
    size_t new_track_index = 0;
    modify([&](nlohmann::json& root) {
        auto* ev = find_event(root, event_path);
        if (!ev) return;

        ensure_array(*ev, "tracks");
        new_track_index = (*ev)["tracks"].size();

        std::string resolved_sample = sample_id;
        if (resolved_sample.empty() && root.contains("samples") && root["samples"].is_array() && !root["samples"].empty()) {
            resolved_sample = root["samples"][0].value("id", "");
        }

        nlohmann::json inst = {
            { "kind", "single" },
            { "sample_id", resolved_sample },
            { "offset_ms", 0.0 },
            { "length_ms", 2000.0 },
            { "start_offset_ms", 0.0 },
            { "loop", true },
            { "fade_in_ms", 0.0 },
            { "fade_out_ms", 0.0 },
            { "volume_db_base", 0.0 },
            { "pitch_semitones_base", 0.0 },
            { "volume_db_random_range", 0.0 },
            { "pitch_semitones_random_range", 0.0 },
            { "modulators", nlohmann::json::array() }
        };

        (*ev)["tracks"].push_back({
            { "volume_db", 0.0 },
            { "pan", 0.0 },
            { "automations", nlohmann::json::array() },
            { "instruments", nlohmann::json::array({ std::move(inst) }) }
        });

        if (!ev->contains("user_props") || !(*ev)["user_props"].is_object()) {
            (*ev)["user_props"] = nlohmann::json::object();
        }
        (*ev)["user_props"]["track_" + std::to_string(new_track_index) + "_name"] =
            "Stem " + std::to_string(new_track_index + 1);

        added = true;
    });

    if (added) {
        select({ ItemType::Event, event_path, static_cast<int>(new_track_index), 0 });
    }
    return added;
}

bool AuthoringModel::import_audio_stem_layer(const std::string& event_path, const fs::path& source_path,
                                             std::string* out_sample_id, double* out_duration_ms)
{
    const bool was_in_gesture = in_gesture_;
    if (!was_in_gesture) {
        begin_gesture();
    }

    std::string sample_id;
    double duration_ms = 0.0;
    if (!import_audio_asset(source_path, &sample_id, &duration_ms)) {
        if (!was_in_gesture) {
            cancel_gesture();
        }
        return false;
    }

    if (out_sample_id) *out_sample_id = sample_id;
    if (out_duration_ms) *out_duration_ms = duration_ms;

    bool added = false;
    size_t new_track_index = 0;
    modify([&](nlohmann::json& root) {
        auto* ev = find_event(root, event_path);
        if (!ev) return;

        ensure_array(*ev, "tracks");
        new_track_index = (*ev)["tracks"].size();

        nlohmann::json inst = {
            { "kind", "single" },
            { "sample_id", sample_id },
            { "offset_ms", 0.0 },
            { "length_ms", duration_ms > 0.0 ? duration_ms : 2000.0 },
            { "start_offset_ms", 0.0 },
            { "loop", true },
            { "fade_in_ms", 0.0 },
            { "fade_out_ms", 0.0 },
            { "volume_db_base", 0.0 },
            { "pitch_semitones_base", 0.0 },
            { "volume_db_random_range", 0.0 },
            { "pitch_semitones_random_range", 0.0 },
            { "modulators", nlohmann::json::array() }
        };

        (*ev)["tracks"].push_back({
            { "volume_db", 0.0 },
            { "pan", 0.0 },
            { "automations", nlohmann::json::array() },
            { "instruments", nlohmann::json::array({ std::move(inst) }) }
        });

        std::string track_name = source_path.stem().string();
        if (track_name.empty()) {
            track_name = "Stem " + std::to_string(new_track_index + 1);
        }
        if (!ev->contains("user_props") || !(*ev)["user_props"].is_object()) {
            (*ev)["user_props"] = nlohmann::json::object();
        }
        (*ev)["user_props"]["track_" + std::to_string(new_track_index) + "_name"] = track_name;

        select({ ItemType::Event, event_path, static_cast<int>(new_track_index), 0 });
        added = true;
    });

    if (!added) {
        if (!was_in_gesture) {
            cancel_gesture();
        }
        return false;
    }

    if (!was_in_gesture) {
        end_gesture();
    }
    return true;
}

bool AuthoringModel::rename_item(ItemType type, const std::string& old_id, const std::string& new_id)
{
    if (old_id.empty() || new_id.empty() || old_id == new_id) {
        return false;
    }

    modify([&](nlohmann::json& root) {
        switch (type) {
            case ItemType::Event: {
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        if (ev.value("kind", "event") == "event") {
                            if (ev.value("path", "") == old_id) {
                                ev["path"] = new_id;
                            }
                            if (ev.contains("tracks") && ev["tracks"].is_array()) {
                                for (auto& trk : ev["tracks"]) {
                                    if (trk.contains("instruments") && trk["instruments"].is_array()) {
                                        for (auto& inst : trk["instruments"]) {
                                            if (inst.value("kind", "") == "event" && inst.value("target_event", "") == old_id) {
                                                inst["target_event"] = new_id;
                                            }
                                            if (inst.value("kind", "") == "command" && inst.value("command_target", "") == old_id) {
                                                const auto ct = inst.value("command_type", "");
                                                if (ct == "start_event" || ct == "stop_event") {
                                                    inst["command_target"] = new_id;
                                                }
                                            }
                                            if ((inst.value("kind", "") == "multi" || inst.value("kind", "") == "scatterer") &&
                                                inst.contains("playlist") && inst["playlist"].is_array()) {
                                                for (auto& pe : inst["playlist"]) {
                                                    if (pe.value("target_event", "") == old_id) {
                                                        pe["target_event"] = new_id;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    for (auto& v : root["vcas"]) {
                        if (v.contains("events") && v["events"].is_array()) {
                            for (auto& e : v["events"]) {
                                if (e == old_id) e = new_id;
                            }
                        }
                    }
                }
                break;
            }

            case ItemType::Snapshot: {
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        if (ev.value("kind", "") == "snapshot") {
                            if (ev.value("path", "") == old_id) {
                                ev["path"] = new_id;
                            }
                        } else {
                            if (ev.contains("tracks") && ev["tracks"].is_array()) {
                                for (auto& trk : ev["tracks"]) {
                                    if (trk.contains("instruments") && trk["instruments"].is_array()) {
                                        for (auto& inst : trk["instruments"]) {
                                            if (inst.value("kind", "") == "command" && inst.value("command_target", "") == old_id) {
                                                const auto ct = inst.value("command_type", "");
                                                if (ct == "snapshot_on" || ct == "snapshot_off") {
                                                    inst["command_target"] = new_id;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }

            case ItemType::Bus: {
                if (old_id == "bus:/") return; // cannot rename master
                if (root.contains("buses") && root["buses"].is_array()) {
                    for (auto& b : root["buses"]) {
                        if (b.value("path", "") == old_id) {
                            b["path"] = new_id;
                        }
                        if (b.value("parent", "") == old_id) {
                            b["parent"] = new_id;
                        }
                        if (b.contains("effects") && b["effects"].is_array()) {
                            for (auto& fx : b["effects"]) {
                                if (fx.value("sidechain_bus", "") == old_id) {
                                    fx["sidechain_bus"] = new_id;
                                }
                            }
                        }
                    }
                }
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        if (ev.value("bus_path", "") == old_id) {
                            ev["bus_path"] = new_id;
                        }
                        if (ev.contains("snapshot_targets") && ev["snapshot_targets"].is_array()) {
                            for (auto& st : ev["snapshot_targets"]) {
                                if (st.value("bus_path", "") == old_id) {
                                    st["bus_path"] = new_id;
                                }
                            }
                        }
                    }
                }
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    for (auto& v : root["vcas"]) {
                        if (v.contains("buses") && v["buses"].is_array()) {
                            for (auto& b : v["buses"]) {
                                if (b == old_id) b = new_id;
                            }
                        }
                    }
                }
                if (root.contains("effect_automations") && root["effect_automations"].is_array()) {
                    for (auto& ea : root["effect_automations"]) {
                        if (ea.value("bus_path", "") == old_id) {
                            ea["bus_path"] = new_id;
                        }
                    }
                }
                break;
            }

            case ItemType::Vca: {
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    for (auto& v : root["vcas"]) {
                        if (v.value("path", "") == old_id) {
                            v["path"] = new_id;
                        }
                    }
                }
                break;
            }

            case ItemType::Sample: {
                if (root.contains("samples") && root["samples"].is_array()) {
                    for (auto& s : root["samples"]) {
                        if (s.value("id", "") == old_id) {
                            s["id"] = new_id;
                        }
                    }
                }
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        if (ev.contains("tracks") && ev["tracks"].is_array()) {
                            for (auto& trk : ev["tracks"]) {
                                if (trk.contains("instruments") && trk["instruments"].is_array()) {
                                    for (auto& inst : trk["instruments"]) {
                                        if (inst.value("kind", "") == "single" && inst.value("sample_id", "") == old_id) {
                                            inst["sample_id"] = new_id;
                                        }
                                        if ((inst.value("kind", "") == "multi" || inst.value("kind", "") == "scatterer") &&
                                            inst.contains("playlist") && inst["playlist"].is_array()) {
                                            for (auto& pe : inst["playlist"]) {
                                                if (pe.value("sample_id", "") == old_id) {
                                                    pe["sample_id"] = new_id;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (root.contains("audio_table") && root["audio_table"].is_array()) {
                    for (auto& entry : root["audio_table"]) {
                        if (entry.value("sample_id", "") == old_id) {
                            entry["sample_id"] = new_id;
                        }
                    }
                }
                break;
            }

            case ItemType::GlobalParameter: {
                if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
                    for (auto& gp : root["global_parameters"]) {
                        if (gp.value("name", "") == old_id) {
                            gp["name"] = new_id;
                        }
                    }
                }
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        bool shadowed_by_local = false;
                        if (ev.contains("parameters") && ev["parameters"].is_array()) {
                            for (const auto& lp : ev["parameters"]) {
                                if (lp.value("name", "") == old_id) {
                                    shadowed_by_local = true;
                                    break;
                                }
                            }
                        }
                        const auto update_condition = [&](nlohmann::json& cond) {
                            if (cond.contains("terms") && cond["terms"].is_array()) {
                                for (auto& term : cond["terms"]) {
                                    if (term.contains("param") && term["param"].value("scope", "") == "global"
                                        && term["param"].value("name", "") == old_id) {
                                        term["param"]["name"] = new_id;
                                    }
                                }
                            }
                        };
                        const auto update_automations = [&](nlohmann::json& autos) {
                            if (autos.is_array()) {
                                for (auto& a : autos) {
                                    if (!a.value("is_timeline", false) && a.contains("param")
                                        && a["param"].value("scope", "") == "global" && a["param"].value("name", "") == old_id) {
                                        a["param"]["name"] = new_id;
                                    }
                                }
                            }
                        };
                        if (ev.contains("automations")) update_automations(ev["automations"]);
                        if (ev.contains("tracks") && ev["tracks"].is_array()) {
                            for (auto& trk : ev["tracks"]) {
                                if (trk.contains("automations")) update_automations(trk["automations"]);
                                if (trk.contains("instruments") && trk["instruments"].is_array()) {
                                    for (auto& inst : trk["instruments"]) {
                                        if (inst.contains("condition")) update_condition(inst["condition"]);
                                        if (!shadowed_by_local && inst.value("sheet_parameter", "") == old_id) {
                                            inst["sheet_parameter"] = new_id;
                                        }
                                    }
                                }
                            }
                        }
                        if (ev.contains("logic_items") && ev["logic_items"].is_array()) {
                            for (auto& li : ev["logic_items"]) {
                                if (li.contains("condition")) update_condition(li["condition"]);
                            }
                        }
                    }
                }
                if (root.contains("effect_automations") && root["effect_automations"].is_array()) {
                    for (auto& ea : root["effect_automations"]) {
                        if (ea.contains("param") && ea["param"].value("scope", "") == "global" && ea["param"].value("name", "") == old_id) {
                            ea["param"]["name"] = new_id;
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    });

    if (current_selection_.id_or_path == old_id && current_selection_.type == type) {
        current_selection_.id_or_path = new_id;
    }

    for (auto* l : listeners_) {
        l->on_item_renamed(type, old_id, new_id);
    }
    return true;
}

bool AuthoringModel::delete_item(ItemType type, const std::string& id)
{
    if (id.empty()) return false;

    modify([&](nlohmann::json& root) {
        switch (type) {
            case ItemType::Event:
            case ItemType::Snapshot: {
                if (root.contains("events") && root["events"].is_array()) {
                    auto& evs = root["events"];
                    for (auto it = evs.begin(); it != evs.end();) {
                        if (it->value("path", "") == id) {
                            it = evs.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                // Clean from VCAs
                if (type == ItemType::Event && root.contains("vcas") && root["vcas"].is_array()) {
                    for (auto& v : root["vcas"]) {
                        if (v.contains("events") && v["events"].is_array()) {
                            auto& ve = v["events"];
                            for (auto it = ve.begin(); it != ve.end();) {
                                if (*it == id) it = ve.erase(it); else ++it;
                            }
                        }
                    }
                }
                break;
            }

            case ItemType::Bus: {
                if (id == "bus:/") return; // cannot delete master
                if (root.contains("buses") && root["buses"].is_array()) {
                    auto& buses = root["buses"];
                    for (auto it = buses.begin(); it != buses.end();) {
                        if (it->value("path", "") == id) {
                            it = buses.erase(it);
                        } else {
                            if (it->value("parent", "") == id) it->operator[]("parent") = "bus:/";
                            ++it;
                        }
                    }
                }
                // Clean from VCAs and Events
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    for (auto& v : root["vcas"]) {
                        if (v.contains("buses") && v["buses"].is_array()) {
                            auto& vb = v["buses"];
                            for (auto it = vb.begin(); it != vb.end();) {
                                if (*it == id) it = vb.erase(it); else ++it;
                            }
                        }
                    }
                }
                if (root.contains("events") && root["events"].is_array()) {
                    for (auto& ev : root["events"]) {
                        if (ev.value("bus_path", "") == id) ev["bus_path"] = "bus:/";
                    }
                }
                break;
            }

            case ItemType::Vca: {
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    auto& vcas = root["vcas"];
                    for (auto it = vcas.begin(); it != vcas.end();) {
                        if (it->value("path", "") == id) it = vcas.erase(it); else ++it;
                    }
                }
                break;
            }

            case ItemType::Sample: {
                if (root.contains("samples") && root["samples"].is_array()) {
                    auto& smpls = root["samples"];
                    for (auto it = smpls.begin(); it != smpls.end();) {
                        if (it->value("id", "") == id) it = smpls.erase(it); else ++it;
                    }
                }
                if (root.contains("audio_table") && root["audio_table"].is_array()) {
                    auto& table = root["audio_table"];
                    for (auto it = table.begin(); it != table.end();) {
                        if (it->value("sample_id", "") == id) it = table.erase(it); else ++it;
                    }
                }
                break;
            }

            case ItemType::GlobalParameter: {
                if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
                    auto& gps = root["global_parameters"];
                    for (auto it = gps.begin(); it != gps.end();) {
                        if (it->value("name", "") == id) it = gps.erase(it); else ++it;
                    }
                }
                break;
            }

            default:
                break;
        }
    });

    if (current_selection_.id_or_path == id && current_selection_.type == type) {
        clear_selection();
    }

    for (auto* l : listeners_) {
        l->on_item_deleted(type, id);
    }
    return true;
}

bool AuthoringModel::duplicate_item(ItemType type, const std::string& id, std::string* out_new_id)
{
    if (id.empty()) return false;
    std::string new_id = id + "_copy";

    modify([&](nlohmann::json& root) {
        switch (type) {
            case ItemType::Event:
            case ItemType::Snapshot: {
                if (root.contains("events") && root["events"].is_array()) {
                    for (const auto& ev : root["events"]) {
                        if (ev.value("path", "") == id) {
                            auto copy = ev;
                            int suffix = 1;
                            while (true) {
                                bool conflict = false;
                                for (const auto& other : root["events"]) {
                                    if (other.value("path", "") == new_id) { conflict = true; break; }
                                }
                                if (!conflict) break;
                                new_id = id + "_copy" + std::to_string(++suffix);
                            }
                            copy["path"] = new_id;
                            root["events"].push_back(std::move(copy));
                            break;
                        }
                    }
                }
                break;
            }

            case ItemType::Bus: {
                if (id == "bus:/") return;
                if (root.contains("buses") && root["buses"].is_array()) {
                    for (const auto& b : root["buses"]) {
                        if (b.value("path", "") == id) {
                            auto copy = b;
                            int suffix = 1;
                            while (true) {
                                bool conflict = false;
                                for (const auto& other : root["buses"]) {
                                    if (other.value("path", "") == new_id) { conflict = true; break; }
                                }
                                if (!conflict) break;
                                new_id = id + "_copy" + std::to_string(++suffix);
                            }
                            copy["path"] = new_id;
                            root["buses"].push_back(std::move(copy));
                            break;
                        }
                    }
                }
                break;
            }

            case ItemType::Vca: {
                if (root.contains("vcas") && root["vcas"].is_array()) {
                    for (const auto& v : root["vcas"]) {
                        if (v.value("path", "") == id) {
                            auto copy = v;
                            int suffix = 1;
                            while (true) {
                                bool conflict = false;
                                for (const auto& other : root["vcas"]) {
                                    if (other.value("path", "") == new_id) { conflict = true; break; }
                                }
                                if (!conflict) break;
                                new_id = id + "_copy" + std::to_string(++suffix);
                            }
                            copy["path"] = new_id;
                            root["vcas"].push_back(std::move(copy));
                            break;
                        }
                    }
                }
                break;
            }

            case ItemType::GlobalParameter: {
                if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
                    for (const auto& gp : root["global_parameters"]) {
                        if (gp.value("name", "") == id) {
                            auto copy = gp;
                            int suffix = 1;
                            while (true) {
                                bool conflict = false;
                                for (const auto& other : root["global_parameters"]) {
                                    if (other.value("name", "") == new_id) { conflict = true; break; }
                                }
                                if (!conflict) break;
                                new_id = id + "_copy" + std::to_string(++suffix);
                            }
                            copy["name"] = new_id;
                            root["global_parameters"].push_back(std::move(copy));
                            break;
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    });

    if (out_new_id) *out_new_id = new_id;
    select({ type, new_id });
    return true;
}

std::vector<std::string> AuthoringModel::get_event_paths() const
{
    std::vector<std::string> paths;
    const auto& root = doc_.get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("kind", "event") == "event") {
                paths.push_back(ev.value("path", ""));
            }
        }
    }
    return paths;
}

std::vector<std::string> AuthoringModel::get_instrument_targets(const std::string& event_path, const std::string& command_type) const
{
    if (command_type == "snapshot_on" || command_type == "snapshot_off") return get_snapshot_paths();
    auto paths = get_event_paths();
    if (command_type == "stop_event" || paths.empty()) return paths;

    std::unordered_map<std::string, std::vector<std::string>> parents;
    for (const auto& event : json()["events"]) {
        const auto owner = event.value("path", "");
        const auto tracks = event.find("tracks");
        if (tracks == event.end() || !tracks->is_array()) continue;
        for (const auto& track : *tracks) {
            const auto instruments = track.find("instruments");
            if (instruments == track.end() || !instruments->is_array()) continue;
            for (const auto& instrument : *instruments) {
                const auto kind = instrument.value("kind", "single");
                if (kind == "event") {
                    parents[instrument.value("target_event", "")].push_back(owner);
                } else if (kind == "command" && instrument.value("command_type", "") == "start_event") {
                    parents[instrument.value("command_target", "")].push_back(owner);
                } else if (kind == "multi" || kind == "scatterer") {
                    const auto playlist = instrument.find("playlist");
                    if (playlist == instrument.end() || !playlist->is_array()) continue;
                    for (const auto& entry : *playlist) {
                        const auto target = entry.value("target_event", "");
                        if (!target.empty()) parents[target].push_back(owner);
                    }
                }
            }
        }
    }

    std::unordered_set<std::string> excluded{event_path, ""};
    std::vector<std::string> pending{event_path};
    while (!pending.empty()) {
        auto path = std::move(pending.back());
        pending.pop_back();
        const auto found = parents.find(path);
        if (found == parents.end()) continue;
        for (const auto& parent : found->second) {
            if (excluded.insert(parent).second) pending.push_back(parent);
        }
    }
    paths.erase(std::remove_if(paths.begin(), paths.end(), [&](const auto& path) {
        return excluded.count(path) != 0;
    }), paths.end());
    return paths;
}

std::vector<std::string> AuthoringModel::get_snapshot_paths() const
{
    std::vector<std::string> paths;
    const auto& root = doc_.get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("kind", "") == "snapshot") {
                paths.push_back(ev.value("path", ""));
            }
        }
    }
    return paths;
}

std::vector<std::string> AuthoringModel::get_bus_paths() const
{
    std::vector<std::string> paths{ "bus:/" };
    const auto& root = doc_.get_root_json();
    if (root.contains("buses") && root["buses"].is_array()) {
        for (const auto& b : root["buses"]) {
            paths.push_back(b.value("path", ""));
        }
    }
    return paths;
}

std::vector<std::string> AuthoringModel::get_vca_paths() const
{
    std::vector<std::string> paths;
    const auto& root = doc_.get_root_json();
    if (root.contains("vcas") && root["vcas"].is_array()) {
        for (const auto& v : root["vcas"]) {
            paths.push_back(v.value("path", ""));
        }
    }
    return paths;
}

std::vector<std::string> AuthoringModel::get_sample_ids() const
{
    std::vector<std::string> ids;
    const auto& root = doc_.get_root_json();
    if (root.contains("samples") && root["samples"].is_array()) {
        for (const auto& s : root["samples"]) {
            ids.push_back(s.value("id", ""));
        }
    }
    return ids;
}

std::vector<std::pair<std::string, std::string>> AuthoringModel::get_audio_table_entries() const
{
    std::vector<std::pair<std::string, std::string>> entries;
    const auto& root = doc_.get_root_json();
    if (root.contains("audio_table") && root["audio_table"].is_array()) {
        for (const auto& entry : root["audio_table"]) {
            if (!entry.is_object()) continue;
            entries.emplace_back(entry.value("key", ""), entry.value("sample_id", ""));
        }
    }
    return entries;
}

std::vector<std::string> AuthoringModel::get_global_parameter_names() const
{
    std::vector<std::string> names;
    const auto& root = doc_.get_root_json();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& gp : root["global_parameters"]) {
            names.push_back(gp.value("name", ""));
        }
    }
    return names;
}

std::vector<std::string> AuthoringModel::get_local_parameter_names(const std::string& event_path) const
{
    std::vector<std::string> names;
    const auto& root = doc_.get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path && ev.contains("parameters") && ev["parameters"].is_array()) {
                for (const auto& p : ev["parameters"]) {
                    names.push_back(p.value("name", ""));
                }
                break;
            }
        }
    }
    return names;
}

bool AuthoringModel::add_track(const std::string& event_path)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ensure_array(ev, "tracks");
                    ev["tracks"].push_back({
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "automations", nlohmann::json::array() },
                        { "instruments", nlohmann::json::array() }
                    });
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

namespace {

void remap_track_user_props(nlohmann::json& ev, const std::vector<size_t>& new_to_old)
{
    if (!ev.contains("user_props") || !ev["user_props"].is_object()) return;
    auto& up = ev["user_props"];

    constexpr std::string_view prefix = "track_";
    std::map<size_t, std::vector<std::pair<std::string, nlohmann::json>>> props_by_old;
    std::vector<std::string> keys_to_erase;

    for (auto it = up.begin(); it != up.end(); ++it) {
        const std::string& key = it.key();
        if (key.rfind(prefix, 0) != 0) continue;

        size_t pos = prefix.size();
        size_t idx = 0;
        size_t digits = 0;
        while (pos < key.size() && std::isdigit(static_cast<unsigned char>(key[pos]))) {
            idx = idx * 10 + static_cast<size_t>(key[pos] - '0');
            ++pos;
            ++digits;
        }
        if (digits == 0 || pos >= key.size() || key[pos] != '_' || pos + 1 >= key.size()) continue;

        props_by_old[idx].emplace_back(key.substr(pos + 1), it.value());
        keys_to_erase.push_back(key);
    }

    for (const auto& k : keys_to_erase) {
        up.erase(k);
    }

    for (size_t new_idx = 0; new_idx < new_to_old.size(); ++new_idx) {
        const auto found = props_by_old.find(new_to_old[new_idx]);
        if (found == props_by_old.end()) continue;
        for (const auto& [suffix, val] : found->second) {
            up["track_" + std::to_string(new_idx) + "_" + suffix] = val;
        }
    }
}

} // namespace

bool AuthoringModel::remove_track(const std::string& event_path, size_t track_index)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        const size_t old_n = trks.size();
                        trks.erase(trks.begin() + track_index);
                        std::vector<size_t> new_to_old;
                        new_to_old.reserve(old_n - 1);
                        for (size_t i = 0; i < old_n; ++i) {
                            if (i != track_index) new_to_old.push_back(i);
                        }
                        remap_track_user_props(ev, new_to_old);
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::duplicate_track(const std::string& event_path, size_t track_index)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        const size_t old_n = trks.size();
                        nlohmann::json dup = trks[track_index];
                        trks.insert(trks.begin() + track_index + 1, std::move(dup));
                        std::vector<size_t> new_to_old;
                        new_to_old.reserve(old_n + 1);
                        for (size_t i = 0; i < old_n; ++i) {
                            new_to_old.push_back(i);
                            if (i == track_index) new_to_old.push_back(i);
                        }
                        remap_track_user_props(ev, new_to_old);
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::reorder_track(const std::string& event_path, size_t src_index, size_t dest_index)
{
    if (src_index == dest_index) return true;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (src_index < trks.size() && dest_index < trks.size()) {
                        const size_t old_n = trks.size();
                        auto trk = std::move(trks[src_index]);
                        trks.erase(trks.begin() + src_index);
                        trks.insert(trks.begin() + dest_index, std::move(trk));

                        std::vector<size_t> new_to_old;
                        new_to_old.reserve(old_n);
                        for (size_t i = 0; i < old_n; ++i) {
                            new_to_old.push_back(i);
                        }
                        const size_t moved = new_to_old[src_index];
                        new_to_old.erase(new_to_old.begin() + src_index);
                        new_to_old.insert(new_to_old.begin() + dest_index, moved);
                        remap_track_user_props(ev, new_to_old);
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_instrument(const std::string& event_path, size_t track_index, const std::string& kind,
                                    const std::string& target, double offset_ms)
{
    if (kind == "event" || kind == "command") {
        const auto targets = get_instrument_targets(event_path, kind == "command" ? "start_event" : "");
        if (target.empty() || std::find(targets.begin(), targets.end(), target) == targets.end()) return false;
    }

    const double safe_offset_ms = std::max(0.0, offset_ms);
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        auto& insts = trks[track_index]["instruments"];
                        if (!insts.is_array()) insts = nlohmann::json::array();

                        nlohmann::json new_inst;
                        if (kind == "command") {
                            new_inst = {{ "kind", kind }, { "offset_ms", safe_offset_ms },
                                        { "command_type", "start_event" }, { "command_target", target }};
                        } else {
                            new_inst = {
                                { "kind", kind },
                                { "offset_ms", safe_offset_ms },
                                { "length_ms", 500.0 },
                                { "start_offset_ms", 0.0 },
                                { "loop", false },
                                { "fade_in_ms", 0.0 },
                                { "fade_out_ms", 0.0 },
                                { "volume_db_base", 0.0 },
                                { "pitch_semitones_base", 0.0 },
                                { "volume_db_random_range", 0.0 },
                                { "pitch_semitones_random_range", 0.0 },
                                { "modulators", nlohmann::json::array() }
                            };
                        }

                        if (kind == "single") {
                            std::string default_sample = "";
                            if (root.contains("samples") && root["samples"].is_array() && !root["samples"].empty()) {
                                default_sample = root["samples"][0].value("id", "");
                            }
                            new_inst["sample_id"] = default_sample;
                        } else if (kind == "multi" || kind == "scatterer") {
                            new_inst["playlist_mode"] = "random";
                            new_inst["playlist_scope"] = "instance";
                            new_inst["playlist"] = nlohmann::json::array();
                            if (kind == "scatterer") {
                                new_inst["length_ms"] = 2000.0;
                                new_inst["spawn_interval_min_ms"] = 200.0;
                                new_inst["spawn_interval_max_ms"] = 1000.0;
                                new_inst["polyphony"] = 4;
                                new_inst["total_sounds"] = 0;
                                new_inst["min_scatter_distance"] = 1.0f;
                                new_inst["max_scatter_distance"] = 10.0f;
                            }
                        } else if (kind == "event") {
                            new_inst["target_event"] = target;
                        } else if (kind == "programmer") {
                            new_inst["programmer_key"] = target;
                        }

                        insts.push_back(std::move(new_inst));
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_instrument_target(const std::string& event_path, size_t track_index, size_t instrument_index,
                                           const std::string& target)
{
    auto* instruments = find_instruments(json(), event_path, track_index);
    if (instruments == nullptr || instrument_index >= instruments->size() || target.empty()) return false;
    auto& instrument = (*instruments)[instrument_index];
    const auto kind = instrument.value("kind", "single");
    if (kind != "event" && kind != "command") return false;
    const char* field = kind == "event" ? "target_event" : "command_target";
    const auto command_type = kind == "command" ? instrument.value("command_type", "start_event") : "";
    const auto targets = get_instrument_targets(event_path, command_type);
    if (std::find(targets.begin(), targets.end(), target) == targets.end()) return false;
    auto updated = instrument;
    updated[field] = target;
    if (kind == "command") {
        updated["command_type"] = command_type;
        for (const auto* key : { "length_ms", "start_offset_ms", "loop", "fade_in_ms", "fade_out_ms",
                                "volume_db_base", "pitch_semitones_base", "volume_db_random_range",
                                "pitch_semitones_random_range", "modulators" }) {
            updated.erase(key);
        }
    }
    if (updated != instrument) {
        modify([&](nlohmann::json&) { instrument = std::move(updated); });
    }
    return true;
}

bool AuthoringModel::set_command_type(const std::string& event_path, size_t track_index, size_t instrument_index,
                                      const std::string& command_type)
{
    if (command_type != "start_event" && command_type != "stop_event" &&
        command_type != "snapshot_on" && command_type != "snapshot_off") {
        return false;
    }
    auto* instruments = find_instruments(json(), event_path, track_index);
    if (instruments == nullptr || instrument_index >= instruments->size()) return false;
    auto& instrument = (*instruments)[instrument_index];
    if (instrument.value("kind", "single") != "command") return false;
    const auto targets = get_instrument_targets(event_path, command_type);
    const auto cur_target = instrument.value("command_target", "");
    auto updated = instrument;
    updated["command_type"] = command_type;
    if (std::find(targets.begin(), targets.end(), cur_target) == targets.end()) {
        updated["command_target"] = targets.empty() ? std::string{} : targets.front();
    }
    if (updated != instrument) {
        modify([&](nlohmann::json&) { instrument = std::move(updated); });
    }
    return true;
}

bool AuthoringModel::set_scatterer_properties(const std::string& event_path, size_t track_index, size_t instrument_index,
                                              double spawn_interval_min_ms, double spawn_interval_max_ms,
                                              uint32_t polyphony, uint32_t total_sounds,
                                              float min_scatter_distance, float max_scatter_distance)
{
    auto* instruments = find_instruments(json(), event_path, track_index);
    if (instruments == nullptr || instrument_index >= instruments->size()) return false;
    auto& instrument = (*instruments)[instrument_index];
    if (instrument.value("kind", "single") != "scatterer") return false;

    const double min_int = std::max(1.0, spawn_interval_min_ms);
    const double max_int = std::max(min_int, spawn_interval_max_ms);
    const uint32_t poly = std::clamp<uint32_t>(polyphony, 1u, 64u);
    const float min_d = std::max(0.0f, min_scatter_distance);
    const float max_d = std::max(min_d, max_scatter_distance);

    auto updated = instrument;
    updated["spawn_interval_min_ms"] = min_int;
    updated["spawn_interval_max_ms"] = max_int;
    updated["polyphony"] = poly;
    updated["total_sounds"] = total_sounds;
    updated["min_scatter_distance"] = min_d;
    updated["max_scatter_distance"] = max_d;
    if (updated != instrument) {
        modify([&](nlohmann::json&) { instrument = std::move(updated); });
    }
    return true;
}

bool AuthoringModel::set_programmer_key(const std::string& event_path, size_t track_index, size_t instrument_index,
                                          const std::string& key)
{
    auto* instruments = find_instruments(json(), event_path, track_index);
    if (instruments == nullptr || instrument_index >= instruments->size()) return false;
    auto& instrument = (*instruments)[instrument_index];
    if (instrument.value("kind", "single") != "programmer") return false;
    if (instrument.value("programmer_key", "") != key) {
        modify([&](nlohmann::json&) { instrument["programmer_key"] = key; });
    }
    return true;
}

bool AuthoringModel::set_audio_table_entry(const std::string& key, const std::string& sample_id)
{
    if (key.empty() || sample_id.empty()) return false;
    const auto sample_ids = get_sample_ids();
    if (std::find(sample_ids.begin(), sample_ids.end(), sample_id) == sample_ids.end()) return false;
    bool changed = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("audio_table") || !root["audio_table"].is_array()) {
            root["audio_table"] = nlohmann::json::array();
        }
        for (auto& entry : root["audio_table"]) {
            if (entry.is_object() && entry.value("key", "") == key) {
                if (entry.value("sample_id", "") != sample_id) {
                    entry["sample_id"] = sample_id;
                    changed = true;
                }
                return;
            }
        }
        root["audio_table"].push_back({ { "key", key }, { "sample_id", sample_id } });
        changed = true;
    });
    return changed;
}

bool AuthoringModel::remove_audio_table_entry(const std::string& key)
{
    if (key.empty()) return false;
    const auto& current = doc_.get_root_json();
    if (!current.contains("audio_table") || !current["audio_table"].is_array()) return false;
    bool exists = false;
    for (const auto& entry : current["audio_table"]) {
        if (entry.is_object() && entry.value("key", "") == key) {
            exists = true;
            break;
        }
    }
    if (!exists) return false;
    modify([&](nlohmann::json& root) {
        auto& table = root["audio_table"];
        for (auto it = table.begin(); it != table.end();) {
            if (it->is_object() && it->value("key", "") == key) it = table.erase(it);
            else ++it;
        }
    });
    return true;
}

bool AuthoringModel::duplicate_instrument(const std::string& event_path, size_t track_index, size_t instrument_index)
{
    auto* instruments = find_instruments(json(), event_path, track_index);
    if (instruments == nullptr || instrument_index >= instruments->size()) return false;
    auto copy = (*instruments)[instrument_index];
    modify([&](nlohmann::json&) { instruments->push_back(std::move(copy)); });
    return true;
}

bool AuthoringModel::remove_instrument(const std::string& event_path, size_t track_index, size_t instrument_index)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        auto& insts = trks[track_index]["instruments"];
                        if (insts.is_array() && instrument_index < insts.size()) {
                            insts.erase(insts.begin() + instrument_index);
                            ok = true;
                        }
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::move_instrument(const std::string& event_path, size_t src_track, size_t inst_index, size_t dest_track, double new_offset_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (src_track < trks.size() && dest_track < trks.size()) {
                        auto& src_insts = trks[src_track]["instruments"];
                        if (src_insts.is_array() && inst_index < src_insts.size()) {
                            nlohmann::json inst = std::move(src_insts[inst_index]);
                            src_insts.erase(src_insts.begin() + inst_index);
                            inst["offset_ms"] = std::max(0.0, new_offset_ms);
                            auto& dst_insts = trks[dest_track]["instruments"];
                            if (!dst_insts.is_array()) dst_insts = nlohmann::json::array();
                            dst_insts.push_back(std::move(inst));
                            ok = true;
                        }
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::resize_instrument(const std::string& event_path, size_t track_index, size_t inst_index, double new_offset_ms, double new_length_ms, double new_start_offset_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        auto& insts = trks[track_index]["instruments"];
                        if (insts.is_array() && inst_index < insts.size()) {
                            auto& inst = insts[inst_index];
                            inst["offset_ms"] = std::max(0.0, new_offset_ms);
                            if (inst.value("kind", "single") != "command") {
                                inst["length_ms"] = std::max(1.0, new_length_ms);
                                if (new_start_offset_ms >= 0.0) {
                                    inst["start_offset_ms"] = new_start_offset_ms;
                                }
                            }
                            ok = true;
                        }
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_instrument_fades(const std::string& event_path, size_t track_index, size_t inst_index, double fade_in_ms, double fade_out_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        auto& insts = trks[track_index]["instruments"];
                        if (insts.is_array() && inst_index < insts.size()) {
                            auto& inst = insts[inst_index];
                            inst["fade_in_ms"] = std::max(0.0, fade_in_ms);
                            inst["fade_out_ms"] = std::max(0.0, fade_out_ms);
                            ok = true;
                        }
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_local_parameter(const std::string& event_path, const std::string& name, const std::string& kind)
{
    if (!valid_non_null_string(name)) return false;
    for (const auto& existing : get_local_parameter_names(event_path)) {
        if (existing == name) return false;
    }
    nlohmann::json param;
    if (!build_normalized_parameter({ { "name", name }, { "kind", kind } }, param)) {
        return false;
    }
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (auto* ev = find_event(root, event_path)) {
            ensure_array(*ev, "parameters");
            (*ev)["parameters"].push_back(std::move(param));
            ok = true;
        }
    });
    return ok;
}

bool AuthoringModel::update_local_parameter(const std::string& event_path, const std::string& name, const nlohmann::json& spec)
{
    nlohmann::json with_name = spec.is_object() ? spec : nlohmann::json::object();
    if (!with_name.contains("name")) with_name["name"] = name;
    nlohmann::json normalized;
    if (!build_normalized_parameter(with_name, normalized)) return false;
    const std::string new_name = normalized.value("name", name);
    if (new_name != name && !rename_local_parameter(event_path, name, new_name)) {
        return false;
    }

    bool ok = false;
    modify([&](nlohmann::json& root) {
        auto* ev = find_event(root, event_path);
        if (!ev || !ev->contains("parameters") || !(*ev)["parameters"].is_array()) return;
        for (auto& p : (*ev)["parameters"]) {
            if (p.value("name", "") == new_name) {
                p = std::move(normalized);
                ok = true;
                break;
            }
        }
    });
    return ok;
}

bool AuthoringModel::update_global_parameter(const std::string& name, const nlohmann::json& spec)
{
    nlohmann::json with_name = spec.is_object() ? spec : nlohmann::json::object();
    if (!with_name.contains("name")) with_name["name"] = name;
    nlohmann::json normalized;
    if (!build_normalized_parameter(with_name, normalized)) return false;
    const std::string new_name = normalized.value("name", name);
    if (new_name != name && !rename_item(ItemType::GlobalParameter, name, new_name)) {
        return false;
    }

    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("global_parameters") || !root["global_parameters"].is_array()) return;
        for (auto& p : root["global_parameters"]) {
            if (p.value("name", "") == new_name) {
                p = std::move(normalized);
                ok = true;
                break;
            }
        }
    });
    return ok;
}

bool AuthoringModel::rename_local_parameter(const std::string& event_path, const std::string& old_name, const std::string& new_name)
{
    if (!valid_non_null_string(old_name) || !valid_non_null_string(new_name) || old_name == new_name) {
        return false;
    }
    for (const auto& existing : get_local_parameter_names(event_path)) {
        if (existing == new_name) return false;
    }

    bool ok = false;
    modify([&](nlohmann::json& root) {
        auto* ev = find_event(root, event_path);
        if (!ev || !ev->contains("parameters") || !(*ev)["parameters"].is_array()) return;
        for (auto& p : (*ev)["parameters"]) {
            if (p.value("name", "") == old_name) {
                p["name"] = new_name;
                ok = true;
                break;
            }
        }
        if (!ok) return;

        if (ev->value("intensity_param", "") == old_name) {
            (*ev)["intensity_param"] = new_name;
        }
        const auto update_condition = [&](nlohmann::json& cond) {
            if (cond.contains("terms") && cond["terms"].is_array()) {
                for (auto& term : cond["terms"]) {
                    if (term.contains("param") && term["param"].value("scope", "") == "local"
                        && term["param"].value("name", "") == old_name) {
                        term["param"]["name"] = new_name;
                    }
                }
            }
        };
        const auto update_automations = [&](nlohmann::json& autos) {
            if (autos.is_array()) {
                for (auto& a : autos) {
                    if (!a.value("is_timeline", false) && a.contains("param")
                        && a["param"].value("scope", "") == "local" && a["param"].value("name", "") == old_name) {
                        a["param"]["name"] = new_name;
                    }
                }
            }
        };
        if (ev->contains("automations")) update_automations((*ev)["automations"]);
        if (ev->contains("tracks") && (*ev)["tracks"].is_array()) {
            for (auto& trk : (*ev)["tracks"]) {
                if (trk.contains("automations")) update_automations(trk["automations"]);
                if (trk.contains("instruments") && trk["instruments"].is_array()) {
                    for (auto& inst : trk["instruments"]) {
                        if (inst.contains("condition")) update_condition(inst["condition"]);
                        if (inst.value("sheet_parameter", "") == old_name) {
                            inst["sheet_parameter"] = new_name;
                        }
                    }
                }
            }
        }
        if (ev->contains("logic_items") && (*ev)["logic_items"].is_array()) {
            for (auto& li : (*ev)["logic_items"]) {
                if (li.contains("condition")) update_condition(li["condition"]);
            }
        }
    });
    return ok;
}

bool AuthoringModel::remove_local_parameter(const std::string& event_path, const std::string& name)
{
    if (!valid_non_null_string(name)) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        auto* ev = find_event(root, event_path);
        if (!ev || !ev->contains("parameters") || !(*ev)["parameters"].is_array()) return;
        auto& params = (*ev)["parameters"];
        for (auto it = params.begin(); it != params.end();) {
            if (it->value("name", "") == name) {
                it = params.erase(it);
                ok = true;
            } else {
                ++it;
            }
        }
        if (!ok) return;
        if (ev->contains("tracks") && (*ev)["tracks"].is_array()) {
            for (auto& trk : (*ev)["tracks"]) {
                if (trk.contains("instruments") && trk["instruments"].is_array()) {
                    for (auto& inst : trk["instruments"]) {
                        if (inst.value("sheet_parameter", "") == name) {
                            inst.erase("sheet_parameter");
                            inst.erase("param_min");
                            inst.erase("param_max");
                        }
                    }
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_instrument_sheet_range(const std::string& event_path, size_t track_index, size_t instrument_index,
                                                const std::string& sheet_parameter, double param_min, double param_max)
{
    if (!sheet_parameter.empty() && (!valid_non_null_string(sheet_parameter) ||
        !std::isfinite(param_min) || !std::isfinite(param_max))) {
        return false;
    }

    bool ok = false;
    modify([&](nlohmann::json& root) {
        auto* instruments = find_instruments(root, event_path, track_index);
        if (!instruments || instrument_index >= instruments->size()) return;
        auto& inst = (*instruments)[instrument_index];
        if (sheet_parameter.empty()) {
            inst.erase("sheet_parameter");
            inst.erase("param_min");
            inst.erase("param_max");
            ok = true;
            return;
        }
        const std::string kind = inst.value("kind", "single");
        if (kind == "command" || kind == "silence") return;
        const auto* param = find_parameter_json(root, event_path, "", sheet_parameter);
        if (!param) return;
        const auto [dom_min, dom_max] = parameter_json_domain(*param);
        double lo = std::min(param_min, param_max);
        double hi = std::max(param_min, param_max);
        lo = std::clamp(lo, dom_min, dom_max);
        hi = std::clamp(hi, dom_min, dom_max);
        inst["sheet_parameter"] = sheet_parameter;
        inst["param_min"] = lo;
        inst["param_max"] = hi;
        ok = true;
    });
    return ok;
}

bool AuthoringModel::set_event_user_property(const std::string& event_path, const std::string& key, const std::string& value)
{
    if (!valid_non_null_string(key) || !valid_non_null_string(value, true)) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (auto* ev = find_event(root, event_path)) {
            if (!ev->contains("user_props") || !(*ev)["user_props"].is_object()) {
                (*ev)["user_props"] = nlohmann::json::object();
            }
            (*ev)["user_props"][key] = value;
            ok = true;
        }
    });
    return ok;
}

bool AuthoringModel::remove_event_user_property(const std::string& event_path, const std::string& key)
{
    if (!valid_non_null_string(key)) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (auto* ev = find_event(root, event_path)) {
            if (ev->contains("user_props") && (*ev)["user_props"].is_object()) {
                ok = (*ev)["user_props"].erase(key) > 0;
            }
        }
    });
    return ok;
}

namespace {

const nlohmann::json* find_event_tracks(const nlohmann::json& root, const std::string& event_path)
{
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                return &ev["tracks"];
            }
        }
    }
    return nullptr;
}

const nlohmann::json* find_event_track(const nlohmann::json& root, const std::string& event_path, size_t track_index)
{
    const auto* trks = find_event_tracks(root, event_path);
    if (trks && track_index < trks->size() && (*trks)[track_index].is_object()) {
        return &(*trks)[track_index];
    }
    return nullptr;
}

} // namespace

size_t AuthoringModel::get_track_count(const std::string& event_path) const
{
    const auto* trks = find_event_tracks(doc_.get_root_json(), event_path);
    return trks ? trks->size() : 0;
}

double AuthoringModel::get_track_volume(const std::string& event_path, size_t track_index) const
{
    const auto* trk = find_event_track(doc_.get_root_json(), event_path, track_index);
    return trk ? trk->value("volume_db", 0.0) : 0.0;
}

double AuthoringModel::get_track_pan(const std::string& event_path, size_t track_index) const
{
    const auto* trk = find_event_track(doc_.get_root_json(), event_path, track_index);
    return trk ? trk->value("pan", 0.0) : 0.0;
}

int AuthoringModel::get_track_output_track(const std::string& event_path, size_t track_index) const
{
    const auto* trk = find_event_track(doc_.get_root_json(), event_path, track_index);
    return trk ? trk->value("output_track", -1) : -1;
}

bool AuthoringModel::set_track_volume(const std::string& event_path, size_t track_index, double volume_db)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        trks[track_index]["volume_db"] = volume_db;
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_track_pan(const std::string& event_path, size_t track_index, double pan)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
                    auto& trks = ev["tracks"];
                    if (track_index < trks.size()) {
                        trks[track_index]["pan"] = std::clamp(pan, -1.0, 1.0);
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_track_muted(const std::string& event_path, size_t track_index, bool muted)
{
    bool ok = false;
    modify_audition([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    if (!ev.contains("user_props") || !ev["user_props"].is_object()) {
                        ev["user_props"] = nlohmann::json::object();
                    }
                    ev["user_props"]["track_" + std::to_string(track_index) + "_mute"] = muted ? "true" : "false";
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::set_track_solo(const std::string& event_path, size_t track_index, bool solo)
{
    bool ok = false;
    modify_audition([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    if (!ev.contains("user_props") || !ev["user_props"].is_object()) {
                        ev["user_props"] = nlohmann::json::object();
                    }
                    ev["user_props"]["track_" + std::to_string(track_index) + "_solo"] = solo ? "true" : "false";
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::is_track_muted(const std::string& event_path, size_t track_index) const
{
    const auto& root = doc_.get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path && ev.contains("user_props") && ev["user_props"].is_object()) {
                const std::string key = "track_" + std::to_string(track_index) + "_mute";
                return ev["user_props"].value(key, "false") == "true";
            }
        }
    }
    return false;
}

bool AuthoringModel::is_track_solo(const std::string& event_path, size_t track_index) const
{
    const auto& root = doc_.get_root_json();
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") == event_path && ev.contains("user_props") && ev["user_props"].is_object()) {
                const std::string key = "track_" + std::to_string(track_index) + "_solo";
                return ev["user_props"].value(key, "false") == "true";
            }
        }
    }
    return false;
}

namespace {

std::string read_track_user_prop(const nlohmann::json& root,
                                 const std::string& event_path,
                                 size_t track_index,
                                 const char* prop_suffix,
                                 const char* legacy_track_key,
                                 const std::string& fallback_value)
{
    if (root.contains("events") && root["events"].is_array()) {
        for (const auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            if (ev.contains("tracks") && ev["tracks"].is_array() && track_index < ev["tracks"].size()) {
                if (ev.contains("user_props") && ev["user_props"].is_object()) {
                    const std::string key = "track_" + std::to_string(track_index) + prop_suffix;
                    const std::string prop = ev["user_props"].value(key, std::string{});
                    if (!prop.empty()) return prop;
                }
                const auto& trk = ev["tracks"][track_index];
                if (trk.is_object()) {
                    const std::string legacy = trk.value(legacy_track_key, std::string{});
                    if (!legacy.empty()) return legacy;
                }
            }
            break;
        }
    }
    return fallback_value;
}

bool write_track_user_prop(nlohmann::json& ev,
                           size_t track_index,
                           const char* prop_suffix,
                           const char* legacy_track_key,
                           const std::string& value)
{
    if (!ev.contains("tracks") || !ev["tracks"].is_array() || track_index >= ev["tracks"].size()) {
        return false;
    }
    auto& trk = ev["tracks"][track_index];
    if (!trk.is_object()) return false;
    trk.erase(legacy_track_key);

    const std::string key = "track_" + std::to_string(track_index) + prop_suffix;
    if (value.empty()) {
        if (ev.contains("user_props") && ev["user_props"].is_object()) {
            ev["user_props"].erase(key);
        }
    } else {
        if (!ev.contains("user_props") || !ev["user_props"].is_object()) {
            ev["user_props"] = nlohmann::json::object();
        }
        ev["user_props"][key] = value;
    }
    return true;
}

} // namespace

std::string AuthoringModel::get_track_color(const std::string& event_path, size_t track_index) const
{
    return read_track_user_prop(
        doc_.get_root_json(), event_path, track_index,
        "_color", "color", OaeTheme::TrackColors::hexForIndex(track_index));
}

bool AuthoringModel::set_track_color(const std::string& event_path, size_t track_index, const std::string& hex_color)
{
    if (!valid_non_null_string(hex_color, false)) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ok = write_track_user_prop(ev, track_index, "_color", "color", hex_color);
                    break;
                }
            }
        }
    });
    return ok;
}

std::string AuthoringModel::get_track_name(const std::string& event_path, size_t track_index) const
{
    return read_track_user_prop(
        doc_.get_root_json(), event_path, track_index,
        "_name", "name", "Track " + std::to_string(track_index + 1));
}

bool AuthoringModel::set_track_name(const std::string& event_path, size_t track_index, const std::string& name)
{
    if (!valid_non_null_string(name, true)) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ok = write_track_user_prop(ev, track_index, "_name", "name", name);
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_marker(const std::string& event_path, const std::string& name, double position_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ensure_array(ev, "logic_items");
                    auto& items = ev["logic_items"];
                    const std::string base_name = name.empty() ? "Marker" : name;
                    std::string unique_name = base_name;
                    for (int suffix = 2; std::any_of(items.begin(), items.end(), [&](const auto& item) {
                        return item.value("kind", "") == "marker" && item.value("name", "") == unique_name;
                    }); ++suffix) {
                        unique_name = base_name + " " + std::to_string(suffix);
                    }
                    ev["logic_items"].push_back({
                        { "kind", "marker" },
                        { "name", unique_name },
                        { "position_ms", std::clamp(position_ms, 0.0, 86400000.0) }
                    });
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_tempo_marker(const std::string& event_path, double bpm, int ts_upper, int ts_lower, double position_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ensure_array(ev, "logic_items");
                    ev["logic_items"].push_back({
                        { "kind", "tempo" },
                        { "bpm", std::max(1.0, bpm) },
                        { "ts_upper", std::max(1, ts_upper) },
                        { "ts_lower", ts_lower },
                        { "position_ms", std::max(0.0, position_ms) }
                    });
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_loop_region(const std::string& event_path, double start_ms, double length_ms, int iterations)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path) {
                    ensure_array(ev, "logic_items");
                    const double start = std::clamp(start_ms, 0.0, 86399999.0);
                    ev["logic_items"].push_back({
                        { "kind", "loop_region" },
                        { "start_ms", start },
                        { "length_ms", std::clamp(length_ms, 1.0, 86400000.0 - start) },
                        { "iterations", iterations },
                        { "until_key_off", false }
                    });
                    ok = true;
                    break;
                }
            }
        }
    });
    return ok;
}

bool AuthoringModel::add_transition_marker(const std::string& event_path, double position_ms, const std::string& target_marker,
                                           const std::string& quantization, double crossfade_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("events") || !root["events"].is_array()) return;
        for (auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            ensure_array(ev, "logic_items");
            auto& items = ev["logic_items"];
            std::string resolved_target = target_marker;
            if (resolved_target.empty()) {
                for (const auto& item : items) {
                    if (item.value("kind", "") == "marker") {
                        resolved_target = item.value("name", "");
                        if (!resolved_target.empty()) break;
                    }
                }
            }
            if (resolved_target.empty()) break;
            const std::string q = (quantization == "beat" || quantization == "bar") ? quantization : "immediate";
            items.push_back({
                { "kind", "transition_marker" },
                { "position_ms", std::clamp(position_ms, 0.0, 86400000.0) },
                { "target_marker", resolved_target },
                { "quantization", q },
                { "crossfade_ms", std::clamp(crossfade_ms, 0.0, 60000.0) }
            });
            ok = true;
            break;
        }
    });
    return ok;
}

bool AuthoringModel::add_transition_region(const std::string& event_path, double start_ms, double length_ms, const std::string& target_marker,
                                           const std::string& quantization, double crossfade_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("events") || !root["events"].is_array()) return;
        for (auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            ensure_array(ev, "logic_items");
            auto& items = ev["logic_items"];
            std::string resolved_target = target_marker;
            if (resolved_target.empty()) {
                for (const auto& item : items) {
                    if (item.value("kind", "") == "marker") {
                        resolved_target = item.value("name", "");
                        if (!resolved_target.empty()) break;
                    }
                }
            }
            if (resolved_target.empty()) break;
            const double start = std::clamp(start_ms, 0.0, 86399999.0);
            const std::string q = (quantization == "beat" || quantization == "bar") ? quantization : "immediate";
            items.push_back({
                { "kind", "transition_region" },
                { "start_ms", start },
                { "length_ms", std::clamp(length_ms, 1.0, 86400000.0 - start) },
                { "target_marker", resolved_target },
                { "quantization", q },
                { "crossfade_ms", std::clamp(crossfade_ms, 0.0, 60000.0) }
            });
            ok = true;
            break;
        }
    });
    return ok;
}

bool AuthoringModel::add_sustain_point(const std::string& event_path, double position_ms)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("events") || !root["events"].is_array()) return;
        for (auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            ensure_array(ev, "logic_items");
            ev["logic_items"].push_back({
                { "kind", "sustain_point" },
                { "position_ms", std::clamp(position_ms, 0.0, 86400000.0) }
            });
            ok = true;
            break;
        }
    });
    return ok;
}

bool AuthoringModel::rename_marker(const std::string& event_path, size_t item_index, const std::string& new_name)
{
    if (new_name.empty()) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("events") || !root["events"].is_array()) return;
        for (auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            if (!ev.contains("logic_items") || !ev["logic_items"].is_array()) break;
            auto& items = ev["logic_items"];
            if (item_index >= items.size() || items[item_index].value("kind", "") != "marker") break;

            const std::string old_name = items[item_index].value("name", "");
            std::string unique_name = new_name;
            auto name_taken = [&](const std::string& candidate) {
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i != item_index && items[i].value("kind", "") == "marker" && items[i].value("name", "") == candidate) {
                        return true;
                    }
                }
                return false;
            };
            for (int suffix = 2; name_taken(unique_name); ++suffix) {
                unique_name = new_name + " " + std::to_string(suffix);
            }
            if (unique_name == old_name) break;

            items[item_index]["name"] = unique_name;
            for (auto& item : items) {
                const auto kind = item.value("kind", "");
                if ((kind == "transition_marker" || kind == "transition_region")
                    && item.value("target_marker", "") == old_name) {
                    item["target_marker"] = unique_name;
                }
            }
            ok = true;
            break;
        }
    });
    return ok;
}

bool AuthoringModel::update_tempo_marker(const std::string& event_path, size_t item_index, double bpm, int ts_upper, int ts_lower)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (!root.contains("events") || !root["events"].is_array()) return;
        for (auto& ev : root["events"]) {
            if (ev.value("path", "") != event_path) continue;
            if (!ev.contains("logic_items") || !ev["logic_items"].is_array()) break;
            auto& items = ev["logic_items"];
            if (item_index >= items.size() || items[item_index].value("kind", "") != "tempo") break;

            const int valid_lower = (ts_lower == 2 || ts_lower == 4 || ts_lower == 8 || ts_lower == 16) ? ts_lower : 4;
            items[item_index]["bpm"] = std::clamp(bpm, 20.0, 300.0);
            items[item_index]["ts_upper"] = std::clamp(ts_upper, 1, 16);
            items[item_index]["ts_lower"] = valid_lower;
            ok = true;
            break;
        }
    });
    return ok;
}

bool AuthoringModel::remove_logic_item(const std::string& event_path, size_t item_index)
{
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (root.contains("events") && root["events"].is_array()) {
            for (auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("logic_items") && ev["logic_items"].is_array()) {
                    auto& items = ev["logic_items"];
                    if (item_index < items.size()) {
                        items.erase(items.begin() + item_index);
                        ok = true;
                    }
                    break;
                }
            }
        }
    });
    return ok;
}

namespace {

bool normalize_modulator(nlohmann::json& mod)
{
    if (!mod.is_object()) return false;
    const auto kind = mod.value("kind", "");
    if (kind == "random") {
        const auto target = mod.value("target", "volume");
        if (target != "volume" && target != "pitch" && target != "pan") return false;
        const double max_range = (target == "volume") ? 80.0 : (target == "pitch" ? 48.0 : 1.0);
        const double range = std::clamp(mod.value("range", 0.0), 0.0, max_range);
        mod = { { "kind", "random" }, { "target", target }, { "range", range } };
        return true;
    }
    if (kind == "ahdsr") {
        const double a = std::clamp(mod.value("attack_ms", 0.0), 0.0, 86400000.0);
        const double h = std::clamp(mod.value("hold_ms", 0.0), 0.0, 86400000.0);
        const double d = std::clamp(mod.value("decay_ms", 0.0), 0.0, 86400000.0);
        const double s = std::clamp(mod.value("sustain", 1.0), 0.0, 1.0);
        const double r = std::clamp(mod.value("release_ms", 0.0), 0.0, 86400000.0);
        const bool key_off = mod.value("release_on_key_off", false);
        mod = {
            { "kind", "ahdsr" },
            { "attack_ms", a },
            { "hold_ms", h },
            { "decay_ms", d },
            { "sustain", s },
            { "release_ms", r },
            { "release_on_key_off", key_off }
        };
        return true;
    }
    return false;
}

nlohmann::json* find_modulator_owner(nlohmann::json& root, const ItemSelection& owner)
{
    if (!root.contains("events") || !root["events"].is_array()) return nullptr;
    for (auto& ev : root["events"]) {
        if (!ev.is_object() || ev.value("path", "") != owner.id_or_path) continue;
        if (owner.type != ItemType::Event) return nullptr;
        if (owner.track_index < 0 && owner.instrument_index < 0) return &ev;
        if (owner.track_index >= 0 && owner.instrument_index >= 0) {
            if (!ev.contains("tracks") || !ev["tracks"].is_array()) return nullptr;
            const auto ti = static_cast<size_t>(owner.track_index);
            if (ti >= ev["tracks"].size()) return nullptr;
            auto& insts = ev["tracks"][ti]["instruments"];
            const auto ii = static_cast<size_t>(owner.instrument_index);
            if (!insts.is_array() || ii >= insts.size()) return nullptr;
            const auto kind = insts[ii].value("kind", "single");
            if (kind == "command" || kind == "silence") return nullptr;
            return &insts[ii];
        }
    }
    return nullptr;
}

template <typename Json>
Json* find_automation_owner(Json& root, const std::string& event_path, int track_index)
{
    if (!root.contains("events") || !root["events"].is_array()) return nullptr;
    for (auto& ev : root["events"]) {
        if (!ev.is_object() || ev.value("path", "") != event_path) continue;
        if (track_index < 0) {
            return &ev;
        }
        if (!ev.contains("tracks") || !ev["tracks"].is_array()) return nullptr;
        const auto ti = static_cast<size_t>(track_index);
        if (ti >= ev["tracks"].size()) return nullptr;
        auto& track = ev["tracks"][ti];
        return track.is_object() ? &track : nullptr;
    }
    return nullptr;
}

nlohmann::json* find_automation_list(nlohmann::json& root, const std::string& event_path, int track_index)
{
    auto* owner = find_automation_owner(root, event_path, track_index);
    if (!owner) return nullptr;
    ensure_array(*owner, "automations");
    return &(*owner)["automations"];
}

template <typename Json>
Json* find_timeline_volume_curve(Json& track)
{
    const auto autos = track.find("automations");
    if (autos == track.end() || !autos->is_array()) return nullptr;
    const auto curve = std::find_if(autos->begin(), autos->end(), [](const nlohmann::json& entry) {
        if (!entry.is_object()) return false;
        const auto source = entry.find("is_timeline");
        const auto target = entry.find("target");
        return source != entry.end() && *source == true && target != entry.end() && *target == "volume";
    });
    return curve == autos->end() ? nullptr : &*curve;
}

bool normalize_curve_points(const nlohmann::json& input, nlohmann::json& out_points)
{
    if (!input.is_array() || input.empty()) return false;
    struct Pt { double x; double y; std::string shape; };
    std::vector<Pt> pts;
    pts.reserve(std::min<size_t>(input.size(), 32));
    for (const auto& item : input) {
        if (!item.is_object()) continue;
        const double x = item.value("x", 0.0);
        const double y = item.value("y", 0.0);
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        std::string shape = item.value("shape", "linear");
        if (shape != "linear" && shape != "hold" && shape != "ease_in" &&
            shape != "ease_out" && shape != "s_curve") {
            shape = "linear";
        }
        pts.push_back({ x, y, shape });
        if (pts.size() >= 32) break;
    }
    if (pts.empty()) return false;
    std::stable_sort(pts.begin(), pts.end(), [](const Pt& a, const Pt& b) { return a.x < b.x; });
    for (size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].x <= pts[i - 1].x) {
            pts[i].x = pts[i - 1].x + 1e-4;
        }
    }
    out_points = nlohmann::json::array();
    for (const auto& p : pts) {
        out_points.push_back({ { "x", p.x }, { "y", p.y }, { "shape", p.shape } });
    }
    return true;
}

} // namespace

bool AuthoringModel::add_modulator(const ItemSelection& owner, const nlohmann::json& modulator)
{
    nlohmann::json normalized = modulator;
    if (!normalize_modulator(normalized)) return false;
    if (find_modulator_owner(json(), owner) == nullptr) return false;
    bool ok = false;
    modify([&](nlohmann::json& root) {
        if (auto* target = find_modulator_owner(root, owner)) {
            if (!target->contains("modulators") || !(*target)["modulators"].is_array()) {
                (*target)["modulators"] = nlohmann::json::array();
            }
            (*target)["modulators"].push_back(normalized);
            ok = true;
        }
    });
    return ok;
}

bool AuthoringModel::update_modulator(const ItemSelection& owner, size_t mod_index, const nlohmann::json& modulator)
{
    nlohmann::json normalized = modulator;
    if (!normalize_modulator(normalized)) return false;
    auto* current = find_modulator_owner(json(), owner);
    if (current == nullptr || !current->contains("modulators") || !(*current)["modulators"].is_array() ||
        mod_index >= (*current)["modulators"].size()) {
        return false;
    }
    if ((*current)["modulators"][mod_index] == normalized) return true;
    modify([&](nlohmann::json& root) {
        if (auto* target = find_modulator_owner(root, owner)) {
            (*target)["modulators"][mod_index] = normalized;
        }
    });
    return true;
}

bool AuthoringModel::remove_modulator(const ItemSelection& owner, size_t mod_index)
{
    auto* current = find_modulator_owner(json(), owner);
    if (current == nullptr || !current->contains("modulators") || !(*current)["modulators"].is_array() ||
        mod_index >= (*current)["modulators"].size()) {
        return false;
    }
    modify([&](nlohmann::json& root) {
        if (auto* target = find_modulator_owner(root, owner)) {
            auto& mods = (*target)["modulators"];
            mods.erase(mods.begin() + static_cast<ptrdiff_t>(mod_index));
        }
    });
    return true;
}

std::pair<double, double> AuthoringModel::get_parameter_domain(const std::string& event_path,
                                                               const std::string& scope,
                                                               const std::string& name) const
{
    const auto& root = doc_.get_root_json();
    if (const auto* param = find_parameter_json(root, event_path, scope, name)) {
        const auto [mn, mx] = parameter_json_domain(*param);
        return { mn, (mx > mn) ? mx : (mn + 1.0) };
    }
    return { 0.0, 1.0 };
}

nlohmann::json AuthoringModel::get_track_timeline_volume_points(const std::string& event_path, int track_index) const
{
    if (track_index >= 0) {
        if (const auto* track = find_automation_owner(json(), event_path, track_index)) {
            if (const auto* entry = find_timeline_volume_curve(*track)) {
                const auto curve = entry->find("curve");
                if (curve != entry->end() && curve->is_object()) {
                    const auto points = curve->find("points");
                    if (points != curve->end() && points->is_array()) return *points;
                }
            }
        }
    }
    return nlohmann::json::array();
}

bool AuthoringModel::set_track_timeline_volume_points(const std::string& event_path, int track_index,
                                                     const nlohmann::json& points, bool create_if_missing)
{
    if (track_index < 0 || !points.is_array() || points.empty() || points.size() > 32) return false;

    bool updated = false;
    modify([&](nlohmann::json& root) {
        auto* track = find_automation_owner(root, event_path, track_index);
        if (!track) return;
        if (auto* entry = find_timeline_volume_curve(*track)) {
            (*entry)["curve"]["points"] = points;
        } else {
            if (!create_if_missing) return;
            const auto autos = track->find("automations");
            if (autos != track->end() && !autos->is_array()) return;
            ensure_array(*track, "automations");
            (*track)["automations"].push_back({
                { "target", "volume" },
                { "is_timeline", true },
                { "curve", { { "points", points } } }
            });
        }
        updated = true;
    });
    return updated;
}

bool AuthoringModel::clear_track_timeline_volume_points(const std::string& event_path, int track_index)
{
    if (track_index < 0) return false;
    const auto* autos = find_automation_list(json(), event_path, track_index);
    if (autos == nullptr) return false;

    size_t entry_index = 0;
    bool found = false;
    for (size_t i = 0; i < autos->size(); ++i) {
        const auto& entry = (*autos)[i];
        if (entry.is_object() && entry.value("is_timeline", false) && entry.value("target", "") == "volume") {
            entry_index = i;
            found = true;
            break;
        }
    }
    if (!found) return false;

    modify([&](nlohmann::json& root) {
        if (auto* list = find_automation_list(root, event_path, track_index)) {
            if (entry_index < list->size()) {
                list->erase(list->begin() + static_cast<ptrdiff_t>(entry_index));
            }
        }
    });
    return true;
}

bool AuthoringModel::add_parameter_automation(const std::string& event_path, int track_index,
                                              const std::string& target, const std::string& param_scope,
                                              const std::string& param_name, const nlohmann::json& points)
{
    if ((target != "volume" && target != "pitch" && target != "pan") ||
        (param_scope != "local" && param_scope != "global") || param_name.empty()) {
        return false;
    }
    nlohmann::json normalized_pts;
    if (!normalize_curve_points(points, normalized_pts)) return false;
    if (find_automation_list(json(), event_path, track_index) == nullptr) return false;
    modify([&](nlohmann::json& root) {
        if (auto* autos = find_automation_list(root, event_path, track_index)) {
            autos->push_back({
                { "target", target },
                { "is_timeline", false },
                { "param", { { "scope", param_scope }, { "name", param_name } } },
                { "curve", { { "points", normalized_pts } } }
            });
        }
    });
    return true;
}

bool AuthoringModel::update_automation_curve(const std::string& event_path, int track_index,
                                             size_t auto_index, const nlohmann::json& points)
{
    nlohmann::json normalized_pts;
    if (!normalize_curve_points(points, normalized_pts)) return false;
    auto* current = find_automation_list(json(), event_path, track_index);
    if (current == nullptr || auto_index >= current->size()) return false;
    if ((*current)[auto_index]["curve"]["points"] == normalized_pts) return true;
    modify([&](nlohmann::json& root) {
        if (auto* autos = find_automation_list(root, event_path, track_index)) {
            (*autos)[auto_index]["curve"]["points"] = normalized_pts;
        }
    });
    return true;
}

bool AuthoringModel::remove_automation(const std::string& event_path, int track_index, size_t auto_index)
{
    auto* current = find_automation_list(json(), event_path, track_index);
    if (current == nullptr || auto_index >= current->size()) return false;
    modify([&](nlohmann::json& root) {
        if (auto* autos = find_automation_list(root, event_path, track_index)) {
            autos->erase(autos->begin() + static_cast<ptrdiff_t>(auto_index));
        }
    });
    return true;
}

double AuthoringModel::opacity_pct_to_db(double opacity_pct) noexcept
{
    const double clamped = std::clamp(opacity_pct, 0.0, 100.0);
    return -80.0 + (clamped / 100.0) * 80.0;
}

double AuthoringModel::db_to_opacity_pct(double volume_db) noexcept
{
    const double clamped = std::clamp(volume_db, -80.0, 0.0);
    return ((clamped + 80.0) / 80.0) * 100.0;
}

double AuthoringModel::evaluate_layer_mask_gain(const LayerMaskSpec& mask, double param_norm) noexcept
{
    if (mask.state != LayerMaskSpec::State::MaskActive) {
        return 1.0;
    }

    const double p = std::clamp(param_norm, 0.0, 1.0);
    constexpr double kEps = 1e-4;
    const double eff_fade = std::max(mask.fade_norm, 0.001);

    if (p >= mask.min_norm && p <= mask.max_norm) {
        return 1.0;
    }
    if (p < mask.min_norm) {
        if (mask.min_norm <= kEps) return 1.0;
        const double fade_start = std::max(0.0, mask.min_norm - eff_fade);
        if (p <= fade_start) return 0.0;
        return std::clamp((p - fade_start) / (mask.min_norm - fade_start), 0.0, 1.0);
    }

    if (mask.max_norm >= 1.0 - kEps) return 1.0;
    const double fade_end = std::min(1.0, mask.max_norm + eff_fade);
    if (p >= fade_end) return 0.0;
    return std::clamp((fade_end - p) / (fade_end - mask.max_norm), 0.0, 1.0);
}

LayerMaskSpec AuthoringModel::get_track_layer_mask(const std::string& event_path, int track_index) const
{
    LayerMaskSpec result;
    if (track_index < 0) return result;

    const auto* track = find_automation_owner(json(), event_path, track_index);
    if (!track) return result;

    const auto autos_it = track->find("automations");
    if (autos_it == track->end() || !autos_it->is_array()) {
        return result;
    }

    int param_volume_count = 0;
    int matched_index = -1;
    for (size_t i = 0; i < autos_it->size(); ++i) {
        const auto& entry = (*autos_it)[i];
        if (!entry.is_object()) continue;
        if (!entry.value("is_timeline", false) && entry.value("target", "") == "volume") {
            ++param_volume_count;
            matched_index = static_cast<int>(i);
        }
    }

    if (param_volume_count == 0) {
        result.state = LayerMaskSpec::State::AlwaysActive;
        return result;
    }
    if (param_volume_count > 1) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }

    const auto& entry = (*autos_it)[static_cast<size_t>(matched_index)];
    result.auto_index = matched_index;

    const auto param_it = entry.find("param");
    if (param_it == entry.end() || !param_it->is_object()) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }
    result.param_scope = param_it->value("scope", "local");
    result.param_name = param_it->value("name", "");
    if (result.param_name.empty()) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }

    const auto curve_it = entry.find("curve");
    if (curve_it == entry.end() || !curve_it->is_object()) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }
    const auto pts_it = curve_it->find("points");
    if (pts_it == curve_it->end() || !pts_it->is_array() || pts_it->size() < 2 || pts_it->size() > 4) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }

    const auto [dom_min, dom_max] = get_parameter_domain(event_path, result.param_scope, result.param_name);
    const double dom_span = (dom_max > dom_min) ? (dom_max - dom_min) : 1.0;

    std::vector<double> norm_x;
    std::vector<bool> is_high;
    norm_x.reserve(pts_it->size());
    is_high.reserve(pts_it->size());

    for (const auto& pt : *pts_it) {
        if (!pt.is_object() || pt.value("shape", "linear") != "linear") {
            result.state = LayerMaskSpec::State::CustomCurve;
            return result;
        }
        const double x = pt.value("x", 0.0);
        const double y = pt.value("y", 0.0);
        const bool high = std::abs(y) <= 0.5;
        const bool low = y <= -60.0;
        if (!high && !low) {
            result.state = LayerMaskSpec::State::CustomCurve;
            return result;
        }
        norm_x.push_back(std::clamp((x - dom_min) / dom_span, 0.0, 1.0));
        is_high.push_back(high);
    }

    int first_high = -1;
    int last_high = -1;
    for (size_t i = 0; i < is_high.size(); ++i) {
        if (is_high[i]) {
            if (first_high < 0) first_high = static_cast<int>(i);
            last_high = static_cast<int>(i);
        }
    }
    if (first_high < 0) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }

    for (int i = first_high; i <= last_high; ++i) {
        if (!is_high[static_cast<size_t>(i)]) {
            result.state = LayerMaskSpec::State::CustomCurve;
            return result;
        }
    }

    const int n = static_cast<int>(is_high.size());
    if (first_high > 1 || (n - 1 - last_high) > 1) {
        result.state = LayerMaskSpec::State::CustomCurve;
        return result;
    }

    result.state = LayerMaskSpec::State::MaskActive;
    if (first_high == 0 && last_high == n - 1) {
        result.min_norm = norm_x.front();
        result.max_norm = norm_x.back();
        result.fade_norm = 0.0;
    } else {
        result.min_norm = (first_high == 0) ? 0.0 : norm_x[static_cast<size_t>(first_high)];
        result.max_norm = (last_high == n - 1) ? 1.0 : norm_x[static_cast<size_t>(last_high)];
        const double fade_in = (first_high == 1)
            ? std::max(0.0, norm_x[1] - norm_x[0])
            : 0.0;
        const double fade_out = (last_high + 1 < n)
            ? std::max(0.0, norm_x[static_cast<size_t>(n - 1)] - norm_x[static_cast<size_t>(last_high)])
            : 0.0;
        result.fade_norm = std::clamp(std::max(fade_in, fade_out), 0.0, 0.5);
    }
    return result;
}

bool AuthoringModel::set_track_layer_mask(const std::string& event_path, int track_index,
                                          const std::string& param_scope, const std::string& param_name,
                                          double min_norm, double max_norm, double fade_norm,
                                          bool force_overwrite)
{
    if (track_index < 0 || (param_scope != "local" && param_scope != "global") || param_name.empty()) {
        return false;
    }

    const auto existing = get_track_layer_mask(event_path, track_index);
    if (existing.state == LayerMaskSpec::State::CustomCurve && !force_overwrite) {
        return false;
    }

    const double clamped_min = std::clamp(min_norm, 0.0, 1.0);
    const double clamped_max = std::clamp(max_norm, clamped_min, 1.0);
    const double clamped_fade = std::clamp(fade_norm, 0.0, 0.5);

    const auto [dom_min, dom_max] = get_parameter_domain(event_path, param_scope, param_name);
    const double dom_span = (dom_max > dom_min) ? (dom_max - dom_min) : 1.0;
    const auto to_dom = [dom_min, dom_span](double t) {
        return dom_min + std::clamp(t, 0.0, 1.0) * dom_span;
    };

    constexpr double kEps = 1e-4;
    const double eff_fade = std::max(clamped_fade, 0.001);

    nlohmann::json raw_points = nlohmann::json::array();
    if (clamped_min > kEps) {
        raw_points.push_back({ { "x", to_dom(std::max(0.0, clamped_min - eff_fade)) }, { "y", -80.0 }, { "shape", "linear" } });
        raw_points.push_back({ { "x", to_dom(clamped_min) }, { "y", 0.0 }, { "shape", "linear" } });
    } else {
        raw_points.push_back({ { "x", to_dom(0.0) }, { "y", 0.0 }, { "shape", "linear" } });
    }

    if (clamped_max < 1.0 - kEps) {
        if (clamped_max > clamped_min + kEps) {
            raw_points.push_back({ { "x", to_dom(clamped_max) }, { "y", 0.0 }, { "shape", "linear" } });
        }
        raw_points.push_back({ { "x", to_dom(std::min(1.0, clamped_max + eff_fade)) }, { "y", -80.0 }, { "shape", "linear" } });
    } else {
        raw_points.push_back({ { "x", to_dom(1.0) }, { "y", 0.0 }, { "shape", "linear" } });
    }

    nlohmann::json normalized_pts;
    if (!normalize_curve_points(raw_points, normalized_pts)) {
        return false;
    }

    if (find_automation_list(json(), event_path, track_index) == nullptr) {
        return false;
    }

    modify([&](nlohmann::json& root) {
        auto* autos = find_automation_list(root, event_path, track_index);
        if (!autos) return;

        nlohmann::json new_entry = {
            { "target", "volume" },
            { "is_timeline", false },
            { "param", { { "scope", param_scope }, { "name", param_name } } },
            { "curve", { { "points", normalized_pts } } }
        };

        if (existing.state == LayerMaskSpec::State::MaskActive &&
            existing.auto_index >= 0 &&
            static_cast<size_t>(existing.auto_index) < autos->size()) {
            (*autos)[static_cast<size_t>(existing.auto_index)] = std::move(new_entry);
            return;
        }

        for (auto it = autos->begin(); it != autos->end();) {
            if (it->is_object() && !it->value("is_timeline", false) && it->value("target", "") == "volume") {
                it = autos->erase(it);
            } else {
                ++it;
            }
        }
        autos->push_back(std::move(new_entry));
    });
    return true;
}

bool AuthoringModel::clear_track_layer_mask(const std::string& event_path, int track_index)
{
    const auto existing = get_track_layer_mask(event_path, track_index);
    if (existing.state != LayerMaskSpec::State::MaskActive || existing.auto_index < 0) {
        return false;
    }
    return remove_automation(event_path, track_index, static_cast<size_t>(existing.auto_index));
}

bool AuthoringModel::add_effect_automation(const std::string& bus_path, int position, size_t effect_index,
                                           const std::string& effect_type, int param_id,
                                           const std::string& global_param_name, const nlohmann::json& points)
{
    if (bus_path.empty() || global_param_name.empty() || param_id < 0) return false;
    nlohmann::json normalized_pts;
    if (!normalize_curve_points(points, normalized_pts)) return false;
    modify([&](nlohmann::json& root) {
        ensure_array(root, "effect_automations");
        root["effect_automations"].push_back({
            { "bus_path", bus_path },
            { "position", position == 1 ? 1 : 0 },
            { "effect_index", static_cast<uint32_t>(effect_index) },
            { "effect_type", effect_type },
            { "param_id", static_cast<uint32_t>(param_id) },
            { "param", { { "scope", "global" }, { "name", global_param_name } } },
            { "curve", { { "points", normalized_pts } } }
        });
    });
    return true;
}

bool AuthoringModel::update_effect_automation_curve(size_t ea_index, const nlohmann::json& points)
{
    nlohmann::json normalized_pts;
    if (!normalize_curve_points(points, normalized_pts)) return false;
    const auto& root = doc_.get_root_json();
    if (!root.contains("effect_automations") || !root["effect_automations"].is_array() ||
        ea_index >= root["effect_automations"].size()) {
        return false;
    }
    if (root["effect_automations"][ea_index]["curve"]["points"] == normalized_pts) return true;
    modify([&](nlohmann::json& r) {
        r["effect_automations"][ea_index]["curve"]["points"] = normalized_pts;
    });
    return true;
}

bool AuthoringModel::remove_effect_automation(size_t ea_index)
{
    const auto& root = doc_.get_root_json();
    if (!root.contains("effect_automations") || !root["effect_automations"].is_array() ||
        ea_index >= root["effect_automations"].size()) {
        return false;
    }
    modify([&](nlohmann::json& r) {
        auto& eas = r["effect_automations"];
        eas.erase(eas.begin() + static_cast<ptrdiff_t>(ea_index));
    });
    return true;
}

namespace {

/** Finds the persisted owner of an insert chain. */
template <typename Json>
Json* find_effect_owner(Json& root, const ItemSelection& target)
{
    const auto& path = target.id_or_path;
    if (target.type == ItemType::Event && target.track_index >= 0) {
        const auto events = root.find("events");
        if (events == root.end() || !events->is_array()) return nullptr;
        for (auto& event : *events) {
            if (!event.is_object() || event.value("path", "") != path) continue;
            const auto tracks = event.find("tracks");
            const auto index = static_cast<size_t>(target.track_index);
            if (tracks == event.end() || !tracks->is_array() || index >= tracks->size()) return nullptr;
            auto& track = (*tracks)[index];
            return track.is_object() ? &track : nullptr;
        }
        return nullptr;
    }
    if (target.type != ItemType::Bus) return nullptr;
    if (path == "bus:/") {
        const auto mixer = root.find("mixer");
        if (mixer == root.end() || !mixer->is_object()) return nullptr;
        const auto master = mixer->find("master");
        return master != mixer->end() && master->is_object() ? &*master : nullptr;
    }
    const auto buses = root.find("buses");
    if (buses == root.end() || !buses->is_array()) return nullptr;
    for (auto& bus : *buses) {
        if (bus.is_object() && bus.value("path", "") == path) return &bus;
    }
    return nullptr;
}

} // namespace

const nlohmann::json* AuthoringModel::get_effects(const ItemSelection& target) const
{
    const auto* owner = find_effect_owner(json(), target);
    if (owner == nullptr) return nullptr;
    const auto effects = owner->find("effects");
    return effects != owner->end() && effects->is_array() ? &*effects : nullptr;
}

bool AuthoringModel::add_effect_to_bus(const std::string& bus_path, const std::string& effect_type, int position)
{
    return add_effect({ ItemType::Bus, bus_path }, effect_type, position);
}

nlohmann::json AuthoringModel::default_effect_parameters(const std::string& effect_type)
{
    nlohmann::json params;
    if (effect_type == "limiter") {
        params = { -0.1, 10.0, 1.5 };
    } else if (effect_type == "compressor") {
        params = { 0.0, 2.0, 10.0, 100.0, 0.0 };
    } else if (effect_type == "eq3") {
        params = { 0.0, 0.0, 0.0, 250.0, 2500.0 };
    } else if (effect_type == "lowpass") {
        params = { 20000.0, 0.707 };
    } else if (effect_type == "highpass") {
        params = { 20.0, 0.707 };
    } else if (effect_type == "reverb") {
        params = { 0.5, 0.5, 1.0 };
    } else if (effect_type == "delay") {
        params = { 250.0, 0.3 };
    } else {
        return {};
    }
    return params;
}

bool AuthoringModel::add_effect(const ItemSelection& target, const std::string& effect_type, int position)
{
    if (!doc_.is_loaded() || position < 0 || position > 1) return false;
    const bool master = target.type == ItemType::Bus && target.id_or_path == "bus:/";
    if (!master && find_effect_owner(json(), target) == nullptr) return false;

    auto params = default_effect_parameters(effect_type);
    if (params.empty()) return false;

    const auto* effects = get_effects(target);
    if (effects != nullptr && std::count_if(effects->begin(), effects->end(), [position](const auto& effect) {
        return effect.value("position", 0) == position;
    }) >= 8) return false;

    modify([&](nlohmann::json& root) {
        auto& owner = master ? root["mixer"]["master"] : *find_effect_owner(root, target);
        ensure_array(owner, "effects");
        owner["effects"].push_back({
            { "type", effect_type },
            { "position", position },
            { "bypass", false },
            { "wet", 1.0 },
            { "dry", 0.0 },
            { "params", std::move(params) }
        });
    });
    return true;
}

bool AuthoringModel::remove_effect_from_bus(const std::string& bus_path, size_t effect_index)
{
    return remove_effect({ ItemType::Bus, bus_path }, effect_index);
}

bool AuthoringModel::remove_effect(const ItemSelection& target, size_t effect_index)
{
    const auto* effects = get_effects(target);
    if (effects == nullptr || effect_index >= effects->size()) return false;
    modify([&](nlohmann::json& root) {
        auto& list = (*find_effect_owner(root, target))["effects"];
        list.erase(list.begin() + effect_index);
    });
    return true;
}

bool AuthoringModel::modify_effect(const ItemSelection& target, size_t effect_index,
                                   const std::function<void(nlohmann::json&)>& mutator)
{
    const auto* effects = get_effects(target);
    if (effects == nullptr || effect_index >= effects->size()) return false;
    auto updated = (*effects)[effect_index];
    mutator(updated);
    if (updated == (*effects)[effect_index]) return false;
    modify([&](nlohmann::json& root) {
        (*find_effect_owner(root, target))["effects"][effect_index] = std::move(updated);
    });
    return true;
}

} // namespace oae::studio
