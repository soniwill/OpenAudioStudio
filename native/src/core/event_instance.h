#pragma once

#include "studio_model.h"
#include <oae_audio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oae {

class EventInstance;
class System;

struct QueuedCallback {
    std::uint32_t type = 0;
    oae_event_instance instance = 0;
    oae_event_callback callback = nullptr;
    void* user_data = nullptr;

    oae_timeline_marker_properties marker_props{};
    std::string marker_name;
    oae_timeline_beat_properties beat_props{};
    oae_event_sound_properties sound_props{};
};

class CallbackQueue {
public:
    static constexpr std::size_t max_capacity = 4096;
    static constexpr std::size_t emergency_threshold = 3072;

    bool enqueue(QueuedCallback item)
    {
        if (queue_.size() >= max_capacity) {
            return false;
        }
        if (queue_.size() >= emergency_threshold) {
            if (item.type == OAE_EVENT_CALLBACK_SOUND_PLAYED ||
                item.type == OAE_EVENT_CALLBACK_SOUND_STOPPED) {
                return false;
            }
        }
        queue_.push_back(std::move(item));
        return true;
    }

    void dispatch()
    {
        if (queue_.empty()) return;
        auto to_dispatch = std::move(queue_);
        queue_.clear();
        for (auto& item : to_dispatch) {
            if (item.callback) {
                const void* props = nullptr;
                if (item.type == OAE_EVENT_CALLBACK_MARKER) {
                    item.marker_props.name = item.marker_name.c_str();
                    props = &item.marker_props;
                } else if (item.type == OAE_EVENT_CALLBACK_BEAT) {
                    props = &item.beat_props;
                } else if (item.type == OAE_EVENT_CALLBACK_SOUND_PLAYED ||
                           item.type == OAE_EVENT_CALLBACK_SOUND_STOPPED) {
                    props = &item.sound_props;
                }
                item.callback(item.type, item.instance, props, item.user_data);
            }
        }
    }

private:
    std::vector<QueuedCallback> queue_;
};

class EventDescription {
public:
    EventDescription(oae_system system, std::shared_ptr<const ModelSet> model_set, const EventDescriptionModel* model);
    ~EventDescription();

    oae_system system() const noexcept { return system_; }
    oae_event_description handle() const noexcept { return handle_; }
    void set_handle(oae_event_description h) noexcept { handle_ = h; }
    const EventDescriptionModel* model() const noexcept { return model_; }
    std::shared_ptr<const ModelSet> model_set() const noexcept { return model_set_; }

    oae_result create_instance(EventInstance** out_instance);
    std::int32_t active_instance_count() const noexcept;

    bool check_and_apply_cooldown(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept;
    void apply_stealing(EventInstance* new_instance) noexcept;

    void register_active(EventInstance* inst) noexcept;
    void unregister_active(EventInstance* inst) noexcept;

    PlaylistState& get_global_playlist(std::size_t flat_idx);

    std::uint64_t next_instance_order() noexcept { return ++instance_order_counter_; }

    bool are_samples_loaded() const noexcept;
    oae_result load_sample_data();
    oae_result unload_sample_data();
    oae_sound resolve_sound(oae_sound direct_sound, std::int32_t sample_index) const noexcept;

private:
    oae_system system_;
    oae_event_description handle_ = 0;
    std::shared_ptr<const ModelSet> model_set_;
    const EventDescriptionModel* model_;
    std::vector<PlaylistState> global_playlists_;
    std::vector<EventInstance*> active_instances_;
    std::uint64_t last_trigger_clock_ = 0;
    bool has_triggered_ = false;
    std::uint64_t instance_order_counter_ = 0;
};

class EventInstance {
    friend class EventDescription;
public:
    explicit EventInstance(EventDescription* description, std::uint32_t nested_depth = 0);
    ~EventInstance();

    EventDescription* description() const noexcept { return description_; }
    oae_event_instance handle() const noexcept { return handle_; }
    void set_handle(oae_event_instance h) noexcept { handle_ = h; }

    oae_playback_state playback_state() const noexcept { return state_; }
    std::uint64_t start_dsp_clock() const noexcept { return start_dsp_clock_; }
    std::uint64_t stop_fadeout_start_clock() const noexcept { return stop_fadeout_start_clock_; }

    oae_result start();
    oae_result stop(int mode);

    void set_volume(float v) noexcept { volume_ = v; update_channel_parameters(); }
    float volume() const noexcept { return volume_; }

    void set_pitch(float p) noexcept { pitch_ = p; update_channel_parameters(); }
    float pitch() const noexcept { return pitch_; }

    void set_pan(float p) noexcept { pan_ = p; update_channel_parameters(); }
    float pan() const noexcept { return pan_; }

    float effective_volume() const noexcept;

    std::uint64_t start_order() const noexcept { return start_order_; }

    void set_callback(oae_event_callback cb, std::uint32_t mask, void* user_data) noexcept {
        callback_ = cb;
        callback_mask_ = mask;
        callback_user_data_ = user_data;
    }

    /**
     * @brief Overrides the destination bus used when starting this instance.
     */
    void set_output_bus_override(oae_bus bus) noexcept { output_bus_override_ = bus; }

    /**
     * @brief Marks this instance as owned and updated by a parent EventInstance.
     */
    void set_child_instance(bool is_child) noexcept { is_child_instance_ = is_child; }

    void key_off() noexcept;

    oae_bus track_bus(std::uint32_t track_index) const noexcept {
        return track_index < track_buses_.size() ? track_buses_[track_index] : 0;
    }

    double timeline_position_ms() const noexcept { return timeline_position_ms_; }
    void set_timeline_position_ms(double pos_ms) noexcept;

    oae_result set_parameter(const std::string& name, float value, bool ignore_seek_speed);
    oae_result set_parameter_with_label(const std::string& name, const std::string& label, bool ignore_seek_speed);
    oae_result get_parameter(const std::string& name, float* out_value, float* out_final_value) const noexcept;
    bool evaluate_condition(const Condition& cond) noexcept;
    void set_rng_seed(std::uint64_t state, std::uint64_t seq = 1ull) noexcept { rng_.seed(state, seq); }

    void update(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept;
    void evaluate_parameter_sheets();

    void set_3d_attributes(const oae_3d_attributes& attrs) noexcept;
    const oae_3d_attributes& get_3d_attributes() const noexcept { return attributes_3d_; }
    void set_spatializer_settings(const oae_spatializer_settings& settings) noexcept;
    const oae_spatializer_settings& get_spatializer_settings() const noexcept { return spatializer_settings_; }
    void refresh_3d_state() noexcept;
    bool try_get_builtin_parameter(const std::string& name, float& out_value) const noexcept;

    bool is_active() const noexcept {
        return state_ == OAE_PLAYBACK_PLAYING ||
               state_ == OAE_PLAYBACK_STARTING ||
               state_ == OAE_PLAYBACK_SUSTAINING ||
               state_ == OAE_PLAYBACK_STOPPING;
    }

private:
    void sync_timeline_jump(double new_pos_ms, double crossfade_ms = 0.0);
    bool resolve_marker_destination_ms(const std::string& name, double& out_pos_ms) const noexcept;
    bool steal_oldest_scatterer_grain(std::size_t track_idx, std::size_t inst_idx) noexcept;

    struct ActiveInstVoice {
        std::size_t track_idx = 0;
        std::size_t inst_idx = 0;
        oae_channel channel = 0;
        bool async = false;
        double region_start_ms = 0.0;
        double region_end_ms = 0.0;
        float volume_db = 0.0f;
        float pitch_st = 0.0f;
        float pan = 0.0f;
    };

    struct ActiveParamVoice {
        std::size_t track_idx = 0;
        std::size_t inst_idx = 0;
        oae_channel channel = 0;
    };

    /**
     * @brief Tracks an active nested child EventInstance spawned by an Event Instrument.
     */
    struct ActiveChildEvent {
        std::size_t track_idx = 0;
        std::size_t inst_idx = 0;
        bool async = false;
        double region_start_ms = 0.0;
        double region_end_ms = 0.0;
        std::unique_ptr<EventDescription> description;
        std::unique_ptr<EventInstance> instance;
    };

    /**
     * @brief Tracks an active Scatterer Instrument spawner and its next grain countdown.
     */
    struct ActiveScattererState {
        std::size_t track_idx = 0;
        std::size_t inst_idx = 0;
        std::size_t global_multi_idx = 0;
        std::size_t instance_multi_idx = 0;
        bool async = false;
        double region_start_ms = 0.0;
        double region_end_ms = 0.0;
        double countdown_ms = 0.0;
        std::uint32_t spawned_count = 0;
        bool active = false;
    };

    void update_channel_parameters() noexcept;
    void emit_callback(std::uint32_t type, const void* props = nullptr, const std::string& name = {});
    void create_track_buses(oae_system sys, oae_bus target_bus);
    void destroy_track_buses() noexcept;
    bool has_active_track_effect_tail(std::uint64_t dsp_clock, std::uint32_t sample_rate) const noexcept;
    void stop_sync_voices_outside(double pos_ms, bool include_before_start = false) noexcept;
    void stop_sync_children_outside(double pos_ms) noexcept;
    void trigger_command_instrument(oae_system sys, const Instrument& inst);
    void trigger_event_instrument(oae_system sys, std::size_t track_idx, std::size_t inst_idx,
                                  const Instrument& inst, oae_bus target_bus);
    void start_scatterer(oae_system sys, std::size_t track_idx, std::size_t inst_idx,
                         const Instrument& inst, oae_bus target_bus, std::uint32_t sample_rate,
                         float base_vol, float base_pitch, float base_pan,
                         std::size_t global_multi_idx, std::size_t instance_multi_idx);
    void spawn_scatterer_grain(oae_system sys, ActiveScattererState& state,
                               const Instrument& inst, oae_bus target_bus, std::uint32_t sample_rate,
                               float base_vol, float base_pitch, float base_pan);
    void update_scatterers(oae_system sys, double delta_ms, double pos_ms, oae_bus fallback_bus,
                           std::uint32_t sample_rate, float event_vol, float event_pitch, float event_pan);
    std::size_t count_active_scatterer_grains(std::size_t track_idx, std::size_t inst_idx) const noexcept;
    oae_channel trigger_instrument(
        oae_system sys, std::size_t track_idx, std::size_t inst_idx, const Instrument& inst,
        oae_bus target_bus, std::uint32_t sample_rate,
        float base_vol, float base_pitch, float base_pan,
        std::size_t& global_multi_idx, std::size_t& instance_multi_idx, bool immediate = false);

    EventDescription* description_;
    oae_event_instance handle_ = 0;
    std::uint32_t nested_depth_ = 0;
    oae_playback_state state_ = OAE_PLAYBACK_STOPPED;
    float volume_ = 1.0f;
    float pitch_ = 1.0f;
    float pan_ = 0.0f;
    float automation_volume_db_ = 0.0f;
    float automation_pitch_st_ = 0.0f;
    float automation_pan_ = 0.0f;
    oae_3d_attributes attributes_3d_{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    oae_spatializer_settings spatializer_settings_{
        0, OAE_ROLLOFF_LINEAR, 1.0f, 20.0f, 0.0f, 0.0f, 360.0f, 360.0f, 1.0f
    };
    float builtin_distance_ = 0.0f;
    float builtin_direction_deg_ = 0.0f;
    float builtin_elevation_deg_ = 0.0f;
    float builtin_cone_angle_deg_ = 0.0f;
    float builtin_speed_ = 0.0f;
    float spatial_gain_ = 1.0f;
    float spatial_pan_ = 0.0f;
    std::uint64_t start_order_ = 0;
    std::uint64_t start_dsp_clock_ = 0;
    std::uint64_t stop_fadeout_start_clock_ = 0;
    std::uint64_t last_update_dsp_clock_ = 0;

    double timeline_position_ms_ = 0.0;
    double pre_scheduled_until_ms_ = 0.0;
    bool has_prestart_timeline_pos_ = false;
    bool key_off_received_ = false;
    bool resumed_from_sustain_ = false;
    std::int32_t loop_repeat_count_ = 0;
    std::unordered_map<std::size_t, std::int32_t> loop_repeat_counts_;
    std::uint64_t next_beat_index_ = 0;
    std::string pending_transition_dest_;
    LogicQuantization pending_transition_quant_ = LogicQuantization::Immediate;
    double pending_transition_xfade_ms_ = 0.0;
    double pending_transition_arm_ms_ = 0.0;

    oae_bus event_master_bus_ = 0;
    oae_bus output_bus_override_ = 0;
    bool is_child_instance_ = false;
    std::vector<oae_bus> track_buses_;
    std::vector<ActiveInstVoice> active_voices_;
    std::vector<ActiveParamVoice> active_param_voices_;
    std::vector<oae_sound> programmer_callback_sounds_;

    oae_event_callback callback_ = nullptr;
    std::uint32_t callback_mask_ = 0;
    void* callback_user_data_ = nullptr;

    std::vector<PlaylistState> instance_playlists_;
    std::vector<oae_channel> playing_channels_;
    std::vector<ActiveChildEvent> active_child_events_;
    std::vector<ActiveScattererState> active_scatterers_;
    std::unordered_map<std::string, ParameterInstance> local_parameters_;
    std::unordered_map<std::string, float> held_global_parameters_;
    Pcg32 rng_{0x55aa1234ull, 0x11223344ull};
};

}
