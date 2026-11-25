#include "bank_writer.h"
#include "bank_parm_codec.h"
#include "bank_track_effects_codec.h"

namespace oae {

uint32_t BankWriter::intern_string(const std::string& str)
{
    const auto it = string_to_index_.find(str);
    if (it != string_to_index_.end()) {
        return it->second;
    }
    const auto idx = static_cast<uint32_t>(string_table_.size());
    string_table_.push_back(str);
    string_to_index_[str] = idx;
    return idx;
}

std::vector<uint8_t> BankWriter::write()
{
    string_table_.clear();
    string_to_index_.clear();

    // Iniciar com string vazia
    intern_string("");

    // 1. Chunks payload writers
    ByteWriter bus_w;
    ByteWriter evts_w;
    ByteWriter smpl_w;
    ByteWriter data_w;
    ByteWriter logc_w;
    ByteWriter parm_w;
    ByteWriter snap_w;
    ByteWriter vcas_w;

    // --- BUSS CHUNK ---
    bus_w.write(static_cast<uint32_t>(buses_.size()));
    for (const auto& b : buses_) {
        bus_w.write(intern_string(b.name));
        bus_w.write(b.parent_index);
        bus_w.write(b.volume);
        bus_w.write(static_cast<uint32_t>(b.effects.size()));
        for (const auto& fx : b.effects) {
            bus_w.write(fx.position);
            bus_w.write(fx.type);
            bus_w.write_bool(fx.bypass);
            bus_w.write(fx.sidechain_bus);
            bus_w.write(static_cast<uint32_t>(fx.params.size()));
            for (float p : fx.params) {
                bus_w.write(p);
            }
            bus_w.write(fx.wet);
            bus_w.write(fx.dry);
        }
    }

    // --- SMPL & DATA & optional STRM CHUNKS ---
    bool has_streaming = false;
    ByteWriter strm_w;
    smpl_w.write(static_cast<uint32_t>(samples_.size()));
    for (const auto& s : samples_) {
        if (s.stream) has_streaming = true;
        data_w.align_to(16);
        const uint64_t offset = static_cast<uint64_t>(data_w.size());
        const uint64_t length = static_cast<uint64_t>(s.data.size());
        data_w.write_bytes(s.data.data(), s.data.size());

        smpl_w.write(offset);
        smpl_w.write(length);
        smpl_w.write(s.length_frames);
        smpl_w.write(s.sample_rate);
    }
    if (has_streaming) {
        strm_w.write(static_cast<uint32_t>(samples_.size()));
        for (const auto& s : samples_) {
            strm_w.write_bool(s.stream);
        }
    }

    // --- EVTS, LOGC, PARM, SNAP CHUNKS ---
    const auto& events = model_set_ ? model_set_->events() : std::vector<EventDescriptionModel>{};
    evts_w.write(static_cast<uint32_t>(events.size()));
    logc_w.write(static_cast<uint32_t>(events.size()));

    // PARM part 1: global parameters
    const auto& global_params = model_set_ ? model_set_->global_parameters() : std::vector<ParameterDefinition>{};
    parm_w.write(static_cast<uint32_t>(global_params.size()));
    for (const auto& gp : global_params) {
        if (!bank_parm::valid_parameter(gp)) throw std::invalid_argument("Invalid global parameter");
        parm_w.write(gp.id.hi);
        parm_w.write(gp.id.lo);
        parm_w.write(intern_string(gp.name));
        parm_w.write(static_cast<uint8_t>(gp.kind));
        parm_w.write(gp.min_value);
        parm_w.write(gp.max_value);
        parm_w.write(gp.default_value);
        parm_w.write(gp.seek_speed);
        parm_w.write(gp.seek_speed_down);
        parm_w.write(static_cast<uint32_t>(gp.labels.size()));
        for (const auto& l : gp.labels) {
            parm_w.write(intern_string(l));
        }
    }

    // PARM part 2: per-event
    parm_w.write(static_cast<uint32_t>(events.size()));
    snap_w.write(static_cast<uint32_t>(events.size()));

    for (const auto& ev : events) {
        // EVTS
        evts_w.write(ev.id.hi);
        evts_w.write(ev.id.lo);
        evts_w.write(intern_string(ev.path));
        evts_w.write(intern_string(ev.bus_path));
        evts_w.write(ev.volume_db);
        evts_w.write(ev.pitch_semitones);
        evts_w.write(ev.pan);
        evts_w.write(ev.max_instances);
        evts_w.write(static_cast<uint8_t>(ev.steal));
        evts_w.write(ev.cooldown_ms);
        evts_w.write(ev.priority);
        evts_w.write(ev.stop_fadeout_ms);
        evts_w.write(static_cast<uint32_t>(ev.tracks.size()));

        std::vector<int32_t> flat_condition_indices;
        std::vector<const Condition*> conditions;
        const auto condition_index = [&](const Condition& c) -> int32_t {
            if (!bank_parm::valid_condition(c)) throw std::invalid_argument("Invalid condition");
            if (c.probability_pct == 100 && c.terms.empty()) return -1;
            if (conditions.size() >= 64) throw std::invalid_argument("More than 64 conditions");
            conditions.push_back(&c);
            return static_cast<int32_t>(conditions.size() - 1);
        };

        for (const auto& trk : ev.tracks) {
            evts_w.write(trk.id.hi);
            evts_w.write(trk.id.lo);
            evts_w.write(trk.volume_db);
            evts_w.write(trk.pan);
            evts_w.write(static_cast<uint32_t>(trk.instruments.size()));
            for (const auto& inst : trk.instruments) {
                evts_w.write(inst.id.hi);
                evts_w.write(inst.id.lo);
                evts_w.write(static_cast<uint8_t>(inst.kind));
                evts_w.write(inst.offset_ms);
                evts_w.write(inst.length_ms);
                evts_w.write(inst.start_offset_ms);
                const uint8_t inst_flags = static_cast<uint8_t>((inst.loop ? 1u : 0u) | (inst.async ? 2u : 0u));
                evts_w.write(inst_flags);
                evts_w.write(inst.fade_in_ms);
                evts_w.write(inst.fade_out_ms);
                evts_w.write(inst.volume_db_base);
                evts_w.write(inst.volume_db_random_range);
                evts_w.write(inst.pitch_st_base);
                evts_w.write(inst.pitch_st_random_range);
                evts_w.write(inst.sample_index);
                evts_w.write(inst.event_ref.hi);
                evts_w.write(inst.event_ref.lo);
                evts_w.write(static_cast<uint8_t>(inst.command_type));
                evts_w.write(inst.command_target.hi);
                evts_w.write(inst.command_target.lo);
                evts_w.write(static_cast<uint8_t>(inst.playlist_mode));
                evts_w.write(static_cast<uint8_t>(inst.playlist_scope));
                evts_w.write(static_cast<uint32_t>(inst.playlist.size()));
                for (const auto& entry : inst.playlist) {
                    evts_w.write(entry.sample_index);
                    evts_w.write(entry.event_guid.hi);
                    evts_w.write(entry.event_guid.lo);
                    evts_w.write(entry.weight);
                }

                // Collect condition index if condition present
                flat_condition_indices.push_back(condition_index(inst.condition));
            }
        }
        evts_w.write(static_cast<uint32_t>(ev.user_props.size()));
        for (const auto& property : ev.user_props) {
            evts_w.write(intern_string(property.first));
            evts_w.write(intern_string(property.second));
        }

        // LOGC
        logc_w.write(ev.id.hi);
        logc_w.write(ev.id.lo);
        logc_w.write(static_cast<uint32_t>(ev.logic_items.size()));
        for (const auto& li : ev.logic_items) {
            logc_w.write(static_cast<uint8_t>(li.kind));
            logc_w.write(li.id.hi);
            logc_w.write(li.id.lo);
            logc_w.write(li.position_ms);
            logc_w.write(li.end_ms);
            logc_w.write(intern_string(li.name));
            logc_w.write(li.bpm);
            logc_w.write(li.ts_upper);
            logc_w.write(li.ts_lower);
            logc_w.write(li.repeat_count);
            logc_w.write_bool(li.until_key_off);
            logc_w.write(intern_string(li.destination));
            logc_w.write(static_cast<uint8_t>(li.quantization));
            logc_w.write(li.crossfade_ms);
            logc_w.write(condition_index(li.condition));
        }

        // PARM per-event
        parm_w.write(ev.id.hi);
        parm_w.write(ev.id.lo);
        parm_w.write(static_cast<uint32_t>(ev.parameters.size()));
        for (const auto& lp : ev.parameters) {
            if (!bank_parm::valid_parameter(lp)) throw std::invalid_argument("Invalid local parameter");
            parm_w.write(lp.id.hi);
            parm_w.write(lp.id.lo);
            parm_w.write(intern_string(lp.name));
            parm_w.write(static_cast<uint8_t>(lp.kind));
            parm_w.write(lp.min_value);
            parm_w.write(lp.max_value);
            parm_w.write(lp.default_value);
            parm_w.write(lp.seek_speed);
            parm_w.write(lp.seek_speed_down);
            parm_w.write(static_cast<uint32_t>(lp.labels.size()));
            for (const auto& l : lp.labels) {
                parm_w.write(intern_string(l));
            }
        }

        parm_w.write(static_cast<uint32_t>(conditions.size()));
        for (const auto* c : conditions) {
            parm_w.write(c->probability_pct);
            parm_w.write(static_cast<uint32_t>(c->terms.size()));
            for (const auto& t : c->terms) {
                bank_parm::write_ref(parm_w, t.param, ev.parameters, global_params);
                parm_w.write(t.min_value); parm_w.write(t.max_value);
            }
        }
        std::size_t mod_count = ev.modulators.size();
        std::size_t auto_count = ev.automations.size();
        for (const auto& track : ev.tracks) {
            auto_count += track.automations.size();
            for (const auto& inst : track.instruments) mod_count += inst.modulators.size();
        }
        parm_w.write(static_cast<uint32_t>(mod_count));
        const auto write_modulator = [&](const Modulator& m, ModulatorOwner owner, uint32_t index) {
            if (!bank_parm::valid_modulator(m)) throw std::invalid_argument("Invalid or asymmetric modulator");
            parm_w.write(static_cast<uint8_t>(m.kind));
            parm_w.write(static_cast<uint8_t>(owner)); parm_w.write(index);
            parm_w.write(static_cast<uint8_t>(m.property));
            parm_w.write(m.kind == ModulatorKind::Random ? m.random_range_max : 0.0f);
            parm_w.write(m.attack_ms); parm_w.write(m.hold_ms); parm_w.write(m.decay_ms);
            parm_w.write(m.sustain); parm_w.write(m.release_ms); parm_w.write_bool(m.release_on_key_off);
        };
        for (const auto& m : ev.modulators) write_modulator(m, ModulatorOwner::Event, 0);
        uint32_t flat_index = 0;
        for (const auto& track : ev.tracks) for (const auto& inst : track.instruments) {
            for (const auto& m : inst.modulators) write_modulator(m, ModulatorOwner::Instrument, flat_index);
            ++flat_index;
        }
        parm_w.write(static_cast<uint32_t>(auto_count));
        const auto write_automation = [&](const Automation& a, AutomationOwner owner, uint32_t index) {
            if (static_cast<unsigned>(a.target) > 2) throw std::invalid_argument("Invalid automation target");
            parm_w.write(static_cast<uint8_t>(owner)); parm_w.write(index);
            parm_w.write(static_cast<uint8_t>(a.target)); parm_w.write(static_cast<uint8_t>(a.is_timeline ? 1 : 0));
            bank_parm::write_ref(parm_w, a.param, ev.parameters, global_params, !a.is_timeline);
            bank_parm::write_curve(parm_w, a.curve);
        };
        for (const auto& a : ev.automations) write_automation(a, AutomationOwner::Event, 0);
        for (uint32_t ti = 0; ti < ev.tracks.size(); ++ti)
            for (const auto& a : ev.tracks[ti].automations) write_automation(a, AutomationOwner::Track, ti);

        parm_w.write(static_cast<uint32_t>(flat_condition_indices.size()));
        for (int32_t c_idx : flat_condition_indices) {
            parm_w.write(c_idx);
        }

        // SNAP per-event
        snap_w.write(ev.id.hi);
        snap_w.write(ev.id.lo);
        snap_w.write(static_cast<uint8_t>(ev.kind));
        snap_w.write(static_cast<uint8_t>(ev.snapshot_mode));
        snap_w.write(ev.fade_in_ms);
        int32_t intensity_idx = -1;
        if (!ev.intensity_param.empty()) {
            for (std::size_t pi = 0; pi < ev.parameters.size(); ++pi) {
                if (ev.parameters[pi].name == ev.intensity_param) {
                    intensity_idx = static_cast<int32_t>(pi);
                    break;
                }
            }
        }
        snap_w.write(intensity_idx);
        snap_w.write(static_cast<uint32_t>(ev.snapshot_targets.size()));
        for (const auto& st : ev.snapshot_targets) {
            snap_w.write(intern_string(st.bus_path));
            snap_w.write(st.value);
            snap_w.write(st.param_id);
            snap_w.write(st.position);
            snap_w.write(static_cast<uint32_t>(st.effect_index));
            snap_w.write(static_cast<int32_t>(st.effect_type));
        }
    }

    // PARM part 3: Effect Automations
    const auto& effect_autos = model_set_ ? model_set_->effect_automations() : std::vector<EffectAutomation>{};
    parm_w.write(static_cast<uint32_t>(effect_autos.size()));
    for (const auto& ea : effect_autos) {
        parm_w.write(intern_string(ea.bus_path));
        parm_w.write(ea.position);
        parm_w.write(static_cast<uint32_t>(ea.effect_index));
        parm_w.write(ea.effect_type);
        parm_w.write(ea.param_index);
        auto ref = ea.param;
        if (!ref.is_global || !bank_parm::resolve(ref, {}, global_params, true))
            throw std::invalid_argument("Unresolved global effect automation parameter");
        parm_w.write(ref.global_id.hi); parm_w.write(ref.global_id.lo);
        bank_parm::write_curve(parm_w, ea.curve);
    }

    // --- VCAS CHUNK ---
    const auto& vcas = model_set_ ? model_set_->vcas() : std::vector<VcaModel>{};
    vcas_w.write(static_cast<uint32_t>(vcas.size()));
    for (const auto& v : vcas) {
        vcas_w.write(intern_string(v.path));
        vcas_w.write(static_cast<uint32_t>(v.bus_paths.size()));
        for (const auto& bp : v.bus_paths) {
            vcas_w.write(intern_string(bp));
        }
        vcas_w.write(static_cast<uint32_t>(v.event_paths.size()));
        for (const auto& ep : v.event_paths) {
            vcas_w.write(intern_string(ep));
        }
    }

    // --- PRGM CHUNK (intern strings before STRS) ---
    ByteWriter prgm_w;
    std::uint32_t prgm_count = 0;
    prgm_w.write(prgm_count);
    for (std::uint32_t ei = 0; ei < events.size(); ++ei) {
        const auto& tracks = events[ei].tracks;
        for (std::uint32_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& instruments = tracks[ti].instruments;
            for (std::uint32_t ii = 0; ii < instruments.size(); ++ii) {
                const auto& inst = instruments[ii];
                if (inst.kind != InstrumentKind::Programmer || inst.programmer_key.empty()) continue;
                prgm_w.write(ei);
                prgm_w.write(ti);
                prgm_w.write(ii);
                prgm_w.write(intern_string(inst.programmer_key));
                ++prgm_count;
            }
        }
    }
    std::memcpy(prgm_w.buffer().data(), &prgm_count, sizeof(prgm_count));

    // --- ATBL CHUNK (intern strings before STRS) ---
    const auto& audio_table = model_set_ ? model_set_->audio_table() : std::vector<AudioTableEntry>{};
    ByteWriter atbl_w;
    atbl_w.write(static_cast<std::uint32_t>(audio_table.size()));
    for (const auto& entry : audio_table) {
        atbl_w.write(intern_string(entry.key));
        atbl_w.write(entry.sample_index);
    }

    // --- PSHT CHUNK (intern parameter names before STRS) ---
    ByteWriter psht_w;
    std::uint32_t psht_count = 0;
    psht_w.write(psht_count);
    for (std::uint32_t ei = 0; ei < events.size(); ++ei) {
        const auto& tracks = events[ei].tracks;
        for (std::uint32_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& instruments = tracks[ti].instruments;
            for (std::uint32_t ii = 0; ii < instruments.size(); ++ii) {
                const auto& inst = instruments[ii];
                if (inst.sheet_parameter.empty()) continue;
                psht_w.write(ei);
                psht_w.write(ti);
                psht_w.write(ii);
                psht_w.write(intern_string(inst.sheet_parameter));
                psht_w.write(inst.param_min);
                psht_w.write(inst.param_max);
                ++psht_count;
            }
        }
    }
    std::memcpy(psht_w.buffer().data(), &psht_count, sizeof(psht_count));

    // --- PMOD CHUNK (parameter velocity & hold_during_playback modifiers) ---
    ByteWriter pmod_w;
    std::uint32_t pmod_count = 0;
    pmod_w.write(pmod_count);
    for (std::uint32_t pi = 0; pi < global_params.size(); ++pi) {
        const auto& gp = global_params[pi];
        if (!bank_parm::valid_parameter(gp)) {
            throw std::invalid_argument("Invalid global parameter modifier");
        }
        if (gp.velocity == 0.0f && !gp.hold_during_playback) continue;
        pmod_w.write(static_cast<std::uint8_t>(1));
        pmod_w.write(static_cast<std::uint32_t>(0));
        pmod_w.write(pi);
        pmod_w.write(gp.velocity);
        pmod_w.write_bool(gp.hold_during_playback);
        ++pmod_count;
    }
    for (std::uint32_t ei = 0; ei < events.size(); ++ei) {
        const auto& locals = events[ei].parameters;
        for (std::uint32_t pi = 0; pi < locals.size(); ++pi) {
            const auto& lp = locals[pi];
            if (!bank_parm::valid_parameter(lp)) {
                throw std::invalid_argument("Invalid local parameter modifier");
            }
            if (lp.velocity == 0.0f && !lp.hold_during_playback) continue;
            pmod_w.write(static_cast<std::uint8_t>(0));
            pmod_w.write(ei);
            pmod_w.write(pi);
            pmod_w.write(lp.velocity);
            pmod_w.write_bool(lp.hold_during_playback);
            ++pmod_count;
        }
    }
    std::memcpy(pmod_w.buffer().data(), &pmod_count, sizeof(pmod_count));

    // --- STRS CHUNK ---
    ByteWriter strs_w;
    strs_w.write(static_cast<uint32_t>(string_table_.size()));
    for (const auto& str : string_table_) {
        strs_w.write(static_cast<uint32_t>(str.size()));
        strs_w.write_bytes(str.data(), str.size());
    }

    ByteWriter idly_w;
    std::uint32_t delay_count = 0;
    idly_w.write(delay_count);
    for (std::uint32_t ei = 0; ei < events.size(); ++ei) {
        const auto& tracks = events[ei].tracks;
        for (std::uint32_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& instruments = tracks[ti].instruments;
            for (std::uint32_t ii = 0; ii < instruments.size(); ++ii) {
                const auto& inst = instruments[ii];
                if (inst.kind != InstrumentKind::Multi || inst.delay_ms_random_range == 0) continue;
                idly_w.write(ei);
                idly_w.write(ti);
                idly_w.write(ii);
                idly_w.write(inst.delay_ms_random_range);
                ++delay_count;
            }
        }
    }
    std::memcpy(idly_w.buffer().data(), &delay_count, sizeof(delay_count));

    ByteWriter track_effects_w;
    bank_track_effects::write(track_effects_w, events);

    // 2. Chunks Assembly
    struct NamedChunk {
        char tag[4];
        const ByteWriter& writer;
    };

    std::vector<NamedChunk> chunks = {
        { {'S','T','R','S'}, strs_w },
        { {'B','U','S','S'}, bus_w },
        { {'E','V','T','S'}, evts_w },
        { {'S','M','P','L'}, smpl_w },
        { {'D','A','T','A'}, data_w },
        { {'L','O','G','C'}, logc_w },
        { {'P','A','R','M'}, parm_w },
        { {'S','N','A','P'}, snap_w },
        { {'V','C','A','S'}, vcas_w }
    };
    if (has_streaming) {
        chunks.push_back({ {'S','T','R','M'}, strm_w });
    }
    if (track_effects_w.size() != 0) {
        chunks.push_back({ {'T','F','X','S'}, track_effects_w });
    }

    if (delay_count > 0) {
        chunks.push_back({ {'I','D','L','Y'}, idly_w });
    }

    ByteWriter sctr_w;
    std::uint32_t scatter_count = 0;
    sctr_w.write(scatter_count);
    for (std::uint32_t ei = 0; ei < events.size(); ++ei) {
        const auto& tracks = events[ei].tracks;
        for (std::uint32_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& instruments = tracks[ti].instruments;
            for (std::uint32_t ii = 0; ii < instruments.size(); ++ii) {
                const auto& inst = instruments[ii];
                if (inst.kind != InstrumentKind::Scatterer) continue;
                sctr_w.write(ei);
                sctr_w.write(ti);
                sctr_w.write(ii);
                sctr_w.write(inst.spawn_interval_min_ms);
                sctr_w.write(inst.spawn_interval_max_ms);
                sctr_w.write(inst.polyphony);
                sctr_w.write(inst.total_sounds);
                sctr_w.write(inst.min_scatter_distance);
                sctr_w.write(inst.max_scatter_distance);
                ++scatter_count;
            }
        }
    }
    std::memcpy(sctr_w.buffer().data(), &scatter_count, sizeof(scatter_count));
    if (scatter_count > 0) {
        chunks.push_back({ {'S','C','T','R'}, sctr_w });
    }
    if (prgm_count > 0) {
        chunks.push_back({ {'P','R','G','M'}, prgm_w });
    }
    if (!audio_table.empty()) {
        chunks.push_back({ {'A','T','B','L'}, atbl_w });
    }
    if (psht_count > 0) {
        chunks.push_back({ {'P','S','H','T'}, psht_w });
    }
    if (pmod_count > 0) {
        chunks.push_back({ {'P','M','O','D'}, pmod_w });
    }
    const uint32_t chunk_count = static_cast<uint32_t>(chunks.size());

    ByteWriter file_w;
    BankHeaderWire header{};
    header.magic[0] = 'O'; header.magic[1] = 'A'; header.magic[2] = 'E'; header.magic[3] = 'B';
    header.version = 1;
    header.flags = 0;
    header.chunk_count = chunk_count;
    file_w.write(header);

    // Placeholder directory
    for (uint32_t i = 0; i < chunk_count; ++i) {
        ChunkDirEntryWire dummy{};
        file_w.write(dummy);
    }

    std::vector<ChunkDirEntryWire> dir_entries(chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        file_w.align_to(16);
        const uint64_t offset = static_cast<uint64_t>(file_w.size());
        const uint64_t length = static_cast<uint64_t>(chunks[i].writer.size());
        const uint32_t crc = calc_crc32(chunks[i].writer.data(), chunks[i].writer.size());

        std::memcpy(dir_entries[i].tag, chunks[i].tag, 4);
        dir_entries[i].offset = offset;
        dir_entries[i].length = length;
        dir_entries[i].crc32 = crc;

        file_w.write_bytes(chunks[i].writer.data(), chunks[i].writer.size());
    }

    // Patch real directory entries
    uint8_t* out_ptr = file_w.buffer().data() + sizeof(BankHeaderWire);
    for (uint32_t i = 0; i < chunk_count; ++i) {
        std::memcpy(out_ptr, &dir_entries[i], sizeof(ChunkDirEntryWire));
        out_ptr += sizeof(ChunkDirEntryWire);
    }

    return file_w.buffer();
}

}
