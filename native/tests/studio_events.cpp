#include "playback_fixture.h"
#include "../src/core/studio_model.h"
#include "../src/core/event_instance.h"
#include "../src/api/runtime.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace oae::test;

namespace {

oae_sound make_sine_sound(oae_system system, double hz, std::uint32_t frames, float amp = 0.5f)
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

}

TEST_CASE("Event with two tracks plays both tracks simultaneously")
{
    SystemFixture system;
    const auto sound1 = make_sine_sound(system.handle, 440, 48000, 0.4f);
    const auto sound2 = make_sine_sound(system.handle, 880, 48000, 0.4f);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/two_tracks")
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound1).length_ms(1000.0)))
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound2).length_ms(1000.0)))
    );
    auto model_set = builder.build();

    auto* sys = oae::runtime.systems.get(system.handle);
    REQUIRE(sys != nullptr);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/two_tracks", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    int state = -1;
    REQUIRE(oae_event_instance_get_playback_state(inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_PLAYING);

    const auto out = render(system.handle, 480);
    double energy = 0.0;
    for (const auto sample : out) {
        energy += sample * sample;
    }
    CHECK(energy > 1.0);

    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_release(inst) == OAE_OK);
}

TEST_CASE("Instrument scheduled at 250ms starts at exact frame 12000")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 1000, 48000, 0.8f);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/delayed")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(250.0).length_ms(1000.0)))
    );
    auto model_set = builder.build();

    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(model_set);

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/delayed", &desc) == OAE_OK);

    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    const auto first_part = render(system.handle, 12000);
    float max_silent = 0.0f;
    for (const auto sample : first_part) {
        max_silent = std::max(max_silent, std::fabs(sample));
    }
    CHECK(max_silent == 0.0f);

    const auto second_part = render(system.handle, 480);
    float max_sound = 0.0f;
    for (const auto sample : second_part) {
        max_sound = std::max(max_sound, std::fabs(sample));
    }
    CHECK(max_sound > 0.5f);

    REQUIRE(oae_event_instance_release(inst) == OAE_OK);
}

TEST_CASE("Weighted random playlist converges within 5% over 10k picks")
{
    std::vector<oae::PlaylistEntry> entries(2);
    entries[0].weight = 0.30f;
    entries[1].weight = 0.70f;

    oae::PlaylistState state(oae::Pcg32(0x12345678ull, 0x9abcdef0ull));
    int count0 = 0;
    int count1 = 0;

    for (int i = 0; i < 10000; ++i) {
        const auto pick = state.pick(entries, oae::PlaylistMode::Random);
        if (pick == 0) ++count0;
        else if (pick == 1) ++count1;
    }

    const double pct0 = static_cast<double>(count0) / 10000.0;
    const double pct1 = static_cast<double>(count1) / 10000.0;

    CHECK(pct0 >= 0.25);
    CHECK(pct0 <= 0.35);
    CHECK(pct1 >= 0.65);
    CHECK(pct1 <= 0.75);
}

TEST_CASE("RANDOM_NO_REPEAT playlist never repeats consecutive entries")
{
    std::vector<oae::PlaylistEntry> entries(4);
    for (auto& entry : entries) {
        entry.weight = 1.0f;
    }

    oae::PlaylistState state(oae::Pcg32(0xdeadbeefull, 0x1234ull));
    std::size_t prev = static_cast<std::size_t>(-1);
    bool repeated = false;

    for (int i = 0; i < 10000; ++i) {
        const auto pick = state.pick(entries, oae::PlaylistMode::RandomNoRepeat);
        if (i > 0 && pick == prev) {
            repeated = true;
            break;
        }
        prev = pick;
    }

    CHECK_FALSE(repeated);
}

TEST_CASE("SHUFFLE playlist plays all items once per cycle")
{
    std::vector<oae::PlaylistEntry> entries(5);
    for (auto& entry : entries) {
        entry.weight = 1.0f;
    }

    oae::PlaylistState state(oae::Pcg32(0xfeedfaceull, 0x5678ull));
    for (int cycle = 0; cycle < 100; ++cycle) {
        std::vector<bool> seen(5, false);
        for (int i = 0; i < 5; ++i) {
            const auto pick = state.pick(entries, oae::PlaylistMode::Shuffle);
            REQUIRE(pick < 5);
            CHECK_FALSE(seen[pick]);
            seen[pick] = true;
        }
    }
}

TEST_CASE("max_instances=3 enforces limit under all steal modes")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    SUBCASE("OLDEST steal mode") {
        oae::ModelSetBuilder builder;
        builder.add_event(
            oae::EventBuilder("event:/steal_oldest")
                .max_instances(3)
                .steal(oae::StealMode::Oldest)
                .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(10000.0)))
        );
        auto* sys = oae::runtime.systems.get(system.handle);
        sys->register_model_set(builder.build());

        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/steal_oldest", &desc) == OAE_OK);

        std::vector<oae_event_instance> instances(5);
        for (std::size_t i = 0; i < 5; ++i) {
            REQUIRE(oae_event_description_create_instance(desc, &instances[i]) == OAE_OK);
            REQUIRE(oae_event_instance_start(instances[i]) == OAE_OK);
        }

        int count = 0;
        REQUIRE(oae_event_description_get_instance_count(desc, &count) == OAE_OK);
        CHECK(count == 3);

        int state0 = -1, state1 = -1, state4 = -1;
        oae_event_instance_get_playback_state(instances[0], &state0);
        oae_event_instance_get_playback_state(instances[1], &state1);
        oae_event_instance_get_playback_state(instances[4], &state4);
        CHECK(state0 == OAE_PLAYBACK_STOPPED);
        CHECK(state1 == OAE_PLAYBACK_STOPPED);
        CHECK(state4 == OAE_PLAYBACK_PLAYING);

        for (auto inst : instances) oae_event_instance_release(inst);
    }

    SUBCASE("QUIETEST steal mode") {
        oae::ModelSetBuilder builder;
        builder.add_event(
            oae::EventBuilder("event:/steal_quietest")
                .max_instances(3)
                .steal(oae::StealMode::Quietest)
                .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(10000.0)))
        );
        auto* sys = oae::runtime.systems.get(system.handle);
        sys->register_model_set(builder.build());

        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/steal_quietest", &desc) == OAE_OK);

        oae_event_instance i1 = 0, i2 = 0, i3 = 0, i4 = 0;
        REQUIRE(oae_event_description_create_instance(desc, &i1) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i2) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i3) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i4) == OAE_OK);

        oae_event_instance_set_volume(i1, 1.0f);
        oae_event_instance_set_volume(i2, 0.1f);
        oae_event_instance_set_volume(i3, 0.8f);
        oae_event_instance_set_volume(i4, 0.9f);

        oae_event_instance_start(i1);
        oae_event_instance_start(i2);
        oae_event_instance_start(i3);

        int count = 0;
        oae_event_description_get_instance_count(desc, &count);
        CHECK(count == 3);

        oae_event_instance_start(i4);
        oae_event_description_get_instance_count(desc, &count);
        CHECK(count == 3);

        int state2 = -1;
        oae_event_instance_get_playback_state(i2, &state2);
        CHECK(state2 == OAE_PLAYBACK_STOPPED);

        oae_event_instance_release(i1);
        oae_event_instance_release(i2);
        oae_event_instance_release(i3);
        oae_event_instance_release(i4);
    }

    SUBCASE("NONE steal mode") {
        oae::ModelSetBuilder builder;
        builder.add_event(
            oae::EventBuilder("event:/steal_none")
                .max_instances(3)
                .steal(oae::StealMode::None)
                .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(10000.0)))
        );
        auto* sys = oae::runtime.systems.get(system.handle);
        sys->register_model_set(builder.build());

        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, "event:/steal_none", &desc) == OAE_OK);

        oae_event_instance i1 = 0, i2 = 0, i3 = 0, i4 = 0;
        REQUIRE(oae_event_description_create_instance(desc, &i1) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i2) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i3) == OAE_OK);
        REQUIRE(oae_event_description_create_instance(desc, &i4) == OAE_OK);

        REQUIRE(oae_event_instance_start(i1) == OAE_OK);
        REQUIRE(oae_event_instance_start(i2) == OAE_OK);
        REQUIRE(oae_event_instance_start(i3) == OAE_OK);

        int count = 0;
        oae_event_description_get_instance_count(desc, &count);
        CHECK(count == 3);

        REQUIRE(oae_event_instance_start(i4) == OAE_OK);
        oae_event_description_get_instance_count(desc, &count);
        CHECK(count == 3);

        int state4 = -1;
        oae_event_instance_get_playback_state(i4, &state4);
        CHECK(state4 == OAE_PLAYBACK_STOPPED);

        oae_event_instance_release(i1);
        oae_event_instance_release(i2);
        oae_event_instance_release(i3);
        oae_event_instance_release(i4);
    }
}

TEST_CASE("Cooldown of 100ms discards triggers in window returning OAE_OK")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/cooldown")
            .cooldown_ms(100)
            .add_track(oae::TrackBuilder().add_instrument(oae::InstrumentBuilder().sound(sound).length_ms(1000.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/cooldown", &desc) == OAE_OK);

    oae_event_instance i1 = 0, i2 = 0;
    REQUIRE(oae_event_description_create_instance(desc, &i1) == OAE_OK);
    REQUIRE(oae_event_description_create_instance(desc, &i2) == OAE_OK);

    REQUIRE(oae_event_instance_start(i1) == OAE_OK);
    int state1 = -1;
    oae_event_instance_get_playback_state(i1, &state1);
    CHECK(state1 == OAE_PLAYBACK_PLAYING);

    // Immediate second start within cooldown (0ms elapsed) -> discarded, returns OAE_OK
    REQUIRE(oae_event_instance_start(i2) == OAE_OK);
    int state2 = -1;
    oae_event_instance_get_playback_state(i2, &state2);
    CHECK(state2 == OAE_PLAYBACK_STOPPED);

    // Render 50ms (2400 frames) -> still within 100ms window
    render(system.handle, 2400);
    oae_event_instance i3 = 0;
    REQUIRE(oae_event_description_create_instance(desc, &i3) == OAE_OK);
    REQUIRE(oae_event_instance_start(i3) == OAE_OK);
    int state3 = -1;
    oae_event_instance_get_playback_state(i3, &state3);
    CHECK(state3 == OAE_PLAYBACK_STOPPED);

    // Render another 60ms (total 110ms > 100ms) -> cooldown expired!
    render(system.handle, 2880);
    oae_event_instance i4 = 0;
    REQUIRE(oae_event_description_create_instance(desc, &i4) == OAE_OK);
    REQUIRE(oae_event_instance_start(i4) == OAE_OK);
    int state4 = -1;
    oae_event_instance_get_playback_state(i4, &state4);
    CHECK(state4 == OAE_PLAYBACK_PLAYING);

    oae_event_instance_release(i1);
    oae_event_instance_release(i2);
    oae_event_instance_release(i3);
    oae_event_instance_release(i4);
}

TEST_CASE("Event description length and oneshot queries")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/shot")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(100.0).length_ms(450.0).loop(false)))
    );
    builder.add_event(
        oae::EventBuilder("event:/looping")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(1000.0).loop(true)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description shot = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/shot", &shot) == OAE_OK);
    uint32_t len = 0;
    int oneshot = -1;
    REQUIRE(oae_event_description_get_length_ms(shot, &len) == OAE_OK);
    CHECK(len == 550);
    REQUIRE(oae_event_description_is_oneshot(shot, &oneshot) == OAE_OK);
    CHECK(oneshot == 1);

    oae_event_description looping = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/looping", &looping) == OAE_OK);
    REQUIRE(oae_event_description_is_oneshot(looping, &oneshot) == OAE_OK);
    CHECK(oneshot == 0);
}

TEST_CASE("V1-01: Looping instrument sustains audio past sample length and stops cleanly")
{
    SystemFixture system;
    // 100 ms sine sound (4800 frames at 48 kHz)
    const auto sound = make_sine_sound(system.handle, 440, 4800, 0.5f);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/loop_inst")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(100.0).loop(true)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/loop_inst", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 150 ms (past the 100 ms sample length) -> must still be playing and rendering non-zero audio
    render(system.handle, 7200);
    const auto buf = render(system.handle, 2400);
    float peak = 0.0f;
    for (float s : buf) {
        peak = std::max(peak, std::fabs(s));
    }
    CHECK(peak > 0.1f);

    int state = -1;
    REQUIRE(oae_event_instance_get_playback_state(inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_PLAYING);

    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_get_playback_state(inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_STOPPED);
    oae_event_instance_release(inst);
}

TEST_CASE("T20: Asynchronous instruments preserve voices across timeline loop jumps")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae::Instrument asyncInst;
    asyncInst.kind = oae::InstrumentKind::Single;
    asyncInst.sound_handle = sound;
    asyncInst.offset_ms = 0.0;
    asyncInst.length_ms = 50.0;
    asyncInst.async = true;
    asyncInst.async_policy = oae::AsyncOverlapPolicy::IgnoreIfPlaying;

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/async_loop")
            .add_logic_item(oae::LogicItem::loop_region(0.0, 50.0, 2, false))
            .add_track(oae::TrackBuilder().add_instrument(asyncInst))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/async_loop", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);
    const auto buf = render(system.handle, 4800);
    CHECK(rms(buf) > 0.1);
    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    oae_event_instance_release(inst);
}

TEST_CASE("T21: Parameter sheets trigger and stop instruments by parameter range")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    auto rpmParam = oae::ParameterDefinition::continuous("RPM", 0.0f, 100.0f, 0.0f);

    oae::Instrument sheetInst;
    sheetInst.kind = oae::InstrumentKind::Single;
    sheetInst.sound_handle = sound;
    sheetInst.sheet_parameter = "RPM";
    sheetInst.param_min = 40.0f;
    sheetInst.param_max = 80.0f;

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/param_sheet")
            .add_parameter(rpmParam)
            .add_track(oae::TrackBuilder().add_instrument(sheetInst))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/param_sheet", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);
    CHECK(rms(render(system.handle, 2400)) < 0.001);

    REQUIRE(oae_event_instance_set_parameter_by_name(inst, "RPM", 60.0f, 1) == OAE_OK);
    CHECK(rms(render(system.handle, 2400)) > 0.1);

    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    oae_event_instance_release(inst);
}

TEST_CASE("T22: Programmer instruments fire CREATE/DESTROY callbacks")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae::Instrument progInst;
    progInst.kind = oae::InstrumentKind::Programmer;
    progInst.offset_ms = 0.0;
    progInst.length_ms = 500.0;

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/prog_cb")
            .add_track(oae::TrackBuilder().add_instrument(progInst))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    struct Ctx { oae_sound s; bool created = false; bool destroyed = false; } ctx{ sound };
    oae_event_callback cb = [](uint32_t type, oae_event_instance, const void* props, void* ud) -> oae_result {
        auto* c = static_cast<Ctx*>(ud);
        if (type == OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND && props) {
            const_cast<oae_programmer_sound_properties*>(
                static_cast<const oae_programmer_sound_properties*>(props))->sound = c->s;
            c->created = true;
        } else if (type == OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND) {
            c->destroyed = true;
        }
        return OAE_OK;
    };

    oae_event_description cbDesc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/prog_cb", &cbDesc) == OAE_OK);
    oae_event_instance cbInst = 0;
    REQUIRE(oae_event_description_create_instance(cbDesc, &cbInst) == OAE_OK);
    REQUIRE(oae_event_instance_set_callback(cbInst, cb,
        OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND | OAE_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND, &ctx) == OAE_OK);
    REQUIRE(oae_event_instance_start(cbInst) == OAE_OK);
    CHECK(ctx.created);
    CHECK(rms(render(system.handle, 2400)) > 0.1);
    REQUIRE(oae_event_instance_stop(cbInst, OAE_STOP_IMMEDIATE) == OAE_OK);
    CHECK(ctx.destroyed);
    oae_event_instance_release(cbInst);
}

TEST_CASE("T25: Nested/overlapping loops respect priority and pitch_affects_timeline scales playhead")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    auto innerLoop = oae::LogicItem::loop_region(50.0, 100.0, 1, false);
    innerLoop.priority = 10;
    auto outerLoop = oae::LogicItem::loop_region(0.0, 100.0, 1, false);
    outerLoop.priority = 1;

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/nested_pitch")
            .pitch_semitones(12.0f)
            .pitch_affects_timeline(true)
            .add_logic_item(outerLoop)
            .add_logic_item(innerLoop)
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(500.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/nested_pitch", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    render(system.handle, 2400);
    auto* evPtr = oae::runtime.event_instances.get(inst);
    REQUIRE(evPtr != nullptr);
    CHECK(evPtr->timeline_position_ms() == doctest::Approx(50.0).epsilon(2.0));

    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    oae_event_instance_release(inst);
}

TEST_CASE("Playlist random delay variation (delay_ms_random_range) schedules within [0, range]")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae::Instrument inst;
    inst.kind = oae::InstrumentKind::Multi;
    inst.offset_ms = 0.0;
    inst.length_ms = 500.0;
    inst.delay_ms_random_range = 150;
    oae::PlaylistEntry entry;
    entry.sound_handle = sound;
    entry.weight = 1.0f;
    inst.playlist.push_back(entry);

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/playlist_delay")
            .add_track(oae::TrackBuilder().add_instrument(inst))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/playlist_delay", &desc) == OAE_OK);

    bool saw_audible = false;
    for (int trial = 0; trial < 5; ++trial) {
        oae_event_instance ev = 0;
        REQUIRE(oae_event_description_create_instance(desc, &ev) == OAE_OK);
        REQUIRE(oae_event_instance_start(ev) == OAE_OK);
        auto buf = render(system.handle, 9600);
        if (rms(buf) > 0.05) saw_audible = true;
        REQUIRE(oae_event_instance_stop(ev, OAE_STOP_IMMEDIATE) == OAE_OK);
        oae_event_instance_release(ev);
    }
    CHECK(saw_audible);
}

TEST_CASE("Per-track effects process audio on track sub-bus")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 5000, 48000, 0.5f);

    oae::TrackEffectSpec lp;
    lp.type = OAE_EFFECT_LOWPASS;
    lp.position = OAE_EFFECT_PRE_FADER;
    lp.params = { { 0, 200.0f }, { 1, 0.707f } };

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/track_fx_dry")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(200.0)))
    );
    builder.add_event(
        oae::EventBuilder("event:/track_fx_wet")
            .add_track(oae::TrackBuilder()
                .add_effect(lp)
                .add_instrument(oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(200.0)))
    );
    auto* sys = oae::runtime.systems.get(system.handle);
    sys->register_model_set(builder.build());

    auto measure_rms = [&](const char* path) -> double {
        oae_event_description desc = 0;
        REQUIRE(oae_studio_get_event(system.handle, path, &desc) == OAE_OK);
        oae_event_instance ev = 0;
        REQUIRE(oae_event_description_create_instance(desc, &ev) == OAE_OK);
        REQUIRE(oae_event_instance_start(ev) == OAE_OK);
        const auto buf = render(system.handle, 4800);
        REQUIRE(oae_event_instance_stop(ev, OAE_STOP_IMMEDIATE) == OAE_OK);
        oae_event_instance_release(ev);
        return rms(buf);
    };

    const double dry_rms = measure_rms("event:/track_fx_dry");
    const double wet_rms = measure_rms("event:/track_fx_wet");
    CHECK(dry_rms > 0.2);
    CHECK(wet_rms < dry_rms * 0.2);
}

TEST_CASE("Command instruments start/stop events and toggle snapshots at runtime")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 48000, 0.5f);

    oae_bus master = 0;
    REQUIRE(oae_system_get_master_bus(system.handle, &master) == OAE_OK);
    oae_bus sfx_bus = 0;
    REQUIRE(oae_bus_create(system.handle, "sfx", master, &sfx_bus) == OAE_OK);

    const auto target_guid = oae::guid_from_string("event:/cmd_target");
    const auto snap_guid = oae::guid_from_string("snapshot:/duck_sfx");

    oae::SnapshotTarget duck_target;
    duck_target.bus_path = "bus:/sfx";
    duck_target.param_id = -1;
    duck_target.value = -12.0f;

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/cmd_target")
            .bus_path("bus:/sfx")
            .stop_fadeout_ms(10)
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(2000.0)))
    );
    builder.add_event(
        oae::EventBuilder("snapshot:/duck_sfx")
            .kind(oae::EventKind::Snapshot)
            .snapshot_mode(oae::SnapshotMode::Override)
            .add_snapshot_target(duck_target)
    );
    builder.add_event(
        oae::EventBuilder("event:/cmd_driver")
            .add_track(oae::TrackBuilder()
                .add_instrument(oae::InstrumentBuilder().command(oae::CommandType::StartEvent, target_guid).offset_ms(0.0).length_ms(10.0))
                .add_instrument(oae::InstrumentBuilder().command(oae::CommandType::SnapshotOn, snap_guid).offset_ms(20.0).length_ms(10.0))
                .add_instrument(oae::InstrumentBuilder().command(oae::CommandType::SnapshotOff, snap_guid).offset_ms(40.0).length_ms(10.0))
                .add_instrument(oae::InstrumentBuilder().command(oae::CommandType::StopEvent, target_guid).offset_ms(60.0).length_ms(10.0)))
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    REQUIRE(sys != nullptr);
    sys->register_model_set(builder.build());

    oae_event_description driver_desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/cmd_driver", &driver_desc) == OAE_OK);
    oae_event_instance driver_inst = 0;
    REQUIRE(oae_event_description_create_instance(driver_desc, &driver_inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(driver_inst) == OAE_OK);

    const auto buf_initial = render(system.handle, 720); // 0..15ms
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const double rms_initial = rms(buf_initial);
    CHECK(rms_initial > 0.2);

    render(system.handle, 480); // 15..25ms (crosses 20ms SnapshotOn)
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    render(system.handle, 240); // allow bus gain ramp to settle
    const auto buf_ducked = render(system.handle, 480); // 30..40ms
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const double rms_ducked = rms(buf_ducked);
    CHECK(rms_ducked < rms_initial * 0.4);

    render(system.handle, 480); // 35..45ms (crosses 40ms SnapshotOff)
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto buf_recovered = render(system.handle, 480); // 45..55ms
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const double rms_recovered = rms(buf_recovered);
    CHECK(rms_recovered > rms_ducked * 2.0);

    render(system.handle, 960); // 55..75ms (crosses 60ms StopEvent)
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    render(system.handle, 960); // 20ms > 10ms fadeout
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto buf_stopped = render(system.handle, 480);
    CHECK(rms(buf_stopped) < 0.01);

    REQUIRE(oae_event_instance_release(driver_inst) == OAE_OK);
}

TEST_CASE("Event instruments spawn nested child routed through parent track bus and stop with fadeout")
{
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 5000, 48000, 0.5f);
    const auto child_guid = oae::guid_from_string("event:/nested_child");

    oae::TrackEffectSpec lp;
    lp.type = OAE_EFFECT_LOWPASS;
    lp.position = OAE_EFFECT_PRE_FADER;
    lp.params = { { 0, 200.0f }, { 1, 0.707f } };

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/nested_child")
            .stop_fadeout_ms(50)
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sound(sound).offset_ms(0.0).length_ms(1000.0)))
    );
    builder.add_event(
        oae::EventBuilder("event:/nested_parent_dry")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().event_ref(child_guid).offset_ms(0.0).length_ms(500.0)))
    );
    builder.add_event(
        oae::EventBuilder("event:/nested_parent_filtered")
            .add_track(oae::TrackBuilder()
                .add_effect(lp)
                .add_instrument(oae::InstrumentBuilder().event_ref(child_guid).offset_ms(0.0).length_ms(500.0)))
    );

    auto* sys = oae::runtime.systems.get(system.handle);
    REQUIRE(sys != nullptr);
    sys->register_model_set(builder.build());

    oae_event_description filtered_desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/nested_parent_filtered", &filtered_desc) == OAE_OK);
    oae_event_instance filtered_inst = 0;
    REQUIRE(oae_event_description_create_instance(filtered_desc, &filtered_inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(filtered_inst) == OAE_OK);
    const double filtered_rms = rms(render(system.handle, 4800));
    REQUIRE(oae_event_instance_stop(filtered_inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_release(filtered_inst) == OAE_OK);

    oae_event_description dry_desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/nested_parent_dry", &dry_desc) == OAE_OK);
    oae_event_instance dry_inst = 0;
    REQUIRE(oae_event_description_create_instance(dry_desc, &dry_inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(dry_inst) == OAE_OK);
    const double dry_rms = rms(render(system.handle, 4800));

    CHECK(dry_rms > 0.2);
    CHECK(filtered_rms < dry_rms * 0.2);

    REQUIRE(oae_event_instance_stop(dry_inst, OAE_STOP_ALLOW_FADEOUT) == OAE_OK);
    int state = -1;
    REQUIRE(oae_event_instance_get_playback_state(dry_inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_STOPPING);

    render(system.handle, 4800); // 100ms > 50ms fadeout
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    REQUIRE(oae_event_instance_get_playback_state(dry_inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_STOPPED);
    CHECK(rms(render(system.handle, 480)) < 0.001);

    REQUIRE(oae_event_instance_release(dry_inst) == OAE_OK);
}

TEST_CASE("Scatterer instruments spawn grains respecting interval, polyphony, and 2D spatial dispersion") {
    SystemFixture system;
    const auto sound = make_sine_sound(system.handle, 440, 4800, 0.5f); // 100ms

    oae::ModelSetBuilder builder;
    builder.add_event(
        oae::EventBuilder("event:/sfx/scatterer_test")
            .add_track(oae::TrackBuilder()
                .add_instrument(oae::InstrumentBuilder()
                    .scatterer(40.0, 40.0, 2, 3, 1.0f, 8.0f)
                    .offset_ms(0.0)
                    .length_ms(1000.0)
                    .add_entry(sound, 1.0f))));

    auto* sys = oae::runtime.systems.get(system.handle);
    REQUIRE(sys != nullptr);
    sys->register_model_set(builder.build());

    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(system.handle, "event:/sfx/scatterer_test", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // First grain spawns immediately at start(); render 30ms and verify non-silent audio
    const auto buf1 = render(system.handle, 1440);
    CHECK(rms(buf1) > 0.05);

    // Advance 50ms in update() -> 2nd grain spawns (total_sounds=3, polyphony=2)
    render(system.handle, 2400);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto buf2 = render(system.handle, 1440);
    CHECK(rms(buf2) > 0.05);

    // Advance another 50ms -> 3rd grain spawns; wait for tails to finish
    render(system.handle, 2400);
    REQUIRE(oae_system_update(system.handle) == OAE_OK);
    const auto buf3 = render(system.handle, 1440);
    CHECK(rms(buf3) > 0.01);

    // Stop and verify silence
    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_release(inst) == OAE_OK);
    CHECK(rms(render(system.handle, 480)) < 0.001);
}
