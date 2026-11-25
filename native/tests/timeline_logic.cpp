#include "playback_fixture.h"
#include "../src/core/studio_model.h"
#include "../src/core/event_instance.h"
#include "../src/api/runtime.h"

#include <cmath>
#include <thread>
#include <vector>

using namespace oae::test;

namespace {

oae_sound make_sine(oae_system system, double hz, std::uint32_t frames, float amp = 0.5f, bool loop = true)
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
    REQUIRE(oae_sound_create_from_memory(system, wav.data(), wav.size(), loop ? OAE_SOUND_LOOP : 0u, &sound) == OAE_OK);
    return sound;
}

double compute_rms(const std::vector<float>& samples)
{
    if (samples.empty()) return 0.0;
    double sum_sq = 0.0;
    for (float s : samples) {
        sum_sq += static_cast<double>(s) * static_cast<double>(s);
    }
    return std::sqrt(sum_sq / static_cast<double>(samples.size()));
}

struct CallbackCapture {
    std::thread::id caller_thread;
    std::vector<std::uint64_t> marker_clocks;
    std::vector<std::string> marker_names;
    std::vector<std::uint64_t> beat_clocks;
    std::vector<std::pair<int, int>> beat_bars;
    bool started = false;
    bool stopped = false;
};

oae_result OAE_CALL test_callback(uint32_t type, oae_event_instance /*instance*/, const void* properties, void* user_data)
{
    auto* cap = static_cast<CallbackCapture*>(user_data);
    cap->caller_thread = std::this_thread::get_id();

    if (type == OAE_EVENT_CALLBACK_STARTED) {
        cap->started = true;
    } else if (type == OAE_EVENT_CALLBACK_STOPPED) {
        cap->stopped = true;
    } else if (type == OAE_EVENT_CALLBACK_MARKER && properties) {
        const auto* mp = static_cast<const oae_timeline_marker_properties*>(properties);
        cap->marker_clocks.push_back(mp->dsp_clock);
        if (mp->name) {
            cap->marker_names.emplace_back(mp->name);
        }
    } else if (type == OAE_EVENT_CALLBACK_BEAT && properties) {
        const auto* bp = static_cast<const oae_timeline_beat_properties*>(properties);
        cap->beat_clocks.push_back(bp->dsp_clock);
        cap->beat_bars.emplace_back(bp->bar, bp->beat);
    }
    return OAE_OK;
}

}

TEST_CASE("MARKER callback reports exact dsp_clock and runs on game thread")
{
    SystemFixture system;
    const auto sound = make_sine(system.handle, 440, 48000);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/markers")
            .add_logic_item(oae::LogicItem::marker("intro", 100.0))
            .add_logic_item(oae::LogicItem::marker("drop", 250.0))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(1000.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/markers", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);

    CallbackCapture capture;
    REQUIRE(oae_event_instance_set_callback(inst, test_callback, OAE_EVENT_CALLBACK_ALL, &capture) == OAE_OK);

    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 4800 frames (100ms at 48kHz = 4800 frames)
    render(system.handle, 4800);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    CHECK(capture.caller_thread == std::this_thread::get_id());
    REQUIRE(capture.marker_names.size() == 1);
    CHECK(capture.marker_names[0] == "intro");
    CHECK(capture.marker_clocks[0] == 4800);

    // Render 7200 more frames (total 12000 frames = 250ms)
    render(system.handle, 7200);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    REQUIRE(capture.marker_names.size() == 2);
    CHECK(capture.marker_names[1] == "drop");
    CHECK(capture.marker_clocks[1] == 12000);

    oae_event_instance_release(inst);
}

TEST_CASE("BEAT callback at 120 BPM arrives every 24000 frames")
{
    SystemFixture system;
    const auto sound = make_sine(system.handle, 440, 48000);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/beats")
            .add_logic_item(oae::LogicItem::tempo(120.0, 4, 4))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(10000.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/beats", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);

    CallbackCapture capture;
    REQUIRE(oae_event_instance_set_callback(inst, test_callback, OAE_EVENT_CALLBACK_BEAT, &capture) == OAE_OK);

    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 3 full beats (72000 frames)
    render(system.handle, 72000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    // Expect beat 0 (0), beat 1 (24000), beat 2 (48000), beat 3 (72000)
    REQUIRE(capture.beat_clocks.size() >= 4);
    CHECK(capture.beat_clocks[0] == 0);
    CHECK(capture.beat_clocks[1] == 24000);
    CHECK(capture.beat_clocks[2] == 48000);
    CHECK(capture.beat_clocks[3] == 72000);

    CHECK(capture.beat_bars[0] == std::make_pair(0, 0));
    CHECK(capture.beat_bars[1] == std::make_pair(0, 1));
    CHECK(capture.beat_bars[2] == std::make_pair(0, 2));
    CHECK(capture.beat_bars[3] == std::make_pair(0, 3));

    oae_event_instance_release(inst);
}

TEST_CASE("Sustain loop region repeats until key_off then proceeds to tail")
{
    SystemFixture system;
    const auto sound = make_sine(system.handle, 440, 48000);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/loop_key_off")
            // Loop between 100ms and 300ms until key_off
            .add_logic_item(oae::LogicItem::loop_region(100.0, 300.0, -1, true))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(1000.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/loop_key_off", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 600ms (28800 frames): would pass 300ms twice if not looping
    render(system.handle, 28800);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    int32_t pos_ms = 0;
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    // Position must be kept inside [100ms, 300ms]
    CHECK(pos_ms >= 100);
    CHECK(pos_ms <= 300);

    // Now send key_off
    REQUIRE(oae_event_instance_key_off(inst) == OAE_OK);

    // Render another 500ms (24000 frames): must break out past 300ms!
    render(system.handle, 24000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms > 300);

    oae_event_instance_release(inst);
}

TEST_CASE("BAR transition jumps exactly on bar boundary")
{
    SystemFixture system;
    const auto sound = make_sine(system.handle, 440, 48000);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/bar_transition")
            .add_logic_item(oae::LogicItem::tempo(120.0, 4, 4))
            .add_logic_item(oae::LogicItem::transition_region(0.0, 5000.0, "section_b", oae::LogicQuantization::Bar))
            .add_logic_item(oae::LogicItem::marker("section_b", 8000.0))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(15000.0)))
    );

    oae::LogicItem target_marker;
    target_marker.kind = oae::LogicItemKind::TransitionMarker;
    target_marker.name = "section_b";
    target_marker.position_ms = 8000.0;

    builder.add_event(
        oae::EventBuilder("event:/bar_trans")
            .add_logic_item(oae::LogicItem::tempo(120.0, 4, 4))
            .add_logic_item(oae::LogicItem::transition_region(0.0, 5000.0, "section_b", oae::LogicQuantization::Bar))
            .add_logic_item(target_marker)
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(15000.0)))
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    const char* event_path = "event:/bar_transition";
    SUBCASE("canonical Marker destination") { event_path = "event:/bar_transition"; }
    SUBCASE("legacy TransitionMarker destination without destination") { event_path = "event:/bar_trans"; }

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, event_path, &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // At 120 BPM 4/4: 1 bar = 4 beats = 96000 frames (2000ms)
    // Render 2 beats (48000 frames): halfway through bar 0
    render(system.handle, 48000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    int32_t pos_ms = 0;
    oae_event_instance_get_timeline_position_ms(inst, &pos_ms);
    CHECK(pos_ms < 8000);

    // Render 48000 more frames to reach frame 96000 (bar 1 starts! beat == 0)
    render(system.handle, 48000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    oae_event_instance_get_timeline_position_ms(inst, &pos_ms);
    // Transition landed on target marker at 8000ms!
    CHECK(pos_ms >= 8000);

    oae_event_instance_release(inst);
}

TEST_CASE("SustainPoint pauses timeline and sync voices until key_off")
{
    SystemFixture system;
    const auto sound = make_sine(system.handle, 440, 48000);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/sustain_test")
            .add_logic_item(oae::LogicItem::sustain_point(200.0))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(1000.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/sustain_test", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Advance 400ms (19200 frames): should stop at SustainPoint (200ms)
    render(system.handle, 19200);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);

    int st = OAE_PLAYBACK_STOPPED;
    REQUIRE(oae_event_instance_get_playback_state(inst, &st) == OAE_OK);
    CHECK(st == OAE_PLAYBACK_SUSTAINING);

    int32_t pos_ms = 0;
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms == 200);

    // Render another 200ms while sustaining: timeline stays frozen at 200ms
    render(system.handle, 9600);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms == 200);

    // Trigger key_off: playback resumes past 200ms and stops cleanly after 1000ms
    REQUIRE(oae_event_instance_key_off(inst) == OAE_OK);
    render(system.handle, 19200);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_event_instance_get_playback_state(inst, &st) == OAE_OK);
    CHECK(st == OAE_PLAYBACK_PLAYING);
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms > 200);

    render(system.handle, 48000);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_event_instance_get_playback_state(inst, &st) == OAE_OK);
    CHECK(st == OAE_PLAYBACK_STOPPED);

    oae_event_instance_release(inst);
}

TEST_CASE("SustainPoint plays instrument at and after sustain point after key_off")
{
    SystemFixture system;
    const auto charge = make_sine(system.handle, 440, 9600);  // 200ms
    const auto release = make_sine(system.handle, 880, 14400); // 300ms

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/charge_sustain_audio")
            .add_logic_item(oae::LogicItem::sustain_point(200.0))
            .add_track(
                oae::TrackBuilder()
                    .add_instrument(oae::InstrumentBuilder().sound(charge).offset_ms(0.0).length_ms(200.0))
                    .add_instrument(oae::InstrumentBuilder().sound(release).offset_ms(200.0).length_ms(300.0))
            )
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/charge_sustain_audio", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    const auto max_abs = [](const std::vector<float>& samples) {
        float m = 0.0f;
        for (float s : samples) m = std::max(m, std::abs(s));
        return m;
    };

    // Render 250ms (12000 frames) in small blocks -> should hit SustainPoint at 200ms
    float peak_charge = 0.0f;
    for (int i = 0; i < 25; ++i) {
        const auto buf = render(system.handle, 480);
        peak_charge = std::max(peak_charge, max_abs(buf));
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
    }
    CHECK(peak_charge > 0.05f);

    int st = OAE_PLAYBACK_STOPPED;
    REQUIRE(oae_event_instance_get_playback_state(inst, &st) == OAE_OK);
    CHECK(st == OAE_PLAYBACK_SUSTAINING);

    // Wait 500ms while sustaining: should be silent
    float peak_sustain = 0.0f;
    for (int i = 0; i < 50; ++i) {
        const auto buf = render(system.handle, 480);
        peak_sustain = std::max(peak_sustain, max_abs(buf));
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
    }
    CHECK(peak_sustain < 0.01f);

    // Press key_off: release instrument at 200ms MUST produce audio!
    REQUIRE(oae_event_instance_key_off(inst) == OAE_OK);
    float peak_release = 0.0f;
    for (int i = 0; i < 35; ++i) {
        const auto buf = render(system.handle, 480);
        peak_release = std::max(peak_release, max_abs(buf));
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
    }
    CHECK(peak_release > 0.05f);

    oae_event_instance_release(inst);
}

TEST_CASE("LoopRegion UntilKeyOff plays post-loop instrument after key_off")
{
    SystemFixture system;
    const auto charge = make_sine(system.handle, 440, 9600);  // 200ms
    const auto hold = make_sine(system.handle, 550, 9600);    // 200ms
    const auto release = make_sine(system.handle, 880, 14400); // 300ms

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/charge_loop_hold_audio")
            .add_logic_item(oae::LogicItem::loop_region(200.0, 400.0, -1, true))
            .add_track(
                oae::TrackBuilder()
                    .add_instrument(oae::InstrumentBuilder().sound(charge).offset_ms(0.0).length_ms(200.0))
                    .add_instrument(oae::InstrumentBuilder().sound(hold).offset_ms(200.0).length_ms(200.0))
                    .add_instrument(oae::InstrumentBuilder().sound(release).offset_ms(400.0).length_ms(300.0))
            )
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/charge_loop_hold_audio", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    const auto max_abs = [](const std::vector<float>& samples) {
        float m = 0.0f;
        for (float s : samples) m = std::max(m, std::abs(s));
        return m;
    };

    // Render 800ms (80 blocks of 10ms) -> loops inside [200ms..400ms], never finishes
    for (int i = 0; i < 80; ++i) {
        render(system.handle, 480);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
    }
    int st = OAE_PLAYBACK_STOPPED;
    REQUIRE(oae_event_instance_get_playback_state(inst, &st) == OAE_OK);
    CHECK(st == OAE_PLAYBACK_PLAYING);
    int32_t pos_ms = 0;
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms >= 200);
    CHECK(pos_ms < 400);

    // Press key_off: loop exits at 400ms and release instrument (400..700ms) MUST play!
    REQUIRE(oae_event_instance_key_off(inst) == OAE_OK);
    float peak_post_loop = 0.0f;
    for (int i = 0; i < 35; ++i) {
        const auto buf = render(system.handle, 480);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        if (pos_ms >= 400) {
            peak_post_loop = std::max(peak_post_loop, max_abs(buf));
        }
    }
    CHECK(peak_post_loop > 0.05f);

    oae_event_instance_release(inst);
}

TEST_CASE("TransitionMarker evaluates condition, Immediate, Beat and Bar quantization to Marker")
{
    SystemFixture system;
    const auto tone = make_sine(system.handle, 440, 48000, 0.5f, true);
    const auto post_jump_tone = make_sine(system.handle, 880, 24000, 0.5f, false);

    oae::ParameterDefinition jump_param;
    jump_param.name = "jump";
    jump_param.kind = oae::ParameterKind::Continuous;
    jump_param.min_value = 0.0f;
    jump_param.max_value = 1.0f;
    jump_param.default_value = 0.0f;

    auto tm_immediate = oae::LogicItem::transition_marker(200.0, "dest", oae::LogicQuantization::Immediate);
    tm_immediate.add_condition_term("jump", 0.5f, 1.0f, false);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/tm_immediate")
            .add_parameter(jump_param)
            .add_logic_item(tm_immediate)
            .add_logic_item(oae::LogicItem::marker("dest", 1200.0))
            .add_track(
                oae::TrackBuilder()
                    .add_instrument(oae::InstrumentBuilder().sound(tone).offset_ms(0.0).length_ms(1000.0))
                    .add_instrument(oae::InstrumentBuilder().sound(post_jump_tone).offset_ms(1500.0).length_ms(500.0))
            )
    );
    builder.add_event(
        oae::EventBuilder("event:/tm_beat")
            .add_logic_item(oae::LogicItem::tempo(120.0, 4, 4))
            .add_logic_item(oae::LogicItem::transition_marker(650.0, "drop", oae::LogicQuantization::Beat))
            .add_logic_item(oae::LogicItem::marker("drop", 4000.0))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(tone).offset_ms(0.0).length_ms(6000.0)))
    );
    builder.add_event(
        oae::EventBuilder("event:/tm_bar")
            .add_logic_item(oae::LogicItem::tempo(120.0, 4, 4))
            .add_logic_item(oae::LogicItem::transition_marker(300.0, "bar_target", oae::LogicQuantization::Bar))
            .add_logic_item(oae::LogicItem::marker("bar_target", 6000.0))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(tone).offset_ms(0.0).length_ms(8000.0)))
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    SUBCASE("Condition false ignores TransitionMarker; condition true jumps immediately and schedules future clips") {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/tm_immediate", &desc) == OAE_OK);

        oae_event_instance inst_skip = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst_skip) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst_skip) == OAE_OK);
        render(system.handle, 14400);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        int32_t pos_ms = 0;
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst_skip, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 300);
        oae_event_instance_stop(inst_skip, OAE_STOP_IMMEDIATE);
        oae_event_instance_release(inst_skip);

        oae_event_instance inst_jump = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst_jump) == OAE_OK);
        REQUIRE(oae_event_instance_set_parameter_by_name(inst_jump, "jump", 1.0f, 1) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst_jump) == OAE_OK);
        render(system.handle, 14400);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst_jump, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 1200);

        render(system.handle, 19200);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst_jump, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 1600);
        const auto buf = render(system.handle, 2400);
        CHECK(compute_rms(buf) > 0.05);

        oae_event_instance_release(inst_jump);
    }

    SUBCASE("Beat quantized TransitionMarker waits for next beat at or after marker position") {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/tm_beat", &desc) == OAE_OK);

        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        render(system.handle, 33600);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        int32_t pos_ms = 0;
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 700);

        render(system.handle, 14400);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms == 4000);

        oae_event_instance_release(inst);
    }

    SUBCASE("Bar quantized TransitionMarker waits for next bar boundary") {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/tm_bar", &desc) == OAE_OK);

        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        render(system.handle, 48000);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        int32_t pos_ms = 0;
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms < 6000);

        render(system.handle, 24000);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms < 6000);

        render(system.handle, 24000);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
        CHECK(pos_ms >= 6000);

        oae_event_instance_release(inst);
    }
}

TEST_CASE("Transition jump with crossfade_ms overlaps fade-out and fade-in cleanly")
{
    SystemFixture system;
    const auto silent_a = make_sine(system.handle, 440, 48000, 0.0f, false);
    const auto loud_b = make_sine(system.handle, 880, 48000, 0.5f, false);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/tm_crossfade")
            .add_logic_item(oae::LogicItem::transition_marker(100.0, "section_b", oae::LogicQuantization::Immediate, 200.0))
            .add_logic_item(oae::LogicItem::marker("section_b", 2000.0))
            .add_track(
                oae::TrackBuilder()
                    .add_instrument(oae::InstrumentBuilder().sound(silent_a).offset_ms(0.0).length_ms(1000.0))
                    .add_instrument(oae::InstrumentBuilder().sound(loud_b).offset_ms(2000.0).length_ms(1000.0))
            )
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/tm_crossfade", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    int total = 0;
    int real = 0;
    REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
    CHECK(total == 1);

    render(system.handle, 5760);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    int32_t pos_ms = 0;
    REQUIRE(oae_event_instance_get_timeline_position_ms(inst, &pos_ms) == OAE_OK);
    CHECK(pos_ms == 2000);

    REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
    CHECK(total == 2);

    const auto early_fade_buf = render(system.handle, 960);
    const double early_rms = compute_rms(early_fade_buf);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
    CHECK(total == 2);

    render(system.handle, 3840);
    const auto mid_fade_buf = render(system.handle, 960);
    const double mid_rms = compute_rms(mid_fade_buf);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
    CHECK(total == 2);
    CHECK(mid_rms > early_rms);

    render(system.handle, 4800);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto post_fade_buf = render(system.handle, 960);
    const double post_rms = compute_rms(post_fade_buf);
    REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
    CHECK(total == 1);
    CHECK(post_rms > mid_rms);

    oae_event_instance_release(inst);
}

TEST_CASE("Scatterer instrument steals oldest grain when polyphony limit is reached")
{
    SystemFixture system;
    const auto loud = make_sine(system.handle, 440, 48000, 0.5f, false);
    const auto silent = make_sine(system.handle, 440, 48000, 0.0f, false);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/scatterer_steal_loud_first")
            .add_track(
                oae::TrackBuilder().add_instrument(
                    oae::InstrumentBuilder()
                        .scatterer(50.0, 50.0, 2, 4, 1.0f, 1.0f, oae::PlaylistMode::Sequential, oae::PlaylistScope::Instance)
                        .offset_ms(0.0)
                        .length_ms(1000.0)
                        .add_entry(loud, 1.0f)
                        .add_entry(loud, 1.0f)
                        .add_entry(silent, 1.0f)
                        .add_entry(silent, 1.0f)
                )
            )
    );
    builder.add_event(
        oae::EventBuilder("event:/scatterer_steal_silent_first")
            .add_track(
                oae::TrackBuilder().add_instrument(
                    oae::InstrumentBuilder()
                        .scatterer(50.0, 50.0, 2, 3, 1.0f, 1.0f, oae::PlaylistMode::Sequential, oae::PlaylistScope::Instance)
                        .offset_ms(0.0)
                        .length_ms(1000.0)
                        .add_entry(silent, 1.0f)
                        .add_entry(silent, 1.0f)
                        .add_entry(loud, 1.0f)
                )
            )
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    SUBCASE("Oldest active grains are stopped when polyphony limit is reached") {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/scatterer_steal_loud_first", &desc) == OAE_OK);
        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        int total = 0;
        int real = 0;
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 1);
        CHECK(compute_rms(render(system.handle, 480)) > 0.05);

        render(system.handle, 1920);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 2);
        CHECK(compute_rms(render(system.handle, 480)) > 0.05);

        render(system.handle, 1920);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 2);
        CHECK(compute_rms(render(system.handle, 480)) > 0.05);

        render(system.handle, 1920);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 2);
        CHECK(compute_rms(render(system.handle, 960)) < 0.001);

        oae_event_instance_release(inst);
    }

    SUBCASE("New grains continue spawning while oldest grains are stolen") {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/scatterer_steal_silent_first", &desc) == OAE_OK);
        oae_event_instance inst = 0;
        REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
        REQUIRE(oae_event_instance_start(inst) == OAE_OK);

        int total = 0;
        int real = 0;
        CHECK(compute_rms(render(system.handle, 480)) < 0.001);

        render(system.handle, 1920);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 2);
        CHECK(compute_rms(render(system.handle, 480)) < 0.001);

        render(system.handle, 1920);
        REQUIRE(oae_system_update(system.handle) == OAE_OK);
        REQUIRE(oae_system_get_channels_playing(system.handle, &total, &real) == OAE_OK);
        CHECK(total == 2);
        CHECK(compute_rms(render(system.handle, 960)) > 0.05);

        oae_event_instance_release(inst);
    }
}
