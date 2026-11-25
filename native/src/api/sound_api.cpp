#include "runtime.h"
#include "arguments.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>

namespace {

constexpr uint32_t soundFlags = OAE_SOUND_LOOP | OAE_SOUND_STREAM;

template <typename Loader>
oae_result create_sound(oae_system system, uint32_t flags, oae_sound& handle, Loader load)
{
    auto* owner = oae::runtime.systems.get(system);
    if (!owner) {
        return OAE_ERR_INVALID_HANDLE;
    }
    if (owner->update() != OAE_OK) {
        return OAE_ERR_NOT_INITIALIZED;
    }

    auto data = std::make_shared<oae::SoundData>();
    data->loop = (flags & OAE_SOUND_LOOP) != 0;
    const auto result = load(*data);
    if (result != OAE_OK) {
        return result;
    }

    const auto created = oae::runtime.sounds.insert(std::make_unique<oae::Sound>(oae::Sound{owner, std::move(data)}));
    if (created == 0) {
        return OAE_ERR_OUT_OF_MEMORY;
    }
    handle = created;
    return OAE_OK;
}

enum class Start { absolute, delay };

oae_result play_sound(oae_system system, oae_sound sound, oae_bus bus, bool paused,
    Start mode, uint64_t requested, oae_channel& output)
{
    auto* owner = oae::runtime.systems.get(system);
    const auto* source = oae::runtime.sounds.get(sound);
    if (!owner || !source || source->owner != owner) {
        return OAE_ERR_INVALID_HANDLE;
    }
    auto* target = oae::runtime.resolve_bus(*owner, bus);
    if (!target) {
        return OAE_ERR_INVALID_HANDLE;
    }

    const auto now = owner->clock();
    auto start = std::max(now, requested);
    if (mode == Start::delay) {
        const auto frames = oae::milliseconds_to_frames(static_cast<uint32_t>(requested), owner->sample_rate());
        if (frames > (std::numeric_limits<uint64_t>::max)() - now) {
            return OAE_ERR_INVALID_PARAM;
        }
        start = now + frames;
    }
    if (start - now > uint64_t{owner->sample_rate()} * 3600u) {
        return OAE_ERR_INVALID_PARAM;
    }

    auto channel = std::make_unique<oae::Channel>(*target, source->data, oae::runtime.next_channel_order(), start);
    auto* instance = channel.get();
    auto result = instance->init();
    if (result != OAE_OK) {
        return result;
    }
    const auto created = oae::runtime.channels.insert(std::move(channel));
    if (created == 0) {
        return OAE_ERR_OUT_OF_MEMORY;
    }
    if (!paused) {
        result = instance->set_paused(false);
        if (result == OAE_OK) {
            result = oae::runtime.try_make_physical(*instance);
        }
        if (result != OAE_OK) {
            oae::runtime.channels.erase(created);
            return result;
        }
    }
    output = created;
    return OAE_OK;
}

template <typename Operation>
oae_result with_channel(oae_channel handle, Operation operation) noexcept
{
    return oae::api_call([&] {
        auto* channel = oae::runtime.channels.get(handle);
        return channel ? operation(*channel) : OAE_ERR_INVALID_HANDLE;
    });
}

template <typename T, typename Getter>
oae_result get_channel_value(oae_channel handle, T* output, Getter getter) noexcept
{
    if (!output) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(handle, [&](const oae::Channel& channel) {
        *output = static_cast<T>((channel.*getter)());
        return OAE_OK;
    });
}

}

extern "C" {

oae_result OAE_CALL oae_sound_create_from_file(oae_system system, const char* path, uint32_t flags, oae_sound* out_sound)
{
    return oae::api_call([&] {
        if (!out_sound || (flags & ~soundFlags) != 0 || !oae::valid_utf8_argument(path)) {
            return OAE_ERR_INVALID_PARAM;
        }
        return create_sound(system, flags, *out_sound, [&](oae::SoundData& data) {
            const auto absolute = std::filesystem::absolute(std::filesystem::u8path(path));
            return oae::load_file(absolute, (flags & OAE_SOUND_STREAM) != 0, data);
        });
    });
}

oae_result OAE_CALL oae_sound_create_from_memory(oae_system system, const void* bytes, size_t size, uint32_t flags, oae_sound* out_sound)
{
    return oae::api_call([&] {
        if (!out_sound || !bytes || size == 0 || (flags & ~uint32_t{OAE_SOUND_LOOP}) != 0) {
            return OAE_ERR_INVALID_PARAM;
        }
        return create_sound(system, flags, *out_sound, [&](oae::SoundData& data) {
            return oae::load_memory(bytes, size, data);
        });
    });
}

oae_result OAE_CALL oae_sound_release(oae_sound sound)
{
    return oae::api_call([&] {
        return oae::runtime.sounds.erase(sound) ? OAE_OK : OAE_ERR_INVALID_HANDLE;
    });
}

oae_result OAE_CALL oae_sound_get_length_ms(oae_sound sound, uint32_t* out_ms)
{
    return oae::api_call([&] {
        if (!out_ms) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* instance = oae::runtime.sounds.get(sound);
        if (!instance) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_ms = oae::frames_to_ms(instance->data->frames, instance->data->rate);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_play_sound(oae_system system, oae_sound sound, oae_bus bus, int paused, oae_channel* out_channel)
{
    return oae::api_call([&] {
        if (!out_channel || (paused != 0 && paused != 1)) {
            return OAE_ERR_INVALID_PARAM;
        }
        return play_sound(system, sound, bus, paused != 0, Start::absolute, 0, *out_channel);
    });
}

oae_result OAE_CALL oae_system_play_sound_at(oae_system system, oae_sound sound, oae_bus bus, uint64_t dsp_clock, oae_channel* out_channel)
{
    return oae::api_call([&] {
        if (!out_channel) {
            return OAE_ERR_INVALID_PARAM;
        }
        return play_sound(system, sound, bus, false, Start::absolute, dsp_clock, *out_channel);
    });
}

oae_result OAE_CALL oae_system_play_sound_delayed(oae_system system, oae_sound sound, oae_bus bus, uint32_t delay_ms, oae_channel* out_channel)
{
    return oae::api_call([&] {
        if (!out_channel || delay_ms > 3'600'000) {
            return OAE_ERR_INVALID_PARAM;
        }
        return play_sound(system, sound, bus, false, Start::delay, delay_ms, *out_channel);
    });
}

oae_result OAE_CALL oae_channel_stop(oae_channel channel)
{
    return with_channel(channel, [](oae::Channel& instance) { return instance.stop(); });
}

oae_result OAE_CALL oae_channel_set_paused(oae_channel channel, int paused)
{
    if (paused != 0 && paused != 1) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        const auto result = instance.set_paused(paused != 0);
        return result == OAE_OK && paused == 0 ? oae::runtime.try_make_physical(instance) : result;
    });
}

oae_result OAE_CALL oae_channel_get_paused(oae_channel channel, int* out_paused)
{
    return get_channel_value(channel, out_paused, &oae::Channel::paused);
}

oae_result OAE_CALL oae_channel_set_volume(oae_channel channel, float volume)
{
    if (!std::isfinite(volume) || volume < 0) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        instance.set_volume(volume);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_get_volume(oae_channel channel, float* out_volume)
{
    return get_channel_value(channel, out_volume, &oae::Channel::volume);
}

oae_result OAE_CALL oae_channel_set_pitch(oae_channel channel, float pitch)
{
    if (!std::isfinite(pitch) || pitch <= 0) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        return instance.set_pitch(pitch);
    });
}

oae_result OAE_CALL oae_channel_get_pitch(oae_channel channel, float* out_pitch)
{
    return get_channel_value(channel, out_pitch, &oae::Channel::pitch);
}

oae_result OAE_CALL oae_channel_is_playing(oae_channel channel, int* out_is_playing)
{
    return get_channel_value(channel, out_is_playing, &oae::Channel::playing);
}

oae_result OAE_CALL oae_channel_set_priority(oae_channel channel, int priority)
{
    if (priority < 0 || priority > 256) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        instance.set_priority(priority);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_get_priority(oae_channel channel, int* out_priority)
{
    return get_channel_value(channel, out_priority, &oae::Channel::priority);
}

oae_result OAE_CALL oae_channel_is_virtual(oae_channel channel, int* out_virtual)
{
    if (!out_virtual) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](const oae::Channel& instance) {
        *out_virtual = instance.physical() ? 0 : 1;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_system_get_channels_playing(oae_system system, int* total, int* real)
{
    return oae::api_call([&] {
        if (!total || !real) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* owner = oae::runtime.systems.get(system);
        if (!owner) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const auto status = owner->update();
        if (status != OAE_OK) {
            return status;
        }
        int playing = 0;
        int physical = 0;
        oae::runtime.channels.for_each([&](const oae::Channel& channel) {
            if (channel.owner() == owner && channel.playing()) {
                ++playing;
                if (channel.physical()) {
                    ++physical;
                }
            }
        });
        *total = playing;
        *real = physical;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_get_position_ms(oae_channel channel, uint32_t* out_ms)
{
    if (!out_ms) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](const oae::Channel& instance) { return instance.position_ms(*out_ms); });
}

oae_result OAE_CALL oae_channel_set_position_ms(oae_channel channel, uint32_t ms)
{
    return with_channel(channel, [&](oae::Channel& instance) { return instance.set_position_ms(ms); });
}

oae_result OAE_CALL oae_channel_set_pan(oae_channel channel, float pan)
{
    if (!oae::valid_pan(pan)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        instance.strip().set_pan(pan);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_get_pan(oae_channel channel, float* out_pan)
{
    if (!out_pan) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](const oae::Channel& instance) {
        *out_pan = instance.strip().pan();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_fade_to(oae_channel channel, float target, uint32_t duration_ms, int curve)
{
    if (!oae::valid_fade(target, curve)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](oae::Channel& instance) {
        const auto* owner = instance.owner();
        instance.strip().fade_to(target, oae::milliseconds_to_frames(duration_ms, owner->sample_rate()),
            static_cast<oae::FadeCurve>(curve), owner->clock());
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_get_fade_gain(oae_channel channel, float* out_gain)
{
    if (!out_gain) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_channel(channel, [&](const oae::Channel& instance) {
        *out_gain = instance.strip().fade_gain(instance.owner()->clock());
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_channel_stop_with_fade(oae_channel channel, uint32_t duration_ms)
{
    return with_channel(channel, [&](oae::Channel& instance) {
        return instance.stop_with_fade(oae::milliseconds_to_frames(duration_ms, instance.owner()->sample_rate()));
    });
}

}
