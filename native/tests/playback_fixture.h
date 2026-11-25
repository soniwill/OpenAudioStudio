#pragma once

#include "heap_guard.h"

#include <oae_audio.h>
#include <doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace oae::test {

class SystemFixture {
public:
    explicit SystemFixture(std::uint32_t maxVoices = 32)
    {
        REQUIRE(oae_system_create(&handle) == OAE_OK);
        oae_init_settings settings{};
        oae_init_settings_default(&settings);
        settings.flags = OAE_INIT_NO_DEVICE;
        settings.max_physical_voices = maxVoices;
        if (oae_system_init(handle, &settings) != OAE_OK) {
            oae_system_release(handle);
            FAIL("Could not initialize a headless system");
        }
    }

    ~SystemFixture() { oae_system_release(handle); }

    oae_system handle = 0;
};

inline std::string fixture_path(const char* name)
{
    return (std::filesystem::u8path(OAE_TEST_FIXTURES) / name).u8string();
}

inline std::vector<char> fixture_bytes(const char* name)
{
    std::ifstream input(std::filesystem::u8path(fixture_path(name)), std::ios::binary);
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

inline std::vector<float> render(oae_system system, std::uint32_t frames)
{
    std::vector<float> output(static_cast<std::size_t>(frames) * 2, -2.0f);
    oae_result result = OAE_ERR_INTERNAL;
    std::size_t operations = 0;
    {
        oae::test::HeapGuard guard;
        result = oae_system_render(system, output.data(), frames);
        operations = guard.operations();
    }
    REQUIRE(result == OAE_OK);
    CHECK(operations == 0);
    return output;
}

inline double rms(const std::vector<float>& samples)
{
    double sum = 0;
    for (const auto sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

inline oae_sound load_wav(oae_system system, std::uint32_t flags = OAE_SOUND_DEFAULT)
{
    const auto bytes = fixture_bytes("tone.wav");
    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_memory(system, bytes.data(), bytes.size(), flags, &sound) == OAE_OK);
    return sound;
}

inline oae_channel play(oae_system system, oae_sound sound, int paused = 0, oae_bus bus = 0)
{
    oae_channel channel = 0;
    REQUIRE(oae_system_play_sound(system, sound, bus, paused, &channel) == OAE_OK);
    return channel;
}

}
