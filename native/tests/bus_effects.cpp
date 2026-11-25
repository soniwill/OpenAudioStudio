#include "playback_fixture.h"
#include "../src/core/bus_dsp.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>

using namespace oae::test;

namespace {

template <typename Sample>
oae_sound make_sound(oae_system system, std::uint32_t frames, std::uint32_t flags, Sample sample)
{
    const auto bytes = frames * 2;
    std::vector<std::uint8_t> wav{
        'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,
        0x80,0xBB,0,0,0,0x77,1,0,2,0,16,0,'d','a','t','a',0,0,0,0
    };
    for (unsigned i = 0; i < 4; ++i) {
        wav[4 + i] = static_cast<std::uint8_t>((bytes + 36) >> (i * 8));
        wav[40 + i] = static_cast<std::uint8_t>(bytes >> (i * 8));
    }
    wav.resize(44 + bytes);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const auto value = static_cast<std::uint16_t>(static_cast<std::int16_t>(32767 * sample(i)));
        wav[44 + 2 * i] = static_cast<std::uint8_t>(value);
        wav[45 + 2 * i] = static_cast<std::uint8_t>(value >> 8);
    }
    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), flags, &sound) == OAE_OK);
    return sound;
}

oae_sound tone(oae_system system, double hz, double amplitude)
{
    return make_sound(system, 48000, OAE_SOUND_LOOP, [&](std::uint32_t i) {
        return amplitude * std::sin(2.0 * 3.14159265358979323846 * hz * i / 48000.0);
    });
}

oae_bus bus(oae_system system, const char* name)
{
    oae_bus value = 0;
    REQUIRE(oae_bus_create(system, name, 0, &value) == OAE_OK);
    return value;
}

oae_bus master(oae_system system)
{
    oae_bus value = 0;
    REQUIRE(oae_system_get_master_bus(system, &value) == OAE_OK);
    return value;
}

oae_effect effect(oae_bus target, int type, int position = OAE_EFFECT_POST_FADER, int index = OAE_EFFECT_INDEX_LAST)
{
    oae_effect value = 0;
    REQUIRE(oae_bus_add_effect_ex(target, type, position, index, &value) == OAE_OK);
    return value;
}

double left_rms(const std::vector<float>& output)
{
    double sum = 0;
    for (std::size_t i = 0; i < output.size(); i += 2) {
        sum += static_cast<double>(output[i]) * output[i];
    }
    return std::sqrt(sum / static_cast<double>(output.size() / 2));
}

float peak(const std::vector<float>& output)
{
    float value = 0;
    for (const auto sample : output) {
        value = std::max(value, std::fabs(sample));
    }
    return value;
}

}

TEST_CASE("Look-ahead limiter enforces its ceiling and meter matches rendered PCM")
{
    SystemFixture system;
    const auto target = master(system.handle);
    const auto limiter = effect(target, OAE_EFFECT_LIMITER);
    const auto sound = tone(system.handle, 1000, 0.999);
    for (int i = 0; i < 10; ++i) {
        play(system.handle, sound);
    }
    const auto out = render(system.handle, 4800);
    const auto ceiling = std::pow(10.0f, -0.1f / 20.0f);
    CHECK(peak(out) <= ceiling);
    CHECK(peak(out) > 0.97f);
    for (std::size_t i = 0; i < 72 * 2; ++i) {
        CHECK(out[i] == 0.0f);
    }
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    oae_meter meter{};
    REQUIRE(oae_bus_get_meter(target, &meter) == OAE_OK);
    CHECK(meter.channel_count == 2);
    CHECK(meter.frame_count == 4800);
    CHECK(meter.peak[0] == peak(out));
    CHECK(meter.rms[0] == doctest::Approx(left_rms(out)));
    REQUIRE(oae_bus_set_mute(target, 1) == OAE_OK);
    CHECK(peak(render(system.handle, 256)) == 0.0f);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_bus_get_meter(target, &meter) == OAE_OK);
    CHECK(meter.frame_count == 256);
    CHECK(meter.peak[0] == 0.0f);
    CHECK(meter.rms[0] == 0.0f);
    REQUIRE(oae_effect_release(limiter) == OAE_OK);
}

TEST_CASE("Sidechain uses the previous output block regardless of bus creation order")
{
    for (const bool voiceFirst : {false, true}) {
        CAPTURE(voiceFirst);
        SystemFixture system;
        const auto first = bus(system.handle, voiceFirst ? "vo" : "music");
        const auto second = bus(system.handle, voiceFirst ? "music" : "vo");
        const auto voice = voiceFirst ? first : second;
        const auto music = voiceFirst ? second : first;
        REQUIRE(oae_bus_set_pan(voice, 1.0f) == OAE_OK);
        REQUIRE(oae_bus_set_pan(music, -1.0f) == OAE_OK);
        const auto compressor = effect(music, OAE_EFFECT_COMPRESSOR);
        REQUIRE(oae_effect_set_param(compressor, OAE_COMPRESSOR_ATTACK_MS, 0.1f) == OAE_OK);
        REQUIRE(oae_effect_set_param(compressor, OAE_COMPRESSOR_RELEASE_MS, 5.0f) == OAE_OK);
        REQUIRE(oae_effect_set_sidechain(compressor, voice) == OAE_OK);
        play(system.handle, tone(system.handle, 1500, 0.8), 0, voice);
        play(system.handle, tone(system.handle, 1500, 0.12), 0, music);
        const auto initial = left_rms(render(system.handle, 256));
        CHECK(initial == doctest::Approx(0.12).epsilon(0.03));
        const auto ducked = left_rms(render(system.handle, 256));
        CHECK(ducked < initial * 0.5);
        REQUIRE(oae_bus_set_mute(voice, 1) == OAE_OK);
        CHECK(left_rms(render(system.handle, 256)) < initial * 0.5);
        render(system.handle, 2400);
        CHECK(left_rms(render(system.handle, 256)) == doctest::Approx(initial).epsilon(0.03));
    }
}

TEST_CASE("Biquad and crossover filters attenuate their target bands and survive a resonant sweep")
{
    SystemFixture system;
    const auto target = master(system.handle);
    const auto highTone = tone(system.handle, 5000, 0.4);
    const auto channel = play(system.handle, highTone);
    const auto baseline = rms(render(system.handle, 4800));
    REQUIRE(baseline > 0.25);
    const auto lowpass = effect(target, OAE_EFFECT_LOWPASS);
    REQUIRE(oae_effect_set_param(lowpass, OAE_FILTER_CUTOFF_HZ, 1000) == OAE_OK);
    render(system.handle, 4800);
    CHECK(rms(render(system.handle, 4800)) < baseline * 0.25);
    REQUIRE(oae_effect_set_param(lowpass, OAE_FILTER_Q, 10) == OAE_OK);
    REQUIRE(oae_effect_set_param(lowpass, OAE_FILTER_CUTOFF_HZ, 20000) == OAE_OK);
    render(system.handle, 9600);
    REQUIRE(oae_effect_set_param(lowpass, OAE_FILTER_CUTOFF_HZ, 10) == OAE_OK);
    const auto swept = render(system.handle, 9600);
    CHECK(std::all_of(swept.begin(), swept.end(), [](float x) { return std::isfinite(x) && std::fabs(x) < 8.0f; }));
    REQUIRE(oae_effect_release(lowpass) == OAE_OK);

    const auto eq = effect(target, OAE_EFFECT_EQ3);
    render(system.handle, 4800);
    CHECK(rms(render(system.handle, 4800)) == doctest::Approx(baseline).epsilon(0.05));
    REQUIRE(oae_effect_set_param(eq, OAE_EQ3_HIGH_DB, -40) == OAE_OK);
    render(system.handle, 9600);
    CHECK(rms(render(system.handle, 4800)) < baseline * 0.4);
    REQUIRE(oae_effect_release(eq) == OAE_OK);
    REQUIRE(oae_channel_stop(channel) == OAE_OK);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    const auto highpass = effect(target, OAE_EFFECT_HIGHPASS);
    REQUIRE(oae_effect_set_param(highpass, OAE_FILTER_CUTOFF_HZ, 1000) == OAE_OK);
    play(system.handle, tone(system.handle, 100, 0.4));
    render(system.handle, 4800);
    CHECK(rms(render(system.handle, 4800)) < baseline * 0.03);
}

TEST_CASE("Bypass and dry-only processing preserve PCM bits")
{
    SystemFixture system;
    const auto target = master(system.handle);
    const auto sound = tone(system.handle, 1000, 0.3);
    const auto capture = [&] {
        const auto channel = play(system.handle, sound);
        auto output = render(system.handle, 480);
        REQUIRE(oae_channel_stop(channel) == OAE_OK);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        return output;
    };
    const auto reference = capture();
    const auto echo = effect(target, OAE_EFFECT_DELAY);
    REQUIRE(oae_effect_set_bypass(echo, 1) == OAE_OK);
    const auto bypassed = capture();
    CHECK(std::memcmp(reference.data(), bypassed.data(), reference.size() * sizeof(float)) == 0);
    REQUIRE(oae_effect_set_bypass(echo, 0) == OAE_OK);
    REQUIRE(oae_effect_set_wet_dry(echo, 0.0f, 1.0f) == OAE_OK);
    const auto dry = capture();
    CHECK(std::memcmp(reference.data(), dry.data(), reference.size() * sizeof(float)) == 0);
    float wet = -1, dryGain = -1;
    REQUIRE(oae_effect_get_wet_dry(echo, &wet, &dryGain) == OAE_OK);
    CHECK(wet == 0);
    CHECK(dryGain == 1);
}

TEST_CASE("Delay and Freeverb tails survive source reclaim and master mute silences them")
{
    SystemFixture system;
    const auto target = master(system.handle);
    const auto impulse = make_sound(system.handle, 32, OAE_SOUND_DEFAULT,
        [](std::uint32_t i) { return i == 0 ? 0.8 : 0.0; });
    const auto echo = effect(target, OAE_EFFECT_DELAY);
    REQUIRE(oae_effect_set_param(echo, OAE_DELAY_TIME_MS, 10) == OAE_OK);
    REQUIRE(oae_effect_set_param(echo, OAE_DELAY_FEEDBACK, 0.5f) == OAE_OK);
    play(system.handle, impulse);
    CHECK(peak(render(system.handle, 64)) == 0.0f);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto delayed = render(system.handle, 480);
    CHECK(delayed[(480 - 64) * 2] == doctest::Approx(0.8).epsilon(0.001));
    CHECK(peak(render(system.handle, 480)) == doctest::Approx(0.4).epsilon(0.001));
    REQUIRE(oae_effect_release(echo) == OAE_OK);

    effect(target, OAE_EFFECT_REVERB);
    play(system.handle, impulse);
    render(system.handle, 64);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    CHECK(peak(render(system.handle, 2000)) > 0.005f);
    REQUIRE(oae_bus_set_mute(target, 1) == OAE_OK);
    CHECK(peak(render(system.handle, 4800)) == 0.0f);
}

TEST_CASE("Effect positions have independent capacities and releasing a bus invalidates its effects")
{
    SystemFixture system;
    SystemFixture other;
    const auto target = bus(system.handle, "fx");
    const auto foreign = master(other.handle);
    const auto compressor = effect(target, OAE_EFFECT_COMPRESSOR, OAE_EFFECT_PRE_FADER);
    CHECK(oae_effect_set_sidechain(compressor, foreign) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_effect_set_sidechain(compressor, target) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_effect_set_sidechain(compressor, 0) == OAE_ERR_INVALID_HANDLE);
    oae_effect extra = 0;
    CHECK(oae_bus_add_effect_ex(target, OAE_EFFECT_EQ3, OAE_EFFECT_POST_FADER, 4, &extra) == OAE_ERR_INVALID_PARAM);
    for (int i = 1; i < 8; ++i) {
        effect(target, OAE_EFFECT_LOWPASS, OAE_EFFECT_PRE_FADER, 0);
    }
    CHECK(oae_bus_add_effect_ex(target, OAE_EFFECT_LOWPASS, OAE_EFFECT_PRE_FADER, OAE_EFFECT_INDEX_LAST, &extra) == OAE_ERR_INVALID_STATE);
    REQUIRE(oae_bus_add_effect(target, OAE_EFFECT_LOWPASS, OAE_EFFECT_INDEX_LAST, &extra) == OAE_OK);
    float value = 0;
    REQUIRE(oae_effect_get_param(extra, OAE_FILTER_CUTOFF_HZ, &value) == OAE_OK);
    CHECK(value == 20000);
    CHECK(oae_effect_set_param(extra, OAE_FILTER_Q, std::numeric_limits<float>::infinity()) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_effect_set_param(extra, OAE_FILTER_Q, 11) == OAE_ERR_INVALID_PARAM);
    REQUIRE(oae_bus_release(target) == OAE_OK);
    CHECK(oae_effect_get_param(extra, 0, &value) == OAE_ERR_INVALID_HANDLE);
    CHECK(oae_effect_release(compressor) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("Pre-fader and post-fader placement have distinct gain staging")
{
    SystemFixture system;
    const auto target = master(system.handle);
    REQUIRE(oae_bus_set_volume(target, 0.25f) == OAE_OK);
    play(system.handle, tone(system.handle, 1000, 0.8));
    const auto pre = effect(target, OAE_EFFECT_LIMITER, OAE_EFFECT_PRE_FADER);
    REQUIRE(oae_effect_set_param(pre, OAE_LIMITER_CEILING_DB, -12.0f) == OAE_OK);
    render(system.handle, 4800);
    const auto before = peak(render(system.handle, 4800));
    CHECK(before == doctest::Approx(0.25 * std::pow(10.0, -12.0 / 20.0)).epsilon(0.01));
    REQUIRE(oae_effect_release(pre) == OAE_OK);

    const auto post = effect(target, OAE_EFFECT_LIMITER);
    REQUIRE(oae_effect_set_param(post, OAE_LIMITER_CEILING_DB, -12.0f) == OAE_OK);
    render(system.handle, 4800);
    const auto after = peak(render(system.handle, 4800));
    CHECK(after == doctest::Approx(0.2).epsilon(0.01));
}

TEST_CASE("Meter excludes NaN and infinity and clamps oversized samples")
{
    oae::Meter meter;
    const float samples[]{std::numeric_limits<float>::quiet_NaN(), 0.5f,
        -2.0e9f, std::numeric_limits<float>::infinity()};
    CHECK(meter.add(samples, 2, 2) == 1.0e9f);
    meter.close(2);
    const auto& result = meter.snapshot();
    CHECK(result.frame_count == 2);
    CHECK(result.peak[0] == 1.0e9f);
    CHECK(result.peak[1] == 0.5f);
    CHECK(result.rms[0] == doctest::Approx(1e9 / std::sqrt(2.0)));
    CHECK(result.rms[1] == doctest::Approx(0.5 / std::sqrt(2.0)));
}

TEST_CASE("Meter window swaps retain every frame while audio writes concurrently")
{
    oae::Meter meter;
    std::atomic<bool> done{false};
    constexpr std::uint32_t frames = 100000;
    std::thread audio([&] {
        constexpr float sample = 0.25f;
        for (std::uint32_t i = 0; i < frames; ++i) {
            meter.add(&sample, 1, 1);
        }
        done.store(true, std::memory_order_release);
    });
    std::uint64_t measured = 0;
    bool valuesCorrect = true;
    const auto close = [&] {
        meter.close(1);
        const auto& window = meter.snapshot();
        measured += window.frame_count;
        if (window.frame_count != 0) {
            valuesCorrect = valuesCorrect && window.peak[0] == 0.25f && window.rms[0] == 0.25f;
        }
    };
    while (!done.load(std::memory_order_acquire)) {
        close();
    }
    audio.join();
    close();
    CHECK(measured == frames);
    CHECK(valuesCorrect);
}
