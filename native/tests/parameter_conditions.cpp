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
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), 0, &sound) == OAE_OK);
    return sound;
}

}

TEST_CASE("T10a: Parameter clamping, discrete rounding and labeled mapping")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/test_param");
    ev.add_parameter(oae::ParameterDefinition::continuous("speed", 0.0f, 100.0f, 10.0f));
    ev.add_parameter(oae::ParameterDefinition::discrete("gear", 1.0f, 6.0f, 1.0f));
    ev.add_parameter(oae::ParameterDefinition::labeled("surface", {"dirt", "wood", "metal"}, 0.0f));

    auto snd = make_sine(fix.handle, 440.0, 4800);
    oae::InstrumentBuilder inst;
    inst.sound(snd).length_ms(100.0);
    oae::TrackBuilder track;
    track.add_instrument(inst);
    ev.add_track(track);

    builder.add_event(ev);
    auto model_set = builder.build();

    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/test_param", &desc) == OAE_OK);

    oae_event_instance inst_handle = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst_handle) == OAE_OK);

    // Continuous clamping
    float cur = -1.0f, final_val = -1.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "speed", &cur, &final_val) == OAE_OK);
    CHECK(cur == 10.0f);
    CHECK(final_val == 10.0f);

    REQUIRE(oae_event_instance_set_parameter_by_name(inst_handle, "speed", -50.0f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "speed", &cur, &final_val) == OAE_OK);
    CHECK(cur == 0.0f);
    CHECK(final_val == 0.0f);

    REQUIRE(oae_event_instance_set_parameter_by_name(inst_handle, "speed", 250.0f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "speed", &cur, &final_val) == OAE_OK);
    CHECK(cur == 100.0f);
    CHECK(final_val == 100.0f);

    // Discrete rounding and clamping
    REQUIRE(oae_event_instance_set_parameter_by_name(inst_handle, "gear", 3.7f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "gear", &cur, &final_val) == OAE_OK);
    CHECK(cur == 4.0f);

    REQUIRE(oae_event_instance_set_parameter_by_name(inst_handle, "gear", 10.0f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "gear", &cur, &final_val) == OAE_OK);
    CHECK(cur == 6.0f);

    // Labeled parameter
    REQUIRE(oae_event_instance_set_parameter_by_name_with_label(inst_handle, "surface", "wood", 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "surface", &cur, &final_val) == OAE_OK);
    CHECK(cur == 1.0f);

    REQUIRE(oae_event_instance_set_parameter_by_name_with_label(inst_handle, "surface", "metal", 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_handle, "surface", &cur, &final_val) == OAE_OK);
    CHECK(cur == 2.0f);

    // Non-existent label returns error
    CHECK(oae_event_instance_set_parameter_by_name_with_label(inst_handle, "surface", "water", 1) == OAE_ERR_INVALID_PARAM);

    oae_event_instance_release(inst_handle);
    oae_sound_release(snd);
}

TEST_CASE("T10a: Probability distribution with PCG32 (50% in 50% +- 5% in 10k triggers)")
{
    oae::Pcg32 rng(0x987654321ULL, 0x12345ULL);
    oae::Condition cond;
    cond.probability_pct = 50.0f;

    std::uint32_t hits = 0;
    const std::uint32_t total = 10000;
    for (std::uint32_t i = 0; i < total; ++i) {
        const float roll = rng.next_float() * 100.0f;
        if (roll < cond.probability_pct) {
            ++hits;
        }
    }

    const double ratio = static_cast<double>(hits) / static_cast<double>(total);
    CHECK(ratio >= 0.45);
    CHECK(ratio <= 0.55);
}

TEST_CASE("T10a: Global parameter affects all instances and updates correctly")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("global_weather", 0.0f, 1.0f, 0.2f));

    oae::EventBuilder ev1("event:/ambient1");
    auto snd1 = make_sine(fix.handle, 300.0, 4800);
    ev1.add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(snd1)));
    builder.add_event(ev1);

    oae::EventBuilder ev2("event:/ambient2");
    auto snd2 = make_sine(fix.handle, 600.0, 4800);
    ev2.add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(snd2)));
    builder.add_event(ev2);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description d1 = 0, d2 = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/ambient1", &d1) == OAE_OK);
    REQUIRE(oae_studio_get_event(fix.handle, "event:/ambient2", &d2) == OAE_OK);

    oae_event_instance i1 = 0, i2 = 0;
    REQUIRE(oae_event_description_create_instance(d1, &i1) == OAE_OK);
    REQUIRE(oae_event_description_create_instance(d2, &i2) == OAE_OK);

    float val1 = 0.0f, val2 = 0.0f, glob = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(i1, "global_weather", &val1, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(i2, "global_weather", &val2, nullptr) == OAE_OK);
    REQUIRE(oae_studio_get_global_parameter_by_name(fix.handle, "global_weather", &glob, nullptr) == OAE_OK);
    CHECK(val1 == 0.2f);
    CHECK(val2 == 0.2f);
    CHECK(glob == 0.2f);

    // Modify global parameter
    REQUIRE(oae_studio_set_global_parameter_by_name(fix.handle, "global_weather", 0.85f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(i1, "global_weather", &val1, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(i2, "global_weather", &val2, nullptr) == OAE_OK);
    REQUIRE(oae_studio_get_global_parameter_by_name(fix.handle, "global_weather", &glob, nullptr) == OAE_OK);
    CHECK(val1 == 0.85f);
    CHECK(val2 == 0.85f);
    CHECK(glob == 0.85f);

    oae_event_instance_release(i1);
    oae_event_instance_release(i2);
    oae_sound_release(snd1);
    oae_sound_release(snd2);
}

TEST_CASE("T10a: Seek speed deterministic interpolation via DSP time")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/car_engine");
    // seek_speed = 200 units/s, seek_speed_down = 400 units/s
    ev.add_parameter(oae::ParameterDefinition::continuous("rpm", 0.0f, 1000.0f, 0.0f, 200.0f, 400.0f));

    auto snd = make_sine(fix.handle, 220.0, 96000);
    ev.add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(snd).loop(true).length_ms(10000.0)));
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/car_engine", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Target 500.0f without ignoring seek speed
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 500.0f, 0) == OAE_OK);

    float cur = -1.0f, final_val = -1.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "rpm", &cur, &final_val) == OAE_OK);
    CHECK(cur == 0.0f);
    CHECK(final_val == 500.0f);

    // Render 24000 frames (0.5 s DSP at 48kHz)
    // Step = 200 * 0.5 = 100
    std::vector<float> buf(24000 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 24000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "rpm", &cur, &final_val) == OAE_OK);
    CHECK(std::abs(cur - 100.0f) < 2.0f);
    CHECK(final_val == 500.0f);

    // Render 96000 frames (2.0 s DSP) -> should reach target 500
    buf.resize(96000 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 96000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "rpm", &cur, &final_val) == OAE_OK);
    CHECK(cur == 500.0f);

    // Now seek_speed_down (400 units/s): target 100.0f
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 100.0f, 0) == OAE_OK);
    // Render 24000 frames (0.5 s) -> delta = 400 * 0.5 = 200 -> cur should be 300.0f
    buf.resize(24000 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 24000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "rpm", &cur, &final_val) == OAE_OK);
    CHECK(std::abs(cur - 300.0f) < 2.0f);
    CHECK(final_val == 100.0f);

    // Test ignore_seek_speed == 1
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "rpm", 850.0f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "rpm", &cur, &final_val) == OAE_OK);
    CHECK(cur == 850.0f);
    CHECK(final_val == 850.0f);

    oae_event_instance_release(inst);
    oae_sound_release(snd);
}

TEST_CASE("T10a: Conditional instrument triggering and timeline loop control")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/conditional_event");
    ev.add_parameter(oae::ParameterDefinition::labeled("surface", {"dirt", "wood"}, 0.0f));
    ev.add_parameter(oae::ParameterDefinition::continuous("loop_active", 0.0f, 1.0f, 1.0f));

    auto snd_dirt = make_sine(fix.handle, 200.0, 96000, 0.4f);
    auto snd_wood = make_sine(fix.handle, 800.0, 96000, 0.4f);

    oae::InstrumentBuilder inst1;
    inst1.sound(snd_dirt).length_ms(1000.0);
    inst1.add_condition_term("surface", 0.0f, 0.0f);

    oae::InstrumentBuilder inst2;
    inst2.sound(snd_wood).length_ms(1000.0);
    inst2.add_condition_term("surface", 1.0f, 1.0f);

    oae::TrackBuilder track;
    track.add_instrument(inst1);
    track.add_instrument(inst2);
    ev.add_track(track);

    // Loop region between 50ms and 100ms conditioned to loop_active in [1.0, 1.0]
    auto loop_item = oae::LogicItem::loop_region(50.0, 100.0, -1);
    loop_item.add_condition_term("loop_active", 1.0f, 1.0f);
    ev.add_logic_item(loop_item);

    builder.add_event(ev);
    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/conditional_event", &desc) == OAE_OK);

    // Test 1: surface = "wood" triggers inst2 only
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_set_parameter_by_name_with_label(inst, "surface", "wood", 1) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 12000 frames (250 ms at 48kHz).
    // Loop is active, so timeline position should remain looped inside [50, 100] ms!
    std::vector<float> buf(12000 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 12000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    int32_t pos_ms = 0;
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms >= 50);
    CHECK(pos_ms <= 100);

    // Deactivate loop condition
    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "loop_active", 0.0f, 1) == OAE_OK);

    // Render another 12000 frames (250 ms) -> timeline must advance past 100 ms!
    REQUIRE(oae_system_render(fix.handle, buf.data(), 12000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms > 100);

    oae_event_instance_release(inst);
    oae_sound_release(snd_dirt);
    oae_sound_release(snd_wood);
}

TEST_CASE("T10b: Parameter velocity and clamping")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("global_decay", 0.0f, 100.0f, 30.0f).with_velocity(-50.0f));
    oae::EventBuilder ev("event:/param_velocity");
    ev.add_parameter(oae::ParameterDefinition::continuous("rpm", 0.0f, 100.0f, 10.0f).with_velocity(20.0f));
    auto snd = make_sine(fix.handle, 330.0, 48000, 0.25f);
    oae::InstrumentBuilder inst;
    inst.sound(snd).length_ms(5000.0);
    oae::TrackBuilder track;
    track.add_instrument(inst);
    ev.add_track(track);
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/param_velocity", &desc) == OAE_OK);
    oae_event_instance instance = 0;
    REQUIRE(oae_event_description_create_instance(desc, &instance) == OAE_OK);
    REQUIRE(oae_event_instance_start(instance) == OAE_OK);

    std::vector<float> buf(24000 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 24000) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    float rpm_val = 0.0f;
    float decay_val = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(instance, "rpm", &rpm_val, nullptr) == OAE_OK);
    REQUIRE(oae_studio_get_global_parameter_by_name(fix.handle, "global_decay", &decay_val, nullptr) == OAE_OK);
    CHECK(rpm_val == doctest::Approx(20.0f).epsilon(0.05));
    CHECK(decay_val == doctest::Approx(5.0f).epsilon(0.05));

    // Advance 5 seconds -> rpm clamps at 100.0f, global_decay clamps at 0.0f
    for (int i = 0; i < 10; ++i) {
        REQUIRE(oae_system_render(fix.handle, buf.data(), 24000) == OAE_OK);
    }
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(instance, "rpm", &rpm_val, nullptr) == OAE_OK);
    REQUIRE(oae_studio_get_global_parameter_by_name(fix.handle, "global_decay", &decay_val, nullptr) == OAE_OK);
    CHECK(rpm_val == doctest::Approx(100.0f));
    CHECK(decay_val == doctest::Approx(0.0f));

    oae_event_instance_release(instance);
    oae_sound_release(snd);
}

TEST_CASE("T10b: Parameter hold_during_playback locks active instance values")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("global_held", 0.0f, 1.0f, 0.2f).with_hold(true));
    oae::EventBuilder ev("event:/param_hold");
    ev.add_parameter(oae::ParameterDefinition::continuous("local_held", 0.0f, 1.0f, 0.25f).with_hold(true));
    ev.add_parameter(oae::ParameterDefinition::labeled("mode_held", {"a", "b"}, 0.0f).with_hold(true));
    auto snd = make_sine(fix.handle, 440.0, 48000, 0.2f);
    oae::InstrumentBuilder inst;
    inst.sound(snd).length_ms(1000.0);
    oae::TrackBuilder track;
    track.add_instrument(inst);
    ev.add_track(track);
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/param_hold", &desc) == OAE_OK);

    oae_event_instance inst_a = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst_a) == OAE_OK);
    REQUIRE(oae_event_instance_set_parameter_by_name(inst_a, "local_held", 0.5f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst_a) == OAE_OK);

    // Active local param ignores mutations
    REQUIRE(oae_event_instance_set_parameter_by_name(inst_a, "local_held", 0.9f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_set_parameter_by_name_with_label(inst_a, "mode_held", "b", 1) == OAE_OK);
    CHECK(oae_event_instance_set_parameter_by_name_with_label(inst_a, "mode_held", "invalid", 1) == OAE_ERR_INVALID_PARAM);

    float local_v = 0.0f;
    float mode_v = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_a, "local_held", &local_v, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_a, "mode_held", &mode_v, nullptr) == OAE_OK);
    CHECK(local_v == doctest::Approx(0.5f));
    CHECK(mode_v == doctest::Approx(0.0f));

    // Global param changes system-wide, but active inst_a keeps snapshot (0.2f) while new inst_b sees 0.8f
    REQUIRE(oae_studio_set_global_parameter_by_name(fix.handle, "global_held", 0.8f, 1) == OAE_OK);
    float global_a = 0.0f;
    float global_sys = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_a, "global_held", &global_a, nullptr) == OAE_OK);
    REQUIRE(oae_studio_get_global_parameter_by_name(fix.handle, "global_held", &global_sys, nullptr) == OAE_OK);
    CHECK(global_a == doctest::Approx(0.2f));
    CHECK(global_sys == doctest::Approx(0.8f));

    oae_event_instance inst_b = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst_b) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst_b) == OAE_OK);
    float global_b = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_b, "global_held", &global_b, nullptr) == OAE_OK);
    CHECK(global_b == doctest::Approx(0.8f));

    // Once stopped, local param unlocks
    REQUIRE(oae_event_instance_stop(inst_a, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_set_parameter_by_name(inst_a, "local_held", 0.85f, 1) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst_a, "local_held", &local_v, nullptr) == OAE_OK);
    CHECK(local_v == doctest::Approx(0.85f));

    oae_event_instance_release(inst_b);
    oae_event_instance_release(inst_a);
    oae_sound_release(snd);
}

TEST_CASE("T10c: Parameter Sheet triggers and stops instruments by range")
{
    SystemFixture fix;

    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("mood", 0.0f, 10.0f, 0.0f));
    oae::EventBuilder ev("event:/param_sheet");
    ev.add_parameter(oae::ParameterDefinition::continuous("speed", 0.0f, 100.0f, 0.0f));

    auto snd_speed = make_sine(fix.handle, 440.0, 96000, 0.4f);
    auto snd_mood = make_sine(fix.handle, 880.0, 96000, 0.4f);

    oae::InstrumentBuilder inst_speed;
    inst_speed.sound(snd_speed).loop(true).parameter_region("speed", 20.0, 60.0);
    oae::InstrumentBuilder inst_mood;
    inst_mood.sound(snd_mood).loop(true).parameter_region("mood", 5.0, 10.0);

    oae::TrackBuilder track;
    track.add_instrument(inst_speed);
    track.add_instrument(inst_mood);
    ev.add_track(track);
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    const auto compute_rms = [](const std::vector<float>& samples) {
        double acc = 0.0;
        for (float s : samples) acc += static_cast<double>(s) * static_cast<double>(s);
        return std::sqrt(acc / static_cast<double>(samples.size()));
    };

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/param_sheet", &desc) == OAE_OK);
    oae_event_instance instance = 0;
    REQUIRE(oae_event_description_create_instance(desc, &instance) == OAE_OK);
    REQUIRE(oae_event_instance_start(instance) == OAE_OK);

    std::vector<float> buf(2400 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    CHECK(compute_rms(buf) < 0.001);

    REQUIRE(oae_event_instance_set_parameter_by_name(instance, "speed", 35.0f, 1) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    CHECK(compute_rms(buf) > 0.05);

    REQUIRE(oae_event_instance_set_parameter_by_name(instance, "speed", 75.0f, 1) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    CHECK(compute_rms(buf) < 0.001);

    REQUIRE(oae_studio_set_global_parameter_by_name(fix.handle, "mood", 7.0f, 1) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    CHECK(compute_rms(buf) > 0.05);

    oae_event_instance_release(instance);
    oae_sound_release(snd_speed);
    oae_sound_release(snd_mood);
}

TEST_CASE("T10d: 3D attributes, built-in 3D parameters, cone attenuation, and spatializer envelopment")
{
    SystemFixture fix;

    const auto compute_lr_rms = [](const std::vector<float>& interleaved) {
        double acc_l = 0.0;
        double acc_r = 0.0;
        const std::size_t frames = interleaved.size() / 2;
        for (std::size_t i = 0; i < frames; ++i) {
            const double l = interleaved[2 * i];
            const double r = interleaved[2 * i + 1];
            acc_l += l * l;
            acc_r += r * r;
        }
        return std::pair<double, double>{
            std::sqrt(acc_l / static_cast<double>(frames)),
            std::sqrt(acc_r / static_cast<double>(frames))
        };
    };

    auto snd_main = make_sine(fix.handle, 440.0, 96000, 0.5f);
    auto snd_sheet = make_sine(fix.handle, 880.0, 96000, 0.4f);

    oae::ModelSetBuilder builder;
    oae::EventBuilder ev("event:/spatial_3d");
    oae::InstrumentBuilder inst_main;
    inst_main.sound(snd_main).loop(true);
    oae::InstrumentBuilder inst_sheet;
    inst_sheet.sound(snd_sheet).loop(true).parameter_region("Distance", 0.0, 4.0);

    oae::TrackBuilder track;
    track.add_instrument(inst_main);
    track.add_instrument(inst_sheet);
    ev.add_track(track);
    builder.add_event(ev);

    auto model_set = builder.build();
    auto* sys = oae::runtime.systems.get(fix.handle);
    REQUIRE(sys);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/spatial_3d", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);

    // Null & invalid parameter guards
    oae_3d_attributes bad_attrs{};
    bad_attrs.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(oae_system_set_listener_3d_attributes(fix.handle, nullptr) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_set_listener_3d_attributes(fix.handle, &bad_attrs) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_event_instance_set_3d_attributes(inst, nullptr) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_event_instance_set_3d_attributes(inst, &bad_attrs) == OAE_ERR_INVALID_PARAM);

    oae_spatializer_settings bad_settings{1, OAE_ROLLOFF_LINEAR, 10.0f, 5.0f, 0.0f, 0.0f, 360.0f, 360.0f, 1.0f};
    CHECK(oae_event_instance_set_spatializer_settings(inst, &bad_settings) == OAE_ERR_INVALID_PARAM);

    // Listener at origin facing +Z with up +Y; emitter at (3, 4, 0) with velocity (3, 0, 4)
    oae_3d_attributes emitter_attrs{
        {3.0f, 4.0f, 0.0f},
        {3.0f, 0.0f, 4.0f},
        {-0.6f, -0.8f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &emitter_attrs) == OAE_OK);

    float dist = 0.0f, dir = 0.0f, elev = 0.0f, cone = 0.0f, spd = 0.0f;
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "Distance", &dist, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "Direction", &dir, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "Elevation", &elev, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "EventConeAngle", &cone, nullptr) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "Speed", &spd, nullptr) == OAE_OK);

    CHECK(dist == doctest::Approx(5.0f).epsilon(0.01));
    CHECK(dir == doctest::Approx(90.0f).epsilon(0.01));
    CHECK(elev == doctest::Approx(53.1301f).epsilon(0.05));
    CHECK(cone == doctest::Approx(0.0f).epsilon(0.05));
    CHECK(spd == doctest::Approx(5.0f).epsilon(0.01));

    // Move listener so emitter at (3, 4, 0) is to the left when listener faces -Z
    oae_3d_attributes listener_rot{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    REQUIRE(oae_system_set_listener_3d_attributes(fix.handle, &listener_rot) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "Direction", &dir, nullptr) == OAE_OK);
    CHECK(dir == doctest::Approx(-90.0f).epsilon(0.05));

    // Restore default listener
    oae_3d_attributes listener_default{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}};
    REQUIRE(oae_system_set_listener_3d_attributes(fix.handle, &listener_default) == OAE_OK);

    // Verify distance rolloff and directional cone on rendered output
    oae_3d_attributes front_attrs{
        {0.0f, 0.0f, 6.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &front_attrs) == OAE_OK);

    oae_spatializer_settings spatial{
        1, OAE_ROLLOFF_LINEAR, 2.0f, 10.0f, 0.0f, 0.0f, 60.0f, 120.0f, 0.25f
    };
    REQUIRE(oae_event_instance_set_spatializer_settings(inst, &spatial) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    std::vector<float> buf(2400 * 2);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    const auto [rms_lin_l, rms_lin_r] = compute_lr_rms(buf);
    const double rms_lin = 0.5 * (rms_lin_l + rms_lin_r);
    CHECK(rms_lin > 0.05);

    // Quadratic rolloff at dist=6 (linear=0.5 => squared=0.25) should equal ~0.5 * linear RMS
    spatial.rolloff = OAE_ROLLOFF_LINEAR_SQUARED;
    REQUIRE(oae_event_instance_set_spatializer_settings(inst, &spatial) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    const auto [rms_sq_l, rms_sq_r] = compute_lr_rms(buf);
    const double rms_sq = 0.5 * (rms_sq_l + rms_sq_r);
    CHECK(rms_sq == doctest::Approx(rms_lin * 0.5).epsilon(0.05));

    // Turn emitter forward away from listener (+Z) -> outside outer cone -> cone_outer_gain = 0.25
    spatial.rolloff = OAE_ROLLOFF_LINEAR;
    REQUIRE(oae_event_instance_set_spatializer_settings(inst, &spatial) == OAE_OK);
    front_attrs.forward = {0.0f, 0.0f, 1.0f};
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &front_attrs) == OAE_OK);
    REQUIRE(oae_event_instance_get_parameter_by_name(inst, "EventConeAngle", &cone, nullptr) == OAE_OK);
    CHECK(cone == doctest::Approx(180.0f).epsilon(0.05));

    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    const auto [rms_cone_l, rms_cone_r] = compute_lr_rms(buf);
    const double rms_cone = 0.5 * (rms_cone_l + rms_cone_r);
    CHECK(rms_cone == doctest::Approx(rms_lin * 0.25).epsilon(0.05));

    // Verify Envelopment (Sound Size & Min Extent): hard right vs wide extent
    oae_3d_attributes right_attrs{
        {2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &right_attrs) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    const auto [rms_hard_l, rms_hard_r] = compute_lr_rms(buf);
    CHECK(rms_hard_r > rms_hard_l * 4.0);

    spatial.min_extent_deg = 180.0f;
    REQUIRE(oae_event_instance_set_spatializer_settings(inst, &spatial) == OAE_OK);
    REQUIRE(oae_system_render(fix.handle, buf.data(), 2400) == OAE_OK);
    const auto [rms_wide_l, rms_wide_r] = compute_lr_rms(buf);
    CHECK(rms_wide_l == doctest::Approx(rms_wide_r).epsilon(0.05));

    // Also verify built-in Distance parameter sheet triggers inst_sheet inside [0, 4]
    spatial.min_extent_deg = 0.0f;
    spatial.min_distance = 1.0f;
    spatial.max_distance = 20.0f;
    REQUIRE(oae_event_instance_set_spatializer_settings(inst, &spatial) == OAE_OK);
    right_attrs.position = {3.0f, 0.0f, 0.0f};
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &right_attrs) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    int total_ch = 0, real_ch = 0;
    REQUIRE(oae_system_get_channels_playing(fix.handle, &total_ch, &real_ch) == OAE_OK);
    CHECK(total_ch == 2);

    right_attrs.position = {8.0f, 0.0f, 0.0f};
    REQUIRE(oae_event_instance_set_3d_attributes(inst, &right_attrs) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    REQUIRE(oae_system_get_channels_playing(fix.handle, &total_ch, &real_ch) == OAE_OK);
    CHECK(total_ch == 1);

    oae_event_instance_release(inst);
    oae_sound_release(snd_main);
    oae_sound_release(snd_sheet);
}
