#include "StudioEngine.h"
#include <cmath>

namespace oae::studio {

namespace {

inline float db_to_linear(float db)
{
    if (db <= -80.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

} // namespace

StudioEngine::StudioEngine() = default;

StudioEngine::~StudioEngine()
{
    shutdown();
}

oae_result StudioEngine::init_headless()
{
    if (initialized_) return OAE_ERR_ALREADY_INITIALIZED;

    auto res = oae_system_create(&system_);
    if (res != OAE_OK) return res;

    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags |= OAE_INIT_NO_DEVICE;

    res = oae_system_init(system_, &settings);
    if (res != OAE_OK) {
        oae_system_release(system_);
        system_ = 0;
        return res;
    }

    initialized_ = true;
    return OAE_OK;
}

oae_result StudioEngine::init_device()
{
    if (initialized_) return OAE_ERR_ALREADY_INITIALIZED;

    auto res = oae_system_create(&system_);
    if (res != OAE_OK) return res;

    oae_init_settings settings{};
    oae_init_settings_default(&settings);
    settings.flags = OAE_INIT_DEFAULT;

    res = oae_system_init(system_, &settings);
    if (res != OAE_OK) {
        oae_system_release(system_);
        system_ = 0;
        return res;
    }

    initialized_ = true;
    return OAE_OK;
}

void StudioEngine::shutdown()
{
    if (initialized_ && system_ != 0) {
        oae_system_release(system_);
        system_ = 0;
        initialized_ = false;
    }
}

oae_result StudioEngine::load_bank_file(const std::string& path, oae_bank* out_bank)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_load_bank_file(system_, path.c_str(), out_bank);
}

oae_result StudioEngine::load_bank_memory(const void* data, size_t size, oae_bank* out_bank)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_load_bank_memory(system_, data, size, out_bank);
}

oae_result StudioEngine::unload_bank(oae_bank bank)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_unload_bank(system_, bank);
}

oae_result StudioEngine::play_one_shot(const std::string& event_path)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_play_one_shot(system_, event_path.c_str());
}

oae_result StudioEngine::create_event_instance(const std::string& event_path, oae_event_instance* out_instance)
{
    if (!initialized_ || !out_instance) return OAE_ERR_NOT_INITIALIZED;
    oae_event_description desc = 0;
    auto res = oae_studio_get_event(system_, event_path.c_str(), &desc);
    if (res != OAE_OK) return res;
    return oae_event_description_create_instance(desc, out_instance);
}

oae_result StudioEngine::get_bus(const std::string& path, oae_bus* out_bus)
{
    if (!initialized_ || !out_bus) return OAE_ERR_NOT_INITIALIZED;
    return oae_system_get_bus(system_, path.c_str(), out_bus);
}

oae_result StudioEngine::set_bus_volume_db(oae_bus bus, float volume_db)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_bus_set_volume(bus, db_to_linear(volume_db));
}

oae_result StudioEngine::set_bus_pan(oae_bus bus, float pan)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_bus_set_pan(bus, pan);
}

oae_result StudioEngine::set_bus_mute(oae_bus bus, bool mute)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_bus_set_mute(bus, mute ? 1 : 0);
}

oae_result StudioEngine::get_bus_meter(oae_bus bus, oae_meter* out_meter)
{
    if (!initialized_ || !out_meter) return OAE_ERR_NOT_INITIALIZED;
    return oae_bus_get_meter(bus, out_meter);
}

oae_result StudioEngine::get_vca(const std::string& path, oae_vca* out_vca)
{
    if (!initialized_ || !out_vca) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_get_vca(system_, path.c_str(), out_vca);
}

oae_result StudioEngine::set_vca_volume_db(oae_vca vca, float volume_db)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_vca_set_volume(vca, db_to_linear(volume_db));
}

oae_result StudioEngine::set_global_parameter(const std::string& name, float value)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_studio_set_global_parameter_by_name(system_, name.c_str(), value, 0);
}

oae_result StudioEngine::set_instance_parameter(oae_event_instance instance, const std::string& name, float value)
{
    if (!initialized_ || instance == 0) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_set_parameter_by_name(instance, name.c_str(), value, 0);
}

oae_result StudioEngine::get_instance_timeline_position(oae_event_instance instance, int32_t* out_ms)
{
    if (!initialized_ || instance == 0 || !out_ms) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_get_timeline_position_ms(instance, out_ms);
}

oae_result StudioEngine::set_instance_timeline_position(oae_event_instance instance, int32_t ms)
{
    if (!initialized_ || instance == 0) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_set_timeline_position_ms(instance, ms);
}

oae_result StudioEngine::get_instance_playback_state(oae_event_instance instance, int* out_state)
{
    if (!initialized_ || instance == 0 || !out_state) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_get_playback_state(instance, out_state);
}

oae_result StudioEngine::get_instance_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus)
{
    if (!initialized_ || instance == 0 || !out_bus) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_get_track_bus(instance, track_index, out_bus);
}

oae_result StudioEngine::key_off(oae_event_instance instance)
{
    if (!initialized_ || instance == 0) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_key_off(instance);
}

oae_result StudioEngine::stop_instance(oae_event_instance instance, bool allow_fadeout)
{
    if (!initialized_ || instance == 0) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_stop(instance, allow_fadeout ? OAE_STOP_ALLOW_FADEOUT : OAE_STOP_IMMEDIATE);
}

oae_result StudioEngine::release_instance(oae_event_instance instance)
{
    if (!initialized_ || instance == 0) return OAE_ERR_NOT_INITIALIZED;
    return oae_event_instance_release(instance);
}

oae_result StudioEngine::update()
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_system_update(system_);
}

oae_result StudioEngine::render(float* out_buffer, uint32_t frames)
{
    if (!initialized_) return OAE_ERR_NOT_INITIALIZED;
    return oae_system_render(system_, out_buffer, frames);
}

} // namespace oae::studio
