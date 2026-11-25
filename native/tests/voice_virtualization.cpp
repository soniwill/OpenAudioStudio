#include "playback_fixture.h"

#include <array>

using namespace oae::test;

namespace {

int virtual_state(oae_channel channel)
{
    int state = -1;
    REQUIRE(oae_channel_is_virtual(channel, &state) == OAE_OK);
    return state;
}

std::uint32_t position(oae_channel channel)
{
    std::uint32_t ms = 0;
    REQUIRE(oae_channel_get_position_ms(channel, &ms) == OAE_OK);
    return ms;
}

void counts(oae_system system, int expectedTotal, int expectedReal)
{
    int total = -1;
    int real = -1;
    REQUIRE(oae_system_get_channels_playing(system, &total, &real) == OAE_OK);
    CHECK(total == expectedTotal);
    CHECK(real == expectedReal);
}

}

TEST_CASE("Voice cap and ranking use priority effective bus gain and creation order")
{
    SystemFixture system(4);
    oae_bus attenuated = 0;
    oae_bus loud = 0;
    REQUIRE(oae_bus_create(system.handle, "attenuated", 0, &attenuated) == OAE_OK);
    REQUIRE(oae_bus_create(system.handle, "loud", 0, &loud) == OAE_OK);
    REQUIRE(oae_bus_set_volume(attenuated, 0.125f) == OAE_OK);
    REQUIRE(oae_bus_set_volume(loud, 0.75f) == OAE_OK);

    const auto sound = load_wav(system.handle, OAE_SOUND_LOOP);
    std::array<oae_channel, 10> voices{};
    for (std::size_t i = 0; i < voices.size(); ++i) {
        const oae_bus bus = i == 7 ? loud : i == 9 ? attenuated : 0;
        voices[i] = play(system.handle, sound, 0, bus);
        CHECK(virtual_state(voices[i]) == (i < 4 ? 0 : 1));
    }
    counts(system.handle, 10, 4);
    int priority = -1;
    REQUIRE(oae_channel_get_priority(voices[8], &priority) == OAE_OK);
    CHECK(priority == 128);
    CHECK(oae_channel_set_priority(0, -1) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_channel_set_priority(voices[8], 257) == OAE_ERR_INVALID_PARAM);

    REQUIRE(oae_channel_set_priority(voices[8], 0) == OAE_OK);
    REQUIRE(oae_channel_set_volume(voices[8], 0.0f) == OAE_OK);
    REQUIRE(oae_channel_set_volume(voices[7], 2.0f) == OAE_OK);
    REQUIRE(oae_channel_set_volume(voices[9], 2.0f) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(virtual_state(voices[8]) == 0);
    CHECK(virtual_state(voices[7]) == 0);
    CHECK(virtual_state(voices[0]) == 0);
    CHECK(virtual_state(voices[1]) == 0);
    CHECK(virtual_state(voices[2]) == 1);
    CHECK(virtual_state(voices[9]) == 1);

    REQUIRE(oae_bus_set_mute(loud, 1) == OAE_OK);
    REQUIRE(oae_channel_set_volume(voices[0], 0.05f) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(virtual_state(voices[7]) == 1);
    CHECK(virtual_state(voices[0]) == 1);
    CHECK(virtual_state(voices[2]) == 0);
    CHECK(virtual_state(voices[3]) == 0);
    counts(system.handle, 10, 4);
}

TEST_CASE("Virtual clock respects pitch both kinds of pause and loop")
{
    SystemFixture system(1);
    oae_bus bus = 0;
    REQUIRE(oae_bus_create(system.handle, "virtual", 0, &bus) == OAE_OK);
    const auto sound = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto blocker = play(system.handle, sound);
    REQUIRE(oae_channel_set_volume(blocker, 0.0f) == OAE_OK);
    const auto voice = play(system.handle, sound, 0, bus);
    CHECK(virtual_state(voice) == 1);
    REQUIRE(oae_channel_set_pitch(voice, 2.0f) == OAE_OK);
    CHECK(rms(render(system.handle, 2400)) == 0);
    CHECK(position(voice) == 100);

    REQUIRE(oae_channel_set_paused(voice, 1) == OAE_OK);
    render(system.handle, 2400);
    CHECK(position(voice) == 100);
    REQUIRE(oae_channel_set_paused(voice, 0) == OAE_OK);

    REQUIRE(oae_bus_set_paused(bus, 1) == OAE_OK);
    render(system.handle, 2400);
    CHECK(position(voice) == 100);
    REQUIRE(oae_bus_set_paused(bus, 0) == OAE_OK);
    render(system.handle, 4800);
    CHECK(position(voice) == 50);

    REQUIRE(oae_channel_set_priority(voice, 0) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(virtual_state(voice) == 0);
    CHECK(virtual_state(blocker) == 1);
    CHECK(position(voice) == 50);
    CHECK(rms(render(system.handle, 1200)) > 0.25);
    CHECK(position(voice) >= 99);
}

TEST_CASE("Reclaim frees a physical slot before ranking and expires virtual one shots")
{
    SystemFixture system(1);
    const auto oneShot = load_wav(system.handle);
    const auto loop = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto first = play(system.handle, oneShot);
    render(system.handle, 11040);
    const auto next = play(system.handle, loop);
    CHECK(virtual_state(next) == 1);
    render(system.handle, 1440);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    int state = -1;
    CHECK(oae_channel_is_virtual(first, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(virtual_state(next) == 0);
    CHECK(position(next) == 30);

    const auto silent = play(system.handle, oneShot);
    CHECK(virtual_state(silent) == 1);
    render(system.handle, 12000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(oae_channel_is_virtual(silent, &state) == OAE_ERR_INVALID_HANDLE);
    counts(system.handle, 1, 1);
}
