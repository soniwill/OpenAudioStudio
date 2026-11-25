#pragma once

#include <oae_audio.h>

#include <string>
#include <vector>

namespace oae::studio {

class StudioEngine {
public:
    StudioEngine();
    ~StudioEngine();

    StudioEngine(const StudioEngine&) = delete;
    StudioEngine& operator=(const StudioEngine&) = delete;

    oae_result init_headless();
    oae_result init_device();
    void shutdown();

    oae_result load_bank_file(const std::string& path, oae_bank* out_bank);
    oae_result load_bank_memory(const void* data, size_t size, oae_bank* out_bank);
    oae_result unload_bank(oae_bank bank);

    oae_result play_one_shot(const std::string& event_path);
    oae_result create_event_instance(const std::string& event_path, oae_event_instance* out_instance);

    oae_result get_bus(const std::string& path, oae_bus* out_bus);
    oae_result set_bus_volume_db(oae_bus bus, float volume_db);
    oae_result set_bus_pan(oae_bus bus, float pan);
    oae_result set_bus_mute(oae_bus bus, bool mute);
    oae_result get_bus_meter(oae_bus bus, oae_meter* out_meter);

    oae_result get_vca(const std::string& path, oae_vca* out_vca);
    oae_result set_vca_volume_db(oae_vca vca, float volume_db);

    oae_result set_global_parameter(const std::string& name, float value);
    oae_result set_instance_parameter(oae_event_instance instance, const std::string& name, float value);
    oae_result get_instance_timeline_position(oae_event_instance instance, int32_t* out_ms);
    oae_result set_instance_timeline_position(oae_event_instance instance, int32_t ms);
    oae_result get_instance_playback_state(oae_event_instance instance, int* out_state);
    /** Retrieves the native bus handle for an event instance track. */
    oae_result get_instance_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus);
    oae_result key_off(oae_event_instance instance);
    oae_result stop_instance(oae_event_instance instance, bool allow_fadeout);
    oae_result release_instance(oae_event_instance instance);

    oae_result update();
    oae_result render(float* out_buffer, uint32_t frames);

    oae_system system_handle() const noexcept { return system_; }
    bool is_initialized() const noexcept { return initialized_; }

private:
    oae_system system_ = 0;
    bool initialized_ = false;
};

} // namespace oae::studio
