#include "playback_fixture.h"
#include "heap_guard.h"

#include <limits>

using namespace oae::test;

namespace {

oae_bus create_bus(oae_system system, const char* name, oae_bus parent = 0)
{
    oae_bus bus = 0;
    REQUIRE(oae_bus_create(system, name, parent, &bus) == OAE_OK);
    return bus;
}

int playing(oae_channel channel)
{
    int result = -1;
    REQUIRE(oae_channel_is_playing(channel, &result) == OAE_OK);
    return result;
}

}

TEST_CASE("Bus gain composes through ancestors and pause freezes playback")
{
    SystemFixture system;
    const auto tone = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto referenceChannel = play(system.handle, tone);
    const auto reference = rms(render(system.handle, 4800));
    REQUIRE(reference > 0.25);
    REQUIRE(oae_channel_stop(referenceChannel) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    const auto parent = create_bus(system.handle, "sfx");
    const auto child = create_bus(system.handle, "ui", parent);
    REQUIRE(oae_bus_set_volume(parent, 0.5f) == OAE_OK);
    REQUIRE(oae_bus_set_volume(child, 0.5f) == OAE_OK);
    const auto channel = play(system.handle, tone, 0, child);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(reference * 0.25).epsilon(0.05));

    REQUIRE(oae_bus_set_mute(parent, 1) == OAE_OK);
    int muted = -1;
    REQUIRE(oae_bus_get_mute(parent, &muted) == OAE_OK);
    CHECK(muted == 1);
    REQUIRE(oae_bus_get_mute(child, &muted) == OAE_OK);
    CHECK(muted == 0);
    CHECK(rms(render(system.handle, 4800)) == 0);
    REQUIRE(oae_bus_set_mute(parent, 0) == OAE_OK);
    float gain = 0;
    REQUIRE(oae_bus_get_volume(parent, &gain) == OAE_OK);
    CHECK(gain == 0.5f);
    CHECK(rms(render(system.handle, 1200)) > 0.06);

    REQUIRE(oae_bus_set_paused(parent, 1) == OAE_OK);
    int paused = -1;
    REQUIRE(oae_bus_get_paused(parent, &paused) == OAE_OK);
    CHECK(paused == 1);
    REQUIRE(oae_bus_get_paused(child, &paused) == OAE_OK);
    CHECK(paused == 0);
    std::uint32_t before = 0;
    std::uint32_t after = 0;
    REQUIRE(oae_channel_get_position_ms(channel, &before) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) == 0);
    REQUIRE(oae_channel_get_position_ms(channel, &after) == OAE_OK);
    CHECK(after == before);
    REQUIRE(oae_bus_set_paused(parent, 0) == OAE_OK);
    CHECK(rms(render(system.handle, 1200)) > 0.06);
}

TEST_CASE("Master lookup uses bus:/ and paths are case sensitive and scoped per system")
{
    SystemFixture first;
    SystemFixture second;
    oae_bus master = 0;
    oae_bus lookup = 0;
    REQUIRE(oae_system_get_master_bus(first.handle, &master) == OAE_OK);
    REQUIRE(oae_system_get_bus(first.handle, "bus:/", &lookup) == OAE_OK);
    CHECK(lookup == master);
    CHECK(oae_bus_release(master) == OAE_ERR_INVALID_STATE);
    CHECK(oae_bus_set_volume(0, 1.0f) == OAE_ERR_INVALID_HANDLE);

    const auto parent = create_bus(first.handle, "sfx");
    const auto child = create_bus(first.handle, "UI", parent);
    REQUIRE(oae_system_get_bus(first.handle, "bus:/sfx/UI", &lookup) == OAE_OK);
    CHECK(lookup == child);
    CHECK(oae_system_get_bus(first.handle, "bus:/sfx/ui", &lookup) == OAE_ERR_NOT_FOUND);
    CHECK(oae_system_get_bus(first.handle, "bus:/sfx/", &lookup) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_bus_create(first.handle, "UI", parent, &lookup) == OAE_ERR_ALREADY_EXISTS);
    CHECK(oae_bus_create(first.handle, "foo/bar", parent, &lookup) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_bus_create(second.handle, "cross", parent, &lookup) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_system_get_bus(second.handle, "bus:/sfx", &lookup) == OAE_ERR_NOT_FOUND);
    CHECK(oae_bus_set_volume(0, std::numeric_limits<float>::infinity()) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_bus_set_mute(0, 2) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_bus_get_paused(0, nullptr) == OAE_ERR_INVALID_PARAM);

    const auto tone = load_wav(first.handle);
    oae_channel channel = 0;
    const auto secondBus = create_bus(second.handle, "sfx");
    CHECK(oae_system_play_sound(first.handle, tone, secondBus, 0, &channel) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("Stop all and release operate on a complete subtree")
{
    SystemFixture system;
    const auto parent = create_bus(system.handle, "parent");
    const auto child = create_bus(system.handle, "child", parent);
    const auto grandchild = create_bus(system.handle, "grandchild", child);
    const auto sibling = create_bus(system.handle, "sibling");
    const auto sound = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto a = play(system.handle, sound, 0, parent);
    const auto b = play(system.handle, sound, 0, grandchild);
    const auto outside = play(system.handle, sound, 0, sibling);
    REQUIRE(oae_bus_stop_all(parent) == OAE_OK);
    CHECK(playing(a) == 0);
    CHECK(playing(b) == 0);
    CHECK(playing(outside) == 1);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    int state = 0;
    CHECK(oae_channel_is_playing(a, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_channel_is_playing(b, &state) == OAE_ERR_INVALID_HANDLE);

    const auto replacement = play(system.handle, sound, 0, grandchild);
    REQUIRE(oae_bus_release(parent) == OAE_OK);
    CHECK(oae_bus_get_mute(parent, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_bus_get_mute(child, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_bus_get_mute(grandchild, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_channel_is_playing(replacement, &state) == OAE_ERR_INVALID_HANDLE);
    CHECK(playing(outside) == 1);
    oae_bus lookup = 0;
    CHECK(oae_system_get_bus(system.handle, "bus:/parent/child/grandchild", &lookup) == OAE_ERR_NOT_FOUND);
    CHECK(create_bus(system.handle, "parent") != parent);
}

TEST_CASE("Bus depth permits 32 descendants and rejects the next level")
{
    SystemFixture system;
    oae_bus parent = 0;
    oae_bus first = 0;
    for (unsigned depth = 1; depth <= 32; ++depth) {
        parent = create_bus(system.handle, "node", parent);
        if (depth == 1) {
            first = parent;
        }
    }
    oae_bus rejected = 0;
    CHECK(oae_bus_create(system.handle, "too-deep", parent, &rejected) == OAE_ERR_INVALID_PARAM);
    const auto sound = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto channel = play(system.handle, sound, 0, parent);
    CHECK(rms(render(system.handle, 4800)) > 0.25);
    REQUIRE(oae_bus_release(first) == OAE_OK);
    int state = 0;
    CHECK(oae_channel_is_playing(channel, &state) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("T18: Audio sends and returns support pre/post-fader routing, cycle rejection, and lock-free removal")
{
    SystemFixture system;
    const auto tone = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto refCh = play(system.handle, tone);
    const auto reference = rms(render(system.handle, 4800));
    REQUIRE(reference > 0.25);
    REQUIRE(oae_channel_stop(refCh) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    const auto sfx = create_bus(system.handle, "sfx");
    oae_bus reverb = 0;
    REQUIRE(oae_bus_create_return(system.handle, "reverb", 0, &reverb) == OAE_OK);
    oae_bus lookup = 0;
    REQUIRE(oae_system_get_bus(system.handle, "return:/reverb", &lookup) == OAE_OK);
    CHECK(lookup == reverb);

    // Cycle rejection: self-send, parent->child, indirect cycle
    oae_bus master = 0;
    REQUIRE(oae_system_get_master_bus(system.handle, &master) == OAE_OK);
    CHECK(oae_bus_add_send(sfx, sfx, 0.5f, OAE_EFFECT_POST_FADER) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_bus_add_send(master, sfx, 0.5f, OAE_EFFECT_POST_FADER) == OAE_ERR_INVALID_PARAM);
    REQUIRE(oae_bus_add_send(sfx, reverb, 0.5f, OAE_EFFECT_POST_FADER) == OAE_OK);
    CHECK(oae_bus_add_send(sfx, reverb, 0.5f, OAE_EFFECT_POST_FADER) == OAE_ERR_ALREADY_EXISTS);
    CHECK(oae_bus_add_send(reverb, sfx, 0.5f, OAE_EFFECT_POST_FADER) == OAE_ERR_INVALID_PARAM);

    // Post-fader send: fader at 0 silences send
    REQUIRE(oae_bus_set_volume(sfx, 0.0f) == OAE_OK);
    const auto ch = play(system.handle, tone, 0, sfx);
    (void)ch;
    CHECK(rms(render(system.handle, 4800)) == 0.0);

    // Post-fader send: fader 0.5, send 0.5 -> sfx(0.5) + reverb(0.25) = 0.75 * reference
    REQUIRE(oae_bus_set_volume(sfx, 0.5f) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(reference * 0.75).epsilon(0.06));

    // Mute send -> only dry sfx(0.5) remains
    REQUIRE(oae_bus_set_send_mute(sfx, reverb, 1) == OAE_OK);
    int sendMuted = 0;
    REQUIRE(oae_bus_get_send_mute(sfx, reverb, &sendMuted) == OAE_OK);
    CHECK(sendMuted == 1);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(reference * 0.50).epsilon(0.05));
    REQUIRE(oae_bus_set_send_mute(sfx, reverb, 0) == OAE_OK);

    // Switch to pre-fader send: fader at 0 still feeds reverb at 0.5 * reference
    REQUIRE(oae_bus_remove_send(sfx, reverb) == OAE_OK);
    REQUIRE(oae_bus_add_send(sfx, reverb, 0.5f, OAE_EFFECT_PRE_FADER) == OAE_OK);
    REQUIRE(oae_bus_set_volume(sfx, 0.0f) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(reference * 0.50).epsilon(0.05));

    // Adjust send level lock-free and verify zero heap allocations during render
    REQUIRE(oae_bus_set_send_level(sfx, reverb, 0.25f) == OAE_OK);
    float level = 0.0f;
    REQUIRE(oae_bus_get_send_level(sfx, reverb, &level) == OAE_OK);
    CHECK(level == doctest::Approx(0.25f));
    {
        std::vector<float> pcm(4800 * 2, 0.0f);
        std::size_t heapOps = 0;
        {
            HeapGuard guard;
            REQUIRE(oae_system_render(system.handle, pcm.data(), 4800) == OAE_OK);
            heapOps = guard.operations();
        }
        CHECK(heapOps == 0);
        CHECK(rms(pcm) == doctest::Approx(reference * 0.25).epsilon(0.05));
    }

    // Releasing return bus cleanly detaches incoming sends
    REQUIRE(oae_bus_release(reverb) == OAE_OK);
    CHECK(oae_bus_get_send_level(sfx, reverb, &level) == OAE_ERR_INVALID_HANDLE);
    CHECK(rms(render(system.handle, 2400)) == 0.0);
}
