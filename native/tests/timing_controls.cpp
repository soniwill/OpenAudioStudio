#include "playback_fixture.h"
#include "../src/common/strip_math.h"

#include <cstdint>
#include <vector>

using namespace oae::test;

namespace {

oae_sound constant_sound(oae_system system)
{
    constexpr std::uint32_t bytes = 48'000;
    std::vector<std::uint8_t> wav{
        'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
        0x80, 0xBB, 0, 0, 0x00, 0x77, 0x01, 0, 2, 0, 16, 0,
        'd', 'a', 't', 'a', 0, 0, 0, 0
    };
    for (std::uint32_t i = 0; i < 4; ++i) {
        wav[4 + i] = static_cast<std::uint8_t>((bytes + 36) >> (i * 8));
        wav[40 + i] = static_cast<std::uint8_t>(bytes >> (i * 8));
    }
    wav.resize(44 + bytes);
    for (std::size_t i = 45; i < wav.size(); i += 2) {
        wav[i] = 0x20;
    }

    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), OAE_SOUND_LOOP, &sound) == OAE_OK);
    return sound;
}

std::uint64_t clock_of(oae_system system)
{
    std::uint64_t clock = 0;
    REQUIRE(oae_system_get_dsp_clock(system, &clock) == OAE_OK);
    return clock;
}

std::size_t first_audible(const std::vector<float>& samples, std::size_t channel)
{
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        if (std::fabs(samples[frame * 2 + channel]) > 0.01f) {
            return frame;
        }
    }
    return samples.size() / 2;
}

}

TEST_CASE("Channel and bus pan apply hard silence and smooth changes over 128 frames")
{
    SystemFixture system;
    const auto sound = constant_sound(system.handle);
    oae_bus bus = 0;
    REQUIRE(oae_bus_create(system.handle, "sfx", 0, &bus) == OAE_OK);
    const auto channel = play(system.handle, sound, 0, bus);
    REQUIRE(oae_channel_set_pan(channel, -1.0f) == OAE_OK);
    float pan = 0;
    REQUIRE(oae_channel_get_pan(channel, &pan) == OAE_OK);
    CHECK(pan == -1.0f);
    const auto initial = render(system.handle, 256);
    CHECK(initial[200] == doctest::Approx(0.25 * oae::sqrt2));
    CHECK(first_audible(initial, 1) == 256);
    for (std::size_t i = 1; i < initial.size(); i += 2) {
        CHECK(initial[i] == 0.0f);
    }

    REQUIRE(oae_channel_set_pan(channel, 0.0f) == OAE_OK);
    const auto ramp = render(system.handle, 128);
    CHECK(ramp[1] > 0.0f);
    CHECK(ramp[1] < 0.01f);
    CHECK(ramp[127 * 2] == ramp[127 * 2 + 1]);
    CHECK(ramp[127 * 2] == 0.25f);

    REQUIRE(oae_bus_set_pan(bus, 1.0f) == OAE_OK);
    REQUIRE(oae_bus_get_pan(bus, &pan) == OAE_OK);
    CHECK(pan == 1.0f);
    const auto busRamp = render(system.handle, 128);
    CHECK(busRamp[0] > 0.24f);
    CHECK(busRamp[127 * 2] == 0.0f);
    CHECK(busRamp[127 * 2 + 1] == doctest::Approx(0.25 * oae::sqrt2));
    CHECK(oae_channel_set_pan(0, 1.1f) == OAE_ERR_INVALID_PARAM);
}

TEST_CASE("Bus gain ramps and an independent fade follows the DSP clock")
{
    SystemFixture system;
    oae_bus bus = 0;
    REQUIRE(oae_bus_create(system.handle, "bus", 0, &bus) == OAE_OK);
    REQUIRE(oae_bus_set_volume(bus, 0.5f) == OAE_OK);
    const auto sound = constant_sound(system.handle);
    play(system.handle, sound, 0, bus);
    CHECK(render(system.handle, 64)[40] == 0.125f);

    REQUIRE(oae_bus_set_volume(bus, 0.0f) == OAE_OK);
    const auto ramp = render(system.handle, 128);
    CHECK(ramp[0] > 0.12f);
    CHECK(ramp[127 * 2] == 0.0f);
    REQUIRE(oae_bus_set_volume(bus, 1.0f) == OAE_OK);
    render(system.handle, 128);
    REQUIRE(oae_bus_fade_to(bus, 0.0f, 500, OAE_FADE_LINEAR) == OAE_OK);
    render(system.handle, 12000);
    float gain = 0;
    REQUIRE(oae_bus_get_fade_gain(bus, &gain) == OAE_OK);
    CHECK(gain == 0.5f);
    CHECK(rms(render(system.handle, 240)) == doctest::Approx(0.125).epsilon(0.1));
    render(system.handle, 12000);
    CHECK(rms(render(system.handle, 128)) == 0.0);
    REQUIRE(oae_bus_get_volume(bus, &gain) == OAE_OK);
    CHECK(gain == 1.0f);
}

TEST_CASE("Stop with fade uses a linear ramp and frees the handle after update")
{
    SystemFixture system;
    const auto channel = play(system.handle, constant_sound(system.handle));
    render(system.handle, 16);
    REQUIRE(oae_channel_stop_with_fade(channel, 10) == OAE_OK);
    const auto firstHalf = render(system.handle, 240);
    CHECK(firstHalf.front() == 0.25f);
    float gain = 0;
    REQUIRE(oae_channel_get_fade_gain(channel, &gain) == OAE_OK);
    CHECK(gain == 0.5f);
    const auto lastHalf = render(system.handle, 240);
    CHECK(lastHalf.front() == 0.125f);
    CHECK(rms(render(system.handle, 128)) == 0.0);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(oae_channel_get_fade_gain(channel, &gain) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_channel_fade_to(0, -1, 1, OAE_FADE_LINEAR) == OAE_ERR_INVALID_PARAM);
}

TEST_CASE("Scheduled sounds start at exact frame offsets after silence")
{
    SystemFixture system;
    CHECK(rms(render(system.handle, 341)) == 0.0);
    const auto now = clock_of(system.handle);
    REQUIRE(now == 341);
    const auto sound = constant_sound(system.handle);
    oae_channel a = 0;
    oae_channel b = 0;
    constexpr std::uint64_t first = 137;
    constexpr std::uint64_t second = first + 5760;
    REQUIRE(oae_system_play_sound_at(system.handle, sound, 0, now + first, &a) == OAE_OK);
    REQUIRE(oae_system_play_sound_at(system.handle, sound, 0, now + second, &b) == OAE_OK);
    REQUIRE(oae_channel_set_pan(a, -1.0f) == OAE_OK);
    REQUIRE(oae_channel_set_pan(b, 1.0f) == OAE_OK);
    const auto samples = render(system.handle, 6250);
    const auto left = first_audible(samples, 0);
    const auto right = first_audible(samples, 1);
    CHECK(left == first);
    CHECK(right == second);
    CHECK(right - left == 5760);
    CHECK(clock_of(system.handle) == now + 6250);
}

TEST_CASE("Past schedules start immediately and a virtual schedule keeps its start")
{
    SystemFixture system(1);
    const auto sound = constant_sound(system.handle);
    CHECK(rms(render(system.handle, 64)) == 0.0);
    oae_channel a = 0;
    oae_channel b = 0;
    REQUIRE(oae_system_play_sound_at(system.handle, sound, 0, 0, &a) == OAE_OK);
    REQUIRE(oae_channel_set_pan(a, -1.0f) == OAE_OK);
    REQUIRE(oae_system_play_sound_delayed(system.handle, sound, 0, 10, &b) == OAE_OK);
    CHECK(first_audible(render(system.handle, 64), 0) == 0);
    int virtualVoice = 0;
    REQUIRE(oae_channel_is_virtual(b, &virtualVoice) == OAE_OK);
    CHECK(virtualVoice == 1);
    std::uint32_t position = 1;
    REQUIRE(oae_channel_get_position_ms(b, &position) == OAE_OK);
    CHECK(position == 0);

    REQUIRE(oae_channel_stop(a) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto samples = render(system.handle, 512);
    CHECK(first_audible(samples, 0) == 416);
    CHECK(oae_system_play_sound_delayed(system.handle, sound, 0, 3'600'001, &a) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_play_sound_at(system.handle, sound, 0, clock_of(system.handle) + 172'800'001, &a) == OAE_ERR_INVALID_PARAM);
}

TEST_CASE("Unity-rate channels can enable pitch after using the no-latency path")
{
    SystemFixture system;
    const auto channel = play(system.handle, constant_sound(system.handle));
    render(system.handle, 480);
    std::uint32_t before = 0;
    REQUIRE(oae_channel_get_position_ms(channel, &before) == OAE_OK);
    CHECK(before == 10);
    REQUIRE(oae_channel_set_pitch(channel, 2.0f) == OAE_OK);
    render(system.handle, 4800);
    std::uint32_t after = 0;
    REQUIRE(oae_channel_get_position_ms(channel, &after) == OAE_OK);
    CHECK(after >= 205);
    CHECK(after <= 215);
}

TEST_CASE("Stop fade that ends before a scheduled start never starts the source")
{
    SystemFixture system;
    const auto sound = constant_sound(system.handle);
    oae_channel channel = 0;
    REQUIRE(oae_system_play_sound_at(system.handle, sound, 0, 200, &channel) == OAE_OK);
    REQUIRE(oae_channel_stop_with_fade(channel, 2) == OAE_OK);
    CHECK(rms(render(system.handle, 256)) == 0.0);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    int playing = 0;
    CHECK(oae_channel_is_playing(channel, &playing) == OAE_ERR_INVALID_HANDLE);
}
