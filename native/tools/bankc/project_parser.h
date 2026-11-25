#pragma once

#include "project_schema.h"
#include "../../src/core/effect.h"
#include <functional>
#include <regex>
#include <set>
#include <limits>

namespace bankc {
using namespace oae;

inline std::string pointer_key(const std::string& key) {
    std::string result;
    for (char c : key) result += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
    return result;
}

[[noreturn]] inline void fail(const std::string& path, const std::string& message) {
    throw std::invalid_argument("Error at " + (path.empty() ? "/" : path) + ": " + message);
}

/** Validates the keywords used by the embedded project schema and applies its defaults. */
class SchemaValidator {
public:
    SchemaValidator() : schema_(json::parse(oae_project_schema_json)) {}
    void validate(json& value) const { check(value, schema_, ""); }
private:
    void check(json& value, const json& rule, const std::string& path) const {
        if (rule.contains("$ref")) {
            check(value, schema_.at(json::json_pointer(rule["$ref"].get<std::string>().substr(1))), path);
        }
        for (const auto* keyword : {"oneOf", "anyOf"}) {
            if (!rule.contains(keyword)) continue;
            size_t matches = 0;
            json normalized;
            std::string detail;
            for (const auto& alternative : rule[keyword]) {
                json copy = value;
                try { check(copy, alternative, path); normalized = std::move(copy); ++matches; }
                catch (const std::invalid_argument& e) {
                    if (detail.empty()) detail = e.what();
                    if (alternative.contains("properties")) {
                        for (const auto& property : alternative["properties"].items()) {
                            if (property.value().contains("const") && value.contains(property.key()) &&
                                value[property.key()] == property.value()["const"]) detail = e.what();
                        }
                    }
                }
            }
            if (matches == 0) throw std::invalid_argument(detail.empty() ? "Error at " + path + ": no valid variant" : detail);
            if (std::string(keyword) == "oneOf" && matches != 1) fail(path, "Ambiguous variant.");
            value = std::move(normalized);
        }
        if (rule.contains("type")) {
            const auto type = rule["type"].get<std::string>();
            const bool valid = type == "object" ? value.is_object() : type == "array" ? value.is_array() :
                type == "string" ? value.is_string() : type == "boolean" ? value.is_boolean() :
                type == "number" ? value.is_number() : type == "integer" ? value.is_number() &&
                std::floor(value.get<double>()) == value.get<double>() : false;
            if (!valid) fail(path, "Expected " + type + ".");
        }
        if (rule.contains("const") && value != rule["const"]) fail(path, "Unexpected discriminator or version.");
        if (rule.contains("enum") && std::find(rule["enum"].begin(), rule["enum"].end(), value) == rule["enum"].end())
            fail(path, "Unknown enumeration value.");
        if (value.is_number()) {
            const double n = value.get<double>();
            if (!std::isfinite(n) || std::abs(n) > std::numeric_limits<float>::max()) fail(path, "Number is not finite or representable.");
            if (rule.contains("minimum") && n < rule["minimum"].get<double>()) fail(path, "Below minimum.");
            if (rule.contains("maximum") && n > rule["maximum"].get<double>()) fail(path, "Above maximum.");
            if (rule.contains("exclusiveMinimum") && n <= rule["exclusiveMinimum"].get<double>()) fail(path, "Must exceed minimum.");
        }
        if (value.is_string()) {
            const auto text = value.get<std::string>();
            if (text.find('\0') != std::string::npos) fail(path, "NUL is not permitted.");
            if (rule.contains("minLength") && text.size() < rule["minLength"].get<size_t>()) fail(path, "String is empty.");
            if (rule.contains("pattern")) {
                auto pattern = rule["pattern"].get<std::string>();
                size_t pos = 0;
                while ((pos = pattern.find("\\u0000", pos)) != std::string::npos) pattern.replace(pos, 6, "\\x00");
                if (!std::regex_search(text, std::regex(pattern))) fail(path, "Invalid path or string format.");
            }
        }
        if (value.is_array()) {
            if (rule.contains("minItems") && value.size() < rule["minItems"].get<size_t>()) fail(path, "Too few items.");
            if (rule.contains("maxItems") && value.size() > rule["maxItems"].get<size_t>()) fail(path, "Too many items.");
            for (size_t i = 0; i < value.size(); ++i) {
                if (rule.value("uniqueItems", false)) for (size_t j = 0; j < i; ++j)
                    if (value[i] == value[j]) fail(path + "/" + std::to_string(i), "Duplicate item.");
                if (rule.contains("items")) check(value[i], rule["items"], path + "/" + std::to_string(i));
            }
        }
        if (value.is_object()) {
            if (rule.contains("required")) for (const auto& key : rule["required"])
                if (!value.contains(key.get<std::string>())) fail(path + "/" + pointer_key(key.get<std::string>()), "Required field missing.");
            const auto properties = rule.value("properties", json::object());
            for (auto& item : value.items()) {
                const auto child = path + "/" + pointer_key(item.key());
                if (rule.contains("propertyNames")) { json key = item.key(); check(key, rule["propertyNames"], child); }
                if (properties.contains(item.key())) check(item.value(), properties[item.key()], child);
                else if (rule.contains("additionalProperties")) {
                    if (rule["additionalProperties"] == false) fail(child, "Unknown field.");
                    if (rule["additionalProperties"].is_object()) check(item.value(), rule["additionalProperties"], child);
                }
            }
            for (const auto& item : properties.items()) if (!value.contains(item.key()) && item.value().contains("default"))
                value[item.key()] = item.value()["default"];
        }
    }
    json schema_;
};

inline const json& array_at(const json& value, const char* key) {
    static const json empty = json::array();
    return value.contains(key) ? value.at(key) : empty;
}

/** Converts validated authoring objects into the immutable engine model and bank records. */
class ProjectParser {
public:
    ProjectParser(json& project, fs::path directory) : root_(project), directory_(fs::canonical(directory)) {}
    void compile(BankWriter& writer, std::unordered_map<std::string, Guid>& ids) {
        migrate();
        SchemaValidator{}.validate(root_);
        parse_buses(writer);
        parse_samples(writer);
        globals_ = parameters(array_at(root_, "global_parameters"), "/global_parameters");
        const auto& events = array_at(root_, "events");
        for (size_t i = 0; i < events.size(); ++i) {
            const auto path = events[i]["path"].get<std::string>();
            identity(path, "/events/" + std::to_string(i) + "/path");
            if (!ids.emplace(path, make_deterministic_guid(path)).second) fail("/events/" + std::to_string(i) + "/path", "Duplicate event path.");
        }
        ids_ = &ids;
        ModelSetBuilder builder;
        for (const auto& p : globals_) builder.add_global_parameter(p);
        for (size_t i = 0; i < events.size(); ++i) builder.add_event(event(events[i], "/events/" + std::to_string(i)));
        std::set<std::string> done, visiting;
        std::function<void(const std::string&)> visit = [&](const std::string& node) {
            if (done.count(node)) return;
            if (!visiting.insert(node).second) fail(edge_paths_[node], "Nested event cycle detected.");
            for (const auto& target : dependencies_[node]) visit(target);
            visiting.erase(node); done.insert(node);
        };
        for (const auto& id : ids) visit(id.first);
        const auto& effect_autos = array_at(root_, "effect_automations");
        for (size_t i = 0; i < effect_autos.size(); ++i) {
            const auto& a = effect_autos[i]; const auto path = "/effect_automations/" + std::to_string(i);
            const auto* range = effect_target(a, path);
            EffectAutomation result;
            result.bus_path = a["bus_path"]; result.position = a["position"]; result.effect_index = a["effect_index"];
            result.effect_type = effect_type(a["effect_type"]); result.param_index = a["param_id"];
            result.param = param(a["param"], {}, path + "/param");
            const auto& definition = definition_for(result.param, {});
            result.curve = curve(a["curve"], path + "/curve", definition.min_value, definition.max_value, range->low, range->high);
            builder.add_effect_automation(result);
        }
        std::set<std::string> vca_paths;
        const auto& vcas = array_at(root_, "vcas");
        for (size_t i = 0; i < vcas.size(); ++i) {
            const auto& v = vcas[i]; const auto path = "/vcas/" + std::to_string(i);
            VcaModel result; result.path = v["path"]; identity(result.path, path + "/path");
            if (!vca_paths.insert(result.path).second) fail(path + "/path", "Duplicate VCA path.");
            for (const auto& b : array_at(v, "buses")) { require_bus(b, path + "/buses"); result.bus_paths.push_back(b); }
            for (const auto& e : array_at(v, "events")) { require_event(e, false, path + "/events"); result.event_paths.push_back(e); }
            builder.add_vca(result);
        }
        std::set<std::string> audio_table_keys;
        const auto& audio_table = array_at(root_, "audio_table");
        for (size_t i = 0; i < audio_table.size(); ++i) {
            const auto& entry = audio_table[i];
            const auto ap = "/audio_table/" + std::to_string(i);
            const std::string key = entry["key"];
            if (key.empty()) fail(ap + "/key", "Audio table key must not be empty.");
            if (!audio_table_keys.insert(key).second) fail(ap + "/key", "Duplicate audio table key.");
            AudioTableEntry table_entry;
            table_entry.key = key;
            table_entry.sample_index = sample(entry["sample_id"], ap + "/sample_id");
            builder.add_audio_table_entry(std::move(table_entry));
        }
        writer.set_model_set(builder.build());
    }
private:
    void migrate() {
        if (!root_.is_object()) return;
        if (root_.contains("events") && root_["events"].is_array()) for (auto& ev : root_["events"]) {
            if (!ev.is_object()) continue;
            if (!ev.contains("kind") && ev.contains("path") && ev["path"].is_string())
                ev["kind"] = ev["path"].get<std::string>().rfind("snapshot:/", 0) == 0 ? "snapshot" : "event";
            if (ev.contains("snapshot_targets") && ev["snapshot_targets"].is_array()) for (auto& target : ev["snapshot_targets"])
                if (target.is_object() && target.value("param_id", json(-1)) == -1) {
                    if (target.contains("position") && target["position"] == 0) target.erase("position");
                    if (target.contains("effect_index") && target["effect_index"] == 0) target.erase("effect_index");
                }
            std::function<void(json&)> legacy = [&](json& value) {
                if (value.is_array()) { for (auto& child : value) legacy(child); return; }
                if (!value.is_object()) return;
                if (value.contains("param_name")) {
                    if (value.contains("param")) fail("/events", "Both param and legacy param_name supplied.");
                    const auto name = value["param_name"];
                    size_t local = 0, global = 0;
                    for (const auto& p : array_at(ev, "parameters")) if (p.is_object() && p.value("name", json()) == name) ++local;
                    for (const auto& p : array_at(root_, "global_parameters")) if (p.is_object() && p.value("name", json()) == name) ++global;
                    if (local + global != 1) fail("/events", "Legacy param_name is missing or ambiguous; use explicit param scope.");
                    value["param"] = {{"scope", global ? "global" : "local"}, {"name", name}};
                    value.erase("param_name");
                }
                if (value.value("kind", json()) == "random" && (value.contains("min") || value.contains("max"))) {
                    if (value.contains("range") || !value.contains("min") || !value.contains("max") ||
                        !value["min"].is_number() || !value["max"].is_number() || value["min"].get<double>() != -value["max"].get<double>())
                        fail("/events", "Legacy RANDOM must be symmetric min=-max and have no range.");
                    value["range"] = value["max"]; value.erase("min"); value.erase("max");
                }
                if (value.contains("instruments") && value["instruments"].is_array()) for (auto& inst : value["instruments"])
                    if (inst.is_object() && !inst.contains("kind") && inst.contains("sample_id")) inst["kind"] = "single";
                for (auto& child : value) legacy(child);
            };
            legacy(ev);
        }
    }
    static void identity(const std::string& value, const std::string& path) {
        auto offset = value.find(":/");
        if (offset == std::string::npos) return;
        offset += 2;
        while (offset < value.size()) {
            auto end = value.find('/', offset); if (end == std::string::npos) end = value.size();
            const auto segment = value.substr(offset, end - offset);
            if (segment.empty() || segment == "." || segment == "..") fail(path, "Invalid identity segment.");
            offset = end + 1;
        }
    }
    static int effect_type(const std::string& name) {
        static const std::vector<std::string> names{"limiter", "compressor", "eq3", "lowpass", "highpass", "reverb", "delay"};
        return static_cast<int>(std::find(names.begin(), names.end(), name) - names.begin());
    }
    /** Parses the effect settings shared by bus and per-instance track chains. */
    static BusEntry::EffectDesc parse_effect(const json& effect, const std::string& path, int (&counts)[2]) {
        BusEntry::EffectDesc parsed;
        parsed.type = effect_type(effect["type"]);
        parsed.position = effect["position"];
        parsed.bypass = effect["bypass"];
        parsed.wet = effect["wet"];
        parsed.dry = effect["dry"];
        parsed.params = effect["params"].get<std::vector<float>>();
        if (++counts[parsed.position] > 8) fail(path, "More than eight effects in this position.");
        for (size_t index = 0; index < parsed.params.size(); ++index) {
            const auto* range = effect_parameter(parsed.type, static_cast<int>(index));
            if (!range || parsed.params[index] < range->low || parsed.params[index] > range->high) {
                fail(path + "/params/" + std::to_string(index), "Effect parameter outside native range.");
            }
        }
        if (parsed.type == OAE_EFFECT_EQ3 && parsed.params[3] >= parsed.params[4]) {
            fail(path + "/params", "EQ crossover frequencies must increase.");
        }
        return parsed;
    }
    void require_bus(const std::string& name, const std::string& path) const {
        if (!bus_indices_.count(name)) fail(path, "Unknown bus '" + name + "'.");
    }
    Guid require_event(const std::string& name, bool snapshot, const std::string& path) const {
        auto it = ids_->find(name);
        if (it == ids_->end() || (name.rfind("snapshot:/", 0) == 0) != snapshot) fail(path, "Unknown event or wrong target kind '" + name + "'.");
        return it->second;
    }
    void parse_buses(BankWriter& writer) {
        std::map<std::string, json> definitions;
        std::map<std::string, std::string> paths;
        json master = {{"volume", 1.0}, {"effects", json::array()}};
        if (root_.contains("mixer") && root_["mixer"].contains("master")) master = root_["mixer"]["master"];
        if (master.value("mute", false)) fail("/mixer/master/mute", "mute=true has no lossless BUSS v1 representation.");
        master["path"] = "bus:/"; definitions["bus:/"] = master; paths["bus:/"] = "/mixer/master";
        const auto& buses = array_at(root_, "buses");
        for (size_t i = 0; i < buses.size(); ++i) {
            const auto name = buses[i]["path"].get<std::string>(); const auto path = "/buses/" + std::to_string(i);
            identity(name, path + "/path");
            if (!definitions.emplace(name, buses[i]).second) fail(path + "/path", "Duplicate bus (master is implicit).");
            paths[name] = path;
        }
        std::set<std::string> visiting;
        std::function<void(const std::string&)> add = [&](const std::string& name) {
            if (bus_indices_.count(name)) return;
            if (!definitions.count(name)) fail("/buses", "Unknown parent bus '" + name + "'.");
            if (!visiting.insert(name).second) fail(paths[name] + "/parent", "Bus cycle.");
            const auto& b = definitions[name];
            BusEntry entry; entry.name = name; entry.volume = b.value("volume", 1.0f);
            if (name != "bus:/") { const auto parent = b.value("parent", std::string("bus:/")); add(parent); entry.parent_index = bus_indices_.at(parent); }
            bus_indices_[name] = static_cast<int32_t>(buses_.size()); buses_.push_back(entry); visiting.erase(name);
        };
        for (const auto& item : definitions) add(item.first);
        for (auto& bus : buses_) {
            int counts[2]{};
            const auto& effects = array_at(definitions[bus.name], "effects");
            for (size_t i = 0; i < effects.size(); ++i) {
                const auto& e = effects[i]; const auto path = paths[bus.name] + "/effects/" + std::to_string(i);
                auto effect = parse_effect(e, path, counts);
                if (e.contains("sidechain_bus")) {
                    const std::string target = e["sidechain_bus"]; require_bus(target, path + "/sidechain_bus");
                    if (target == bus.name) fail(path + "/sidechain_bus", "Self sidechain is invalid.");
                    effect.sidechain_bus = bus_indices_.at(target);
                }
                bus.effects.push_back(effect);
            }
        }
        for (const auto& bus : buses_) writer.add_bus(bus);
    }
    void parse_samples(BankWriter& writer) {
        const auto& samples = array_at(root_, "samples");
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto path = "/samples/" + std::to_string(i); const std::string id = samples[i]["id"];
            if (!samples_.emplace(id, static_cast<int32_t>(i)).second) fail(path + "/id", "Duplicate sample ID.");
            std::error_code ec;
            const auto resolved = fs::canonical(directory_ / fs::u8path(samples[i]["file"].get<std::string>()), ec);
            if (ec || !fs::is_regular_file(resolved)) fail(path + "/file", "Audio file not found.");
            const auto relative = resolved.lexically_relative(directory_);
            if (relative.empty() || *relative.begin() == "..") fail(path + "/file", "Audio must remain inside project root.");
            SampleEntry sample;
            sample.stream = samples[i].value("stream", false);
            if (!read_file_bytes(resolved, sample.data)) fail(path + "/file", "Cannot read audio file.");
            ma_decoder decoder;
            auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
            if (ma_decoder_init_memory(sample.data.data(), sample.data.size(), &config, &decoder) != MA_SUCCESS) fail(path + "/file", "Unsupported or invalid audio.");
            sample.sample_rate = decoder.outputSampleRate;
            ma_uint64 frames = 0;
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) != MA_SUCCESS || frames == 0) {
                std::vector<float> buffer(static_cast<size_t>(decoder.outputChannels) * 4096);
                ma_uint64 read = 0;
                do { const auto result = ma_decoder_read_pcm_frames(&decoder, buffer.data(), 4096, &read); frames += read; if (result != MA_SUCCESS) break; } while (read != 0);
            }
            ma_decoder_uninit(&decoder);
            sample.length_frames = frames;
            if (frames == 0 || sample.sample_rate == 0) fail(path + "/file", "Audio contains no valid frames.");
            writer.add_sample(std::move(sample));
        }
    }
    static std::vector<ParameterDefinition> parameters(const json& values, const std::string& path) {
        std::vector<ParameterDefinition> result; std::set<std::string> names;
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i]; const auto at = path + "/" + std::to_string(i);
            ParameterDefinition p; p.name = value["name"]; p.id = make_deterministic_guid(at + ":" + p.name);
            if (!names.insert(p.name).second) fail(at + "/name", "Duplicate parameter.");
            const std::string kind = value["kind"];
            p.kind = kind == "labeled" ? ParameterKind::Labeled : kind == "discrete" ? ParameterKind::Discrete : ParameterKind::Continuous;
            if (p.kind == ParameterKind::Labeled) { p.labels = value["labels"].get<std::vector<std::string>>(); p.max_value = static_cast<float>(p.labels.size() - 1); }
            else { p.min_value = value["min"]; p.max_value = value["max"]; }
            p.default_value = value.value("default_value", 0.0f); p.seek_speed = value.value("seek_speed", 0.0f); p.seek_speed_down = value.value("seek_speed_down", 0.0f);
            p.velocity = value.value("velocity", 0.0f); p.hold_during_playback = value.value("hold_during_playback", false);
            if (!(p.min_value <= p.default_value && p.default_value <= p.max_value)) fail(at + "/default_value", "Expected min <= default <= max.");
            result.push_back(p);
        }
        return result;
    }
    ParamRef param(const json& value, const std::vector<ParameterDefinition>& locals, const std::string& path) const {
        ParamRef result; result.name = value["name"]; result.is_global = value["scope"] == "global";
        const auto& definitions = result.is_global ? globals_ : locals;
        for (size_t i = 0; i < definitions.size(); ++i) if (definitions[i].name == result.name) {
            if (result.is_global) result.global_id = definitions[i].id; else result.local_index = static_cast<uint32_t>(i);
            return result;
        }
        fail(path + "/name", "Unknown parameter in selected scope.");
    }
    const ParameterDefinition& definition_for(const ParamRef& ref, const std::vector<ParameterDefinition>& locals) const {
        const auto& definitions = ref.is_global ? globals_ : locals;
        for (const auto& p : definitions) if (p.name == ref.name) return p;
        fail("/", "Internal parameter resolution failure.");
    }
    Condition condition(const json& value, const std::vector<ParameterDefinition>& locals, const std::string& path) const {
        Condition result; result.probability_pct = value.value("probability_pct", 100.0f);
        const auto& terms = array_at(value, "terms");
        for (size_t i = 0; i < terms.size(); ++i) {
            const auto at = path + "/terms/" + std::to_string(i); ConditionTerm term;
            term.param = param(terms[i]["param"], locals, at + "/param"); term.min_value = terms[i]["min"]; term.max_value = terms[i]["max"];
            const auto& p = definition_for(term.param, locals);
            if (!(p.min_value <= term.min_value && term.min_value <= term.max_value && term.max_value <= p.max_value)) fail(at, "Condition interval outside parameter domain.");
            result.terms.push_back(term);
        }
        return result;
    }
    static Curve curve(const json& value, const std::string& path, double xmin, double xmax, float ymin, float ymax) {
        Curve result; float previous = -std::numeric_limits<float>::infinity();
        const auto& points = value["points"];
        const std::vector<std::string> shapes{"linear", "hold", "ease_in", "ease_out", "s_curve"};
        for (size_t i = 0; i < points.size(); ++i) {
            CurvePoint p; p.x = points[i]["x"]; p.y = points[i]["y"];
            const std::string shape = points[i]["shape"]; p.shape = static_cast<CurveShape>(std::find(shapes.begin(), shapes.end(), shape) - shapes.begin());
            if (!(p.x > previous && p.x >= xmin && p.x <= xmax)) fail(path + "/points/" + std::to_string(i) + "/x", "Curve X must increase within source domain.");
            if (!(p.y >= ymin && p.y <= ymax)) fail(path + "/points/" + std::to_string(i) + "/y", "Curve Y outside target domain.");
            previous = p.x; result.points.push_back(p);
        }
        return result;
    }
    std::vector<Automation> automations(const json& values, const std::vector<ParameterDefinition>& locals, const std::string& path) const {
        std::vector<Automation> result;
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& v = values[i]; const auto at = path + "/" + std::to_string(i); Automation a;
            const std::string target = v["target"]; a.target = target == "volume" ? AutomationTarget::Volume : target == "pitch" ? AutomationTarget::Pitch : AutomationTarget::Pan;
            a.is_timeline = v["is_timeline"]; double xmin = 0, xmax = 86400000;
            if (!a.is_timeline) { a.param = param(v["param"], locals, at + "/param"); const auto& p = definition_for(a.param, locals); xmin = p.min_value; xmax = p.max_value; }
            a.curve = curve(v["curve"], at + "/curve", xmin, xmax, target == "pan" ? -1.0f : -200.0f, target == "pan" ? 1.0f : 200.0f);
            result.push_back(a);
        }
        return result;
    }
    static std::vector<Modulator> modulators(const json& values, const std::string& path) {
        std::vector<Modulator> result;
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& v = values[i]; Modulator m;
            if (v["kind"] == "ahdsr") {
                m.kind = ModulatorKind::Ahdsr; m.attack_ms = v["attack_ms"]; m.hold_ms = v["hold_ms"]; m.decay_ms = v["decay_ms"];
                m.sustain = v["sustain"]; m.release_ms = v["release_ms"]; m.release_on_key_off = v["release_on_key_off"];
            } else {
                const std::string target = v["target"]; const float range = v["range"];
                const float max = target == "volume" ? 80.0f : target == "pitch" ? 48.0f : 1.0f;
                if (range > max) fail(path + "/" + std::to_string(i) + "/range", "RANDOM exceeds target range.");
                m.property = target == "volume" ? ModulatorProperty::Volume : target == "pitch" ? ModulatorProperty::Pitch : ModulatorProperty::Pan;
                m.random_range_min = -range; m.random_range_max = range;
            }
            result.push_back(m);
        }
        return result;
    }
    int32_t sample(const std::string& name, const std::string& path) const {
        const auto it = samples_.find(name); if (it == samples_.end()) fail(path, "Unknown sample '" + name + "'."); return it->second;
    }
    Guid dependency(const std::string& target, const std::string& owner, const std::string& path) {
        const auto id = require_event(target, false, path); dependencies_[owner].push_back(target); edge_paths_[target] = path; return id;
    }
    const ParamRange* effect_target(const json& value, const std::string& path) const {
        const std::string bus = value["bus_path"]; require_bus(bus, path + "/bus_path");
        const auto& effects = buses_[static_cast<size_t>(bus_indices_.at(bus))].effects;
        uint32_t index = 0;
        for (const auto& effect : effects) if (effect.position == value["position"].get<int>()) {
            if (index++ != value["effect_index"].get<uint32_t>()) continue;
            if (effect.type != effect_type(value["effect_type"])) fail(path + "/effect_type", "Effect type mismatch.");
            const auto id = value["param_id"].get<uint32_t>();
            const auto* range = id <= static_cast<uint32_t>(INT_MAX) ? effect_parameter(effect.type, static_cast<int>(id)) : nullptr;
            if (!range) fail(path + "/param_id", "Invalid effect parameter ID."); return range;
        }
        fail(path + "/effect_index", "Effect not found at position and index.");
    }
    EventDescriptionModel event(const json& value, const std::string& path) {
        EventDescriptionModel e; e.path = value["path"]; e.id = ids_->at(e.path);
        e.kind = value["kind"] == "snapshot" ? EventKind::Snapshot : EventKind::Event;
        e.parameters = parameters(array_at(value, "parameters"), path + "/parameters");
        e.priority = value["priority"]; e.stop_fadeout_ms = value["stop_fadeout_ms"];
        if (value.contains("user_props")) for (const auto& p : value["user_props"].items()) e.user_props.emplace_back(p.key(), p.value().get<std::string>());
        if (e.kind == EventKind::Snapshot) {
            e.snapshot_mode = value["snapshot_mode"] == "blending" ? SnapshotMode::Blending : SnapshotMode::Override; e.fade_in_ms = value["fade_in_ms"];
            if (value.contains("intensity_param")) {
                e.intensity_param = value["intensity_param"];
                const auto ref = param(json{{"scope", "local"}, {"name", e.intensity_param}}, e.parameters, path + "/intensity_param");
                const auto& p = definition_for(ref, e.parameters);
                if (p.kind != ParameterKind::Continuous || p.min_value != 0 || p.max_value != 100) fail(path + "/intensity_param", "Intensity must be continuous 0..100.");
            }
            for (size_t i = 0; i < value["snapshot_targets"].size(); ++i) {
                const auto& v = value["snapshot_targets"][i]; const auto at = path + "/snapshot_targets/" + std::to_string(i); SnapshotTarget t;
                t.bus_path = v["bus_path"]; require_bus(t.bus_path, at + "/bus_path"); t.value = v["value"]; t.param_id = v.value("param_id", -1);
                if (t.param_id >= 0) { const auto* range = effect_target(v, at); if (t.value < range->low || t.value > range->high) fail(at + "/value", "Snapshot target outside effect range."); t.position = v["position"]; t.effect_index = v["effect_index"]; }
                e.snapshot_targets.push_back(t);
            }
            return e;
        }
        e.bus_path = value["bus_path"]; require_bus(e.bus_path, path + "/bus_path");
        e.volume_db = value["volume_db"]; e.pitch_semitones = value["pitch_semitones"]; e.pan = value["pan"];
        e.max_instances = value["max_instances"]; e.cooldown_ms = value["cooldown_ms"];
        e.steal = value["steal_mode"] == "none" ? StealMode::None : value["steal_mode"] == "quietest" ? StealMode::Quietest : StealMode::Oldest;
        e.modulators = modulators(array_at(value, "modulators"), path + "/modulators");
        e.automations = automations(array_at(value, "automations"), e.parameters, path + "/automations");
        const auto& tracks = array_at(value, "tracks");
        for (size_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& v = tracks[ti]; const auto at = path + "/tracks/" + std::to_string(ti); Track t;
            t.id = make_deterministic_guid(e.path + at); t.volume_db = v["volume_db"]; t.pan = v["pan"];
            t.automations = automations(array_at(v, "automations"), e.parameters, at + "/automations");
            const auto& effects = array_at(v, "effects");
            int counts[2]{};
            for (size_t fi = 0; fi < effects.size(); ++fi) {
                const auto& definition = effects[fi];
                const auto effect_path = at + "/effects/" + std::to_string(fi);
                if (definition.contains("sidechain_bus")) {
                    fail(effect_path + "/sidechain_bus", "Track effects do not support a sidechain bus.");
                }
                const auto effect = parse_effect(definition, effect_path, counts);
                TrackEffectSpec track_effect;
                track_effect.type = effect.type;
                track_effect.position = effect.position;
                track_effect.bypass = effect.bypass;
                track_effect.wet = effect.wet;
                track_effect.dry = effect.dry;
                for (size_t index = 0; index < effect.params.size(); ++index) {
                    track_effect.params.emplace_back(static_cast<int>(index), effect.params[index]);
                }
                t.effects.push_back(std::move(track_effect));
            }
            for (size_t ii = 0; ii < v["instruments"].size(); ++ii) {
                const auto& inst = v["instruments"][ii]; const auto ip = at + "/instruments/" + std::to_string(ii); Instrument in;
                const std::string kind = inst["kind"]; in.id = make_deterministic_guid(e.path + ip);
                in.kind = kind == "multi" ? InstrumentKind::Multi
                    : kind == "scatterer" ? InstrumentKind::Scatterer
                    : kind == "programmer" ? InstrumentKind::Programmer
                    : kind == "event" ? InstrumentKind::Event
                    : kind == "command" ? InstrumentKind::Command
                    : kind == "silence" ? InstrumentKind::Silence
                    : InstrumentKind::Single;
                in.offset_ms = inst["offset_ms"]; in.length_ms = inst.value("length_ms", 0.0); in.start_offset_ms = inst.value("start_offset_ms", 0.0);
                in.loop = inst.value("loop", false); in.async = inst.value("async", false); in.fade_in_ms = inst.value("fade_in_ms", 0.0); in.fade_out_ms = inst.value("fade_out_ms", 0.0);
                in.volume_db_base = inst.value("volume_db_base", 0.0f); in.pitch_st_base = inst.value("pitch_semitones_base", 0.0f);
                in.volume_db_random_range = inst.value("volume_db_random_range", 0.0f); in.pitch_st_random_range = inst.value("pitch_semitones_random_range", 0.0f);
                if (in.offset_ms + in.length_ms > 86400000) fail(ip + "/length_ms", "Instrument end exceeds timeline limit.");
                if (inst.contains("condition")) in.condition = condition(inst["condition"], e.parameters, ip + "/condition");
                in.modulators = modulators(array_at(inst, "modulators"), ip + "/modulators");
                if (inst.contains("sheet_parameter")) {
                    in.sheet_parameter = inst["sheet_parameter"].get<std::string>();
                    const ParameterDefinition* sp = nullptr;
                    for (const auto& lp : e.parameters) if (lp.name == in.sheet_parameter) { sp = &lp; break; }
                    if (!sp) for (const auto& gp : globals_) if (gp.name == in.sheet_parameter) { sp = &gp; break; }
                    if (!sp) fail(ip + "/sheet_parameter", "Unknown parameter for parameter sheet.");
                    in.param_min = inst.value("param_min", static_cast<double>(sp->min_value));
                    in.param_max = inst.value("param_max", static_cast<double>(sp->max_value));
                    if (!(in.param_min <= in.param_max &&
                          static_cast<float>(in.param_min) >= sp->min_value &&
                          static_cast<float>(in.param_max) <= sp->max_value)) {
                        fail(ip + "/param_min", "Parameter sheet range outside parameter domain.");
                    }
                }
                if (in.kind == InstrumentKind::Single) in.sample_index = sample(inst["sample_id"], ip + "/sample_id");
                else if (in.kind == InstrumentKind::Programmer) in.programmer_key = inst.value("programmer_key", std::string{});
                else if (in.kind == InstrumentKind::Event) in.event_ref = dependency(inst["target_event"], e.path, ip + "/target_event");
                else if (in.kind == InstrumentKind::Command) {
                    const std::string type = inst["command_type"], target = inst["command_target"];
                    in.command_type = type == "start_event" ? CommandType::StartEvent : type == "stop_event" ? CommandType::StopEvent : type == "snapshot_on" ? CommandType::SnapshotOn : CommandType::SnapshotOff;
                    in.command_target = require_event(target, type == "snapshot_on" || type == "snapshot_off", ip + "/command_target");
                    if (in.command_type == CommandType::StartEvent) dependency(target, e.path, ip + "/command_target");
                } else if (in.kind == InstrumentKind::Multi || in.kind == InstrumentKind::Scatterer) {
                    in.delay_ms_random_range = inst.value("delay_ms_random_range", 0u);
                    const std::string mode = inst.value("playlist_mode", "random");
                    in.playlist_mode = mode == "random_no_repeat" ? PlaylistMode::RandomNoRepeat : mode == "sequential" ? PlaylistMode::Sequential : mode == "shuffle" ? PlaylistMode::Shuffle : PlaylistMode::Random;
                    in.playlist_scope = inst.value("playlist_scope", "instance") == "instance" ? PlaylistScope::Instance : PlaylistScope::Global;
                    for (size_t pi = 0; pi < inst["playlist"].size(); ++pi) {
                        const auto& item = inst["playlist"][pi]; const auto pp = ip + "/playlist/" + std::to_string(pi); PlaylistEntry entry; entry.weight = item["weight"];
                        if (item.contains("sample_id")) entry.sample_index = sample(item["sample_id"], pp + "/sample_id");
                        else entry.event_guid = dependency(item["target_event"], e.path, pp + "/target_event");
                        in.playlist.push_back(entry);
                    }
                    if (in.kind == InstrumentKind::Scatterer) {
                        in.spawn_interval_min_ms = inst.value("spawn_interval_min_ms", 200.0);
                        in.spawn_interval_max_ms = inst.value("spawn_interval_max_ms", 1000.0);
                        in.polyphony = inst.value("polyphony", 4u);
                        in.total_sounds = inst.value("total_sounds", 0u);
                        in.min_scatter_distance = inst.value("min_scatter_distance", 1.0f);
                        in.max_scatter_distance = inst.value("max_scatter_distance", 10.0f);
                        if (!std::isfinite(in.spawn_interval_min_ms) || in.spawn_interval_min_ms < 1.0)
                            fail(ip + "/spawn_interval_min_ms", "Scatterer min interval must be >= 1 ms.");
                        if (!std::isfinite(in.spawn_interval_max_ms) || in.spawn_interval_max_ms < in.spawn_interval_min_ms)
                            fail(ip + "/spawn_interval_max_ms", "Scatterer max interval must be >= min interval.");
                        if (in.polyphony == 0 || in.polyphony > 64)
                            fail(ip + "/polyphony", "Scatterer polyphony must be in 1..64.");
                        if (!std::isfinite(in.min_scatter_distance) || in.min_scatter_distance < 0.0f)
                            fail(ip + "/min_scatter_distance", "Scatterer min distance must be >= 0.");
                        if (!std::isfinite(in.max_scatter_distance) || in.max_scatter_distance < in.min_scatter_distance)
                            fail(ip + "/max_scatter_distance", "Scatterer max distance must be >= min distance.");
                    }
                }
                t.instruments.push_back(in);
            }
            e.tracks.push_back(t);
        }
        std::set<std::string> markers;
        const auto& items = array_at(value, "logic_items");
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& v = items[i]; const auto at = path + "/logic_items/" + std::to_string(i); const std::string kind = v["kind"]; LogicItem item;
            item.id = make_deterministic_guid(e.path + at);
            if (kind == "marker") { item.kind = LogicItemKind::Marker; item.name = v["name"]; if (!markers.insert(item.name).second) fail(at + "/name", "Duplicate marker."); }
            else if (kind == "tempo") { item.kind = LogicItemKind::Tempo; item.bpm = v["bpm"]; item.ts_upper = v["ts_upper"]; item.ts_lower = v["ts_lower"]; }
            else if (kind == "loop_region") { item.kind = LogicItemKind::LoopRegion; item.repeat_count = v["iterations"]; item.until_key_off = v["until_key_off"]; }
            else if (kind == "sustain_point") item.kind = LogicItemKind::SustainPoint;
            else item.kind = kind == "transition_marker" ? LogicItemKind::TransitionMarker : LogicItemKind::TransitionRegion;
            item.position_ms = v.value("position_ms", v.value("start_ms", 0.0));
            if (v.contains("length_ms")) { item.end_ms = item.position_ms + v["length_ms"].get<double>(); if (item.end_ms > 86400000) fail(at + "/length_ms", "Region exceeds timeline domain."); }
            if (v.contains("target_marker")) { item.destination = v["target_marker"]; item.crossfade_ms = v.value("crossfade_ms", 0.0); const std::string q = v.value("quantization", "immediate"); item.quantization = q == "beat" ? LogicQuantization::Beat : q == "bar" ? LogicQuantization::Bar : LogicQuantization::Immediate; }
            if (v.contains("condition")) item.condition = condition(v["condition"], e.parameters, at + "/condition");
            e.logic_items.push_back(item);
        }
        for (size_t i = 0; i < e.logic_items.size(); ++i) if (!e.logic_items[i].destination.empty() && !markers.count(e.logic_items[i].destination)) fail(path + "/logic_items/" + std::to_string(i) + "/target_marker", "Unknown marker.");
        return e;
    }
    json& root_;
    fs::path directory_;
    std::vector<ParameterDefinition> globals_;
    std::vector<BusEntry> buses_;
    std::map<std::string, int32_t> bus_indices_;
    std::unordered_map<std::string, int32_t> samples_;
    std::unordered_map<std::string, Guid>* ids_ = nullptr;
    std::map<std::string, std::vector<std::string>> dependencies_;
    std::map<std::string, std::string> edge_paths_;
};
}
