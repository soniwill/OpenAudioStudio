#pragma once

#include "../common/ahdsr.h"
#include "../common/curve.h"
#include "../common/pcg32.h"
#include <oae_audio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace oae {

struct Guid {
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;

    constexpr bool operator==(const Guid& o) const noexcept { return hi == o.hi && lo == o.lo; }
    constexpr bool operator!=(const Guid& o) const noexcept { return !(*this == o); }
    constexpr bool is_null() const noexcept { return hi == 0 && lo == 0; }
};

Guid guid_from_string(const std::string& s) noexcept;

enum class InstrumentKind : std::uint8_t {
    Single = 0,
    Multi = 1,
    Event = 2,
    Command = 3,
    Silence = 4,
    Programmer = 5,
    Scatterer = 6
};

enum class AsyncOverlapPolicy : std::uint8_t {
    Overlap = 0,
    IgnoreIfPlaying = 1,
    CutPrevious = 2
};

enum class CommandType : std::uint8_t {
    StartEvent = 0,
    StopEvent = 1,
    SnapshotOn = 2,
    SnapshotOff = 3
};

enum class PlaylistMode : std::uint8_t {
    Random = 0,
    RandomNoRepeat = 1,
    Sequential = 2,
    Shuffle = 3
};

enum class PlaylistScope : std::uint8_t {
    Global = 0,
    Instance = 1
};

enum class StealMode : std::uint8_t {
    Oldest = 0,
    Quietest = 1,
    None = 2
};

enum class ModulatorKind : std::uint8_t {
    Random = 0,
    Ahdsr = 1
};

enum class ModulatorProperty : std::uint8_t {
    Volume = 0,
    Pitch = 1,
    Pan = 2
};

enum class ModulatorOwner : std::uint8_t {
    Event = 0,
    Track = 1,
    Instrument = 2
};

struct Modulator {
    Guid id{0, 0};
    ModulatorKind kind = ModulatorKind::Random;
    ModulatorOwner owner = ModulatorOwner::Event;
    std::uint32_t owner_index = 0;
    ModulatorProperty property = ModulatorProperty::Volume;
    float random_range_min = 0.0f;
    float random_range_max = 0.0f;

    float attack_ms = 0.0f;
    float hold_ms = 0.0f;
    float decay_ms = 0.0f;
    float sustain = 1.0f;
    float release_ms = 0.0f;
    bool release_on_key_off = false;

    static Modulator random_volume(float range_db) {
        Modulator m;
        m.kind = ModulatorKind::Random;
        m.property = ModulatorProperty::Volume;
        m.random_range_min = -std::clamp(range_db, 0.0f, 80.0f);
        m.random_range_max = std::clamp(range_db, 0.0f, 80.0f);
        return m;
    }

    static Modulator random_pitch(float range_st) {
        Modulator m;
        m.kind = ModulatorKind::Random;
        m.property = ModulatorProperty::Pitch;
        m.random_range_min = -std::clamp(range_st, 0.0f, 48.0f);
        m.random_range_max = std::clamp(range_st, 0.0f, 48.0f);
        return m;
    }

    static Modulator random_pan(float range_pan) {
        Modulator m;
        m.kind = ModulatorKind::Random;
        m.property = ModulatorProperty::Pan;
        m.random_range_min = -std::clamp(range_pan, 0.0f, 1.0f);
        m.random_range_max = std::clamp(range_pan, 0.0f, 1.0f);
        return m;
    }

    static Modulator ahdsr(float a_ms, float h_ms, float d_ms, float s, float r_ms, bool key_off = false) {
        Modulator m;
        m.kind = ModulatorKind::Ahdsr;
        m.attack_ms = a_ms;
        m.hold_ms = h_ms;
        m.decay_ms = d_ms;
        m.sustain = std::clamp(s, 0.0f, 1.0f);
        m.release_ms = r_ms;
        m.release_on_key_off = key_off;
        return m;
    }
};

struct ParamRef {
    bool is_global = false;
    std::uint32_t local_index = 0;
    std::string name;
    Guid global_id{0, 0};
};

enum class AutomationOwner : std::uint8_t {
    Event = 0,
    Track = 1
};

enum class AutomationTarget : std::uint8_t {
    Volume = 0,
    Pitch = 1,
    Pan = 2
};

struct Automation {
    Guid id{0, 0};
    AutomationOwner owner = AutomationOwner::Event;
    std::uint32_t owner_index = 0;
    ParamRef param;
    bool is_timeline = false;
    AutomationTarget target = AutomationTarget::Volume;
    Curve curve;

    static Automation volume_param(std::string param_name, Curve c, bool is_global = false) {
        Automation a;
        a.param.name = std::move(param_name);
        a.param.is_global = is_global;
        a.target = AutomationTarget::Volume;
        a.curve = std::move(c);
        return a;
    }

    static Automation pitch_param(std::string param_name, Curve c, bool is_global = false) {
        Automation a;
        a.param.name = std::move(param_name);
        a.param.is_global = is_global;
        a.target = AutomationTarget::Pitch;
        a.curve = std::move(c);
        return a;
    }

    static Automation pan_param(std::string param_name, Curve c, bool is_global = false) {
        Automation a;
        a.param.name = std::move(param_name);
        a.param.is_global = is_global;
        a.target = AutomationTarget::Pan;
        a.curve = std::move(c);
        return a;
    }

    static Automation timeline_volume(Curve c) {
        Automation a;
        a.is_timeline = true;
        a.target = AutomationTarget::Volume;
        a.curve = std::move(c);
        return a;
    }
};

struct EffectAutomation {
    Guid id{0, 0};
    std::string bus_path;
    int position = 0;
    int effect_type = 0;
    std::uint32_t effect_index = 0;
    std::uint32_t param_index = 0;
    ParamRef param;
    Curve curve;
};

enum class ParameterKind : std::uint8_t {
    Continuous = 0,
    Discrete = 1,
    Labeled = 2
};

struct ParameterDefinition {
    Guid id{0, 0};
    std::string name;
    ParameterKind kind = ParameterKind::Continuous;
    float min_value = 0.0f;
    float max_value = 1.0f;
    float default_value = 0.0f;
    float seek_speed = 0.0f;
    float seek_speed_down = 0.0f;
    float velocity = 0.0f;
    bool hold_during_playback = false;
    std::vector<std::string> labels;

    ParameterDefinition with_velocity(float v) const noexcept {
        ParameterDefinition copy = *this;
        copy.velocity = v;
        return copy;
    }

    ParameterDefinition with_hold(bool hold = true) const noexcept {
        ParameterDefinition copy = *this;
        copy.hold_during_playback = hold;
        return copy;
    }

    static ParameterDefinition continuous(std::string name, float min_v, float max_v, float def_v, float seek = 0.0f, float seek_down = 0.0f) {
        ParameterDefinition p;
        p.name = std::move(name);
        p.kind = ParameterKind::Continuous;
        p.min_value = min_v;
        p.max_value = max_v;
        p.default_value = def_v;
        p.seek_speed = seek;
        p.seek_speed_down = seek_down;
        return p;
    }

    static ParameterDefinition discrete(std::string name, float min_v, float max_v, float def_v, float seek = 0.0f, float seek_down = 0.0f) {
        ParameterDefinition p;
        p.name = std::move(name);
        p.kind = ParameterKind::Discrete;
        p.min_value = min_v;
        p.max_value = max_v;
        p.default_value = def_v;
        p.seek_speed = seek;
        p.seek_speed_down = seek_down;
        return p;
    }

    static ParameterDefinition labeled(std::string name, std::vector<std::string> labels, float def_idx = 0.0f, float seek = 0.0f, float seek_down = 0.0f) {
        ParameterDefinition p;
        p.name = std::move(name);
        p.kind = ParameterKind::Labeled;
        p.min_value = 0.0f;
        p.max_value = labels.empty() ? 0.0f : static_cast<float>(labels.size() - 1);
        p.default_value = def_idx;
        p.seek_speed = seek;
        p.seek_speed_down = seek_down;
        p.labels = std::move(labels);
        return p;
    }
};

struct ConditionTerm {
    ParamRef param;
    float min_value = 0.0f;
    float max_value = 1.0f;
};

struct Condition {
    float probability_pct = 100.0f;
    std::vector<ConditionTerm> terms;

    void add_term(std::string name, float min_val, float max_val, bool is_global = false) {
        ConditionTerm term;
        term.param.name = std::move(name);
        term.param.is_global = is_global;
        term.min_value = min_val;
        term.max_value = max_val;
        terms.push_back(std::move(term));
    }
};

enum class LogicItemKind : std::uint8_t {
    Marker = 0,
    Tempo = 1,
    LoopRegion = 2,
    SustainPoint = 3,
    TransitionMarker = 4,
    TransitionRegion = 5
};

enum class LogicQuantization : std::uint8_t {
    Immediate = 0,
    Beat = 1,
    Bar = 2
};

struct LogicItem {
    Guid id{0, 0};
    LogicItemKind kind = LogicItemKind::Marker;
    double position_ms = 0.0;
    double end_ms = 0.0;
    std::string name;
    double bpm = 120.0;
    std::int32_t ts_upper = 4;
    std::int32_t ts_lower = 4;
    std::int32_t repeat_count = -1;
    std::int32_t priority = 0;
    bool until_key_off = false;
    std::string destination;
    LogicQuantization quantization = LogicQuantization::Immediate;
    double crossfade_ms = 0.0;
    std::int32_t condition_index = -1;
    Condition condition;

    static LogicItem marker(std::string name, double pos_ms) {
        LogicItem item;
        item.kind = LogicItemKind::Marker;
        item.name = std::move(name);
        item.position_ms = pos_ms;
        return item;
    }

    static LogicItem tempo(double bpm, std::int32_t upper, std::int32_t lower, double pos_ms = 0.0) {
        LogicItem item;
        item.kind = LogicItemKind::Tempo;
        item.bpm = bpm;
        item.ts_upper = upper;
        item.ts_lower = lower;
        item.position_ms = pos_ms;
        return item;
    }

    static LogicItem loop_region(double start_ms, double end_ms, std::int32_t repeats = -1, bool key_off = false) {
        LogicItem item;
        item.kind = LogicItemKind::LoopRegion;
        item.position_ms = start_ms;
        item.end_ms = end_ms;
        item.repeat_count = repeats;
        item.until_key_off = key_off;
        return item;
    }

    static LogicItem sustain_point(double pos_ms) {
        LogicItem item;
        item.kind = LogicItemKind::SustainPoint;
        item.position_ms = pos_ms;
        return item;
    }

    static LogicItem transition_marker(double pos_ms, std::string dest, LogicQuantization q = LogicQuantization::Immediate, double xfade = 0.0) {
        LogicItem item;
        item.kind = LogicItemKind::TransitionMarker;
        item.position_ms = pos_ms;
        item.destination = std::move(dest);
        item.quantization = q;
        item.crossfade_ms = xfade;
        return item;
    }

    static LogicItem transition_region(double start_ms, double end_ms, std::string dest, LogicQuantization q, double xfade = 0.0) {
        LogicItem item;
        item.kind = LogicItemKind::TransitionRegion;
        item.position_ms = start_ms;
        item.end_ms = end_ms;
        item.destination = std::move(dest);
        item.quantization = q;
        item.crossfade_ms = xfade;
        return item;
    }

    LogicItem& with_condition(Condition c) {
        condition = std::move(c);
        return *this;
    }

    LogicItem& with_probability(float pct) {
        condition.probability_pct = pct;
        return *this;
    }

    LogicItem& add_condition_term(std::string param_name, float min_val, float max_val, bool is_global = false) {
        condition.add_term(std::move(param_name), min_val, max_val, is_global);
        return *this;
    }
};

struct PlaylistEntry {
    std::int32_t sample_index = -1;
    Guid event_guid{0, 0};
    float weight = 1.0f;
    std::string sound_path;
    oae_sound sound_handle = 0;
};

struct Instrument {
    Guid id{0, 0};
    InstrumentKind kind = InstrumentKind::Single;
    double offset_ms = 0.0;
    double length_ms = 0.0;
    double start_offset_ms = 0.0;
    bool loop = false;
    double fade_in_ms = 0.0;
    double fade_out_ms = 0.0;
    float volume_db_base = 0.0f;
    float volume_db_random_range = 0.0f;
    float pitch_st_base = 0.0f;
    float pitch_st_random_range = 0.0f;
    std::uint32_t delay_ms_random_range = 0;
    std::int32_t sample_index = -1;
    std::string sound_path;
    oae_sound sound_handle = 0;
    Guid event_ref{0, 0};
    CommandType command_type = CommandType::StartEvent;
    Guid command_target{0, 0};
    PlaylistMode playlist_mode = PlaylistMode::Random;
    PlaylistScope playlist_scope = PlaylistScope::Global;
    std::vector<PlaylistEntry> playlist;
    Condition condition;
    std::vector<Modulator> modulators;
    bool async = false;
    AsyncOverlapPolicy async_policy = AsyncOverlapPolicy::Overlap;
    std::string sheet_parameter;
    double param_min = 0.0;
    double param_max = 0.0;
    std::string programmer_key;
    double spawn_interval_min_ms = 200.0;
    double spawn_interval_max_ms = 1000.0;
    std::uint32_t polyphony = 4;
    std::uint32_t total_sounds = 0;
    float min_scatter_distance = 1.0f;
    float max_scatter_distance = 10.0f;
};

struct TrackEffectSpec {
    int type = 0;
    int position = OAE_EFFECT_POST_FADER;
    std::vector<std::pair<int, float>> params;
    bool bypass = false;
    float wet = 1.0f;
    float dry = 0.0f;
};

struct Track {
    Guid id{0, 0};
    float volume_db = 0.0f;
    float pan = 0.0f;
    std::int32_t output_track = -1;
    std::vector<TrackEffectSpec> effects;
    std::vector<Instrument> instruments;
    std::vector<Automation> automations;
};

enum class EventKind : std::uint8_t {
    Event = 0,
    Snapshot = 1
};

enum class SnapshotMode : std::uint8_t {
    Override = 0,
    Blending = 1
};

struct SnapshotTarget {
    int effect_type = -1;
    std::string bus_path;
    int param_id = -1; // -1 = bus volume (dB delta); >= 0 = effect parameter ID
    int position = 0;  // 0 = POST_FADER, 1 = PRE_FADER
    std::size_t effect_index = 0;
    float value = 0.0f; // dB delta if param_id == -1, or raw parameter value
};

struct VcaModel {
    Guid id{0, 0};
    std::string path;
    std::vector<std::string> bus_paths;
    std::vector<std::string> event_paths;
    float default_volume = 1.0f;
};

struct EventDescriptionModel {
    std::vector<std::pair<std::string, std::string>> user_props;
    Guid id{0, 0};
    std::string path;
    std::string bus_path;
    float volume_db = 0.0f;
    float pitch_semitones = 0.0f;
    float pan = 0.0f;
    bool pitch_affects_timeline = false;
    std::uint32_t max_instances = 0;
    StealMode steal = StealMode::Oldest;
    std::uint32_t cooldown_ms = 0;
    std::int32_t priority = 128;
    std::uint32_t stop_fadeout_ms = 0;
    EventKind kind = EventKind::Event;
    SnapshotMode snapshot_mode = SnapshotMode::Override;
    std::uint32_t fade_in_ms = 0;
    std::string intensity_param;
    std::vector<SnapshotTarget> snapshot_targets;
    std::vector<Track> tracks;
    std::vector<LogicItem> logic_items;
    std::vector<ParameterDefinition> parameters;
    std::vector<Modulator> modulators;
    std::vector<Automation> automations;

    std::uint32_t total_length_ms() const noexcept;
    bool is_oneshot() const noexcept;
};

class ParameterInstance {
public:
    ParameterInstance() = default;
    explicit ParameterInstance(ParameterDefinition def)
        : def_(std::move(def)), current_value_(def_.default_value), target_value_(def_.default_value)
    {
        clamp_and_snap();
    }

    const ParameterDefinition& definition() const noexcept { return def_; }

    void set_value(float value, bool ignore_seek_speed = false) noexcept
    {
        float v = value;
        if (def_.kind == ParameterKind::Discrete || def_.kind == ParameterKind::Labeled) {
            v = std::round(v);
        }
        v = std::clamp(v, def_.min_value, def_.max_value);
        target_value_ = v;
        if (ignore_seek_speed || (def_.seek_speed <= 0.0f && def_.seek_speed_down <= 0.0f)) {
            current_value_ = v;
        }
    }

    oae_result set_value_with_label(const std::string& label, bool ignore_seek_speed = false) noexcept
    {
        for (std::size_t i = 0; i < def_.labels.size(); ++i) {
            if (def_.labels[i] == label) {
                set_value(static_cast<float>(i), ignore_seek_speed);
                return OAE_OK;
            }
        }
        return OAE_ERR_INVALID_PARAM;
    }

    void advance_time(double delta_seconds) noexcept
    {
        if (delta_seconds <= 0.0) {
            return;
        }
        if (def_.velocity != 0.0f) {
            const float vdelta = static_cast<float>(static_cast<double>(def_.velocity) * delta_seconds);
            target_value_ = std::clamp(target_value_ + vdelta, def_.min_value, def_.max_value);
            if (def_.seek_speed <= 0.0f && def_.seek_speed_down <= 0.0f) {
                if (def_.kind == ParameterKind::Discrete || def_.kind == ParameterKind::Labeled) {
                    current_value_ = std::clamp(std::round(target_value_), def_.min_value, def_.max_value);
                } else {
                    current_value_ = target_value_;
                }
                return;
            }
        }
        const float effective_target =
            (def_.kind == ParameterKind::Discrete || def_.kind == ParameterKind::Labeled)
                ? std::clamp(std::round(target_value_), def_.min_value, def_.max_value)
                : target_value_;
        if (current_value_ == effective_target) {
            return;
        }
        if (effective_target > current_value_) {
            const float speed = def_.seek_speed;
            if (speed <= 0.0f) {
                current_value_ = effective_target;
            } else {
                const float step = static_cast<float>(static_cast<double>(speed) * delta_seconds);
                current_value_ = std::min(effective_target, current_value_ + step);
            }
        } else {
            const float speed = (def_.seek_speed_down > 0.0f) ? def_.seek_speed_down : def_.seek_speed;
            if (speed <= 0.0f) {
                current_value_ = effective_target;
            } else {
                const float step = static_cast<float>(static_cast<double>(speed) * delta_seconds);
                current_value_ = std::max(effective_target, current_value_ - step);
            }
        }
    }

    float current_value() const noexcept { return current_value_; }
    float target_value() const noexcept { return target_value_; }

private:
    void clamp_and_snap() noexcept
    {
        float v = current_value_;
        if (def_.kind == ParameterKind::Discrete || def_.kind == ParameterKind::Labeled) {
            v = std::round(v);
        }
        v = std::clamp(v, def_.min_value, def_.max_value);
        current_value_ = v;
        target_value_ = v;
    }

    ParameterDefinition def_;
    float current_value_ = 0.0f;
    float target_value_ = 0.0f;
};

struct AudioTableEntry {
    std::string key;
    std::int32_t sample_index = -1;
    oae_sound sound_handle = 0;
};

class ModelSet {
public:
    const EventDescriptionModel* find_event_by_path(const std::string& path) const noexcept;
    const EventDescriptionModel* find_event_by_guid(const Guid& guid) const noexcept;
    const std::vector<EventDescriptionModel>& events() const noexcept { return events_; }
    const std::vector<ParameterDefinition>& global_parameters() const noexcept { return global_parameters_; }
    const ParameterDefinition* find_global_parameter(const std::string& name) const noexcept;
    const std::vector<EffectAutomation>& effect_automations() const noexcept { return effect_automations_; }
    const std::vector<VcaModel>& vcas() const noexcept { return vcas_; }
    const VcaModel* find_vca(const std::string& path) const noexcept;
    const std::vector<AudioTableEntry>& audio_table() const noexcept { return audio_table_; }
    const AudioTableEntry* find_audio_table_entry(const std::string& key) const noexcept {
        for (const auto& entry : audio_table_) {
            if (entry.key == key) return &entry;
        }
        return nullptr;
    }

    friend class ModelSetBuilder;
private:
    std::vector<EventDescriptionModel> events_;
    std::vector<ParameterDefinition> global_parameters_;
    std::vector<EffectAutomation> effect_automations_;
    std::vector<VcaModel> vcas_;
    std::vector<AudioTableEntry> audio_table_;
    std::unordered_map<std::string, std::size_t> path_to_index_;
    std::unordered_map<std::uint64_t, std::size_t> guid_hi_to_index_;
    std::unordered_map<std::string, std::size_t> global_param_to_index_;
    std::unordered_map<std::string, std::size_t> vca_to_index_;
};

class InstrumentBuilder {
public:
    InstrumentBuilder& kind(InstrumentKind k) noexcept { inst_.kind = k; return *this; }
    InstrumentBuilder& offset_ms(double ms) noexcept { inst_.offset_ms = ms; return *this; }
    InstrumentBuilder& length_ms(double ms) noexcept { inst_.length_ms = ms; return *this; }
    InstrumentBuilder& sound(oae_sound s) noexcept { inst_.sound_handle = s; inst_.kind = InstrumentKind::Single; return *this; }
    InstrumentBuilder& sound_path(const std::string& p) { inst_.sound_path = p; inst_.kind = InstrumentKind::Single; return *this; }
    InstrumentBuilder& sample_index(int32_t idx) noexcept { inst_.sample_index = idx; return *this; }
    InstrumentBuilder& loop(bool l) noexcept { inst_.loop = l; return *this; }
    InstrumentBuilder& event_ref(const Guid& ref) noexcept { inst_.event_ref = ref; inst_.kind = InstrumentKind::Event; return *this; }
    InstrumentBuilder& command(CommandType type, const Guid& target) noexcept {
        inst_.command_type = type;
        inst_.command_target = target;
        inst_.kind = InstrumentKind::Command;
        return *this;
    }
    InstrumentBuilder& multi(PlaylistMode mode, PlaylistScope scope = PlaylistScope::Global) noexcept {
        inst_.kind = InstrumentKind::Multi;
        inst_.playlist_mode = mode;
        inst_.playlist_scope = scope;
        return *this;
    }
    InstrumentBuilder& scatterer(double min_interval_ms, double max_interval_ms,
                                 std::uint32_t polyphony = 4, std::uint32_t total_sounds = 0,
                                 float min_dist = 1.0f, float max_dist = 10.0f,
                                 PlaylistMode mode = PlaylistMode::Random,
                                 PlaylistScope scope = PlaylistScope::Instance) noexcept {
        inst_.kind = InstrumentKind::Scatterer;
        inst_.spawn_interval_min_ms = min_interval_ms;
        inst_.spawn_interval_max_ms = max_interval_ms;
        inst_.polyphony = polyphony;
        inst_.total_sounds = total_sounds;
        inst_.min_scatter_distance = min_dist;
        inst_.max_scatter_distance = max_dist;
        inst_.playlist_mode = mode;
        inst_.playlist_scope = scope;
        return *this;
    }
    InstrumentBuilder& add_entry(oae_sound sound, float weight = 1.0f) {
        PlaylistEntry entry;
        entry.sound_handle = sound;
        entry.weight = weight;
        inst_.playlist.push_back(entry);
        return *this;
    }
    InstrumentBuilder& add_entry(const std::string& sound_path, float weight = 1.0f) {
        PlaylistEntry entry;
        entry.sound_path = sound_path;
        entry.weight = weight;
        inst_.playlist.push_back(entry);
        return *this;
    }
    InstrumentBuilder& add_entry(const Guid& nested_event, float weight = 1.0f) {
        PlaylistEntry entry;
        entry.event_guid = nested_event;
        entry.weight = weight;
        inst_.playlist.push_back(entry);
        return *this;
    }
    InstrumentBuilder& condition(Condition c) {
        inst_.condition = std::move(c);
        return *this;
    }
    InstrumentBuilder& probability(float pct) {
        inst_.condition.probability_pct = pct;
        return *this;
    }
    InstrumentBuilder& add_condition_term(std::string name, float min_val, float max_val, bool is_global = false) {
        inst_.condition.add_term(std::move(name), min_val, max_val, is_global);
        return *this;
    }
    InstrumentBuilder& add_modulator(Modulator m) {
        inst_.modulators.push_back(std::move(m));
        return *this;
    }
    InstrumentBuilder& async(bool a, AsyncOverlapPolicy policy = AsyncOverlapPolicy::Overlap) noexcept {
        inst_.async = a;
        inst_.async_policy = policy;
        return *this;
    }
    InstrumentBuilder& parameter_region(std::string param_name, double min_val, double max_val) {
        inst_.sheet_parameter = std::move(param_name);
        inst_.param_min = min_val;
        inst_.param_max = max_val;
        return *this;
    }
    InstrumentBuilder& programmer(std::string key = {}) {
        inst_.kind = InstrumentKind::Programmer;
        inst_.programmer_key = std::move(key);
        return *this;
    }

    const Instrument& build() const noexcept { return inst_; }

private:
    Instrument inst_;
};

class TrackBuilder {
public:
    TrackBuilder& volume_db(float db) noexcept { track_.volume_db = db; return *this; }
    TrackBuilder& pan(float p) noexcept { track_.pan = p; return *this; }
    TrackBuilder& output_track(std::int32_t track_idx) noexcept { track_.output_track = track_idx; return *this; }
    TrackBuilder& add_effect(TrackEffectSpec eff) {
        track_.effects.push_back(std::move(eff));
        return *this;
    }
    TrackBuilder& add_instrument(const Instrument& inst) {
        track_.instruments.push_back(inst);
        return *this;
    }
    TrackBuilder& add_instrument(const InstrumentBuilder& b) {
        track_.instruments.push_back(b.build());
        return *this;
    }
    TrackBuilder& add_automation(Automation a) {
        track_.automations.push_back(std::move(a));
        return *this;
    }

    const Track& build() const noexcept { return track_; }

private:
    Track track_;
};

class EventBuilder {
public:
    explicit EventBuilder(std::string path) {
        event_.path = std::move(path);
    }

    EventBuilder& bus_path(std::string path) { event_.bus_path = std::move(path); return *this; }
    EventBuilder& volume_db(float db) noexcept { event_.volume_db = db; return *this; }
    EventBuilder& pitch_semitones(float st) noexcept { event_.pitch_semitones = st; return *this; }
    EventBuilder& pitch_affects_timeline(bool enabled) noexcept { event_.pitch_affects_timeline = enabled; return *this; }
    EventBuilder& pan(float p) noexcept { event_.pan = p; return *this; }
    EventBuilder& max_instances(std::uint32_t m) noexcept { event_.max_instances = m; return *this; }
    EventBuilder& steal(StealMode s) noexcept { event_.steal = s; return *this; }
    EventBuilder& cooldown_ms(std::uint32_t ms) noexcept { event_.cooldown_ms = ms; return *this; }
    EventBuilder& priority(std::int32_t prio) noexcept { event_.priority = prio; return *this; }
    EventBuilder& stop_fadeout_ms(std::uint32_t ms) noexcept { event_.stop_fadeout_ms = ms; return *this; }
    EventBuilder& kind(EventKind k) noexcept { event_.kind = k; return *this; }
    EventBuilder& snapshot_mode(SnapshotMode m) noexcept { event_.snapshot_mode = m; return *this; }
    EventBuilder& fade_in_ms(std::uint32_t ms) noexcept { event_.fade_in_ms = ms; return *this; }
    EventBuilder& intensity_param(std::string p) { event_.intensity_param = std::move(p); return *this; }
    EventBuilder& add_snapshot_target(SnapshotTarget t) { event_.snapshot_targets.push_back(std::move(t)); return *this; }
    EventBuilder& add_logic_item(const LogicItem& item) { event_.logic_items.push_back(item); return *this; }
    EventBuilder& add_parameter(ParameterDefinition p) { event_.parameters.push_back(std::move(p)); return *this; }
    EventBuilder& add_modulator(Modulator m) { event_.modulators.push_back(std::move(m)); return *this; }
    EventBuilder& add_automation(Automation a) { event_.automations.push_back(std::move(a)); return *this; }
    EventBuilder& add_track(const Track& t) { event_.tracks.push_back(t); return *this; }
    EventBuilder& add_track(const TrackBuilder& b) { event_.tracks.push_back(b.build()); return *this; }

    const EventDescriptionModel& build() const noexcept { return event_; }

private:
    EventDescriptionModel event_;
};

class ModelSetBuilder {
public:
    ModelSetBuilder& add_event(EventDescriptionModel event);
    ModelSetBuilder& add_event(const EventBuilder& b) { return add_event(b.build()); }
    ModelSetBuilder& add_global_parameter(ParameterDefinition param) {
        global_parameters_.push_back(std::move(param));
        return *this;
    }
    ModelSetBuilder& add_effect_automation(EffectAutomation ea) {
        effect_automations_.push_back(std::move(ea));
        return *this;
    }
    ModelSetBuilder& add_vca(VcaModel vca) {
        vcas_.push_back(std::move(vca));
        return *this;
    }
    ModelSetBuilder& add_audio_table_entry(AudioTableEntry entry) {
        audio_table_.push_back(std::move(entry));
        return *this;
    }
    std::shared_ptr<const ModelSet> build();

private:
    std::vector<EventDescriptionModel> events_;
    std::vector<ParameterDefinition> global_parameters_;
    std::vector<EffectAutomation> effect_automations_;
    std::vector<VcaModel> vcas_;
    std::vector<AudioTableEntry> audio_table_;
};

// Non-deterministic seeding for playlist RNGs: each PlaylistState (and therefore
// each independent play of an event) draws from a distinct PCG32 stream, while
// a given instance remains deterministic across its own picks. Tests can still
// pin a specific stream via PlaylistState::set_rng_seed or an explicit
// constructor argument.
inline Pcg32 make_playlist_rng() noexcept
{
    static std::atomic<std::uint64_t> playlist_rng_counter{1ull};
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto seq = playlist_rng_counter.fetch_add(1ull, std::memory_order_relaxed);
    const auto state = ticks ^ (seq * 0x9e3779b97f4a7c15ull);
    return Pcg32(state, seq);
}

class PlaylistState {
public:
    explicit PlaylistState(Pcg32 rng = make_playlist_rng()) noexcept
        : rng_(rng) {}

    std::size_t pick(const std::vector<PlaylistEntry>& entries, PlaylistMode mode);

    void set_rng_seed(std::uint64_t state, std::uint64_t seq = 1ull) noexcept {
        rng_.seed(state, seq);
    }

private:
    Pcg32 rng_;
    std::size_t last_picked_ = static_cast<std::size_t>(-1);
    std::size_t seq_index_ = 0;
    std::vector<std::size_t> shuffle_deck_;
    std::size_t shuffle_pos_ = 0;
};

}
