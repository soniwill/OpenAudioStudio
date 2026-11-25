#ifndef OAE_AUDIO_H
#define OAE_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(OAE_AUDIO_STATIC)
#    define OAE_API
#  elif defined(OAE_AUDIO_BUILD)
#    define OAE_API __declspec(dllexport)
#  else
#    define OAE_API __declspec(dllimport)
#  endif
#  define OAE_CALL __cdecl
#else
#  define OAE_API __attribute__((visibility("default")))
#  define OAE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum oae_result {
    OAE_OK = 0,
    OAE_ERR_INVALID_PARAM = 1,
    OAE_ERR_INVALID_HANDLE = 2,
    OAE_ERR_NOT_INITIALIZED = 3,
    OAE_ERR_ALREADY_INITIALIZED = 4,
    OAE_ERR_OUT_OF_MEMORY = 5,
    OAE_ERR_FILE_NOT_FOUND = 6,
    OAE_ERR_FORMAT = 7,
    OAE_ERR_DEVICE = 8,
    OAE_ERR_NOT_FOUND = 9,
    OAE_ERR_ALREADY_EXISTS = 10,
    OAE_ERR_INVALID_STATE = 11,
    OAE_ERR_INTERNAL = 100
} oae_result;

typedef uint64_t oae_system;
typedef uint64_t oae_sound;
typedef uint64_t oae_channel;
typedef uint64_t oae_bus;
typedef uint64_t oae_effect;
typedef uint64_t oae_bank;
typedef uint64_t oae_event_description;
typedef uint64_t oae_event_instance;
typedef uint64_t oae_vca;

typedef enum oae_init_flags {
    OAE_INIT_DEFAULT = 0,
    OAE_INIT_NO_DEVICE = 1 << 0
} oae_init_flags;

typedef struct oae_init_settings {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t max_physical_voices;
    uint32_t flags;
} oae_init_settings;

typedef enum oae_sound_flags {
    OAE_SOUND_DEFAULT = 0,
    OAE_SOUND_LOOP = 1 << 0,
    OAE_SOUND_STREAM = 1 << 1
} oae_sound_flags;

typedef enum oae_fade_curve {
    OAE_FADE_LINEAR = 0,
    OAE_FADE_LOG    = 1,
    OAE_FADE_SCURVE = 2
} oae_fade_curve;

typedef enum oae_effect_type {
    OAE_EFFECT_LIMITER    = 0,
    OAE_EFFECT_COMPRESSOR = 1,
    OAE_EFFECT_EQ3        = 2,
    OAE_EFFECT_LOWPASS    = 3,
    OAE_EFFECT_HIGHPASS   = 4,
    OAE_EFFECT_REVERB     = 5,
    OAE_EFFECT_DELAY      = 6
} oae_effect_type;

typedef enum oae_effect_position {
    OAE_EFFECT_POST_FADER = 0,
    OAE_EFFECT_PRE_FADER  = 1
} oae_effect_position;

#define OAE_EFFECT_INDEX_LAST (-1)

typedef enum oae_limiter_param {
    OAE_LIMITER_CEILING_DB    = 0,
    OAE_LIMITER_RELEASE_MS    = 1,
    OAE_LIMITER_INPUT_GAIN_DB = 2
} oae_limiter_param;

typedef enum oae_compressor_param {
    OAE_COMPRESSOR_THRESHOLD_DB = 0,
    OAE_COMPRESSOR_RATIO        = 1,
    OAE_COMPRESSOR_ATTACK_MS    = 2,
    OAE_COMPRESSOR_RELEASE_MS   = 3,
    OAE_COMPRESSOR_MAKEUP_DB    = 4
} oae_compressor_param;

typedef enum oae_eq3_param {
    OAE_EQ3_LOW_DB        = 0,
    OAE_EQ3_MID_DB        = 1,
    OAE_EQ3_HIGH_DB       = 2,
    OAE_EQ3_LOW_XOVER_HZ  = 3,
    OAE_EQ3_HIGH_XOVER_HZ = 4
} oae_eq3_param;

typedef enum oae_filter_param {
    OAE_FILTER_CUTOFF_HZ = 0,
    OAE_FILTER_Q         = 1
} oae_filter_param;

typedef enum oae_reverb_param {
    OAE_REVERB_ROOM_SIZE = 0,
    OAE_REVERB_DAMPING   = 1,
    OAE_REVERB_WIDTH     = 2
} oae_reverb_param;

typedef enum oae_delay_param {
    OAE_DELAY_TIME_MS  = 0,
    OAE_DELAY_FEEDBACK = 1
} oae_delay_param;

#define OAE_METER_MAX_CHANNELS 8
typedef struct oae_meter {
    uint32_t channel_count;
    uint32_t frame_count;
    float peak[OAE_METER_MAX_CHANNELS];
    float rms[OAE_METER_MAX_CHANNELS];
} oae_meter;

typedef enum oae_playback_state {
    OAE_PLAYBACK_PLAYING    = 0,
    OAE_PLAYBACK_SUSTAINING = 1,
    OAE_PLAYBACK_STOPPED    = 2,
    OAE_PLAYBACK_STARTING   = 3,
    OAE_PLAYBACK_STOPPING   = 4
} oae_playback_state;

typedef enum oae_event_kind {
    OAE_EVENT_KIND_EVENT    = 0,
    OAE_EVENT_KIND_SNAPSHOT = 1
} oae_event_kind;

typedef enum oae_snapshot_mode {
    OAE_SNAPSHOT_OVERRIDE = 0,
    OAE_SNAPSHOT_BLENDING = 1
} oae_snapshot_mode;

typedef enum oae_stop_mode {
    OAE_STOP_ALLOW_FADEOUT = 0,
    OAE_STOP_IMMEDIATE     = 1
} oae_stop_mode;

typedef enum oae_rolloff_type {
    OAE_ROLLOFF_LINEAR         = 0,
    OAE_ROLLOFF_INVERSE        = 1,
    OAE_ROLLOFF_LINEAR_SQUARED = 2
} oae_rolloff_type;

typedef struct oae_vec3 {
    float x;
    float y;
    float z;
} oae_vec3;

typedef struct oae_3d_attributes {
    oae_vec3 position;
    oae_vec3 velocity;
    oae_vec3 forward;
    oae_vec3 up;
} oae_3d_attributes;

typedef struct oae_spatializer_settings {
    int enabled;
    int rolloff;
    float min_distance;
    float max_distance;
    float sound_size;
    float min_extent_deg;
    float cone_inner_angle_deg;
    float cone_outer_angle_deg;
    float cone_outer_gain;
} oae_spatializer_settings;

typedef enum oae_event_callback_type {
    OAE_EVENT_CALLBACK_STARTED                  = 1 << 0,
    OAE_EVENT_CALLBACK_STOPPED                  = 1 << 1,
    OAE_EVENT_CALLBACK_MARKER                   = 1 << 2,
    OAE_EVENT_CALLBACK_BEAT                     = 1 << 3,
    OAE_EVENT_CALLBACK_STOLEN                   = 1 << 4,
    OAE_EVENT_CALLBACK_SOUND_PLAYED             = 1 << 5,
    OAE_EVENT_CALLBACK_SOUND_STOPPED            = 1 << 6,
    OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND  = 1 << 7,
    OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND = 1 << 8,
    OAE_EVENT_CALLBACK_ALL                      = 0x1FF
} oae_event_callback_type;

typedef struct oae_timeline_marker_properties {
    uint64_t dsp_clock;
    const char* name;
    int32_t position_ms;
    uint32_t marker_index;
} oae_timeline_marker_properties;

typedef struct oae_timeline_beat_properties {
    uint64_t dsp_clock;
    int32_t bar;
    int32_t beat;
    int32_t position_ms;
    float tempo;
    int32_t time_signature_upper;
    int32_t time_signature_lower;
} oae_timeline_beat_properties;

typedef struct oae_event_sound_properties {
    uint64_t dsp_clock;
    oae_sound sound;
    oae_channel channel;
    uint32_t instrument_index;
    uint32_t nested_depth;
} oae_event_sound_properties;

typedef struct oae_programmer_sound_properties {
    const char* name;
    oae_sound sound;
} oae_programmer_sound_properties;

typedef oae_result (OAE_CALL *oae_event_callback)(
    uint32_t type, oae_event_instance instance, const void* properties, void* user_data);

typedef struct oae_struct_sizes {
    uint32_t init_settings;
    uint32_t meter;
    uint32_t timeline_marker_properties;
    uint32_t timeline_beat_properties;
    uint32_t event_sound_properties;
} oae_struct_sizes;

enum { OAE_LOAD_BANK_NONBLOCKING = 1u << 0 };
typedef enum oae_bank_loading_state {
    OAE_BANK_LOADING = 0,
    OAE_BANK_LOADED  = 1,
    OAE_BANK_FAILED  = 2
} oae_bank_loading_state;

OAE_API uint32_t OAE_CALL oae_get_version(void);
OAE_API const char* OAE_CALL oae_result_string(oae_result result);
OAE_API oae_result OAE_CALL oae_init_settings_default(oae_init_settings* settings);
OAE_API oae_result OAE_CALL oae_system_create(oae_system* out_system);
OAE_API oae_result OAE_CALL oae_system_init(oae_system system, const oae_init_settings* settings);
OAE_API oae_result OAE_CALL oae_system_update(oae_system system);
OAE_API oae_result OAE_CALL oae_system_render(oae_system system, float* out, uint32_t frames);
OAE_API oae_result OAE_CALL oae_system_release(oae_system system);

OAE_API oae_result OAE_CALL oae_sound_create_from_file(oae_system system, const char* path, uint32_t flags, oae_sound* out_sound);
OAE_API oae_result OAE_CALL oae_sound_create_from_memory(oae_system system, const void* data, size_t size, uint32_t flags, oae_sound* out_sound);
OAE_API oae_result OAE_CALL oae_sound_release(oae_sound sound);
OAE_API oae_result OAE_CALL oae_sound_get_length_ms(oae_sound sound, uint32_t* out_ms);

OAE_API oae_result OAE_CALL oae_system_play_sound(oae_system system, oae_sound sound, oae_bus bus, int paused, oae_channel* out_channel);
OAE_API oae_result OAE_CALL oae_channel_stop(oae_channel channel);
OAE_API oae_result OAE_CALL oae_channel_set_paused(oae_channel channel, int paused);
OAE_API oae_result OAE_CALL oae_channel_get_paused(oae_channel channel, int* out_paused);
OAE_API oae_result OAE_CALL oae_channel_set_volume(oae_channel channel, float volume);
OAE_API oae_result OAE_CALL oae_channel_get_volume(oae_channel channel, float* out_volume);
OAE_API oae_result OAE_CALL oae_channel_set_pitch(oae_channel channel, float pitch);
OAE_API oae_result OAE_CALL oae_channel_get_pitch(oae_channel channel, float* out_pitch);
OAE_API oae_result OAE_CALL oae_channel_is_playing(oae_channel channel, int* out_is_playing);
OAE_API oae_result OAE_CALL oae_channel_get_position_ms(oae_channel channel, uint32_t* out_ms);
OAE_API oae_result OAE_CALL oae_channel_set_position_ms(oae_channel channel, uint32_t ms);

OAE_API oae_result OAE_CALL oae_system_get_master_bus(oae_system system, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_bus_create(oae_system system, const char* name, oae_bus parent, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_system_get_bus(oae_system system, const char* path, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_bus_set_volume(oae_bus bus, float volume);
OAE_API oae_result OAE_CALL oae_bus_get_volume(oae_bus bus, float* out_volume);
OAE_API oae_result OAE_CALL oae_bus_set_mute(oae_bus bus, int mute);
OAE_API oae_result OAE_CALL oae_bus_get_mute(oae_bus bus, int* out_mute);
OAE_API oae_result OAE_CALL oae_bus_set_paused(oae_bus bus, int paused);
OAE_API oae_result OAE_CALL oae_bus_get_paused(oae_bus bus, int* out_paused);
OAE_API oae_result OAE_CALL oae_bus_stop_all(oae_bus bus);
OAE_API oae_result OAE_CALL oae_bus_release(oae_bus bus);
OAE_API oae_result OAE_CALL oae_bus_create_return(oae_system system, const char* name, oae_bus parent, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_bus_add_send(oae_bus source, oae_bus target, float level, int position);
OAE_API oae_result OAE_CALL oae_bus_remove_send(oae_bus source, oae_bus target);
OAE_API oae_result OAE_CALL oae_bus_set_send_level(oae_bus source, oae_bus target, float level);
OAE_API oae_result OAE_CALL oae_bus_get_send_level(oae_bus source, oae_bus target, float* out_level);
OAE_API oae_result OAE_CALL oae_bus_set_send_mute(oae_bus source, oae_bus target, int mute);
OAE_API oae_result OAE_CALL oae_bus_get_send_mute(oae_bus source, oae_bus target, int* out_mute);

OAE_API oae_result OAE_CALL oae_channel_set_priority(oae_channel channel, int priority);
OAE_API oae_result OAE_CALL oae_channel_get_priority(oae_channel channel, int* out_priority);
OAE_API oae_result OAE_CALL oae_channel_is_virtual(oae_channel channel, int* out_virtual);
OAE_API oae_result OAE_CALL oae_system_get_channels_playing(oae_system system, int* total, int* real);

OAE_API oae_result OAE_CALL oae_channel_set_pan(oae_channel channel, float pan);
OAE_API oae_result OAE_CALL oae_channel_get_pan(oae_channel channel, float* out_pan);
OAE_API oae_result OAE_CALL oae_bus_set_pan(oae_bus bus, float pan);
OAE_API oae_result OAE_CALL oae_bus_get_pan(oae_bus bus, float* out_pan);
OAE_API oae_result OAE_CALL oae_channel_fade_to(oae_channel channel, float target, uint32_t duration_ms, int curve);
OAE_API oae_result OAE_CALL oae_bus_fade_to(oae_bus bus, float target, uint32_t duration_ms, int curve);
OAE_API oae_result OAE_CALL oae_channel_get_fade_gain(oae_channel channel, float* out_gain);
OAE_API oae_result OAE_CALL oae_bus_get_fade_gain(oae_bus bus, float* out_gain);
OAE_API oae_result OAE_CALL oae_channel_stop_with_fade(oae_channel channel, uint32_t duration_ms);
OAE_API oae_result OAE_CALL oae_system_get_dsp_clock(oae_system system, uint64_t* out_frames);
OAE_API oae_result OAE_CALL oae_system_play_sound_at(oae_system system, oae_sound sound, oae_bus bus, uint64_t dsp_clock, oae_channel* out_channel);
OAE_API oae_result OAE_CALL oae_system_play_sound_delayed(oae_system system, oae_sound sound, oae_bus bus, uint32_t delay_ms, oae_channel* out_channel);

OAE_API oae_result OAE_CALL oae_bus_add_effect(oae_bus bus, int type, int32_t index, oae_effect* out_effect);
OAE_API oae_result OAE_CALL oae_bus_add_effect_ex(oae_bus bus, int type, int position, int32_t index, oae_effect* out_effect);
OAE_API oae_result OAE_CALL oae_effect_set_param(oae_effect effect, int32_t param_id, float value);
OAE_API oae_result OAE_CALL oae_effect_get_param(oae_effect effect, int32_t param_id, float* out_value);
OAE_API oae_result OAE_CALL oae_effect_set_bypass(oae_effect effect, int bypass);
OAE_API oae_result OAE_CALL oae_effect_get_bypass(oae_effect effect, int* out_bypass);
OAE_API oae_result OAE_CALL oae_effect_set_wet_dry(oae_effect effect, float wet, float dry);
OAE_API oae_result OAE_CALL oae_effect_get_wet_dry(oae_effect effect, float* out_wet, float* out_dry);
OAE_API oae_result OAE_CALL oae_effect_set_sidechain(oae_effect effect, oae_bus source_bus);
OAE_API oae_result OAE_CALL oae_effect_release(oae_effect effect);
OAE_API oae_result OAE_CALL oae_bus_get_meter(oae_bus bus, oae_meter* out_meter);

OAE_API oae_result OAE_CALL oae_studio_load_bank_memory(oae_system system, const void* bytes, size_t size, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_load_bank_file(oae_system system, const char* path, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_unload_bank(oae_system system, oae_bank bank);

OAE_API oae_result OAE_CALL oae_studio_get_vca(oae_system system, const char* path, oae_vca* out_vca);
OAE_API oae_result OAE_CALL oae_vca_set_volume(oae_vca vca, float volume);
OAE_API oae_result OAE_CALL oae_vca_get_volume(oae_vca vca, float* out_volume);

OAE_API oae_result OAE_CALL oae_studio_get_event(oae_system system, const char* path, oae_event_description* out_description);
OAE_API oae_result OAE_CALL oae_studio_play_one_shot(oae_system system, const char* path);
OAE_API oae_result OAE_CALL oae_event_description_create_instance(oae_event_description description, oae_event_instance* out_instance);
OAE_API oae_result OAE_CALL oae_event_description_get_length_ms(oae_event_description description, uint32_t* out_length_ms);
OAE_API oae_result OAE_CALL oae_event_description_is_oneshot(oae_event_description description, int* out_is_oneshot);
OAE_API oae_result OAE_CALL oae_event_description_get_instance_count(oae_event_description description, int* out_count);
OAE_API oae_result OAE_CALL oae_event_instance_start(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_stop(oae_event_instance instance, int mode);
OAE_API oae_result OAE_CALL oae_event_instance_release(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_get_playback_state(oae_event_instance instance, int* out_state);
OAE_API oae_result OAE_CALL oae_event_instance_get_description(oae_event_instance instance, oae_event_description* out_description);
OAE_API oae_result OAE_CALL oae_event_instance_set_volume(oae_event_instance instance, float volume);
OAE_API oae_result OAE_CALL oae_event_instance_get_volume(oae_event_instance instance, float* out_volume);
OAE_API oae_result OAE_CALL oae_event_instance_set_pitch(oae_event_instance instance, float pitch);
OAE_API oae_result OAE_CALL oae_event_instance_get_pitch(oae_event_instance instance, float* out_pitch);
OAE_API oae_result OAE_CALL oae_event_instance_set_pan(oae_event_instance instance, float pan);
OAE_API oae_result OAE_CALL oae_event_instance_get_pan(oae_event_instance instance, float* out_pan);

OAE_API oae_result OAE_CALL oae_event_instance_set_callback(oae_event_instance instance, oae_event_callback callback, uint32_t mask, void* user_data);
OAE_API oae_result OAE_CALL oae_event_instance_get_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_event_instance_key_off(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_get_timeline_position_ms(oae_event_instance instance, int32_t* out_position_ms);
OAE_API oae_result OAE_CALL oae_event_instance_set_timeline_position_ms(oae_event_instance instance, int32_t position_ms);

OAE_API oae_result OAE_CALL oae_event_instance_set_parameter_by_name(oae_event_instance instance, const char* name, float value, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_event_instance_set_parameter_by_name_with_label(oae_event_instance instance, const char* name, const char* label, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_event_instance_get_parameter_by_name(oae_event_instance instance, const char* name, float* out_value, float* out_final_value);
OAE_API oae_result OAE_CALL oae_studio_set_global_parameter_by_name(oae_system system, const char* name, float value, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_studio_set_global_parameter_by_name_with_label(oae_system system, const char* name, const char* label, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_studio_get_global_parameter_by_name(oae_system system, const char* name, float* out_value, float* out_final_value);

OAE_API oae_result OAE_CALL oae_get_struct_sizes(oae_struct_sizes* out_sizes);
OAE_API oae_result OAE_CALL oae_studio_load_bank_file_ex(oae_system system, const char* path, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_load_bank_memory_ex(oae_system system, const void* bytes, size_t size, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_get_bank_loading_state(oae_system system, oae_bank bank, int* out_state);
OAE_API oae_result OAE_CALL oae_event_description_load_sample_data(oae_event_description description);
OAE_API oae_result OAE_CALL oae_event_description_unload_sample_data(oae_event_description description);

OAE_API oae_result OAE_CALL oae_system_set_listener_3d_attributes(oae_system system, const oae_3d_attributes* attributes);
OAE_API oae_result OAE_CALL oae_system_get_listener_3d_attributes(oae_system system, oae_3d_attributes* out_attributes);
OAE_API oae_result OAE_CALL oae_event_instance_set_3d_attributes(oae_event_instance instance, const oae_3d_attributes* attributes);
OAE_API oae_result OAE_CALL oae_event_instance_get_3d_attributes(oae_event_instance instance, oae_3d_attributes* out_attributes);
OAE_API oae_result OAE_CALL oae_event_instance_set_spatializer_settings(oae_event_instance instance, const oae_spatializer_settings* settings);
OAE_API oae_result OAE_CALL oae_event_instance_get_spatializer_settings(oae_event_instance instance, oae_spatializer_settings* out_settings);

#ifdef __cplusplus
}
#endif
#endif
