#include "event_instance.h"
#include "system.h"
#include "bank.h"
#include "../api/runtime.h"
#include "../common/strip_math.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace oae {

namespace {

std::size_t count_multi_instruments(const EventDescriptionModel* model, PlaylistScope scope)
{
    std::size_t count = 0;
    if (!model) return 0;
    for (const auto& track : model->tracks) {
        for (const auto& inst : track.instruments) {
            if ((inst.kind == InstrumentKind::Multi || inst.kind == InstrumentKind::Scatterer) &&
                inst.playlist_scope == scope) {
                ++count;
            }
        }
    }
    return count;
}

constexpr float kRadToDeg = 57.2957795130823208768f;

float vec3_dot(const oae_vec3& a, const oae_vec3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

oae_vec3 vec3_sub(const oae_vec3& a, const oae_vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

oae_vec3 vec3_cross(const oae_vec3& a, const oae_vec3& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float vec3_length(const oae_vec3& v) noexcept
{
    return std::sqrt(vec3_dot(v, v));
}

oae_vec3 vec3_normalize_or(const oae_vec3& v, const oae_vec3& fallback) noexcept
{
    const float len = vec3_length(v);
    if (len <= 1.0e-6f) {
        return fallback;
    }
    const float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

struct Spatial3DMetrics {
    float distance = 0.0f;
    float direction_deg = 0.0f;
    float elevation_deg = 0.0f;
    float cone_angle_deg = 0.0f;
    float speed = 0.0f;
    float spatial_gain = 1.0f;
    float spatial_pan = 0.0f;
};

Spatial3DMetrics compute_spatial_metrics(
    const oae_3d_attributes& listener,
    const oae_3d_attributes& emitter,
    const oae_spatializer_settings& settings) noexcept
{
    Spatial3DMetrics out{};

    const oae_vec3 f = vec3_normalize_or(listener.forward, {0.0f, 0.0f, 1.0f});
    const oae_vec3 u_raw = vec3_normalize_or(listener.up, {0.0f, 1.0f, 0.0f});
    oae_vec3 r = vec3_cross(u_raw, f);
    if (vec3_length(r) <= 1.0e-6f) {
        const oae_vec3 alt_up = (std::fabs(f.y) < 0.9f) ? oae_vec3{0.0f, 1.0f, 0.0f} : oae_vec3{0.0f, 0.0f, 1.0f};
        r = vec3_cross(alt_up, f);
    }
    r = vec3_normalize_or(r, {1.0f, 0.0f, 0.0f});
    const oae_vec3 u = vec3_normalize_or(vec3_cross(f, r), {0.0f, 1.0f, 0.0f});

    const oae_vec3 diff = vec3_sub(emitter.position, listener.position);
    const float dist = vec3_length(diff);
    const oae_vec3 rel_vel = vec3_sub(emitter.velocity, listener.velocity);

    out.distance = dist;
    out.speed = vec3_length(rel_vel);

    float local_x = 0.0f;
    if (dist > 1.0e-6f) {
        const float inv_dist = 1.0f / dist;
        const oae_vec3 dir{diff.x * inv_dist, diff.y * inv_dist, diff.z * inv_dist};
        local_x = vec3_dot(dir, r);
        const float local_y = vec3_dot(dir, u);
        const float local_z = vec3_dot(dir, f);

        out.direction_deg = std::atan2(local_x, local_z) * kRadToDeg;
        out.elevation_deg = std::asin(std::clamp(local_y, -1.0f, 1.0f)) * kRadToDeg;

        const oae_vec3 ef = vec3_normalize_or(emitter.forward, {0.0f, 0.0f, 1.0f});
        const oae_vec3 to_listener{-dir.x, -dir.y, -dir.z};
        out.cone_angle_deg = std::acos(std::clamp(vec3_dot(ef, to_listener), -1.0f, 1.0f)) * kRadToDeg;
    }

    if (settings.enabled == 0) {
        out.spatial_gain = 1.0f;
        out.spatial_pan = 0.0f;
        return out;
    }

    const float min_d = std::max(0.0f, settings.min_distance);
    const float max_d = std::max(min_d + 1.0e-4f, settings.max_distance);
    float dist_gain = 1.0f;
    if (dist >= max_d) {
        dist_gain = 0.0f;
    } else if (dist > min_d) {
        const float linear = (max_d - dist) / (max_d - min_d);
        if (settings.rolloff == OAE_ROLLOFF_INVERSE) {
            dist_gain = linear * (std::max(0.001f, min_d) / dist);
        } else if (settings.rolloff == OAE_ROLLOFF_LINEAR_SQUARED) {
            dist_gain = linear * linear;
        } else {
            dist_gain = linear;
        }
    }

    const float inner_half = 0.5f * std::clamp(settings.cone_inner_angle_deg, 0.0f, 360.0f);
    const float outer_half = std::max(inner_half, 0.5f * std::clamp(settings.cone_outer_angle_deg, 0.0f, 360.0f));
    const float outer_g = std::clamp(settings.cone_outer_gain, 0.0f, 1.0f);
    float cone_gain = 1.0f;
    if (out.cone_angle_deg >= outer_half) {
        cone_gain = outer_g;
    } else if (out.cone_angle_deg > inner_half && outer_half > inner_half) {
        const float t = (out.cone_angle_deg - inner_half) / (outer_half - inner_half);
        cone_gain = 1.0f + t * (outer_g - 1.0f);
    }

    out.spatial_gain = std::clamp(dist_gain * cone_gain, 0.0f, 1.0f);

    const float raw_pan = (dist > 1.0e-6f) ? std::clamp(local_x, -1.0f, 1.0f) : 0.0f;
    float size_spread_deg = 0.0f;
    if (settings.sound_size > 0.0f) {
        size_spread_deg = (dist > 1.0e-6f)
            ? std::min(360.0f, 2.0f * std::atan2(0.5f * settings.sound_size, dist) * kRadToDeg)
            : 360.0f;
    }
    const float extent_deg = std::clamp(std::max(settings.min_extent_deg, size_spread_deg), 0.0f, 360.0f);
    const float pan_focus = 1.0f - std::clamp(extent_deg / 180.0f, 0.0f, 1.0f);
    out.spatial_pan = std::clamp(raw_pan * pan_focus, -1.0f, 1.0f);
    return out;
}

bool assign_builtin_metric(const Spatial3DMetrics& m, const std::string& name, float& out_value) noexcept
{
    if (name == "Distance") {
        out_value = m.distance;
        return true;
    }
    if (name == "Direction") {
        out_value = m.direction_deg;
        return true;
    }
    if (name == "Elevation") {
        out_value = m.elevation_deg;
        return true;
    }
    if (name == "EventConeAngle") {
        out_value = m.cone_angle_deg;
        return true;
    }
    if (name == "Speed") {
        out_value = m.speed;
        return true;
    }
    return false;
}

}

EventDescription::EventDescription(oae_system system, std::shared_ptr<const ModelSet> model_set, const EventDescriptionModel* model)
    : system_(system), model_set_(std::move(model_set)), model_(model)
{
    const auto global_count = count_multi_instruments(model_, PlaylistScope::Global);
    global_playlists_.resize(global_count);
}

EventDescription::~EventDescription()
{
    for (auto* inst : active_instances_) {
        if (inst) {
            inst->stop(OAE_STOP_IMMEDIATE);
        }
    }
}

oae_result EventDescription::create_instance(EventInstance** out_instance)
{
    if (!out_instance) {
        return OAE_ERR_INVALID_PARAM;
    }
    *out_instance = new EventInstance(this);
    return OAE_OK;
}

std::int32_t EventDescription::active_instance_count() const noexcept
{
    return static_cast<std::int32_t>(active_instances_.size());
}

bool EventDescription::check_and_apply_cooldown(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept
{
    if (model_->cooldown_ms > 0) {
        const auto cooldown_frames = milliseconds_to_frames(model_->cooldown_ms, sample_rate);
        if (has_triggered_ && (dsp_clock >= last_trigger_clock_) && (dsp_clock - last_trigger_clock_ < cooldown_frames)) {
            return false;
        }
    }
    has_triggered_ = true;
    last_trigger_clock_ = dsp_clock;
    return true;
}

void EventDescription::apply_stealing(EventInstance* new_instance) noexcept
{
    if (model_->max_instances == 0 || active_instances_.size() < model_->max_instances) {
        return;
    }

    if (model_->steal == StealMode::None) {
        new_instance->stop(OAE_STOP_IMMEDIATE);
        return;
    }

    if (model_->steal == StealMode::Oldest) {
        EventInstance* oldest = nullptr;
        for (auto* inst : active_instances_) {
            if (inst && inst->is_active()) {
                if (!oldest || inst->start_order() < oldest->start_order()) {
                    oldest = inst;
                }
            }
        }
        if (oldest) {
            oldest->emit_callback(OAE_EVENT_CALLBACK_STOLEN);
            oldest->stop(model_->stop_fadeout_ms > 0 ? OAE_STOP_ALLOW_FADEOUT : OAE_STOP_IMMEDIATE);
            unregister_active(oldest);
        }
        return;
    }

    if (model_->steal == StealMode::Quietest) {
        EventInstance* quietest = nullptr;
        for (auto* inst : active_instances_) {
            if (inst && inst->is_active()) {
                if (!quietest || inst->effective_volume() < quietest->effective_volume()) {
                    quietest = inst;
                }
            }
        }
        if (quietest) {
            quietest->emit_callback(OAE_EVENT_CALLBACK_STOLEN);
            quietest->stop(model_->stop_fadeout_ms > 0 ? OAE_STOP_ALLOW_FADEOUT : OAE_STOP_IMMEDIATE);
            unregister_active(quietest);
        }
    }
}

void EventDescription::register_active(EventInstance* inst) noexcept
{
    if (inst && std::find(active_instances_.begin(), active_instances_.end(), inst) == active_instances_.end()) {
        active_instances_.push_back(inst);
    }
}

void EventDescription::unregister_active(EventInstance* inst) noexcept
{
    const auto it = std::find(active_instances_.begin(), active_instances_.end(), inst);
    if (it != active_instances_.end()) {
        active_instances_.erase(it);
    }
}

PlaylistState& EventDescription::get_global_playlist(std::size_t flat_idx)
{
    if (flat_idx >= global_playlists_.size()) {
        global_playlists_.resize(flat_idx + 1);
    }
    return global_playlists_[flat_idx];
}

bool EventDescription::are_samples_loaded() const noexcept
{
    if (!model_ || model_->kind == EventKind::Snapshot) {
        return true;
    }
    auto* sys = runtime.systems.get(system_);
    auto* bank = sys ? sys->find_bank_for_model_set(model_set_.get()) : nullptr;

    for (const auto& trk : model_->tracks) {
        for (const auto& inst : trk.instruments) {
            if (inst.sample_index >= 0) {
                if (bank) {
                    if (bank->is_nonblocking() && !bank->is_sample_pinned(inst.sample_index)) {
                        return false;
                    }
                } else if (inst.sound_handle == 0) {
                    return false;
                }
            }
            for (const auto& pe : inst.playlist) {
                if (pe.sample_index >= 0) {
                    if (bank) {
                        if (bank->is_nonblocking() && !bank->is_sample_pinned(pe.sample_index)) {
                            return false;
                        }
                    } else if (pe.sound_handle == 0) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

oae_result EventDescription::load_sample_data()
{
    if (!model_) return OAE_ERR_INVALID_STATE;
    auto* sys = runtime.systems.get(system_);
    if (!sys) return OAE_ERR_INVALID_HANDLE;
    auto* bank = sys->find_bank_for_model_set(model_set_.get());
    if (!bank) {
        return OAE_OK;
    }

    std::unordered_set<std::int32_t> sample_indices;
    for (const auto& trk : model_->tracks) {
        for (const auto& inst : trk.instruments) {
            if (inst.sample_index >= 0) {
                sample_indices.insert(inst.sample_index);
            }
            for (const auto& pe : inst.playlist) {
                if (pe.sample_index >= 0) {
                    sample_indices.insert(pe.sample_index);
                }
            }
        }
    }

    for (std::int32_t idx : sample_indices) {
        const auto res = bank->pin_sample(idx);
        if (res != OAE_OK) return res;
    }
    return OAE_OK;
}

oae_result EventDescription::unload_sample_data()
{
    if (!model_) return OAE_ERR_INVALID_STATE;
    auto* sys = runtime.systems.get(system_);
    if (!sys) return OAE_ERR_INVALID_HANDLE;
    auto* bank = sys->find_bank_for_model_set(model_set_.get());
    if (!bank) {
        return OAE_OK;
    }

    std::unordered_set<std::int32_t> sample_indices;
    for (const auto& trk : model_->tracks) {
        for (const auto& inst : trk.instruments) {
            if (inst.sample_index >= 0) {
                sample_indices.insert(inst.sample_index);
            }
            for (const auto& pe : inst.playlist) {
                if (pe.sample_index >= 0) {
                    sample_indices.insert(pe.sample_index);
                }
            }
        }
    }

    for (std::int32_t idx : sample_indices) {
        bank->unpin_sample(idx);
    }
    return OAE_OK;
}

oae_sound EventDescription::resolve_sound(oae_sound direct_sound, std::int32_t sample_index) const noexcept
{
    if (direct_sound != 0) {
        return direct_sound;
    }
    if (sample_index < 0) {
        return 0;
    }
    auto* sys = runtime.systems.get(system_);
    if (!sys) return 0;
    auto* bank = sys->find_bank_for_model_set(model_set_.get());
    if (!bank) return 0;
    return bank->get_sample_sound(sample_index);
}

EventInstance::EventInstance(EventDescription* description, std::uint32_t nested_depth)
    : description_(description), nested_depth_(nested_depth)
{
    if (description_ && description_->model()) {
        const auto* model = description_->model();
        const auto inst_count = count_multi_instruments(model, PlaylistScope::Instance);
        instance_playlists_.resize(inst_count);
        for (const auto& p : model->parameters) {
            local_parameters_.emplace(p.name, ParameterInstance(p));
        }
    }
}

EventInstance::~EventInstance()
{
    stop(OAE_STOP_IMMEDIATE);
    destroy_track_buses();
    if (description_) {
        auto* sys = runtime.systems.get(description_->system());
        if (sys) {
            sys->remove_active_instance(this);
        }
    }
}

void EventInstance::create_track_buses(oae_system sys, oae_bus target_bus)
{
    destroy_track_buses();
    const auto* model = description_ ? description_->model() : nullptr;
    if (!model || model->tracks.empty() || target_bus == 0) {
        return;
    }

    const std::string base_name = "__ev_" + std::to_string(start_order_) + "_" +
        std::to_string(reinterpret_cast<std::uintptr_t>(this));
    if (oae_bus_create(sys, base_name.c_str(), target_bus, &event_master_bus_) != OAE_OK) {
        event_master_bus_ = 0;
        return;
    }

    const std::size_t n = model->tracks.size();
    track_buses_.assign(n, 0);

    // Create buses in topological order (simple multi-pass for track->track routing)
    std::vector<bool> created(n, false);
    for (std::size_t pass = 0; pass < n; ++pass) {
        bool progress = false;
        for (std::size_t t = 0; t < n; ++t) {
            if (created[t]) continue;
            const auto& trk = model->tracks[t];
            oae_bus parent = event_master_bus_;
            if (trk.output_track >= 0 && static_cast<std::size_t>(trk.output_track) < n &&
                static_cast<std::size_t>(trk.output_track) != t) {
                if (!created[static_cast<std::size_t>(trk.output_track)]) {
                    continue;
                }
                parent = track_buses_[static_cast<std::size_t>(trk.output_track)];
            }
            const std::string trk_name = "trk_" + std::to_string(t);
            if (oae_bus_create(sys, trk_name.c_str(), parent, &track_buses_[t]) == OAE_OK) {
                oae_bus_set_volume(track_buses_[t], db_to_linear(trk.volume_db));
                oae_bus_set_pan(track_buses_[t], std::clamp(trk.pan, -1.0f, 1.0f));
                for (const auto& eff_spec : trk.effects) {
                    oae_effect eff = 0;
                    if (oae_bus_add_effect_ex(track_buses_[t], eff_spec.type, eff_spec.position,
                            OAE_EFFECT_INDEX_LAST, &eff) == OAE_OK) {
                        auto high_crossover = eff_spec.params.end();
                        if (eff_spec.type == OAE_EFFECT_EQ3) {
                            high_crossover = std::find_if(eff_spec.params.begin(), eff_spec.params.end(),
                                [](const auto& param) { return param.first == OAE_EQ3_HIGH_XOVER_HZ; });
                            if (high_crossover != eff_spec.params.end()) {
                                float initial_low = 0.0f;
                                oae_effect_get_param(eff, OAE_EQ3_LOW_XOVER_HZ, &initial_low);
                                if (high_crossover->second > initial_low) {
                                    oae_effect_set_param(eff, high_crossover->first, high_crossover->second);
                                } else {
                                    high_crossover = eff_spec.params.end();
                                }
                            }
                        }
                        for (const auto& param : eff_spec.params) {
                            if (high_crossover == eff_spec.params.end() || &param != &*high_crossover) {
                                oae_effect_set_param(eff, param.first, param.second);
                            }
                        }
                        oae_effect_set_bypass(eff, eff_spec.bypass);
                        oae_effect_set_wet_dry(eff, eff_spec.wet, eff_spec.dry);
                    }
                }
            }
            created[t] = true;
            progress = true;
        }
        if (!progress) break;
    }
    // Fallback for any cyclic track routing: attach directly to event_master_bus_
    for (std::size_t t = 0; t < n; ++t) {
        if (!created[t]) {
            const std::string trk_name = "trk_" + std::to_string(t);
            if (oae_bus_create(sys, trk_name.c_str(), event_master_bus_, &track_buses_[t]) == OAE_OK) {
                oae_bus_set_volume(track_buses_[t], db_to_linear(model->tracks[t].volume_db));
                oae_bus_set_pan(track_buses_[t], std::clamp(model->tracks[t].pan, -1.0f, 1.0f));
            }
        }
    }
}

void EventInstance::destroy_track_buses() noexcept
{
    if (event_master_bus_ != 0) {
        oae_bus_release(event_master_bus_);
        event_master_bus_ = 0;
    }
    track_buses_.clear();
}

bool EventInstance::has_active_track_effect_tail(std::uint64_t dsp_clock, std::uint32_t sample_rate) const noexcept
{
    const auto* model = description_ ? description_->model() : nullptr;
    if (!model) return false;
    bool any_effects = false;
    for (const auto& trk : model->tracks) {
        if (!trk.effects.empty()) {
            any_effects = true;
            break;
        }
    }
    if (!any_effects) {
        return false;
    }
    const std::uint64_t elapsed_frames = (dsp_clock >= stop_fadeout_start_clock_)
        ? (dsp_clock - stop_fadeout_start_clock_) : 0;
    const std::uint64_t max_tail_frames = static_cast<std::uint64_t>(sample_rate > 0 ? sample_rate : 48000) * 5ULL;
    if (elapsed_frames >= max_tail_frames) {
        return false;
    }
    for (oae_bus tb : track_buses_) {
        if (tb == 0) continue;
        oae_meter m{};
        if (oae_bus_get_meter(tb, &m) == OAE_OK) {
            for (std::uint32_t c = 0; c < m.channel_count && c < OAE_METER_MAX_CHANNELS; ++c) {
                if (m.peak[c] > 1.0e-4f) {
                    return true;
                }
            }
        }
    }
    if (event_master_bus_ != 0) {
        oae_meter m{};
        if (oae_bus_get_meter(event_master_bus_, &m) == OAE_OK) {
            for (std::uint32_t c = 0; c < m.channel_count && c < OAE_METER_MAX_CHANNELS; ++c) {
                if (m.peak[c] > 1.0e-4f) {
                    return true;
                }
            }
        }
    }
    return false;
}

void EventInstance::set_timeline_position_ms(double pos_ms) noexcept
{
    const double clamped = std::max(0.0, pos_ms);
    if (state_ == OAE_PLAYBACK_PLAYING || state_ == OAE_PLAYBACK_SUSTAINING) {
        sync_timeline_jump(clamped);
    } else {
        timeline_position_ms_ = clamped;
        has_prestart_timeline_pos_ = true;
    }
}

bool EventInstance::resolve_marker_destination_ms(const std::string& name, double& out_pos_ms) const noexcept
{
    const auto* model = description_ ? description_->model() : nullptr;
    if (!model || name.empty()) {
        return false;
    }
    for (const auto& target : model->logic_items) {
        if (target.kind == LogicItemKind::Marker && target.name == name) {
            out_pos_ms = target.position_ms;
            return true;
        }
    }
    for (const auto& target : model->logic_items) {
        if (target.kind == LogicItemKind::TransitionMarker && target.destination.empty() && target.name == name) {
            out_pos_ms = target.position_ms;
            return true;
        }
    }
    return false;
}

void EventInstance::sync_timeline_jump(double new_pos_ms, double crossfade_ms)
{
    timeline_position_ms_ = std::max(0.0, new_pos_ms);
    pre_scheduled_until_ms_ = timeline_position_ms_;

    const bool use_crossfade = crossfade_ms > 0.0;
    const auto fade_ms = use_crossfade
        ? static_cast<std::uint32_t>(std::max(1L, std::lround(crossfade_ms)))
        : 0u;

    if (use_crossfade) {
        for (auto it = active_voices_.begin(); it != active_voices_.end();) {
            if (!it->async) {
                if (it->channel != 0) {
                    oae_channel_stop_with_fade(it->channel, fade_ms);
                }
                it = active_voices_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& s : active_scatterers_) {
            if (!s.async && (timeline_position_ms_ < s.region_start_ms || timeline_position_ms_ >= s.region_end_ms)) {
                s.active = false;
            }
        }
    } else {
        stop_sync_voices_outside(timeline_position_ms_, true);
    }
    stop_sync_children_outside(timeline_position_ms_);

    const auto* model = description_ ? description_->model() : nullptr;
    if (!model) return;

    const oae_system sys = description_->system();
    const auto* system = runtime.systems.get(sys);
    const std::uint32_t sample_rate = system ? system->sample_rate() : 48000u;
    oae_bus fallback_bus = output_bus_override_;
    if (fallback_bus == 0) {
        oae_system_get_master_bus(sys, &fallback_bus);
    }

    std::size_t g_idx = 0;
    std::size_t i_idx = 0;
    const float base_vol = db_to_linear(model->volume_db) * volume_;
    const float base_pitch = semitones_to_pitch(model->pitch_semitones) * pitch_;
    const float base_pan = std::clamp(model->pan + pan_, -1.0f, 1.0f);

    for (std::size_t ti = 0; ti < model->tracks.size(); ++ti) {
        const oae_bus inst_bus = (ti < track_buses_.size() && track_buses_[ti] != 0) ? track_buses_[ti] : fallback_bus;
        for (std::size_t ii = 0; ii < model->tracks[ti].instruments.size(); ++ii) {
            const auto& inst = model->tracks[ti].instruments[ii];
            if (!inst.sheet_parameter.empty() || inst.async) continue;
            const double inst_end = inst.length_ms > 0.0 ? (inst.offset_ms + inst.length_ms) : 1.0e12;
            if (timeline_position_ms_ < inst.offset_ms || timeline_position_ms_ >= inst_end) continue;

            if (inst.kind == InstrumentKind::Single || inst.kind == InstrumentKind::Multi ||
                inst.kind == InstrumentKind::Programmer) {
                const auto rel_ms = static_cast<std::uint32_t>(std::llround(
                    std::max(0.0, (timeline_position_ms_ - inst.offset_ms) + inst.start_offset_ms)));
                oae_channel existing = 0;
                for (const auto& v : active_voices_) {
                    if (v.track_idx == ti && v.inst_idx == ii) {
                        existing = v.channel;
                        break;
                    }
                }
                if (existing != 0) {
                    if (auto* c = runtime.channels.get(existing)) {
                        c->set_position_ms(rel_ms);
                    }
                } else {
                    const oae_channel ch = trigger_instrument(sys, ti, ii, inst, inst_bus, sample_rate,
                        base_vol, base_pitch, base_pan, g_idx, i_idx, true);
                    if (ch != 0) {
                        if (rel_ms > 0) {
                            if (auto* c = runtime.channels.get(ch)) {
                                c->set_position_ms(rel_ms);
                            }
                        }
                        if (use_crossfade) {
                            oae_channel_fade_to(ch, 0.0f, 0, OAE_FADE_LINEAR);
                            oae_channel_fade_to(ch, 1.0f, fade_ms, OAE_FADE_LINEAR);
                        }
                    }
                }
            } else if (inst.kind == InstrumentKind::Event) {
                const bool already_active = std::any_of(active_child_events_.begin(), active_child_events_.end(),
                    [ti, ii](const ActiveChildEvent& c) { return c.track_idx == ti && c.inst_idx == ii; });
                if (!already_active) {
                    trigger_event_instrument(sys, ti, ii, inst, inst_bus);
                }
            } else if (inst.kind == InstrumentKind::Command && inst.offset_ms >= timeline_position_ms_) {
                trigger_command_instrument(sys, inst);
            }
        }
    }
}

void EventInstance::stop_sync_voices_outside(double pos_ms, bool include_before_start) noexcept
{
    for (auto it = active_voices_.begin(); it != active_voices_.end();) {
        const bool outside = pos_ms >= it->region_end_ms || (include_before_start && pos_ms < it->region_start_ms);
        if (!it->async && outside) {
            oae_channel_stop(it->channel);
            it = active_voices_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& s : active_scatterers_) {
        if (!s.async && (pos_ms < s.region_start_ms || pos_ms >= s.region_end_ms)) {
            s.active = false;
        }
    }
}

void EventInstance::stop_sync_children_outside(double pos_ms) noexcept
{
    for (auto it = active_child_events_.begin(); it != active_child_events_.end();) {
        if (!it->async && (pos_ms < it->region_start_ms || pos_ms >= it->region_end_ms)) {
            if (it->instance) {
                it->instance->stop(OAE_STOP_ALLOW_FADEOUT);
            }
            if (!it->instance || !it->instance->is_active()) {
                it = active_child_events_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void EventInstance::trigger_command_instrument(oae_system sys, const Instrument& inst)
{
    if (inst.kind != InstrumentKind::Command || !evaluate_condition(inst.condition)) {
        return;
    }
    auto* sys_ptr = runtime.systems.get(sys);
    if (!sys_ptr) {
        return;
    }

    if (inst.command_type == CommandType::StartEvent || inst.command_type == CommandType::SnapshotOn) {
        std::shared_ptr<const ModelSet> target_set;
        const auto* target_model = sys_ptr->find_event_model_by_guid(inst.command_target, target_set);
        if (!target_model) {
            return;
        }
        if (inst.command_type == CommandType::SnapshotOn && target_model->kind != EventKind::Snapshot) {
            return;
        }

        auto desc = std::make_unique<EventDescription>(sys, std::move(target_set), target_model);
        const auto desc_handle = runtime.event_descriptions.insert(std::move(desc));
        if (desc_handle == 0) {
            return;
        }
        auto* registered_desc = runtime.event_descriptions.get(desc_handle);
        if (!registered_desc) {
            runtime.event_descriptions.erase(desc_handle);
            return;
        }
        registered_desc->set_handle(desc_handle);

        EventInstance* spawned = nullptr;
        if (registered_desc->create_instance(&spawned) != OAE_OK || !spawned) {
            runtime.event_descriptions.erase(desc_handle);
            return;
        }

        const auto inst_handle = runtime.event_instances.insert(std::unique_ptr<EventInstance>(spawned));
        if (inst_handle == 0) {
            delete spawned;
            runtime.event_descriptions.erase(desc_handle);
            return;
        }
        auto* registered_inst = runtime.event_instances.get(inst_handle);
        if (!registered_inst) {
            runtime.event_descriptions.erase(desc_handle);
            return;
        }
        registered_inst->set_handle(inst_handle);
        registered_inst->start();
        return;
    }

    auto active_snapshot = sys_ptr->active_instances();
    for (auto* active : active_snapshot) {
        if (!active || !active->description() || !active->description()->model()) {
            continue;
        }
        const auto* target_model = active->description()->model();
        if (target_model->id == inst.command_target) {
            if (inst.command_type == CommandType::SnapshotOff && target_model->kind != EventKind::Snapshot) {
                continue;
            }
            active->stop(OAE_STOP_ALLOW_FADEOUT);
        }
    }
}

void EventInstance::trigger_event_instrument(oae_system sys, std::size_t track_idx, std::size_t inst_idx,
                                             const Instrument& inst, oae_bus target_bus)
{
    if (inst.kind != InstrumentKind::Event || nested_depth_ >= 16 || !evaluate_condition(inst.condition)) {
        return;
    }

    if (inst.async) {
        if (inst.async_policy == AsyncOverlapPolicy::IgnoreIfPlaying) {
            for (const auto& child : active_child_events_) {
                if (child.track_idx == track_idx && child.inst_idx == inst_idx &&
                    child.instance && child.instance->is_active()) {
                    return;
                }
            }
        } else if (inst.async_policy == AsyncOverlapPolicy::CutPrevious) {
            for (auto it = active_child_events_.begin(); it != active_child_events_.end();) {
                if (it->track_idx == track_idx && it->inst_idx == inst_idx) {
                    if (it->instance) {
                        it->instance->stop(OAE_STOP_IMMEDIATE);
                    }
                    it = active_child_events_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    auto* sys_ptr = runtime.systems.get(sys);
    if (!sys_ptr) {
        return;
    }
    std::shared_ptr<const ModelSet> target_set;
    const auto* target_model = sys_ptr->find_event_model_by_guid(inst.event_ref, target_set);
    if (!target_model) {
        return;
    }

    auto desc = std::make_unique<EventDescription>(sys, std::move(target_set), target_model);
    auto child = std::make_unique<EventInstance>(desc.get(), nested_depth_ + 1);
    child->set_output_bus_override(target_bus);
    child->set_child_instance(true);
    child->set_3d_attributes(attributes_3d_);

    if (child->start() != OAE_OK) {
        return;
    }

    const double end_ms = inst.length_ms > 0.0 ? (inst.offset_ms + inst.length_ms) : 1.0e12;
    ActiveChildEvent entry;
    entry.track_idx = track_idx;
    entry.inst_idx = inst_idx;
    entry.async = inst.async;
    entry.region_start_ms = inst.offset_ms;
    entry.region_end_ms = end_ms;
    entry.description = std::move(desc);
    entry.instance = std::move(child);
    active_child_events_.push_back(std::move(entry));
}

std::size_t EventInstance::count_active_scatterer_grains(std::size_t track_idx, std::size_t inst_idx) const noexcept
{
    std::size_t count = 0;
    for (const auto& v : active_voices_) {
        if (v.track_idx == track_idx && v.inst_idx == inst_idx) {
            int playing = 0;
            if (oae_channel_is_playing(v.channel, &playing) == OAE_OK && playing) {
                ++count;
            }
        }
    }
    for (const auto& c : active_child_events_) {
        if (c.track_idx == track_idx && c.inst_idx == inst_idx && c.instance && c.instance->is_active()) {
            ++count;
        }
    }
    return count;
}

bool EventInstance::steal_oldest_scatterer_grain(std::size_t track_idx, std::size_t inst_idx) noexcept
{
    for (auto it = active_voices_.begin(); it != active_voices_.end(); ++it) {
        if (it->track_idx == track_idx && it->inst_idx == inst_idx) {
            int playing = 0;
            if (oae_channel_is_playing(it->channel, &playing) == OAE_OK && playing != 0) {
                const oae_channel ch = it->channel;
                oae_channel_stop(ch);
                playing_channels_.erase(
                    std::remove(playing_channels_.begin(), playing_channels_.end(), ch),
                    playing_channels_.end());
                active_voices_.erase(it);
                return true;
            }
        }
    }
    for (auto it = active_child_events_.begin(); it != active_child_events_.end(); ++it) {
        if (it->track_idx == track_idx && it->inst_idx == inst_idx &&
            it->instance && it->instance->is_active()) {
            it->instance->stop(OAE_STOP_IMMEDIATE);
            active_child_events_.erase(it);
            return true;
        }
    }
    return false;
}

void EventInstance::spawn_scatterer_grain(
    oae_system sys, ActiveScattererState& state,
    const Instrument& inst, oae_bus target_bus, std::uint32_t sample_rate,
    float base_vol, float base_pitch, float base_pan)
{
    if (inst.playlist.empty()) return;
    if (inst.total_sounds > 0 && state.spawned_count >= inst.total_sounds) {
        state.active = false;
        return;
    }
    while (inst.polyphony > 0 &&
           count_active_scatterer_grains(state.track_idx, state.inst_idx) >= inst.polyphony) {
        if (!steal_oldest_scatterer_grain(state.track_idx, state.inst_idx)) {
            return;
        }
    }

    PlaylistState& pstate = (inst.playlist_scope == PlaylistScope::Global)
        ? description_->get_global_playlist(state.global_multi_idx)
        : instance_playlists_[state.instance_multi_idx];
    const auto picked = pstate.pick(inst.playlist, inst.playlist_mode);
    const auto& entry = inst.playlist[picked];

    float inst_vol_db = inst.volume_db_base;
    float inst_pitch_st = inst.pitch_st_base;
    if (inst.volume_db_random_range > 0.0f) {
        inst_vol_db += rng_.next_range(-inst.volume_db_random_range, inst.volume_db_random_range);
    }
    if (inst.pitch_st_random_range > 0.0f) {
        inst_pitch_st += rng_.next_range(-inst.pitch_st_random_range, inst.pitch_st_random_range);
    }

    const float min_d = std::max(0.0f, inst.min_scatter_distance);
    const float max_d = std::max(min_d, inst.max_scatter_distance);
    const float dist = (max_d > min_d) ? rng_.next_range(min_d, max_d) : min_d;
    const float ref_d = std::max(0.1f, min_d);
    const float dist_atten = ref_d / std::max(ref_d, dist);
    const float scatter_pan = rng_.next_range(-1.0f, 1.0f);

    const float grain_vol = db_to_linear(inst_vol_db) * base_vol * dist_atten;
    const float grain_pitch = semitones_to_pitch(inst_pitch_st) * base_pitch;
    const float grain_pan = std::clamp(base_pan + scatter_pan, -1.0f, 1.0f);

    if (!entry.event_guid.is_null() && nested_depth_ < 16) {
        auto* sys_ptr = runtime.systems.get(sys);
        if (sys_ptr) {
            std::shared_ptr<const ModelSet> target_set;
            const auto* target_model = sys_ptr->find_event_model_by_guid(entry.event_guid, target_set);
            if (target_model) {
                auto desc = std::make_unique<EventDescription>(sys, std::move(target_set), target_model);
                auto child = std::make_unique<EventInstance>(desc.get(), nested_depth_ + 1);
                child->set_output_bus_override(target_bus);
                child->set_child_instance(true);
                child->set_3d_attributes(attributes_3d_);
                child->set_volume(dist_atten);
                child->set_pan(scatter_pan);
                if (child->start() == OAE_OK) {
                    ActiveChildEvent ce;
                    ce.track_idx = state.track_idx;
                    ce.inst_idx = state.inst_idx;
                    ce.async = true;
                    ce.region_start_ms = state.region_start_ms;
                    ce.region_end_ms = state.region_end_ms;
                    ce.description = std::move(desc);
                    ce.instance = std::move(child);
                    active_child_events_.push_back(std::move(ce));
                    ++state.spawned_count;
                }
            }
        }
    } else {
        const oae_sound snd = description_->resolve_sound(entry.sound_handle, entry.sample_index);
        if (snd != 0) {
            oae_channel ch = 0;
            if (oae_system_play_sound(sys, snd, target_bus, false, &ch) == OAE_OK && ch != 0) {
                oae_channel_set_volume(ch, std::clamp(grain_vol, 0.0f, 4.0f));
                oae_channel_set_pitch(ch, std::clamp(grain_pitch, 0.01f, 64.0f));
                oae_channel_set_pan(ch, grain_pan);
                playing_channels_.push_back(ch);
                const float atten_db = 20.0f * std::log10(std::max(1.0e-6f, dist_atten));
                active_voices_.push_back({state.track_idx, state.inst_idx, ch, inst.async,
                    state.region_start_ms, state.region_end_ms, inst_vol_db + atten_db, inst_pitch_st, scatter_pan});
                update_channel_parameters();
                ++state.spawned_count;
            }
        }
    }
    (void)sample_rate;
    if (inst.total_sounds > 0 && state.spawned_count >= inst.total_sounds) {
        state.active = false;
    }
}

void EventInstance::start_scatterer(
    oae_system sys, std::size_t track_idx, std::size_t inst_idx,
    const Instrument& inst, oae_bus target_bus, std::uint32_t sample_rate,
    float base_vol, float base_pitch, float base_pan,
    std::size_t global_multi_idx, std::size_t instance_multi_idx)
{
    if (inst.kind != InstrumentKind::Scatterer || !evaluate_condition(inst.condition)) {
        return;
    }
    for (auto& existing : active_scatterers_) {
        if (existing.track_idx == track_idx && existing.inst_idx == inst_idx) {
            existing.active = true;
            existing.region_start_ms = inst.offset_ms;
            existing.region_end_ms = inst.length_ms > 0.0 ? (inst.offset_ms + inst.length_ms) : 1.0e12;
            return;
        }
    }

    ActiveScattererState state;
    state.track_idx = track_idx;
    state.inst_idx = inst_idx;
    state.global_multi_idx = global_multi_idx;
    state.instance_multi_idx = instance_multi_idx;
    state.async = inst.async;
    state.region_start_ms = inst.offset_ms;
    state.region_end_ms = inst.length_ms > 0.0 ? (inst.offset_ms + inst.length_ms) : 1.0e12;
    state.spawned_count = 0;
    state.active = true;

    spawn_scatterer_grain(sys, state, inst, target_bus, sample_rate, base_vol, base_pitch, base_pan);
    const double min_int = std::max(1.0, inst.spawn_interval_min_ms);
    const double max_int = std::max(min_int, inst.spawn_interval_max_ms);
    state.countdown_ms = (max_int > min_int)
        ? static_cast<double>(rng_.next_range(static_cast<float>(min_int), static_cast<float>(max_int)))
        : min_int;
    active_scatterers_.push_back(state);
}

void EventInstance::update_scatterers(
    oae_system sys, double delta_ms, double pos_ms, oae_bus fallback_bus,
    std::uint32_t sample_rate, float event_vol, float event_pitch, float event_pan)
{
    const auto* model = description_ ? description_->model() : nullptr;
    if (!model || delta_ms <= 0.0) return;

    for (auto& s : active_scatterers_) {
        if (!s.active || s.track_idx >= model->tracks.size()) continue;
        const auto& trk = model->tracks[s.track_idx];
        if (s.inst_idx >= trk.instruments.size()) continue;
        const auto& inst = trk.instruments[s.inst_idx];
        if (!s.async && (pos_ms < s.region_start_ms || pos_ms >= s.region_end_ms)) {
            s.active = false;
            continue;
        }
        if (!evaluate_condition(inst.condition)) continue;

        const bool has_track_bus = (s.track_idx < track_buses_.size() && track_buses_[s.track_idx] != 0);
        const oae_bus target_bus = has_track_bus ? track_buses_[s.track_idx] : fallback_bus;
        const float base_vol = db_to_linear(model->volume_db + (has_track_bus ? 0.0f : trk.volume_db)) * event_vol;
        const float base_pitch = semitones_to_pitch(model->pitch_semitones) * event_pitch;
        const float base_pan = std::clamp(model->pan + (has_track_bus ? 0.0f : trk.pan) + event_pan, -1.0f, 1.0f);

        s.countdown_ms -= delta_ms;
        if (s.countdown_ms <= 0.0) {
            spawn_scatterer_grain(sys, s, inst, target_bus, sample_rate, base_vol, base_pitch, base_pan);
            const double min_int = std::max(1.0, inst.spawn_interval_min_ms);
            const double max_int = std::max(min_int, inst.spawn_interval_max_ms);
            s.countdown_ms = (max_int > min_int)
                ? static_cast<double>(rng_.next_range(static_cast<float>(min_int), static_cast<float>(max_int)))
                : min_int;
        }
    }
}

oae_channel EventInstance::trigger_instrument(
    oae_system sys, std::size_t track_idx, std::size_t inst_idx, const Instrument& inst,
    oae_bus target_bus, std::uint32_t sample_rate,
    float base_vol, float base_pitch, float base_pan,
    std::size_t& global_multi_idx, std::size_t& instance_multi_idx, bool immediate)
{
    if (!evaluate_condition(inst.condition)) {
        return 0;
    }
    if (inst.async) {
        if (inst.async_policy == AsyncOverlapPolicy::IgnoreIfPlaying) {
            for (const auto& v : active_voices_) {
                if (v.track_idx == track_idx && v.inst_idx == inst_idx) {
                    int is_pl = 0;
                    if (oae_channel_is_playing(v.channel, &is_pl) == OAE_OK && is_pl) {
                        return 0;
                    }
                }
            }
        } else if (inst.async_policy == AsyncOverlapPolicy::CutPrevious) {
            for (auto it = active_voices_.begin(); it != active_voices_.end();) {
                if (it->track_idx == track_idx && it->inst_idx == inst_idx) {
                    oae_channel_stop(it->channel);
                    it = active_voices_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    float inst_rand_vol_db = inst.volume_db_base;
    float inst_rand_pitch_st = inst.pitch_st_base;
    if (inst.volume_db_random_range > 0.0f) {
        inst_rand_vol_db += rng_.next_range(-inst.volume_db_random_range, inst.volume_db_random_range);
    }
    if (inst.pitch_st_random_range > 0.0f) {
        inst_rand_pitch_st += rng_.next_range(-inst.pitch_st_random_range, inst.pitch_st_random_range);
    }
    float inst_rand_pan = 0.0f;
    for (const auto& m : inst.modulators) {
        if (m.kind == ModulatorKind::Random) {
            if (m.property == ModulatorProperty::Volume) {
                inst_rand_vol_db += rng_.next_range(m.random_range_min, m.random_range_max);
            } else if (m.property == ModulatorProperty::Pitch) {
                inst_rand_pitch_st += rng_.next_range(m.random_range_min, m.random_range_max);
            } else if (m.property == ModulatorProperty::Pan) {
                inst_rand_pan += rng_.next_range(m.random_range_min, m.random_range_max);
            }
        }
    }

    const float final_vol = db_to_linear(inst_rand_vol_db) * base_vol;
    const float final_pitch = semitones_to_pitch(inst_rand_pitch_st) * base_pitch;
    const float final_pan = std::clamp(base_pan + inst_rand_pan, -1.0f, 1.0f);

    oae_sound snd = 0;
    bool from_prog_cb = false;
    if (inst.kind == InstrumentKind::Single) {
        snd = description_->resolve_sound(inst.sound_handle, inst.sample_index);
    } else if (inst.kind == InstrumentKind::Multi) {
        PlaylistState& pstate = (inst.playlist_scope == PlaylistScope::Global)
            ? description_->get_global_playlist(global_multi_idx++)
            : instance_playlists_[instance_multi_idx++];
        if (!inst.playlist.empty()) {
            const auto picked = pstate.pick(inst.playlist, inst.playlist_mode);
            const auto& entry = inst.playlist[picked];
            snd = description_->resolve_sound(entry.sound_handle, entry.sample_index);
        }
    } else if (inst.kind == InstrumentKind::Programmer) {
        if (callback_ && (callback_mask_ & OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)) {
            oae_programmer_sound_properties props{};
            props.name = inst.programmer_key.c_str();
            props.sound = 0;
            callback_(OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND, handle_, &props, callback_user_data_);
            if (props.sound != 0) {
                snd = props.sound;
                from_prog_cb = true;
            }
        }
        if (snd == 0 && description_ && description_->model_set()) {
            if (const auto* entry = description_->model_set()->find_audio_table_entry(inst.programmer_key)) {
                snd = description_->resolve_sound(entry->sound_handle, entry->sample_index);
            }
        }
    }

    if (snd == 0) {
        return 0;
    }

    const auto random_delay = inst.delay_ms_random_range > 0 ?
        rng_.next_bounded(inst.delay_ms_random_range + 1) : 0;
    const auto offset = immediate ? 0u : static_cast<std::uint32_t>(inst.offset_ms);
    const auto delay = offset + random_delay;
    const auto* system = runtime.systems.get(sys);
    const auto clock = system ? system->clock() : start_dsp_clock_;
    const auto onset = clock + milliseconds_to_frames(delay, sample_rate);
    oae_channel channel = 0;
    const auto res = delay == 0 ? oae_system_play_sound(sys, snd, target_bus, false, &channel) :
        oae_system_play_sound_at(sys, snd, target_bus, onset, &channel);
    if (res != OAE_OK) {
        return 0;
    }

    if (from_prog_cb) {
        programmer_callback_sounds_.push_back(snd);
    }
    if (inst.loop) {
        if (auto* c = runtime.channels.get(channel)) {
            c->set_loop(true);
        }
    }
    oae_channel_set_volume(channel, final_vol);
    oae_channel_set_pitch(channel, final_pitch);
    oae_channel_set_pan(channel, final_pan);
    for (const auto& m : inst.modulators) {
        if (m.kind == ModulatorKind::Ahdsr) {
            AhdsrSpec spec{};
            spec.t_start = onset;
            spec.attack_frames = static_cast<std::uint32_t>(milliseconds_to_frames(static_cast<std::uint32_t>(m.attack_ms), sample_rate));
            spec.hold_frames = static_cast<std::uint32_t>(milliseconds_to_frames(static_cast<std::uint32_t>(m.hold_ms), sample_rate));
            spec.decay_frames = static_cast<std::uint32_t>(milliseconds_to_frames(static_cast<std::uint32_t>(m.decay_ms), sample_rate));
            spec.sustain = m.sustain;
            spec.release_frames = static_cast<std::uint32_t>(milliseconds_to_frames(static_cast<std::uint32_t>(m.release_ms), sample_rate));
            spec.t_rel = ~0ULL;
            if (auto* c = runtime.channels.get(channel)) {
                c->set_ahdsr(spec);
            }
        }
    }
    playing_channels_.push_back(channel);
    const double end_ms = inst.length_ms > 0.0 ? (inst.offset_ms + inst.length_ms + random_delay) : 1.0e12;
    const auto* model = description_->model();
    const auto volume_db = base_vol > 0.0f && volume_ > 0.0f ?
        20.0f * std::log10(base_vol / volume_) - model->volume_db : 0.0f;
    const auto pitch_st = base_pitch > 0.0f && pitch_ > 0.0f ?
        12.0f * std::log2(base_pitch / pitch_) - model->pitch_semitones : 0.0f;
    active_voices_.push_back({track_idx, inst_idx, channel, inst.async, inst.offset_ms, end_ms,
        volume_db + inst_rand_vol_db, pitch_st + inst_rand_pitch_st, base_pan - model->pan - pan_ + inst_rand_pan});
    update_channel_parameters();
    return channel;
}

void EventInstance::evaluate_parameter_sheets()
{
    const auto* model = description_ ? description_->model() : nullptr;
    if (!model || !is_active()) return;
    const oae_system sys = description_->system();
    auto* sys_ptr = runtime.systems.get(sys);
    const std::uint32_t sample_rate = sys_ptr ? sys_ptr->sample_rate() : 48000;
    const std::uint64_t now = sys_ptr ? sys_ptr->clock() : 0;

    std::size_t g_idx = 0;
    std::size_t i_idx = 0;
    for (std::size_t ti = 0; ti < model->tracks.size(); ++ti) {
        const auto& track = model->tracks[ti];
        const oae_bus bus = (ti < track_buses_.size() && track_buses_[ti] != 0) ? track_buses_[ti] : 0;
        for (std::size_t ii = 0; ii < track.instruments.size(); ++ii) {
            const auto& inst = track.instruments[ii];
            if (inst.sheet_parameter.empty()) {
                continue;
            }
            float param_val = 0.0f;
            const bool has_param = (get_parameter(inst.sheet_parameter, &param_val, nullptr) == OAE_OK);
            const bool inside = has_param &&
                static_cast<double>(param_val) >= inst.param_min &&
                static_cast<double>(param_val) <= inst.param_max &&
                evaluate_condition(inst.condition);

            auto existing = std::find_if(active_param_voices_.begin(), active_param_voices_.end(),
                [ti, ii](const ActiveParamVoice& v) { return v.track_idx == ti && v.inst_idx == ii; });

            if (inside && existing == active_param_voices_.end()) {
                oae_bus target_bus = bus;
                if (target_bus == 0) {
                    oae_system_get_master_bus(sys, &target_bus);
                }
                const float base_vol = db_to_linear(model->volume_db) * volume_;
                const float base_pitch = semitones_to_pitch(model->pitch_semitones) * pitch_;
                const float base_pan = std::clamp(model->pan + pan_, -1.0f, 1.0f);
                const auto ch = trigger_instrument(sys, ti, ii, inst, target_bus, sample_rate,
                    base_vol, base_pitch, base_pan, g_idx, i_idx, true);
                if (ch != 0) {
                    active_param_voices_.push_back({ti, ii, ch});
                }
            } else if (!inside && existing != active_param_voices_.end()) {
                if (auto* c = runtime.channels.get(existing->channel)) {
                    if (c->has_ahdsr()) {
                        c->trigger_ahdsr_release(now);
                    } else {
                        oae_channel_stop(existing->channel);
                    }
                }
                active_param_voices_.erase(existing);
            }
        }
    }
}

oae_result EventInstance::set_parameter(const std::string& name, float value, bool ignore_seek_speed)
{
    auto it = local_parameters_.find(name);
    if (it != local_parameters_.end()) {
        if (is_active() && it->second.definition().hold_during_playback) {
            return OAE_OK;
        }
        it->second.set_value(value, ignore_seek_speed);
        if (is_active()) {
            evaluate_parameter_sheets();
        }
        return OAE_OK;
    }
    if (description_) {
        auto* sys = runtime.systems.get(description_->system());
        if (sys && sys->set_global_parameter(name, value, ignore_seek_speed) == OAE_OK) {
            if (is_active()) {
                evaluate_parameter_sheets();
            }
            return OAE_OK;
        }
    }
    return OAE_ERR_INVALID_PARAM;
}

oae_result EventInstance::set_parameter_with_label(const std::string& name, const std::string& label, bool ignore_seek_speed)
{
    auto it = local_parameters_.find(name);
    if (it != local_parameters_.end()) {
        if (is_active() && it->second.definition().hold_during_playback) {
            const auto& labels = it->second.definition().labels;
            return (std::find(labels.begin(), labels.end(), label) != labels.end())
                ? OAE_OK : OAE_ERR_INVALID_PARAM;
        }
        const auto res = it->second.set_value_with_label(label, ignore_seek_speed);
        if (res == OAE_OK && is_active()) {
            evaluate_parameter_sheets();
        }
        return res;
    }
    if (description_) {
        auto* sys = runtime.systems.get(description_->system());
        if (sys && sys->set_global_parameter_with_label(name, label, ignore_seek_speed) == OAE_OK) {
            if (is_active()) {
                evaluate_parameter_sheets();
            }
            return OAE_OK;
        }
    }
    return OAE_ERR_INVALID_PARAM;
}

oae_result EventInstance::get_parameter(const std::string& name, float* out_value, float* out_final_value) const noexcept
{
    auto it = local_parameters_.find(name);
    if (it != local_parameters_.end()) {
        if (out_value) *out_value = it->second.current_value();
        if (out_final_value) *out_final_value = it->second.target_value();
        return OAE_OK;
    }
    if (is_active()) {
        const auto held = held_global_parameters_.find(name);
        if (held != held_global_parameters_.end()) {
            if (out_value) *out_value = held->second;
            if (out_final_value) *out_final_value = held->second;
            return OAE_OK;
        }
    }
    if (description_) {
        auto* sys = runtime.systems.get(description_->system());
        if (sys && sys->get_global_parameter(name, out_value, out_final_value) == OAE_OK) {
            return OAE_OK;
        }
    }
    float builtin_val = 0.0f;
    if (try_get_builtin_parameter(name, builtin_val)) {
        if (out_value) *out_value = builtin_val;
        if (out_final_value) *out_final_value = builtin_val;
        return OAE_OK;
    }
    return OAE_ERR_INVALID_PARAM;
}

bool EventInstance::evaluate_condition(const Condition& cond) noexcept
{
    if (cond.probability_pct < 100.0f) {
        if (cond.probability_pct <= 0.0f) {
            return false;
        }
        const float roll = rng_.next_float() * 100.0f;
        if (roll >= cond.probability_pct) {
            return false;
        }
    }

    if (cond.terms.empty()) {
        return true;
    }

    auto* sys = description_ ? runtime.systems.get(description_->system()) : nullptr;

    for (const auto& term : cond.terms) {
        float val = 0.0f;
        bool found = false;

        if (term.param.is_global) {
            if (is_active()) {
                const auto held = held_global_parameters_.find(term.param.name);
                if (held != held_global_parameters_.end()) {
                    val = held->second;
                    found = true;
                }
            }
            if (!found && sys && sys->find_global_parameter_value(term.param.name, val)) {
                found = true;
            }
            if (!found && try_get_builtin_parameter(term.param.name, val)) {
                found = true;
            }
        } else {
            auto it = local_parameters_.find(term.param.name);
            if (it != local_parameters_.end()) {
                val = it->second.current_value();
                found = true;
            } else if (try_get_builtin_parameter(term.param.name, val)) {
                found = true;
            }
        }

        if (!found) {
            return false;
        }

        if (val < term.min_value || val > term.max_value) {
            return false;
        }
    }

    return true;
}

float EventInstance::effective_volume() const noexcept
{
    if (!description_ || !description_->model()) return volume_ * spatial_gain_;
    return db_to_linear(description_->model()->volume_db) * volume_ * spatial_gain_;
}

void EventInstance::key_off() noexcept
{
    key_off_received_ = true;
    if (state_ == OAE_PLAYBACK_SUSTAINING) {
        state_ = OAE_PLAYBACK_PLAYING;
        resumed_from_sustain_ = true;
        for (const auto& v : active_voices_) {
            if (!v.async) {
                oae_channel_set_paused(v.channel, 0);
                if (std::find(playing_channels_.begin(), playing_channels_.end(), v.channel) == playing_channels_.end()) {
                    playing_channels_.push_back(v.channel);
                }
            }
        }
    }
    const auto now = description_ ? oae::runtime.systems.get(description_->system())->clock() : 0;
    for (const auto ch : playing_channels_) {
        auto* c = runtime.channels.get(ch);
        if (c && c->has_ahdsr()) {
            c->trigger_ahdsr_release(now);
        }
    }
    for (auto& child : active_child_events_) {
        if (child.instance) {
            child.instance->key_off();
        }
    }
}

void EventInstance::update_channel_parameters() noexcept
{
    if (!description_ || !description_->model()) return;
    const auto* model = description_->model();

    for (const auto& voice : active_voices_) {
        const float db = model->volume_db + automation_volume_db_ + voice.volume_db;
        const float gain = (db <= -80.0f ? 0.0f : db_to_linear(db) * volume_) * spatial_gain_;
        const float pitch = semitones_to_pitch(model->pitch_semitones + automation_pitch_st_ + voice.pitch_st) * pitch_;
        const float pan = std::clamp(model->pan + automation_pan_ + voice.pan + pan_ + spatial_pan_, -1.0f, 1.0f);
        oae_channel_set_volume(voice.channel, std::clamp(gain, 0.0f, 4.0f));
        oae_channel_set_pitch(voice.channel, std::clamp(pitch, 0.01f, 64.0f));
        oae_channel_set_pan(voice.channel, pan);
    }
}

void EventInstance::set_3d_attributes(const oae_3d_attributes& attrs) noexcept
{
    attributes_3d_ = attrs;
    refresh_3d_state();
}

void EventInstance::set_spatializer_settings(const oae_spatializer_settings& settings) noexcept
{
    spatializer_settings_ = settings;
    refresh_3d_state();
}

bool EventInstance::try_get_builtin_parameter(const std::string& name, float& out_value) const noexcept
{
    if (name != "Distance" && name != "Direction" && name != "Elevation" &&
        name != "EventConeAngle" && name != "Speed") {
        return false;
    }
    oae_3d_attributes listener{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    if (description_) {
        if (const auto* sys = runtime.systems.get(description_->system())) {
            listener = sys->listener_3d_attributes();
        }
    }
    const Spatial3DMetrics metrics = compute_spatial_metrics(listener, attributes_3d_, spatializer_settings_);
    return assign_builtin_metric(metrics, name, out_value);
}

void EventInstance::refresh_3d_state() noexcept
{
    oae_3d_attributes listener{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    if (description_) {
        if (const auto* sys = runtime.systems.get(description_->system())) {
            listener = sys->listener_3d_attributes();
        }
    }

    const Spatial3DMetrics metrics = compute_spatial_metrics(listener, attributes_3d_, spatializer_settings_);
    builtin_distance_ = metrics.distance;
    builtin_direction_deg_ = metrics.direction_deg;
    builtin_elevation_deg_ = metrics.elevation_deg;
    builtin_cone_angle_deg_ = metrics.cone_angle_deg;
    builtin_speed_ = metrics.speed;
    spatial_gain_ = metrics.spatial_gain;
    spatial_pan_ = metrics.spatial_pan;

    const std::pair<const char*, float> builtin_pairs[] = {
        {"Distance", builtin_distance_},
        {"Direction", builtin_direction_deg_},
        {"Elevation", builtin_elevation_deg_},
        {"EventConeAngle", builtin_cone_angle_deg_},
        {"Speed", builtin_speed_}
    };
    for (const auto& bp : builtin_pairs) {
        auto it = local_parameters_.find(bp.first);
        if (it != local_parameters_.end()) {
            if (!(is_active() && it->second.definition().hold_during_playback)) {
                it->second.set_value(bp.second, true);
            }
        }
    }

    for (auto& child : active_child_events_) {
        if (child.instance) {
            child.instance->set_3d_attributes(attributes_3d_);
        }
    }

    update_channel_parameters();
    if (is_active()) {
        evaluate_parameter_sheets();
    }
}

void EventInstance::emit_callback(std::uint32_t type, const void* props, const std::string& name)
{
    if (!callback_ || (callback_mask_ & type) == 0 || !description_) {
        return;
    }
    auto* sys = runtime.systems.get(description_->system());
    if (!sys) {
        return;
    }

    QueuedCallback cb;
    cb.type = type;
    cb.instance = handle_;
    cb.callback = callback_;
    cb.user_data = callback_user_data_;

    if (type == OAE_EVENT_CALLBACK_MARKER && props) {
        cb.marker_props = *static_cast<const oae_timeline_marker_properties*>(props);
        cb.marker_name = name;
    } else if (type == OAE_EVENT_CALLBACK_BEAT && props) {
        cb.beat_props = *static_cast<const oae_timeline_beat_properties*>(props);
    } else if ((type == OAE_EVENT_CALLBACK_SOUND_PLAYED || type == OAE_EVENT_CALLBACK_SOUND_STOPPED) && props) {
        cb.sound_props = *static_cast<const oae_event_sound_properties*>(props);
    }

    sys->callback_queue().enqueue(std::move(cb));
}

oae_result EventInstance::start()
{
    if (!description_ || !description_->model()) {
        return OAE_ERR_INVALID_STATE;
    }

    if (!description_->are_samples_loaded()) {
        return OAE_ERR_INVALID_STATE;
    }

    const auto* model = description_->model();
    const auto sys = description_->system();

    uint64_t dsp_clock = 0;
    oae_system_get_dsp_clock(sys, &dsp_clock);
    constexpr uint32_t sample_rate = 48000;

    if (!description_->check_and_apply_cooldown(dsp_clock, sample_rate)) {
        return OAE_OK;
    }

    if (model->max_instances > 0 && model->steal == StealMode::None &&
        description_->active_instance_count() >= static_cast<std::int32_t>(model->max_instances)) {
        return OAE_OK;
    }

    description_->apply_stealing(this);

    start_order_ = description_->next_instance_order();
    rng_.seed(0x55aa1234ull + start_order_, 0x11223344ull);
    refresh_3d_state();
    automation_volume_db_ = 0.0f;
    automation_pitch_st_ = 0.0f;
    automation_pan_ = 0.0f;
    start_dsp_clock_ = dsp_clock;
    last_update_dsp_clock_ = dsp_clock;
    if (!has_prestart_timeline_pos_) {
        timeline_position_ms_ = 0.0;
    }
    has_prestart_timeline_pos_ = false;
    key_off_received_ = false;
    resumed_from_sustain_ = false;
    pre_scheduled_until_ms_ = std::numeric_limits<double>::infinity();
    for (const auto& item : model->logic_items) {
        if (item.kind == LogicItemKind::SustainPoint && item.position_ms >= timeline_position_ms_) {
            pre_scheduled_until_ms_ = std::min(pre_scheduled_until_ms_, item.position_ms);
        } else if ((item.kind == LogicItemKind::LoopRegion || item.kind == LogicItemKind::TransitionRegion) &&
                   item.end_ms >= timeline_position_ms_) {
            pre_scheduled_until_ms_ = std::min(pre_scheduled_until_ms_, std::max(timeline_position_ms_, item.position_ms));
        } else if (item.kind == LogicItemKind::TransitionMarker && !item.destination.empty() &&
                   item.position_ms >= timeline_position_ms_) {
            pre_scheduled_until_ms_ = std::min(pre_scheduled_until_ms_, item.position_ms);
        }
    }
    loop_repeat_count_ = 0;
    loop_repeat_counts_.clear();
    next_beat_index_ = 0;
    pending_transition_dest_.clear();
    pending_transition_quant_ = LogicQuantization::Immediate;
    pending_transition_xfade_ms_ = 0.0;
    pending_transition_arm_ms_ = 0.0;
    active_voices_.clear();
    active_param_voices_.clear();
    active_child_events_.clear();
    active_scatterers_.clear();
    programmer_callback_sounds_.clear();
    held_global_parameters_.clear();
    if (auto* s = runtime.systems.get(sys)) {
        for (const auto& kv : s->global_parameters()) {
            if (kv.second.definition().hold_during_playback) {
                held_global_parameters_[kv.first] = kv.second.current_value();
            }
        }
    }

    state_ = OAE_PLAYBACK_PLAYING;
    description_->register_active(this);
    if (!is_child_instance_) {
        if (auto* s = runtime.systems.get(sys)) {
            s->add_active_instance(this);
        }
    }

    emit_callback(OAE_EVENT_CALLBACK_STARTED);

    if (model->kind == EventKind::Snapshot) {
        return OAE_OK;
    }

    oae_bus target_bus = output_bus_override_;
    if (target_bus == 0) {
        if (!model->bus_path.empty()) {
            if (oae_system_get_bus(sys, model->bus_path.c_str(), &target_bus) != OAE_OK) {
                oae_system_get_master_bus(sys, &target_bus);
            }
        } else {
            oae_system_get_master_bus(sys, &target_bus);
        }
    }

    create_track_buses(sys, target_bus);

    std::size_t global_multi_idx = 0;
    std::size_t instance_multi_idx = 0;

    float event_rand_vol_db = 0.0f;
    float event_rand_pitch_st = 0.0f;
    float event_rand_pan = 0.0f;
    for (const auto& m : model->modulators) {
        if (m.kind == ModulatorKind::Random) {
            if (m.property == ModulatorProperty::Volume) {
                event_rand_vol_db += rng_.next_range(m.random_range_min, m.random_range_max);
            } else if (m.property == ModulatorProperty::Pitch) {
                event_rand_pitch_st += rng_.next_range(m.random_range_min, m.random_range_max);
            } else if (m.property == ModulatorProperty::Pan) {
                event_rand_pan += rng_.next_range(m.random_range_min, m.random_range_max);
            }
        }
    }

    for (std::size_t ti = 0; ti < model->tracks.size(); ++ti) {
        const auto& track = model->tracks[ti];
        const bool has_track_bus = (ti < track_buses_.size() && track_buses_[ti] != 0);
        const oae_bus inst_bus = has_track_bus ? track_buses_[ti] : target_bus;
        const float base_vol = db_to_linear(model->volume_db + event_rand_vol_db + (has_track_bus ? 0.0f : track.volume_db)) * volume_;
        const float base_pitch = semitones_to_pitch(model->pitch_semitones + event_rand_pitch_st) * pitch_;
        const float base_pan = std::clamp(model->pan + event_rand_pan + (has_track_bus ? 0.0f : track.pan) + pan_, -1.0f, 1.0f);

        for (std::size_t ii = 0; ii < track.instruments.size(); ++ii) {
            const auto& inst = track.instruments[ii];
            if (!inst.sheet_parameter.empty()) {
                continue;
            }
            if (inst.kind == InstrumentKind::Single || inst.kind == InstrumentKind::Multi ||
                inst.kind == InstrumentKind::Programmer) {
                if (inst.offset_ms <= timeline_position_ms_ || inst.offset_ms < pre_scheduled_until_ms_) {
                    if (timeline_position_ms_ > 0.0 && inst.offset_ms <= timeline_position_ms_) {
                        const oae_channel ch = trigger_instrument(sys, ti, ii, inst, inst_bus, sample_rate,
                            base_vol, base_pitch, base_pan, global_multi_idx, instance_multi_idx, true);
                        if (ch != 0 && timeline_position_ms_ > inst.offset_ms) {
                            oae_channel_set_position_ms(ch, static_cast<std::uint32_t>(timeline_position_ms_ - inst.offset_ms));
                        }
                    } else if (inst.offset_ms <= 0.0 && timeline_position_ms_ == 0.0) {
                        trigger_instrument(sys, ti, ii, inst, inst_bus, sample_rate,
                            base_vol, base_pitch, base_pan, global_multi_idx, instance_multi_idx, true);
                    } else if (inst.offset_ms >= timeline_position_ms_) {
                        trigger_instrument(sys, ti, ii, inst, inst_bus, sample_rate,
                            base_vol, base_pitch, base_pan, global_multi_idx, instance_multi_idx, false);
                    } else if (inst.kind == InstrumentKind::Multi) {
                        if (inst.playlist_scope == PlaylistScope::Global) ++global_multi_idx;
                        else ++instance_multi_idx;
                    }
                } else if (inst.kind == InstrumentKind::Multi) {
                    if (inst.playlist_scope == PlaylistScope::Global) ++global_multi_idx;
                    else ++instance_multi_idx;
                }
            } else if (inst.kind == InstrumentKind::Event && inst.offset_ms <= timeline_position_ms_) {
                trigger_event_instrument(sys, ti, ii, inst, inst_bus);
            } else if (inst.kind == InstrumentKind::Command && inst.offset_ms <= 0.0) {
                trigger_command_instrument(sys, inst);
            } else if (inst.kind == InstrumentKind::Scatterer) {
                const std::size_t g_idx = (inst.playlist_scope == PlaylistScope::Global) ? global_multi_idx++ : 0;
                const std::size_t i_idx = (inst.playlist_scope == PlaylistScope::Instance) ? instance_multi_idx++ : 0;
                if (inst.offset_ms <= 0.0) {
                    start_scatterer(sys, ti, ii, inst, inst_bus, sample_rate,
                        base_vol, base_pitch, base_pan, g_idx, i_idx);
                }
            }
        }
    }

    evaluate_parameter_sheets();
    if (timeline_position_ms_ > 0.0) {
        sync_timeline_jump(timeline_position_ms_);
    }

    return OAE_OK;
}

oae_result EventInstance::stop(int mode)
{
    if (state_ == OAE_PLAYBACK_STOPPED) {
        return OAE_OK;
    }

    const auto* model = description_ ? description_->model() : nullptr;
    if (model && model->kind == EventKind::Snapshot) {
        if (mode == OAE_STOP_ALLOW_FADEOUT && model->stop_fadeout_ms > 0) {
            state_ = OAE_PLAYBACK_STOPPING;
            stop_fadeout_start_clock_ = description_ ? oae::runtime.systems.get(description_->system())->clock() : 0;
        } else {
            state_ = OAE_PLAYBACK_STOPPED;
            emit_callback(OAE_EVENT_CALLBACK_STOPPED);
            if (description_) {
                description_->unregister_active(this);
            }
        }
        return OAE_OK;
    }

    emit_callback(OAE_EVENT_CALLBACK_STOPPED);

    if (mode == OAE_STOP_ALLOW_FADEOUT) {
        stop_fadeout_start_clock_ = description_ ? oae::runtime.systems.get(description_->system())->clock() : 0;
        bool has_ahdsr_channel = false;
        const auto now = stop_fadeout_start_clock_;
        for (const auto ch : playing_channels_) {
            auto* c = runtime.channels.get(ch);
            if (c && c->has_ahdsr()) {
                c->trigger_ahdsr_release(now);
                has_ahdsr_channel = true;
            }
        }
        bool has_track_effects = false;
        if (model) {
            for (const auto& trk : model->tracks) {
                if (!trk.effects.empty()) {
                    has_track_effects = true;
                    break;
                }
            }
        }
        if (description_ && description_->model() && description_->model()->stop_fadeout_ms > 0) {
            for (const auto ch : playing_channels_) {
                auto* c = runtime.channels.get(ch);
                if (!c || !c->has_ahdsr()) {
                    oae_channel_stop_with_fade(ch, description_->model()->stop_fadeout_ms);
                }
            }
            state_ = OAE_PLAYBACK_STOPPING;
        } else if (has_ahdsr_channel || has_track_effects) {
            if (!has_ahdsr_channel) {
                for (const auto ch : playing_channels_) {
                    oae_channel_stop(ch);
                }
            }
            state_ = OAE_PLAYBACK_STOPPING;
        } else {
            for (const auto ch : playing_channels_) {
                oae_channel_stop(ch);
            }
            state_ = OAE_PLAYBACK_STOPPED;
        }
    } else {
        for (const auto ch : playing_channels_) {
            oae_channel_stop(ch);
        }
        state_ = OAE_PLAYBACK_STOPPED;
    }

    for (auto& s : active_scatterers_) {
        s.active = false;
    }

    for (auto it = active_child_events_.begin(); it != active_child_events_.end();) {
        if (it->instance) {
            it->instance->stop(mode);
            if (!it->instance->is_active()) {
                it = active_child_events_.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (mode == OAE_STOP_ALLOW_FADEOUT && !active_child_events_.empty() && state_ == OAE_PLAYBACK_STOPPED) {
        state_ = OAE_PLAYBACK_STOPPING;
    }

    if (state_ == OAE_PLAYBACK_STOPPED) {
        if (callback_ && (callback_mask_ & OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)) {
            for (const auto snd : programmer_callback_sounds_) {
                oae_programmer_sound_properties props{"", snd};
                callback_(OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND, handle_, &props, callback_user_data_);
            }
        }
        programmer_callback_sounds_.clear();
        active_voices_.clear();
        active_param_voices_.clear();
        active_child_events_.clear();
        held_global_parameters_.clear();
        destroy_track_buses();
        if (description_) {
            description_->unregister_active(this);
        }
    }

    return OAE_OK;
}

void EventInstance::update(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept
{
    if (!is_active()) {
        return;
    }

    const auto* model = description_ ? description_->model() : nullptr;
    if (!model) return;

    const std::uint64_t delta_frames = (dsp_clock >= last_update_dsp_clock_)
        ? (dsp_clock - last_update_dsp_clock_) : 0;
    const double delta_sec = static_cast<double>(delta_frames) / static_cast<double>(sample_rate > 0 ? sample_rate : 48000);
    double delta_ms = delta_sec * 1000.0;
    if (model->pitch_affects_timeline) {
        const float pitch_scale = semitones_to_pitch(model->pitch_semitones) * pitch_;
        delta_ms *= static_cast<double>(pitch_scale);
    }

    for (auto& kv : local_parameters_) {
        if (!kv.second.definition().hold_during_playback) {
            kv.second.advance_time(delta_sec);
        }
    }

    if (model->kind == EventKind::Event) {
        evaluate_parameter_sheets();
    }

    if (model->kind == EventKind::Snapshot) {
        if (state_ == OAE_PLAYBACK_STOPPING) {
            const std::uint64_t stopping_elapsed = (dsp_clock >= stop_fadeout_start_clock_) ? (dsp_clock - stop_fadeout_start_clock_) : 0;
            const std::uint64_t fadeout_frames = (static_cast<std::uint64_t>(model->stop_fadeout_ms) * static_cast<std::uint64_t>(sample_rate > 0 ? sample_rate : 48000)) / 1000;
            if (stopping_elapsed >= fadeout_frames) {
                state_ = OAE_PLAYBACK_STOPPED;
                emit_callback(OAE_EVENT_CALLBACK_STOPPED);
                if (description_) {
                    description_->unregister_active(this);
                }
            }
        }
        last_update_dsp_clock_ = dsp_clock;
        return;
    }

    const double t0 = timeline_position_ms_;
    double t1 = t0;

    if (state_ == OAE_PLAYBACK_PLAYING) {
        t1 = t0 + delta_ms;
        double sustain_stop_ms = std::numeric_limits<double>::infinity();
        double loop_stop_ms = std::numeric_limits<double>::infinity();
        for (std::size_t li = 0; li < model->logic_items.size(); ++li) {
            const auto& item = model->logic_items[li];
            if (!key_off_received_ && item.kind == LogicItemKind::SustainPoint) {
                const bool in_range = (t0 == 0.0)
                    ? (item.position_ms >= t0 && item.position_ms <= t1)
                    : (item.position_ms > t0 && item.position_ms <= t1);
                if (in_range && item.position_ms < sustain_stop_ms) {
                    sustain_stop_ms = item.position_ms;
                }
            } else if (item.kind == LogicItemKind::LoopRegion) {
                const auto it_cnt = loop_repeat_counts_.find(li);
                const std::int32_t rep = (it_cnt != loop_repeat_counts_.end()) ? it_cnt->second : 0;
                const bool eligible = item.until_key_off
                    ? !key_off_received_
                    : (item.repeat_count == -1 || rep < item.repeat_count);
                if (eligible && item.end_ms > t0 && item.end_ms <= t1 && evaluate_condition(item.condition)) {
                    loop_stop_ms = std::min(loop_stop_ms, item.end_ms);
                }
            }
        }
        const bool hit_sustain = std::isfinite(sustain_stop_ms);
        if (hit_sustain) {
            t1 = sustain_stop_ms;
        }
        const bool hit_loop_cap = std::isfinite(loop_stop_ms) && (!hit_sustain || loop_stop_ms <= sustain_stop_ms);
        timeline_position_ms_ = t1;

        const oae_system sys = description_->system();
        oae_bus fallback_bus = output_bus_override_;
        if (fallback_bus == 0) {
            oae_system_get_master_bus(sys, &fallback_bus);
        }
        for (std::size_t ti = 0; ti < model->tracks.size(); ++ti) {
            const bool has_track_bus = (ti < track_buses_.size() && track_buses_[ti] != 0);
            const oae_bus inst_bus = has_track_bus ? track_buses_[ti] : fallback_bus;
            const float b_vol = db_to_linear(model->volume_db + (has_track_bus ? 0.0f : model->tracks[ti].volume_db)) * volume_;
            const float b_pitch = semitones_to_pitch(model->pitch_semitones) * pitch_;
            const float b_pan = std::clamp(model->pan + (has_track_bus ? 0.0f : model->tracks[ti].pan) + pan_, -1.0f, 1.0f);
            std::size_t g_idx = 0, i_idx = 0;
            for (std::size_t pti = 0; pti < ti; ++pti) {
                for (const auto& pinst : model->tracks[pti].instruments) {
                    if (pinst.kind == InstrumentKind::Multi || pinst.kind == InstrumentKind::Scatterer) {
                        if (pinst.playlist_scope == PlaylistScope::Global) ++g_idx;
                        else ++i_idx;
                    }
                }
            }
            for (std::size_t ii = 0; ii < model->tracks[ti].instruments.size(); ++ii) {
                const auto& inst = model->tracks[ti].instruments[ii];
                std::size_t cur_g = 0, cur_i = 0;
                if (inst.kind == InstrumentKind::Multi || inst.kind == InstrumentKind::Scatterer) {
                    if (inst.playlist_scope == PlaylistScope::Global) cur_g = g_idx++;
                    else cur_i = i_idx++;
                }
                if (!inst.sheet_parameter.empty()) continue;
                const bool after_t0 = resumed_from_sustain_ ? (inst.offset_ms >= t0) : (inst.offset_ms > t0);
                const bool before_t1 = hit_sustain
                    ? (inst.offset_ms < t1)
                    : (hit_loop_cap ? (inst.offset_ms < loop_stop_ms) : (inst.offset_ms <= t1));
                if (after_t0 && before_t1) {
                    if (inst.kind == InstrumentKind::Single || inst.kind == InstrumentKind::Multi ||
                        inst.kind == InstrumentKind::Programmer) {
                        if (inst.offset_ms > 0.0 && inst.offset_ms >= pre_scheduled_until_ms_) {
                            trigger_instrument(sys, ti, ii, inst, inst_bus, sample_rate,
                                b_vol, b_pitch, b_pan, cur_g, cur_i, true);
                        }
                    } else if (inst.kind == InstrumentKind::Event) {
                        trigger_event_instrument(sys, ti, ii, inst, inst_bus);
                    } else if (inst.kind == InstrumentKind::Command) {
                        trigger_command_instrument(sys, inst);
                    } else if (inst.kind == InstrumentKind::Scatterer) {
                        start_scatterer(sys, ti, ii, inst, inst_bus, sample_rate,
                            b_vol, b_pitch, b_pan, cur_g, cur_i);
                    }
                }
            }
        }
        resumed_from_sustain_ = false;
        stop_sync_voices_outside(timeline_position_ms_);
        stop_sync_children_outside(timeline_position_ms_);
    }

    std::size_t best_loop_idx = static_cast<std::size_t>(-1);
    std::int32_t best_loop_prio = std::numeric_limits<std::int32_t>::min();
    double best_loop_len = std::numeric_limits<double>::max();
    bool jumped_by_transition = false;

    // Process logic items
    for (std::size_t li = 0; li < model->logic_items.size(); ++li) {
        const auto& item = model->logic_items[li];

        if (item.kind == LogicItemKind::Marker) {
            const bool in_range = (t0 == 0.0)
                ? (item.position_ms >= t0 && item.position_ms <= t1)
                : (item.position_ms > t0 && item.position_ms <= t1);
            if (in_range) {
                oae_timeline_marker_properties props{};
                props.dsp_clock = start_dsp_clock_ + milliseconds_to_frames(static_cast<std::uint32_t>(item.position_ms), sample_rate);
                props.position_ms = static_cast<std::int32_t>(std::llround(item.position_ms));
                props.marker_index = static_cast<std::uint32_t>(li);
                emit_callback(OAE_EVENT_CALLBACK_MARKER, &props, item.name);
            }
        } else if (item.kind == LogicItemKind::SustainPoint) {
            const bool in_range = (t0 == 0.0)
                ? (item.position_ms >= t0 && item.position_ms <= t1)
                : (item.position_ms > t0 && item.position_ms <= t1);
            if (!key_off_received_ && in_range) {
                timeline_position_ms_ = item.position_ms;
                state_ = OAE_PLAYBACK_SUSTAINING;
                for (const auto& v : active_voices_) {
                    if (!v.async) {
                        oae_channel_set_paused(v.channel, 1);
                    }
                }
            }
        } else if (item.kind == LogicItemKind::LoopRegion) {
            if (t1 >= item.end_ms && t0 < item.end_ms + 1.0e-6) {
                if (evaluate_condition(item.condition)) {
                    const std::int32_t repeats_done = loop_repeat_counts_[li];
                    const bool eligible = item.until_key_off
                        ? !key_off_received_
                        : (item.repeat_count == -1 || repeats_done < item.repeat_count);
                    if (eligible) {
                        const double loop_len = item.end_ms - item.position_ms;
                        if (item.priority > best_loop_prio ||
                            (item.priority == best_loop_prio && loop_len < best_loop_len)) {
                            best_loop_idx = li;
                            best_loop_prio = item.priority;
                            best_loop_len = loop_len;
                        }
                    }
                }
            }
        } else if (item.kind == LogicItemKind::TransitionMarker && !item.destination.empty()) {
            const bool in_range = (t0 == 0.0)
                ? (item.position_ms >= t0 && item.position_ms <= t1)
                : (item.position_ms > t0 && item.position_ms <= t1);
            if (in_range && evaluate_condition(item.condition)) {
                if (item.quantization == LogicQuantization::Immediate) {
                    double dest_ms = 0.0;
                    if (resolve_marker_destination_ms(item.destination, dest_ms)) {
                        pending_transition_dest_.clear();
                        pending_transition_quant_ = LogicQuantization::Immediate;
                        pending_transition_xfade_ms_ = 0.0;
                        pending_transition_arm_ms_ = 0.0;
                        sync_timeline_jump(dest_ms, item.crossfade_ms);
                        jumped_by_transition = true;
                        break;
                    }
                } else {
                    pending_transition_dest_ = item.destination;
                    pending_transition_quant_ = item.quantization;
                    pending_transition_xfade_ms_ = item.crossfade_ms;
                    pending_transition_arm_ms_ = item.position_ms;
                }
            }
        } else if (item.kind == LogicItemKind::TransitionRegion && !item.destination.empty()) {
            if (t1 >= item.position_ms && t0 < item.end_ms) {
                if (evaluate_condition(item.condition)) {
                    if (item.quantization == LogicQuantization::Immediate) {
                        double dest_ms = 0.0;
                        if (resolve_marker_destination_ms(item.destination, dest_ms)) {
                            pending_transition_dest_.clear();
                            pending_transition_quant_ = LogicQuantization::Immediate;
                            pending_transition_xfade_ms_ = 0.0;
                            pending_transition_arm_ms_ = 0.0;
                            sync_timeline_jump(dest_ms, item.crossfade_ms);
                            jumped_by_transition = true;
                            break;
                        }
                    } else {
                        pending_transition_dest_ = item.destination;
                        pending_transition_quant_ = item.quantization;
                        pending_transition_xfade_ms_ = item.crossfade_ms;
                        pending_transition_arm_ms_ = std::max(t0, item.position_ms);
                    }
                }
            }
        }
    }

    if (!jumped_by_transition && best_loop_idx != static_cast<std::size_t>(-1)) {
        const auto& loop_item = model->logic_items[best_loop_idx];
        ++loop_repeat_counts_[best_loop_idx];
        ++loop_repeat_count_;
        const double loop_len = loop_item.end_ms - loop_item.position_ms;
        const double target_pos = loop_len > 0.0
            ? (loop_item.position_ms + std::fmod(t1 - loop_item.position_ms, loop_len))
            : loop_item.position_ms;
        sync_timeline_jump(target_pos);
    }

    // Process TEMPO / Beats
    double bpm = 120.0;
    std::int32_t ts_upper = 4;
    std::int32_t ts_lower = 4;
    for (const auto& item : model->logic_items) {
        if (item.kind == LogicItemKind::Tempo) {
            bpm = item.bpm;
            ts_upper = item.ts_upper;
            ts_lower = item.ts_lower;
            break;
        }
    }

    const double frames_per_beat = static_cast<double>(sample_rate) * (60.0 / bpm) * (4.0 / static_cast<double>(ts_lower));
    for (;;) {
        const std::uint64_t b_frame = start_dsp_clock_ + static_cast<std::uint64_t>(std::llround(static_cast<double>(next_beat_index_) * frames_per_beat));
        if (b_frame <= dsp_clock) {
            oae_timeline_beat_properties props{};
            props.dsp_clock = b_frame;
            props.bar = static_cast<std::int32_t>(next_beat_index_ / static_cast<std::uint64_t>(ts_upper));
            props.beat = static_cast<std::int32_t>(next_beat_index_ % static_cast<std::uint64_t>(ts_upper));
            props.position_ms = static_cast<std::int32_t>((static_cast<double>(next_beat_index_) * frames_per_beat * 1000.0) / static_cast<double>(sample_rate));
            props.tempo = static_cast<float>(bpm);
            props.time_signature_upper = ts_upper;
            props.time_signature_lower = ts_lower;

            emit_callback(OAE_EVENT_CALLBACK_BEAT, &props);

            if (!pending_transition_dest_.empty() && next_beat_index_ > 0) {
                const bool quant_match =
                    (pending_transition_quant_ == LogicQuantization::Bar)
                        ? (props.beat == 0)
                        : (pending_transition_quant_ == LogicQuantization::Beat);
                if (quant_match &&
                    static_cast<double>(props.position_ms) + 1.0e-3 >= pending_transition_arm_ms_) {
                    double dest_ms = 0.0;
                    const double xfade = pending_transition_xfade_ms_;
                    const bool resolved = resolve_marker_destination_ms(pending_transition_dest_, dest_ms);
                    pending_transition_dest_.clear();
                    pending_transition_quant_ = LogicQuantization::Immediate;
                    pending_transition_xfade_ms_ = 0.0;
                    pending_transition_arm_ms_ = 0.0;
                    if (resolved) {
                        sync_timeline_jump(dest_ms, xfade);
                    }
                }
            }

            ++next_beat_index_;
        } else {
            break;
        }
    }

    playing_channels_.erase(
        std::remove_if(playing_channels_.begin(), playing_channels_.end(), [](oae_channel ch) {
            int is_playing = 0;
            if (oae_channel_is_playing(ch, &is_playing) != OAE_OK || !is_playing) {
                return true;
            }
            return false;
        }),
        playing_channels_.end());

    // Process automations
    float auto_vol_db = 0.0f;
    float auto_pitch_st = 0.0f;
    float auto_pan = 0.0f;
    const auto automation_value = [&](const ParamRef& param, float& value) {
        if (param.is_global) {
            const auto held = held_global_parameters_.find(param.name);
            if (held != held_global_parameters_.end()) {
                value = held->second;
                return true;
            }
            auto* system = description_ ? runtime.systems.get(description_->system()) : nullptr;
            if (system && system->find_global_parameter_value(param.name, value)) {
                return true;
            }
            return try_get_builtin_parameter(param.name, value);
        }
        const auto found = local_parameters_.find(param.name);
        if (found != local_parameters_.end()) {
            value = found->second.current_value();
            return true;
        }
        return try_get_builtin_parameter(param.name, value);
    };

    for (const auto& a : model->automations) {
        float x = 0.0f;
        if (a.is_timeline) {
            x = static_cast<float>(timeline_position_ms_);
        } else {
            float param_val = 0.0f;
            if (automation_value(a.param, param_val)) {
                x = param_val;
            }
        }
        const float y = curve_eval(a.curve, x);
        if (a.target == AutomationTarget::Volume) {
            auto_vol_db += y;
        } else if (a.target == AutomationTarget::Pitch) {
            auto_pitch_st += y;
        } else if (a.target == AutomationTarget::Pan) {
            auto_pan += y;
        }
    }
    for (const auto& track : model->tracks) {
        for (const auto& a : track.automations) {
            float x = 0.0f;
            if (a.is_timeline) {
                x = static_cast<float>(timeline_position_ms_);
            } else {
                float param_val = 0.0f;
                if (automation_value(a.param, param_val)) {
                    x = param_val;
                }
            }
            const float y = curve_eval(a.curve, x);
            if (a.target == AutomationTarget::Volume) {
                auto_vol_db += y;
            } else if (a.target == AutomationTarget::Pitch) {
                auto_pitch_st += y;
            } else if (a.target == AutomationTarget::Pan) {
                auto_pan += y;
            }
        }
    }

    {
        const oae_system sys = description_->system();
        oae_bus fallback_bus = output_bus_override_;
        if (fallback_bus == 0) {
            oae_system_get_master_bus(sys, &fallback_bus);
        }
        update_scatterers(sys, delta_ms, timeline_position_ms_, fallback_bus, sample_rate, volume_, pitch_, pan_);
    }

    active_voices_.erase(std::remove_if(active_voices_.begin(), active_voices_.end(), [](const ActiveInstVoice& voice) {
        int playing = 0;
        return oae_channel_is_playing(voice.channel, &playing) != OAE_OK || !playing;
    }), active_voices_.end());
    automation_volume_db_ = auto_vol_db;
    automation_pitch_st_ = auto_pitch_st;
    automation_pan_ = auto_pan;
    update_channel_parameters();

    for (auto it = active_child_events_.begin(); it != active_child_events_.end();) {
        if (it->instance) {
            it->instance->update(dsp_clock, sample_rate);
            if (!it->instance->is_active()) {
                it = active_child_events_.erase(it);
                continue;
            }
        } else {
            it = active_child_events_.erase(it);
            continue;
        }
        ++it;
    }

    const bool any_scatterer_active = std::any_of(active_scatterers_.begin(), active_scatterers_.end(),
        [](const ActiveScattererState& s) { return s.active; });

    double timeline_end_ms = static_cast<double>(model->total_length_ms());
    for (const auto& item : model->logic_items) {
        if (item.kind == LogicItemKind::LoopRegion || item.kind == LogicItemKind::TransitionRegion) {
            timeline_end_ms = std::max(timeline_end_ms, item.end_ms);
        } else if (item.kind == LogicItemKind::SustainPoint || item.kind == LogicItemKind::TransitionMarker) {
            timeline_end_ms = std::max(timeline_end_ms, item.position_ms);
        }
    }
    bool has_parameter_sheet = false;
    for (const auto& trk : model->tracks) {
        for (const auto& inst : trk.instruments) {
            if (!inst.sheet_parameter.empty()) {
                has_parameter_sheet = true;
                break;
            }
        }
        if (has_parameter_sheet) break;
    }

    if (playing_channels_.empty() &&
        active_child_events_.empty() &&
        !any_scatterer_active &&
        !has_active_track_effect_tail(dsp_clock, sample_rate) &&
        (state_ == OAE_PLAYBACK_STOPPING ||
         (state_ != OAE_PLAYBACK_SUSTAINING && !has_parameter_sheet && timeline_position_ms_ >= timeline_end_ms))) {
        state_ = OAE_PLAYBACK_STOPPED;
        if (callback_ && (callback_mask_ & OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)) {
            for (const auto snd : programmer_callback_sounds_) {
                oae_programmer_sound_properties props{"", snd};
                callback_(OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND, handle_, &props, callback_user_data_);
            }
        }
        programmer_callback_sounds_.clear();
        active_voices_.clear();
        active_param_voices_.clear();
        active_child_events_.clear();
        active_scatterers_.clear();
        held_global_parameters_.clear();
        destroy_track_buses();
        emit_callback(OAE_EVENT_CALLBACK_STOPPED);
        if (description_) {
            description_->unregister_active(this);
        }
    }

    last_update_dsp_clock_ = dsp_clock;
}

}
