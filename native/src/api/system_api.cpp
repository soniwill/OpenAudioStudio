#include <oae_audio.h>

#include "arguments.h"
#include "runtime.h"

#include <cstdint>
#include <memory>

namespace {

using oae::api_call;

constexpr std::uint32_t apiVersion = 1;
constexpr std::uint32_t maxOutputChannels = 8;
constexpr std::uint32_t defaultSampleRate = 48'000;
constexpr std::uint32_t defaultChannels = 2;
constexpr std::uint32_t defaultVoices = 32;
constexpr std::uint32_t initFlags = OAE_INIT_NO_DEVICE;
bool valid_settings(const oae_init_settings& settings) noexcept
{
    return settings.sample_rate != 0 && settings.channels != 0
        && settings.channels <= maxOutputChannels && settings.max_physical_voices != 0
        && (settings.flags & ~initFlags) == 0;
}

}

extern "C" {

uint32_t OAE_CALL oae_get_version(void)
{
    return apiVersion;
}

const char* OAE_CALL oae_result_string(oae_result result)
{
    switch (result) {
        case OAE_OK: return "OK";
        case OAE_ERR_INVALID_PARAM: return "Invalid parameter";
        case OAE_ERR_INVALID_HANDLE: return "Invalid handle";
        case OAE_ERR_NOT_INITIALIZED: return "Not initialized";
        case OAE_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case OAE_ERR_OUT_OF_MEMORY: return "Out of memory";
        case OAE_ERR_FILE_NOT_FOUND: return "File not found";
        case OAE_ERR_FORMAT: return "Invalid format";
        case OAE_ERR_DEVICE: return "Audio device or render error";
        case OAE_ERR_NOT_FOUND: return "Not found";
        case OAE_ERR_ALREADY_EXISTS: return "Already exists";
        case OAE_ERR_INVALID_STATE: return "Invalid state";
        case OAE_ERR_INTERNAL: return "Internal error";
        default: return "Unknown result";
    }
}

oae_result OAE_CALL oae_init_settings_default(oae_init_settings* settings)
{
    return api_call([&] {
        if (!settings) {
            return OAE_ERR_INVALID_PARAM;
        }
        *settings = {defaultSampleRate, defaultChannels, defaultVoices, OAE_INIT_DEFAULT};
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_create(oae_system* out_system)
{
    return api_call([&] {
        if (!out_system) {
            return OAE_ERR_INVALID_PARAM;
        }
        *out_system = 0;
        const auto handle = oae::runtime.systems.insert(std::make_unique<oae::System>());
        if (handle == 0) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        auto* registered = oae::runtime.systems.get(handle);
        if (registered) {
            registered->set_handle(handle);
        }
        *out_system = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_init(oae_system system, const oae_init_settings* settings)
{
    return api_call([&] {
        if (!settings || !valid_settings(*settings)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* instance = oae::runtime.systems.get(system);
        return instance ? oae::runtime.init_system(*instance, *settings) : OAE_ERR_INVALID_HANDLE;
    });
}

oae_result OAE_CALL oae_system_update(oae_system system)
{
    return api_call([&] {
        auto* instance = oae::runtime.systems.get(system);
        if (!instance) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const auto result = instance->update();
        if (result != OAE_OK) {
            return result;
        }
        instance->update_instances(instance->clock(), instance->sample_rate());
        instance->dispatch_callbacks();
        return oae::runtime.update_voices(*instance);
    });
}

oae_result OAE_CALL oae_system_render(oae_system system, float* out, uint32_t frames)
{
    return api_call([&] {
        if (!out) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* instance = oae::runtime.systems.get(system);
        return instance ? instance->render(out, frames) : OAE_ERR_INVALID_HANDLE;
    });
}

oae_result OAE_CALL oae_system_release(oae_system system)
{
    return api_call([&] {
        const auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (owner->in_callback()) {
            return OAE_ERR_INVALID_STATE;
        }
        oae::runtime.banks.for_each([&](oae::Bank& bank) {
            if (bank.owner() == owner) {
                bank.wait_async_load();
            }
        });
        oae::runtime.banks.erase_if([&](const oae::Bank& bank) { return bank.owner() == owner; });
        oae::runtime.destroy_bus_tree(owner->master_bus());
        oae::runtime.sounds.erase_if([&](const oae::Sound& sound) { return sound.owner == owner; });
        oae::runtime.event_instances.erase_if([&](const oae::EventInstance& inst) {
            return inst.description() && inst.description()->system() == system;
        });
        oae::runtime.event_descriptions.erase_if([&](const oae::EventDescription& desc) {
            return desc.system() == system;
        });
        return oae::runtime.systems.erase(system) ? OAE_OK : OAE_ERR_INTERNAL;
    });
}

oae_result OAE_CALL oae_system_get_dsp_clock(oae_system system, uint64_t* out_frames)
{
    return api_call([&] {
        if (!out_frames) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* instance = oae::runtime.systems.get(system);
        if (!instance) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const auto result = instance->update();
        if (result == OAE_OK) {
            *out_frames = instance->clock();
        }
        return result;
    });
}

oae_result OAE_CALL oae_get_struct_sizes(oae_struct_sizes* out_sizes)
{
    return api_call([&] {
        if (!out_sizes) {
            return OAE_ERR_INVALID_PARAM;
        }
        out_sizes->init_settings = sizeof(oae_init_settings);
        out_sizes->meter = sizeof(oae_meter);
        out_sizes->timeline_marker_properties = sizeof(oae_timeline_marker_properties);
        out_sizes->timeline_beat_properties = sizeof(oae_timeline_beat_properties);
        out_sizes->event_sound_properties = sizeof(oae_event_sound_properties);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_set_listener_3d_attributes(oae_system system, const oae_3d_attributes* attributes)
{
    return api_call([&] {
        if (!attributes || !oae::valid_3d_attributes(*attributes)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* instance = oae::runtime.systems.get(system);
        if (!instance) {
            return OAE_ERR_INVALID_HANDLE;
        }
        instance->set_listener_3d_attributes(*attributes);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_get_listener_3d_attributes(oae_system system, oae_3d_attributes* out_attributes)
{
    return api_call([&] {
        if (!out_attributes) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* instance = oae::runtime.systems.get(system);
        if (!instance) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_attributes = instance->listener_3d_attributes();
        return OAE_OK;
    });
}

}
