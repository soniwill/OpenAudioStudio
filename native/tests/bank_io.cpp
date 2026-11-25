#include "playback_fixture.h"
#include "../src/studio/bank_writer.h"
#include "../src/studio/bank_reader.h"
#include "../src/common/pcg32.h"
#include "../src/api/runtime.h"

#include <cmath>
#include <vector>

using namespace oae::test;

TEST_CASE("T12: PARM preserves conditions scopes and hierarchical owners")
{
    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("level", 0, 1, 1));
    oae::EventDescriptionModel event;
    event.path = "event:/parm";
    event.user_props.emplace_back("category", "review");
    event.parameters.push_back(oae::ParameterDefinition::continuous("level", 0, 1, 0));
    oae::Curve curve;
    curve.add_point(0, -80); curve.add_point(1, 0);
    event.automations.push_back(oae::Automation::volume_param("level", curve, true));
    event.tracks.resize(2);
    event.tracks[0].instruments.resize(2);
    event.tracks[1].instruments.resize(2);
    event.tracks[1].automations.push_back(oae::Automation::volume_param("level", curve));
    auto& instrument = event.tracks[1].instruments[1];
    instrument.condition.probability_pct = 0;
    instrument.condition.add_term("level", 0.5f, 1, true);
    instrument.modulators.push_back(oae::Modulator::ahdsr(1, 2, 3, 0.5f, 4, true));
    auto logic = oae::LogicItem::marker("condition", 10);
    logic.condition.add_term("level", 0, 0.5f);
    event.logic_items.push_back(logic);
    builder.add_event(event);
    oae::EffectAutomation effect;
    effect.bus_path = "bus:/";
    effect.param.name = "level"; effect.param.is_global = true; effect.curve = curve;
    builder.add_effect_automation(effect);
    oae::BankWriter writer;
    writer.set_model_set(builder.build());
    const auto bytes = writer.write();
    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse(bytes.data(), bytes.size(), parsed) == OAE_OK);
    const auto* restored = parsed.model_set->find_event_by_path(event.path);
    REQUIRE(restored);
    CHECK(restored->user_props == event.user_props);
    REQUIRE(restored->automations.size() == 1);
    CHECK(restored->automations[0].param.is_global);
    CHECK(restored->automations[0].param.name == "level");
    CHECK_FALSE(restored->automations[0].param.global_id.is_null());
    REQUIRE(restored->tracks[1].automations.size() == 1);
    CHECK(restored->tracks[1].automations[0].owner_index == 1);
    CHECK_FALSE(restored->tracks[1].automations[0].param.is_global);
    CHECK(restored->tracks[1].automations[0].param.name == "level");
    const auto& restored_inst = restored->tracks[1].instruments[1];
    CHECK(restored_inst.condition.probability_pct == 0);
    REQUIRE(restored_inst.condition.terms.size() == 1);
    CHECK(restored_inst.condition.terms[0].param.is_global);
    REQUIRE(restored_inst.modulators.size() == 1);
    CHECK(restored_inst.modulators[0].owner_index == 3);
    CHECK(restored_inst.modulators[0].release_ms == 4);
    CHECK(restored_inst.modulators[0].release_on_key_off);
    REQUIRE(restored->logic_items[0].condition.terms.size() == 1);
    CHECK_FALSE(restored->logic_items[0].condition.terms[0].param.is_global);
    REQUIRE(parsed.model_set->effect_automations().size() == 1);
    CHECK(parsed.model_set->effect_automations()[0].param.name == "level");
}

TEST_CASE("T12: writer rejects unresolved scoped references and asymmetric RANDOM")
{
    oae::EventDescriptionModel event;
    event.path = "event:/invalid";
    oae::Curve curve; curve.add_point(0, 0);
    event.automations.push_back(oae::Automation::volume_param("global_only", curve));
    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("global_only", 0, 1, 0));
    builder.add_event(event);
    oae::BankWriter writer; writer.set_model_set(builder.build());
    CHECK_THROWS_AS(writer.write(), std::invalid_argument);
    event.automations.clear();
    auto random = oae::Modulator::random_pitch(3);
    random.random_range_min = 0;
    event.modulators.push_back(random);
    oae::ModelSetBuilder other; other.add_event(event); writer.set_model_set(other.build());
    CHECK_THROWS_AS(writer.write(), std::invalid_argument);
}

TEST_CASE("T12: loaded bank preserves zero probability and explicit automation scopes")
{
    SystemFixture fix;
    oae::EventDescriptionModel event;
    event.path = "event:/scoped";
    event.parameters.push_back(oae::ParameterDefinition::continuous("level", 0, 1, 0));
    oae::Track track;
    oae::Instrument instrument;
    instrument.sample_index = 0; instrument.length_ms = 1000; instrument.loop = true;
    track.instruments.push_back(instrument); event.tracks.push_back(track);
    oae::Curve curve; curve.add_point(0, -80); curve.add_point(1, 0);
    bool expect_signal = false;
    SUBCASE("global scope bypasses same named local") {
        event.automations.push_back(oae::Automation::volume_param("level", curve, true));
        expect_signal = true;
    }
    SUBCASE("local scope remains local") {
        event.automations.push_back(oae::Automation::volume_param("level", curve, false));
    }
    SUBCASE("zero probability prevents sound") {
        event.tracks[0].instruments[0].condition.probability_pct = 0;
    }
    oae::ModelSetBuilder builder;
    builder.add_global_parameter(oae::ParameterDefinition::continuous("level", 0, 1, 1));
    builder.add_event(event);
    oae::BankWriter writer; writer.set_model_set(builder.build());
    oae::SampleEntry sample;
    const auto wav = fixture_bytes("tone.wav");
    sample.data.assign(wav.begin(), wav.end());
    sample.sample_rate = 48000; sample.length_frames = 48000;
    writer.add_sample(sample);
    const auto bytes = writer.write();
    oae_bank bank = 0;
    REQUIRE(oae_studio_load_bank_memory(fix.handle, bytes.data(), bytes.size(), &bank) == OAE_OK);
    oae_event_description description = 0;
    REQUIRE(oae_studio_get_event(fix.handle, event.path.c_str(), &description) == OAE_OK);
    oae_event_instance instance = 0;
    REQUIRE(oae_event_description_create_instance(description, &instance) == OAE_OK);
    REQUIRE(oae_event_instance_start(instance) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    const auto samples = render(fix.handle, 1024);
    if (expect_signal) CHECK(rms(samples) > 0.001);
    else CHECK(rms(samples) == 0.0);
    REQUIRE(oae_event_instance_release(instance) == OAE_OK);
    REQUIRE(oae_studio_unload_bank(fix.handle, bank) == OAE_OK);
}

TEST_CASE("T12: PARM rejects invalid active fields with valid CRC and normalizes inactive fields")
{
    oae::EventDescriptionModel event;
    event.path = "event:/wire";
    oae::Automation automation;
    automation.is_timeline = true; automation.curve.add_point(0, 0);
    event.automations.push_back(automation);
    oae::ModelSetBuilder builder; builder.add_event(event);
    oae::BankWriter writer; writer.set_model_set(builder.build());
    const auto valid = writer.write();
    std::size_t directory = 0;
    oae::ChunkDirEntryWire entry{};
    for (std::size_t i = 0; i < 9; ++i) {
        const auto offset = sizeof(oae::BankHeaderWire) + i * sizeof(entry);
        std::memcpy(&entry, valid.data() + offset, sizeof(entry));
        if (std::memcmp(entry.tag, "PARM", 4) == 0) { directory = offset; break; }
    }
    REQUIRE(directory != 0);
    const auto mutate = [&](std::size_t offset, uint8_t value) {
        auto bytes = valid;
        bytes[static_cast<size_t>(entry.offset) + offset] = value;
        auto changed = entry;
        changed.crc32 = oae::calc_crc32(bytes.data() + entry.offset, static_cast<size_t>(entry.length));
        std::memcpy(bytes.data() + directory, &changed, sizeof(changed));
        oae::BankParseResult parsed;
        return oae::BankReader::parse(bytes.data(), bytes.size(), parsed);
    };
    CHECK(mutate(40, 2) == OAE_ERR_FORMAT);
    CHECK(mutate(41, 1) == OAE_ERR_FORMAT);
    CHECK(mutate(45, 3) == OAE_ERR_FORMAT);
    CHECK(mutate(46, 2) == OAE_ERR_FORMAT);
    CHECK(mutate(47, 2) == OAE_ERR_FORMAT);
    CHECK(mutate(48, 255) == OAE_OK);
    CHECK(mutate(52, 255) == OAE_OK);
    CHECK(mutate(68, 33) == OAE_ERR_FORMAT);
    CHECK(mutate(81, 1) == OAE_ERR_FORMAT);
}

TEST_CASE("T12: Bank serialization round-trip fidelity for all 9 chunks")
{
    oae::ModelSetBuilder b;

    // 1. Global Parameter
    b.add_global_parameter(oae::ParameterDefinition::continuous("WindSpeed", 0.0f, 100.0f, 15.0f));

    // 2. Event with Tracks, Instruments, Automations, Modulators and Logic
    oae::EventBuilder ev("event:/weapons/laser");
    ev.volume_db(-3.0f)
      .pitch_semitones(2.0f)
      .pan(0.5f)
      .max_instances(4)
      .steal(oae::StealMode::Oldest)
      .cooldown_ms(50)
      .priority(120);

    oae::TrackBuilder trk;
    trk.volume_db(-1.0f).pan(-0.25f);

    oae::InstrumentBuilder inst;
    inst.kind(oae::InstrumentKind::Single)
        .offset_ms(10.0)
        .length_ms(500.0)
        .loop(false);
    trk.add_instrument(inst);
    ev.add_track(trk);

    ev.add_logic_item(oae::LogicItem::marker("hit_point", 250.0));
    ev.add_modulator(oae::Modulator::random_pitch(3.0f));
    ev.add_modulator(oae::Modulator::random_volume(6.0f));
    ev.add_modulator(oae::Modulator::random_pan(0.5f));

    oae::Automation auto_vol;
    auto_vol.target = oae::AutomationTarget::Volume;
    auto_vol.is_timeline = true;
    auto_vol.curve.points.push_back({0.0f, 0.0f, oae::CurveShape::LINEAR});
    auto_vol.curve.points.push_back({500.0f, -24.0f, oae::CurveShape::EASE_OUT});
    ev.add_automation(auto_vol);

    b.add_event(ev);

    // 3. Snapshot
    oae::SnapshotTarget snap_target;
    snap_target.bus_path = "bus:/sfx";
    snap_target.param_id = -1;
    snap_target.value = -6.0f;

    oae::EventBuilder snap("snapshot:/mute_sfx");
    snap.kind(oae::EventKind::Snapshot)
        .snapshot_mode(oae::SnapshotMode::Override)
        .fade_in_ms(150)
        .stop_fadeout_ms(200)
        .priority(180)
        .add_snapshot_target(snap_target);
    b.add_event(snap);

    // 4. VCA
    oae::VcaModel vca;
    vca.path = "vca:/master_vca";
    vca.bus_paths.push_back("bus:/sfx");
    b.add_vca(vca);

    auto mset = b.build();
    REQUIRE(mset);

    // Serialize with BankWriter
    oae::BankWriter writer;
    writer.set_model_set(mset);

    oae::BusEntry bus;
    bus.name = "sfx";
    bus.volume = 0.9f;
    writer.add_bus(bus);

    oae::SampleEntry smpl;
    smpl.data = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E' };
    smpl.length_frames = 12000;
    smpl.sample_rate = 48000;
    writer.add_sample(smpl);

    const auto bank_bytes = writer.write();
    REQUIRE(!bank_bytes.empty());
    REQUIRE(bank_bytes.size() > sizeof(oae::BankHeaderWire));

    // Parse back with BankReader
    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse(bank_bytes.data(), bank_bytes.size(), parsed) == OAE_OK);
    REQUIRE(parsed.model_set != nullptr);

    // Check round-trip fidelity
    const auto* ev_read = parsed.model_set->find_event_by_path("event:/weapons/laser");
    REQUIRE(ev_read != nullptr);
    CHECK(ev_read->volume_db == -3.0f);
    CHECK(ev_read->pitch_semitones == 2.0f);
    CHECK(ev_read->pan == 0.5f);
    CHECK(ev_read->max_instances == 4);
    CHECK(ev_read->steal == oae::StealMode::Oldest);
    CHECK(ev_read->cooldown_ms == 50);
    CHECK(ev_read->priority == 120);
    REQUIRE(ev_read->tracks.size() == 1);
    REQUIRE(ev_read->tracks[0].instruments.size() == 1);
    CHECK(ev_read->tracks[0].instruments[0].offset_ms == 10.0);
    CHECK(ev_read->tracks[0].instruments[0].length_ms == 500.0);
    REQUIRE(ev_read->logic_items.size() == 1);
    CHECK(ev_read->logic_items[0].name == "hit_point");
    CHECK(ev_read->logic_items[0].position_ms == 250.0);
    REQUIRE(ev_read->modulators.size() == 3);
    CHECK(ev_read->modulators[0].random_range_min == -3.0f);
    CHECK(ev_read->modulators[0].random_range_max == 3.0f);
    CHECK(ev_read->modulators[1].random_range_min == -6.0f);
    CHECK(ev_read->modulators[1].random_range_max == 6.0f);
    CHECK(ev_read->modulators[2].random_range_min == -0.5f);
    CHECK(ev_read->modulators[2].random_range_max == 0.5f);
    REQUIRE(ev_read->automations.size() == 1);
    REQUIRE(ev_read->automations[0].curve.points.size() == 2);

    const auto* snap_read = parsed.model_set->find_event_by_path("snapshot:/mute_sfx");
    REQUIRE(snap_read != nullptr);
    CHECK(snap_read->kind == oae::EventKind::Snapshot);
    CHECK(snap_read->snapshot_mode == oae::SnapshotMode::Override);
    CHECK(snap_read->fade_in_ms == 150);
    CHECK(snap_read->stop_fadeout_ms == 200);
    CHECK(snap_read->priority == 180);
    REQUIRE(snap_read->snapshot_targets.size() == 1);
    CHECK(snap_read->snapshot_targets[0].bus_path == "bus:/sfx");
    CHECK(snap_read->snapshot_targets[0].value == -6.0f);

    const auto* vca_read = parsed.model_set->find_vca("vca:/master_vca");
    REQUIRE(vca_read != nullptr);
    REQUIRE(vca_read->bus_paths.size() == 1);
    CHECK(vca_read->bus_paths[0] == "bus:/sfx");

    REQUIRE(parsed.samples.size() == 1);
    CHECK(parsed.samples[0].length_frames == 12000);
    CHECK(parsed.samples[0].sample_rate == 48000);

    REQUIRE(parsed.buses.size() == 1);
    CHECK(parsed.buses[0].name == "sfx");
    CHECK(parsed.buses[0].volume == 0.9f);
}

TEST_CASE("T12: Robustness against malformed headers, magic and versions")
{
    std::vector<uint8_t> valid_bank;
    {
        oae::ModelSetBuilder b;
        oae::EventBuilder ev("event:/test");
        b.add_event(ev);
        oae::BankWriter w;
        w.set_model_set(b.build());
        valid_bank = w.write();
    }

    oae::BankParseResult parsed;

    // Buffer too small
    CHECK(oae::BankReader::parse(valid_bank.data(), sizeof(oae::BankHeaderWire) - 1, parsed) == OAE_ERR_FORMAT);
    CHECK(oae::BankReader::parse(nullptr, valid_bank.size(), parsed) == OAE_ERR_FORMAT);

    // Corrupt magic
    auto corrupted = valid_bank;
    corrupted[0] = 'W'; corrupted[1] = 'A'; corrupted[2] = 'V'; corrupted[3] = 'E';
    CHECK(oae::BankReader::parse(corrupted.data(), corrupted.size(), parsed) == OAE_ERR_FORMAT);

    // Corrupt version
    corrupted = valid_bank;
    corrupted[4] = 2; // version 2 unsupported
    CHECK(oae::BankReader::parse(corrupted.data(), corrupted.size(), parsed) == OAE_ERR_FORMAT);

    // Corrupt flags
    corrupted = valid_bank;
    corrupted[6] = 1;
    CHECK(oae::BankReader::parse(corrupted.data(), corrupted.size(), parsed) == OAE_ERR_FORMAT);
}

TEST_CASE("T12: 10,000 iterations byte fuzzing never causes crash or leaks")
{
    std::vector<uint8_t> base_bank;
    {
        oae::ModelSetBuilder b;
        oae::EventBuilder ev("event:/fuzz/target");
        ev.volume_db(-1.0f).pitch_semitones(1.0f);
        oae::TrackBuilder trk;
        oae::InstrumentBuilder inst;
        inst.length_ms(100.0);
        trk.add_instrument(inst);
        ev.add_track(trk);
        b.add_event(ev);

        oae::BankWriter w;
        w.set_model_set(b.build());
        base_bank = w.write();
    }

    REQUIRE(!base_bank.empty());

    oae::Pcg32 rng(0xDEADBEEF12345678ULL, 0x87654321ULL);

    constexpr int fuzz_iterations = 10000;
    int error_count = 0;

    for (int it = 0; it < fuzz_iterations; ++it) {
        auto mutated = base_bank;
        const auto mut_type = rng.next_bounded(4);

        if (mut_type == 0) {
            // Flip a random bit
            const size_t byte_idx = static_cast<size_t>(rng.next_bounded(static_cast<uint32_t>(mutated.size())));
            const uint8_t bit = static_cast<uint8_t>(1u << rng.next_bounded(8));
            mutated[byte_idx] ^= bit;
        } else if (mut_type == 1) {
            // Overwrite random bytes
            const size_t byte_idx = static_cast<size_t>(rng.next_bounded(static_cast<uint32_t>(mutated.size() - 4)));
            const uint32_t garbage = rng.next();
            std::memcpy(mutated.data() + byte_idx, &garbage, sizeof(garbage));
        } else if (mut_type == 2) {
            // Truncate file
            const size_t new_size = static_cast<size_t>(1 + rng.next_bounded(static_cast<uint32_t>(mutated.size() - 1)));
            mutated.resize(new_size);
        } else if (mut_type == 3) {
            // Append random trailing garbage
            const size_t append_count = static_cast<size_t>(1 + rng.next_bounded(64));
            for (size_t a = 0; a < append_count; ++a) {
                mutated.push_back(static_cast<uint8_t>(rng.next_bounded(256)));
            }
        }

        oae::BankParseResult parsed;
        const auto res = oae::BankReader::parse(mutated.data(), mutated.size(), parsed);
        if (res == OAE_ERR_FORMAT) {
            ++error_count;
        } else {
            CHECK(res == OAE_OK);
        }
    }

    // Overwhelming majority of mutated banks should be recognized as invalid format
    CHECK(error_count > 9000);
}

TEST_CASE("T12: Bank load memory/file and unload lifecycle")
{
    SystemFixture fix;

    std::vector<uint8_t> bank_bytes;
    {
        oae::ModelSetBuilder b;
        oae::EventBuilder ev("event:/ambient/birds");
        ev.volume_db(0.0f);
        b.add_event(ev);

        oae::VcaModel vca;
        vca.path = "vca:/ambient";
        b.add_vca(vca);

        oae::BankWriter w;
        w.set_model_set(b.build());
        bank_bytes = w.write();
    }

    // Load bank from memory
    oae_bank bank = 0;
    REQUIRE(oae_studio_load_bank_memory(fix.handle, bank_bytes.data(), bank_bytes.size(), &bank) == OAE_OK);
    REQUIRE(bank != 0);

    // Retrieve event and VCA from loaded bank
    oae_event_description ev_desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/ambient/birds", &ev_desc) == OAE_OK);
    REQUIRE(ev_desc != 0);

    oae_vca vca_h = 0;
    REQUIRE(oae_studio_get_vca(fix.handle, "vca:/ambient", &vca_h) == OAE_OK);
    REQUIRE(vca_h != 0);

    // Create and start event instance
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(ev_desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    int play_state = -1;
    REQUIRE(oae_event_instance_get_playback_state(inst, &play_state) == OAE_OK);
    CHECK(play_state == OAE_PLAYBACK_PLAYING);

    // Unload bank -> should stop instances and unregister
    REQUIRE(oae_studio_unload_bank(fix.handle, bank) == OAE_OK);

    // Unloading already unloaded bank handle -> INVALID_HANDLE
    CHECK(oae_studio_unload_bank(fix.handle, bank) == OAE_ERR_INVALID_HANDLE);

    oae_event_instance_release(inst);
}

TEST_CASE("T12: Master Bank and Content Bank pattern (FMOD/Wwise)")
{
    SystemFixture fix;

    // 1. Build Master Bank (owns mixer hierarchy and bus DSP effects)
    std::vector<uint8_t> master_bytes;
    {
        oae::BankWriter writer;
        oae::BusEntry master_bus;
        master_bus.name = "bus:/";
        master_bus.parent_index = -1;
        master_bus.volume = 1.0f;
        writer.add_bus(master_bus);

        oae::BusEntry sfx_bus;
        sfx_bus.name = "bus:/sfx";
        sfx_bus.parent_index = 0;
        sfx_bus.volume = 0.8f;

        oae::BusEntry::EffectDesc lpf;
        lpf.type = OAE_EFFECT_LOWPASS;
        lpf.position = OAE_EFFECT_POST_FADER;
        lpf.bypass = false;
        lpf.sidechain_bus = -1;
        lpf.params = {1000.0f, 0.707f};
        lpf.wet = 1.0f;
        lpf.dry = 0.0f;
        sfx_bus.effects.push_back(lpf);
        writer.add_bus(sfx_bus);

        master_bytes = writer.write();
    }

    // Load Master Bank
    oae_bank master_bank = 0;
    REQUIRE(oae_studio_load_bank_memory(fix.handle, master_bytes.data(), master_bytes.size(), &master_bank) == OAE_OK);
    REQUIRE(master_bank != 0);

    oae_bus sfx_handle = 0;
    REQUIRE(oae_system_get_bus(fix.handle, "bus:/sfx", &sfx_handle) == OAE_OK);
    REQUIRE(sfx_handle != 0);
    float sfx_vol = 0.0f;
    REQUIRE(oae_bus_get_volume(sfx_handle, &sfx_vol) == OAE_OK);
    CHECK(sfx_vol == doctest::Approx(0.8f));

    // 2. Build Content Bank (Weapons) with NO buses defined in BUSS chunk
    std::vector<uint8_t> content_bytes;
    {
        oae::ModelSetBuilder b;
        oae::EventBuilder ev("event:/weapons/laser");
        ev.volume_db(0.0f);
        ev.bus_path("bus:/sfx");

        oae::TrackBuilder trk;
        oae::InstrumentBuilder inst;
        inst.kind(oae::InstrumentKind::Single);
        inst.sample_index(0);
        inst.offset_ms(0.0);
        inst.length_ms(500.0);
        trk.add_instrument(inst);
        ev.add_track(trk);
        b.add_event(ev);

        oae::BankWriter writer;
        writer.set_model_set(b.build());

        const auto raw_wav = fixture_bytes("tone.wav");
        oae::SampleEntry smpl;
        smpl.data.assign(raw_wav.begin(), raw_wav.end());
        smpl.length_frames = 24000;
        smpl.sample_rate = 48000;
        writer.add_sample(smpl);

        content_bytes = writer.write();
    }

    // Load Content Bank
    oae_bank content_bank = 0;
    REQUIRE(oae_studio_load_bank_memory(fix.handle, content_bytes.data(), content_bytes.size(), &content_bank) == OAE_OK);
    REQUIRE(content_bank != 0);

    // Play event from Content Bank and render audio
    oae_event_description ev_desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/weapons/laser", &ev_desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(ev_desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);

    const auto samples = render(fix.handle, 1024);
    CHECK(rms(samples) > 0.001f);
    REQUIRE(oae_event_instance_release(inst) == OAE_OK);

    // 3. Unload Content Bank: Master Bank and bus:/sfx must remain 100% alive and intact
    REQUIRE(oae_studio_unload_bank(fix.handle, content_bank) == OAE_OK);

    oae_bus sfx_still_alive = 0;
    REQUIRE(oae_system_get_bus(fix.handle, "bus:/sfx", &sfx_still_alive) == OAE_OK);
    CHECK(sfx_still_alive == sfx_handle);

    // 4. Conflicting Bank: another bank attempting to overwrite bus:/sfx with conflicting volume is rejected
    {
        oae::BankWriter w_conflict;
        oae::BusEntry conflict_bus;
        conflict_bus.name = "bus:/sfx";
        conflict_bus.volume = 0.2f;
        w_conflict.add_bus(conflict_bus);

        const auto conflict_bytes = w_conflict.write();
        oae_bank conflict_bank = 0;
        CHECK(oae_studio_load_bank_memory(fix.handle, conflict_bytes.data(), conflict_bytes.size(), &conflict_bank) == OAE_ERR_ALREADY_EXISTS);
    }

    // 5. Unload Master Bank: mixer bus is released
    REQUIRE(oae_studio_unload_bank(fix.handle, master_bank) == OAE_OK);
    oae_bus sfx_after_unload = 0;
    CHECK(oae_system_get_bus(fix.handle, "bus:/sfx", &sfx_after_unload) == OAE_ERR_NOT_FOUND);
}

TEST_CASE("V1-04: Streaming sample in .bank file uses zero PCM RAM, loops, stops, and unloads")
{
    SystemFixture fix;

    // Build a 100 ms 48 kHz mono 16-bit PCM WAV in memory
    constexpr uint32_t sr = 48000;
    constexpr uint32_t frames = 4800;
    constexpr uint32_t data_bytes = frames * 2;
    std::vector<uint8_t> wav(44 + data_bytes, 0);
    std::memcpy(wav.data() + 0, "RIFF", 4);
    const uint32_t riff_sz = 36 + data_bytes;
    std::memcpy(wav.data() + 4, &riff_sz, 4);
    std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
    const uint32_t fmt_sz = 16;
    const uint16_t audio_fmt = 1, channels = 1, block_align = 2, bits = 16;
    const uint32_t byte_rate = sr * 2;
    std::memcpy(wav.data() + 16, &fmt_sz, 4);
    std::memcpy(wav.data() + 20, &audio_fmt, 2);
    std::memcpy(wav.data() + 22, &channels, 2);
    std::memcpy(wav.data() + 24, &sr, 4);
    std::memcpy(wav.data() + 28, &byte_rate, 4);
    std::memcpy(wav.data() + 32, &block_align, 2);
    std::memcpy(wav.data() + 34, &bits, 2);
    std::memcpy(wav.data() + 36, "data", 4);
    std::memcpy(wav.data() + 40, &data_bytes, 4);
    for (uint32_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * 3.14159265358979323846 * 440.0 * i / sr;
        const int16_t sample = static_cast<int16_t>(std::sin(phase) * 16000.0);
        std::memcpy(wav.data() + 44 + i * 2, &sample, 2);
    }

    oae::BankWriter w;
    oae::SampleEntry se;
    se.data = wav;
    se.length_frames = frames;
    se.sample_rate = sr;
    se.stream = true;
    w.add_sample(se);

    oae::ModelSetBuilder b;
    b.add_event(
        oae::EventBuilder("event:/music/stream_loop")
            .add_track(oae::TrackBuilder().add_instrument(
                oae::InstrumentBuilder().sample_index(0).offset_ms(0.0).length_ms(100.0).loop(true)))
    );
    w.set_model_set(b.build());
    const auto bank_bytes = w.write();

    const auto temp_bank = std::filesystem::temp_directory_path() / "oae_test_stream_v104.bank";
    {
        std::ofstream os(temp_bank, std::ios::binary);
        os.write(reinterpret_cast<const char*>(bank_bytes.data()), static_cast<std::streamsize>(bank_bytes.size()));
    }

    // Verify BankReader::parse_file keeps streaming sample payload out of RAM
    oae::BankParseResult parsed;
    REQUIRE(oae::BankReader::parse_file(temp_bank, parsed) == OAE_OK);
    REQUIRE(parsed.samples.size() == 1);
    CHECK(parsed.samples[0].stream == true);
    CHECK(parsed.samples[0].data.empty());
    CHECK(parsed.samples[0].file_length == wav.size());

    // Load bank from file and play streaming looped event past 100 ms
    oae_bank bank = 0;
    REQUIRE(oae_studio_load_bank_file(fix.handle, temp_bank.u8string().c_str(), &bank) == OAE_OK);
    oae_event_description desc = 0;
    REQUIRE(oae_studio_get_event(fix.handle, "event:/music/stream_loop", &desc) == OAE_OK);
    oae_event_instance inst = 0;
    REQUIRE(oae_event_description_create_instance(desc, &inst) == OAE_OK);
    REQUIRE(oae_event_instance_start(inst) == OAE_OK);

    // Render 150 ms (past 100 ms sample end) -> streaming loop continues rendering audio
    render(fix.handle, 7200);
    const auto out = render(fix.handle, 2400);
    CHECK(rms(out) > 0.05f);

    int state = -1;
    REQUIRE(oae_event_instance_get_playback_state(inst, &state) == OAE_OK);
    CHECK(state == OAE_PLAYBACK_PLAYING);

    REQUIRE(oae_event_instance_stop(inst, OAE_STOP_IMMEDIATE) == OAE_OK);
    REQUIRE(oae_event_instance_release(inst) == OAE_OK);
    REQUIRE(oae_studio_unload_bank(fix.handle, bank) == OAE_OK);
    REQUIRE(oae_system_update(fix.handle) == OAE_OK);
    std::filesystem::remove(temp_bank);
}
