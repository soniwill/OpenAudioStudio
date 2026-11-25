#include "playback_fixture.h"
#include "../src/core/studio_model.h"
#include "../src/core/event_instance.h"
#include "../src/api/runtime.h"

#include <cmath>
#include <string>
#include <vector>

using namespace oae::test;

namespace {

oae_sound make_sine(oae_system system, double hz, std::uint32_t frames, float amp = 0.5f)
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
        const double s = amp * std::sin(2.0 * 3.14159265358979323846 * hz * i / 48000.0);
        const auto val = static_cast<std::uint16_t>(static_cast<std::int16_t>(32767 * s));
        wav[44 + 2 * i] = static_cast<std::uint8_t>(val);
        wav[45 + 2 * i] = static_cast<std::uint8_t>(val >> 8);
    }
    oae_sound sound = 0;
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), OAE_SOUND_LOOP, &sound) == OAE_OK);
    return sound;
}

double compute_rms(const float* buffer, std::size_t count)
{
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        sum += static_cast<double>(buffer[i]) * buffer[i];
    }
    return std::sqrt(sum / static_cast<double>(count));
}

}

TEST_CASE("T10b: AHDSR Envelope audible attack, decay, sustain and release decay")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/synth_lead");

    // AHDSR: attack=50ms, hold=50ms, decay=50ms, sustain=0.5, release=200ms
    auto ahdsr_mod = oae::Modulator::ahdsr(50.0f, 50.0f, 50.0f, 0.5f, 200.0f);

    auto snd = make_sine(fix.handle, 440.0, 96000, 0.8f);
    oae::InstrumentBuilder inst;
    inst.sound(snd).loop(true).length_ms(10000.0).add_modulator(ahdsr_mod);

    ev.add_track(oae::TrackBuilder().add_instrument(inst));
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/synth_lead", &desc) == OAE_OK);

    oae_event_instance inst_handle = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst_handle) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst_handle) == OAE_OK);

    // Render 150 ms (7200 frames at 48kHz): Attack + Hold + Decay -> reaches Sustain = 0.5
    std::vector<float> buf(7200 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 7200) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    // Render next 50 ms (2400 frames) in sustain state: RMS should be steady around sustain level
    buf.resize(2400 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const double sustain_rms = compute_rms(buf.data(), buf.size());
    CHECK(sustain_rms > 0.15);

    // Call stop with ALLOW_FADEOUT -> triggers AHDSR release (200 ms)
    REQUIRE(oae_event_instance_stop(inst_handle, OAE_STOP_ALLOW_FADEOUT) == OAE_OK);

    // Render 100 ms (4800 frames): halfway through 200 ms release -> level should be roughly half of sustain
    buf.resize(4800 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 4800) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const double mid_release_rms = compute_rms(buf.data(), buf.size());
    CHECK(mid_release_rms < sustain_rms);
    CHECK(mid_release_rms > 0.01);

    // Render another 150 ms (7200 frames) -> release finishes at 200 ms, subsequent samples must be exact zero!
    buf.resize(7200 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 7200) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    // The tail after release frames must be exact zero
    float tail_max = 0.0f;
    for (std::size_t i = 4800 * 2; i < buf.size(); ++i) {
        const float a = std::abs(buf[i]);
        if (a > tail_max) tail_max = a;
    }
    CHECK(tail_max == 0.0f);

    int play_state = -1;
    REQUIRE(oae_event_instance_get_playback_state(inst_handle, &play_state) == OAE_OK);
    CHECK(play_state == OAE_PLAYBACK_STOPPED);

    oae_event_instance_release(inst_handle);
    oae_sound_release(snd);
}

TEST_CASE("T10b: RANDOM modulator boundaries and statistical distribution")
{
    oae::Pcg32 rng(0x12345678ULL, 0x87654321ULL);
    auto vol_mod = oae::Modulator::random_volume(6.0f);
    auto pitch_mod = oae::Modulator::random_pitch(12.0f);
    auto pan_mod = oae::Modulator::random_pan(0.5f);

    CHECK(vol_mod.random_range_min == -6.0f);
    CHECK(vol_mod.random_range_max == 6.0f);
    CHECK(pitch_mod.random_range_min == -12.0f);
    CHECK(pitch_mod.random_range_max == 12.0f);
    CHECK(pan_mod.random_range_min == -0.5f);
    CHECK(pan_mod.random_range_max == 0.5f);

    double sum_vol = 0.0, sum_pitch = 0.0, sum_pan = 0.0;
    const std::size_t total = 10000;
    for (std::size_t i = 0; i < total; ++i) {
        const float v = rng.next_range(vol_mod.random_range_min, vol_mod.random_range_max);
        const float p = rng.next_range(pitch_mod.random_range_min, pitch_mod.random_range_max);
        const float pn = rng.next_range(pan_mod.random_range_min, pan_mod.random_range_max);

        CHECK(v >= -6.0f);
        CHECK(v <= 6.0f);
        CHECK(p >= -12.0f);
        CHECK(p <= 12.0f);
        CHECK(pn >= -0.5f);
        CHECK(pn <= 0.5f);

        sum_vol += v;
        sum_pitch += p;
        sum_pan += pn;
    }

    // Mean should be centered near 0.0
    CHECK(std::abs(sum_vol / total) < 0.25);
    CHECK(std::abs(sum_pitch / total) < 0.5);
    CHECK(std::abs(sum_pan / total) < 0.05);
}

TEST_CASE("T10b: Parameter automation curve (RPM to pitch) and Volume -80dB exact silence")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/engine_pitch_vol");
    ev.add_parameter(oae::ParameterDefinition::continuous("rpm", 0.0f, 8000.0f, 1000.0f));
    ev.add_parameter(oae::ParameterDefinition::continuous("fade_cut", 0.0f, 1.0f, 0.0f));

    // Pitch automation curve: 1000 RPM -> 0 st, 4000 RPM -> 12 st, 8000 RPM -> 24 st
    oae::Curve pitch_curve;
    pitch_curve.add_point(1000.0f, 0.0f, oae::CurveShape::LINEAR);
    pitch_curve.add_point(4000.0f, 12.0f, oae::CurveShape::LINEAR);
    pitch_curve.add_point(8000.0f, 24.0f, oae::CurveShape::LINEAR);
    ev.add_automation(oae::Automation::pitch_param("rpm", pitch_curve));

    // Volume automation curve: 0.0 -> 0.0 dB, 1.0 -> -80.0 dB (exact silence)
    oae::Curve vol_curve;
    vol_curve.add_point(0.0f, 0.0f, oae::CurveShape::LINEAR);
    vol_curve.add_point(1.0f, -80.0f, oae::CurveShape::LINEAR);
    ev.add_automation(oae::Automation::volume_param("fade_cut", vol_curve));

    auto snd = make_sine(fix.handle, 100.0, 96000, 0.5f);
    ev.add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(snd).loop(true).length_ms(10000.0)));

    builder.add_event(ev);
    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/engine_pitch_vol", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // 1. Test RPM = 1000 (0 st -> pitch = 1.0)
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 1000.0f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    auto* inst_obj = oae::runtime.event_instances.get(inst);
    REQUIRE(inst_obj);

    // 2. Test RPM = 4000 (+12 st -> pitch ratio = 2.0)
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 4000.0f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    // 3. Test RPM = 8000 (+24 st -> pitch ratio = 4.0)
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 8000.0f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    // 4. Test Volume Automation to -80 dB produces exact zero linear samples
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "fade_cut", 1.0f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    std::vector<float> buf(2400 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);

    float max_sample = 0.0f;
    for (const auto sample : buf) {
        const float a = std::abs(sample);
        if (a > max_sample) max_sample = a;
    }
    CHECK(max_sample == 0.0f);

    oae_event_instance_release(inst);
    oae_sound_release(snd);
}

TEST_CASE("T10b: EffectAutomation with miss cache")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("reverb_amount", 0.0f, 1.0f, 0.0f));

    // Effect automation targeting a non-existent bus initially
    oae::Curve rev_curve;
    rev_curve.add_point(0.0f, 0.0f);
    rev_curve.add_point(1.0f, 1.0f);

    oae::EffectAutomation ea;
    ea.bus_path = "bus:/environment/cave";
    ea.effect_index = 0;
    ea.param_index = 0;
    ea.param.name = "reverb_amount";
    ea.param.is_global = true;
    ea.curve = rev_curve;
    builder.add_effect_automation(ea);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    // Update with non-existent bus: should not crash and should cache the miss
    REQUIRE(oae_studio_set_global_parameter_by_name(fix.handle, "reverb_amount", 0.7f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    CHECK(sys->lookup_bus_cached("bus:/environment/cave") == 0);

    // Now create the target bus and add an effect
    oae_bus env_bus = 0, cave_bus = 0;
    REQUIRE(oae_bus_create(fix.handle, "environment", 0, &env_bus) == OAE_OK);
    REQUIRE(oae_bus_create(fix.handle, "cave", env_bus, &cave_bus) == OAE_OK);

    oae_effect fx = 0;
    REQUIRE(oae_bus_add_effect(cave_bus, OAE_EFFECT_REVERB, OAE_EFFECT_INDEX_LAST, &fx) == OAE_OK);

    // Updating system should invalidate cache and apply the effect parameter!
    REQUIRE(oae_studio_set_global_parameter_by_name(fix.handle, "reverb_amount", 0.85f, 1) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    float param_val = -1.0f;
    REQUIRE(oae_effect_get_param(fx, 0, &param_val) == OAE_OK);
    CHECK(std::abs(param_val - 0.85f) < 1.0e-3f);
}
