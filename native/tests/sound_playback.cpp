#include "playback_fixture.h"

using namespace oae::test;

TEST_CASE("WAV FLAC MP3 and Ogg Vorbis decode from files and memory")
{
    SystemFixture system;
    constexpr std::array formats{"tone.wav", "tone.flac", "tone.mp3", "tone.ogg"};
    for (const auto* name : formats) {
        for (const bool memory : {false, true}) {
            CAPTURE(name);
            CAPTURE(memory);
            oae_sound sound = 0;
            if (memory) {
                const auto bytes = fixture_bytes(name);
                REQUIRE(oae_sound_create_from_memory(system.handle, bytes.data(), bytes.size(), 0, &sound) == OAE_OK);
            } else {
                REQUIRE(oae_sound_create_from_file(system.handle, fixture_path(name).c_str(), 0, &sound) == OAE_OK);
            }
            std::uint32_t duration = 0;
            REQUIRE(oae_sound_get_length_ms(sound, &duration) == OAE_OK);
            CHECK(duration >= 240);
            CHECK(duration <= 320);

            const auto channel = play(system.handle, sound);
            REQUIRE(oae_sound_release(sound) == OAE_OK);
            CHECK(rms(render(system.handle, 9600)) > 0.18);
            REQUIRE(oae_channel_stop(channel) == OAE_OK);
            REQUIRE(oae_system_update(system.handle) == OAE_OK);
            int playing = -1;
            CHECK(oae_channel_is_playing(channel, &playing) == OAE_ERR_INVALID_HANDLE);
        }
    }
}

TEST_CASE("Volume pause seek and sound lifetime are independent of the sound handle")
{
    SystemFixture system;
    const auto sound = load_wav(system.handle);
    const auto baseline = play(system.handle, sound);
    const auto reference = rms(render(system.handle, 4800));
    CHECK(reference == doctest::Approx(0.4 / std::sqrt(2.0)).epsilon(0.05));
    REQUIRE(oae_channel_stop(baseline) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    const auto channel = play(system.handle, sound, 1);
    REQUIRE(oae_sound_release(sound) == OAE_OK);
    std::uint32_t position = 0;
    CHECK(oae_sound_get_length_ms(sound, &position) == OAE_ERR_INVALID_HANDLE);
    oae_channel rejected = 0;
    CHECK(oae_system_play_sound(system.handle, sound, 0, 0, &rejected) == OAE_ERR_INVALID_HANDLE);
    int paused = 0;
    REQUIRE(oae_channel_get_paused(channel, &paused) == OAE_OK);
    CHECK(paused == 1);
    CHECK(rms(render(system.handle, 4800)) == 0.0);
    REQUIRE(oae_channel_set_position_ms(channel, 50) == OAE_OK);
    REQUIRE(oae_channel_get_position_ms(channel, &position) == OAE_OK);
    CHECK(position == 50);

    REQUIRE(oae_channel_set_volume(channel, 0.5f) == OAE_OK);
    float volume = 0;
    REQUIRE(oae_channel_get_volume(channel, &volume) == OAE_OK);
    CHECK(volume == 0.5f);
    REQUIRE(oae_channel_set_paused(channel, 0) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(reference * 0.5).epsilon(0.05));
    REQUIRE(oae_channel_set_paused(channel, 1) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) == 0.0);
    REQUIRE(oae_channel_set_paused(channel, 0) == OAE_OK);
    render(system.handle, 9600);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(oae_channel_get_position_ms(channel, &position) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("Pitch changes output duration and looping survives repeated source ends")
{
    SystemFixture system;
    const auto sound = load_wav(system.handle);
    const auto channel = play(system.handle, sound, 1);
    REQUIRE(oae_channel_set_pitch(channel, 2.0f) == OAE_OK);
    float pitch = 0;
    REQUIRE(oae_channel_get_pitch(channel, &pitch) == OAE_OK);
    CHECK(pitch == 2.0f);
    REQUIRE(oae_channel_set_paused(channel, 0) == OAE_OK);
    render(system.handle, 5760);
    int playing = 0;
    REQUIRE(oae_channel_is_playing(channel, &playing) == OAE_OK);
    CHECK(playing == 1);
    render(system.handle, 480);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(oae_channel_is_playing(channel, &playing) == OAE_ERR_INVALID_HANDLE);

    const auto loop = load_wav(system.handle, OAE_SOUND_LOOP);
    const auto loopingChannel = play(system.handle, loop);
    REQUIRE(oae_sound_release(loop) == OAE_OK);
    CHECK(rms(render(system.handle, 36000)) > 0.25);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_channel_is_playing(loopingChannel, &playing) == OAE_OK);
    CHECK(playing == 1);
}

TEST_CASE("File stream keeps playing after its sound handle is released")
{
    SystemFixture system;
    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_file(system.handle, fixture_path("stream.ogg").c_str(), OAE_SOUND_STREAM, &sound) == OAE_OK);
    const auto channel = play(system.handle, sound);
    REQUIRE(oae_sound_release(sound) == OAE_OK);
    CHECK(rms(render(system.handle, 4800)) > 0.2);
    REQUIRE(oae_channel_set_position_ms(channel, 2200) == OAE_OK);
    std::uint32_t position = 0;
    REQUIRE(oae_channel_get_position_ms(channel, &position) == OAE_OK);
    CHECK(position == 2200);
    bool received = false;
    for (int attempt = 0; attempt < 100 && !received; ++attempt) {
        received = rms(render(system.handle, 480)) > 0.15;
        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    CHECK(received);
    REQUIRE(oae_channel_stop(channel) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(oae_channel_get_position_ms(channel, &position) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("Sound and channel APIs reject invalid input, cross-system use and stale handles")
{
    SystemFixture a;
    SystemFixture b;
    const auto sound = load_wav(a.handle);
    oae_sound badSound = 0;
    oae_channel channel = 0;
    const auto data = fixture_bytes("tone.wav");
    CHECK(oae_sound_create_from_memory(0, data.data(), data.size(), OAE_SOUND_STREAM, &badSound) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_sound_create_from_memory(a.handle, data.data(), data.size(), OAE_SOUND_STREAM, &badSound) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_sound_create_from_memory(a.handle, "bad", 3, 0, &badSound) == OAE_ERR_FORMAT);
    CHECK(oae_sound_create_from_file(a.handle, fixture_path("missing.wav").c_str(), 0, &badSound) == OAE_ERR_FILE_NOT_FOUND);
    CHECK(oae_sound_create_from_file(0, "\xC3", 0, &badSound) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_play_sound(b.handle, sound, 0, 0, &channel) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_system_play_sound(a.handle, sound, 123, 0, &channel) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_channel_set_volume(0, -0.5f) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_channel_set_pitch(0, std::numeric_limits<float>::quiet_NaN()) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_channel_set_pitch(0, 0) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_channel_get_volume(0, nullptr) == OAE_ERR_INVALID_PARAM);
    const auto valid = play(a.handle, sound);
    REQUIRE(oae_system_release(a.handle) == OAE_OK);
    a.handle = 0;
    CHECK(oae_sound_release(sound) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_channel_stop(valid) == OAE_ERR_INVALID_HANDLE);
}
