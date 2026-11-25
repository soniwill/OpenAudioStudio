#include "runtime.h"
#include "arguments.h"

#include <cmath>
#include <string_view>

namespace {

constexpr std::string_view masterPath = "bus:/";
constexpr std::string_view returnPrefix = "return:/";

bool valid_bus_name(std::string_view name) noexcept
{
    return !name.empty() && name != "." && name != ".." &&
        name.find_first_of("/\\") == std::string_view::npos;
}

bool valid_bus_path(std::string_view path) noexcept
{
    if (path.substr(0, masterPath.size()) == masterPath) {
        path.remove_prefix(masterPath.size());
        if (path.empty()) {
            return true;
        }
    } else if (path.substr(0, returnPrefix.size()) == returnPrefix) {
        path.remove_prefix(returnPrefix.size());
        if (path.empty()) {
            return false;
        }
    } else {
        return false;
    }
    for (;;) {
        const auto separator = path.find('/');
        if (!valid_bus_name(path.substr(0, separator))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        path.remove_prefix(separator + 1);
    }
}

oae_bus find_bus(const oae::System& owner, std::string_view path) noexcept
{
    return oae::runtime.buses.find_if([&](const oae::Bus& bus) {
        return bus.owner() == &owner && std::string_view(bus.path()) == path;
    });
}

template <typename Operation>
oae_result with_bus(oae_bus handle, Operation operation) noexcept
{
    return oae::api_call([&] {
        auto* bus = oae::runtime.buses.get(handle);
        return bus ? operation(*bus) : OAE_ERR_INVALID_HANDLE;
    });
}

template <typename T, typename Getter>
oae_result get_bus_value(oae_bus handle, T* output, Getter getter) noexcept
{
    if (!output) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(handle, [&](const oae::Bus& bus) {
        *output = static_cast<T>((bus.*getter)());
        return OAE_OK;
    });
}

}

extern "C" {

oae_result OAE_CALL oae_system_get_master_bus(oae_system system, oae_bus* out_bus)
{
    return oae::api_call([&] {
        if (!out_bus) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const auto handle = owner->master_bus();
        if (handle == 0) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        *out_bus = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_create(oae_system system, const char* name, oae_bus parent, oae_bus* out_bus)
{
    return oae::api_call([&] {
        if (!out_bus || !oae::valid_utf8_argument(name) || !valid_bus_name(name)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto* parentBus = oae::runtime.resolve_bus(*owner, parent);
        if (parent != 0 && !parentBus) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (owner->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        if (parentBus->depth() == oae::Bus::max_depth) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto path = parentBus->path();
        if (parentBus->depth() != 0) {
            path += '/';
        }
        path += name;
        if (find_bus(*owner, path) != 0) {
            return OAE_ERR_ALREADY_EXISTS;
        }
        auto bus = std::make_unique<oae::Bus>(*owner, parentBus, std::move(path));
        const auto result = bus->init();
        if (result != OAE_OK) {
            return result;
        }
        const auto handle = oae::runtime.buses.insert(std::move(bus));
        if (handle == 0) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        *out_bus = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_get_bus(oae_system system, const char* path, oae_bus* out_bus)
{
    return oae::api_call([&] {
        if (!out_bus || !oae::valid_utf8_argument(path) || !valid_bus_path(path)) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (owner->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        const auto handle = find_bus(*owner, path);
        if (handle == 0) {
            return OAE_ERR_NOT_FOUND;
        }
        *out_bus = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_set_volume(oae_bus bus, float volume)
{
    if (!std::isfinite(volume) || volume < 0.0f) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](oae::Bus& instance) {
        instance.set_volume(volume);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_volume(oae_bus bus, float* out_volume)
{
    return get_bus_value(bus, out_volume, &oae::Bus::volume);
}

oae_result OAE_CALL oae_bus_set_mute(oae_bus bus, int mute)
{
    if (mute != 0 && mute != 1) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](oae::Bus& instance) {
        instance.set_muted(mute != 0);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_mute(oae_bus bus, int* out_mute)
{
    return get_bus_value(bus, out_mute, &oae::Bus::muted);
}

oae_result OAE_CALL oae_bus_set_paused(oae_bus bus, int paused)
{
    if (paused != 0 && paused != 1) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](oae::Bus& instance) { return oae::runtime.set_bus_paused(instance, paused != 0); });
}

oae_result OAE_CALL oae_bus_get_paused(oae_bus bus, int* out_paused)
{
    return get_bus_value(bus, out_paused, &oae::Bus::paused);
}

oae_result OAE_CALL oae_bus_stop_all(oae_bus bus)
{
    return with_bus(bus, [](const oae::Bus& root) {
        auto result = OAE_OK;
        oae::runtime.channels.for_each([&](oae::Channel& channel) {
            if (channel.bus().within(root) && channel.stop() != OAE_OK) {
                result = OAE_ERR_INTERNAL;
            }
        });
        return result;
    });
}

oae_result OAE_CALL oae_bus_release(oae_bus bus)
{
    return with_bus(bus, [&](const oae::Bus& root) {
        if (root.depth() == 0) {
            return OAE_ERR_INVALID_STATE;
        }
        oae::runtime.destroy_bus_tree(bus);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_set_pan(oae_bus bus, float pan)
{
    if (!oae::valid_pan(pan)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](oae::Bus& instance) {
        instance.strip().set_pan(pan);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_pan(oae_bus bus, float* out_pan)
{
    if (!out_pan) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](const oae::Bus& instance) {
        *out_pan = instance.strip().pan();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_fade_to(oae_bus bus, float target, uint32_t duration_ms, int curve)
{
    if (!oae::valid_fade(target, curve)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](oae::Bus& instance) {
        const auto* owner = instance.owner();
        instance.strip().fade_to(target, oae::milliseconds_to_frames(duration_ms, owner->sample_rate()),
            static_cast<oae::FadeCurve>(curve), owner->clock());
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_fade_gain(oae_bus bus, float* out_gain)
{
    if (!out_gain) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_bus(bus, [&](const oae::Bus& instance) {
        *out_gain = instance.strip().fade_gain(instance.owner()->clock());
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_create_return(oae_system system, const char* name, oae_bus parent, oae_bus* out_bus)
{
    return oae::api_call([&] {
        if (!out_bus || !oae::valid_utf8_argument(name) || !valid_bus_name(name)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto* parentBus = oae::runtime.resolve_bus(*owner, parent);
        if (parent != 0 && !parentBus) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (parent != 0 && !parentBus->is_return()) {
            return OAE_ERR_INVALID_PARAM;
        }
        if (owner->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        if (parentBus->depth() == oae::Bus::max_depth) {
            return OAE_ERR_INVALID_PARAM;
        }
        std::string path;
        if (parent == 0) {
            path = std::string(returnPrefix) + name;
        } else {
            path = parentBus->path() + '/' + name;
        }
        if (find_bus(*owner, path) != 0) {
            return OAE_ERR_ALREADY_EXISTS;
        }
        auto bus = std::make_unique<oae::Bus>(*owner, parentBus, std::move(path));
        const auto result = bus->init();
        if (result != OAE_OK) {
            return result;
        }
        const auto handle = oae::runtime.buses.insert(std::move(bus));
        if (handle == 0) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        *out_bus = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_add_send(oae_bus source, oae_bus target, float level, int position)
{
    if (!std::isfinite(level) || level < 0.0f ||
        (position != OAE_EFFECT_PRE_FADER && position != OAE_EFFECT_POST_FADER)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return oae::api_call([&] {
        auto* srcBus = oae::runtime.buses.get(source);
        auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        return srcBus->add_send(*dstBus, level, position);
    });
}

oae_result OAE_CALL oae_bus_remove_send(oae_bus source, oae_bus target)
{
    return oae::api_call([&] {
        auto* srcBus = oae::runtime.buses.get(source);
        auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        return srcBus->remove_send(*dstBus);
    });
}

oae_result OAE_CALL oae_bus_set_send_level(oae_bus source, oae_bus target, float level)
{
    if (!std::isfinite(level) || level < 0.0f) {
        return OAE_ERR_INVALID_PARAM;
    }
    return oae::api_call([&] {
        auto* srcBus = oae::runtime.buses.get(source);
        auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        const int slot = srcBus->dsp().find_send(dstBus);
        if (slot < 0) {
            return OAE_ERR_NOT_FOUND;
        }
        srcBus->dsp().set_send_level(static_cast<std::size_t>(slot), level);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_send_level(oae_bus source, oae_bus target, float* out_level)
{
    if (!out_level) {
        return OAE_ERR_INVALID_PARAM;
    }
    return oae::api_call([&] {
        const auto* srcBus = oae::runtime.buses.get(source);
        const auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        const int slot = srcBus->dsp().find_send(dstBus);
        if (slot < 0) {
            return OAE_ERR_NOT_FOUND;
        }
        *out_level = srcBus->dsp().send_level(static_cast<std::size_t>(slot));
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_set_send_mute(oae_bus source, oae_bus target, int mute)
{
    if (mute != 0 && mute != 1) {
        return OAE_ERR_INVALID_PARAM;
    }
    return oae::api_call([&] {
        auto* srcBus = oae::runtime.buses.get(source);
        auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        const int slot = srcBus->dsp().find_send(dstBus);
        if (slot < 0) {
            return OAE_ERR_NOT_FOUND;
        }
        srcBus->dsp().set_send_muted(static_cast<std::size_t>(slot), mute != 0);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_bus_get_send_mute(oae_bus source, oae_bus target, int* out_mute)
{
    if (!out_mute) {
        return OAE_ERR_INVALID_PARAM;
    }
    return oae::api_call([&] {
        const auto* srcBus = oae::runtime.buses.get(source);
        const auto* dstBus = oae::runtime.buses.get(target);
        if (!srcBus || !dstBus || srcBus->owner() != dstBus->owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (srcBus->owner()->update() != OAE_OK) {
            return OAE_ERR_NOT_INITIALIZED;
        }
        const int slot = srcBus->dsp().find_send(dstBus);
        if (slot < 0) {
            return OAE_ERR_NOT_FOUND;
        }
        *out_mute = srcBus->dsp().send_muted(static_cast<std::size_t>(slot)) ? 1 : 0;
        return OAE_OK;
    });
}

}
