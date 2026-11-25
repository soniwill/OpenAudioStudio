#include "bank_reader.h"
#include "bank_parm_codec.h"
#include "bank_track_effects_codec.h"

#include <array>
#include <fstream>
#include <unordered_set>

namespace oae {
namespace {

bool is_valid_utf8(const uint8_t* data, size_t len) noexcept {
    for (size_t i = 0; i < len; ) {
        const uint8_t b = data[i];
        if (b == 0) return false; // NUL embutido proibido
        if (b < 0x80) {
            ++i;
        } else if ((b & 0xE0) == 0xC0) {
            if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80) return false;
            if (b < 0xC2) return false; // overlong
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) return false;
            if (b == 0xE0 && data[i + 1] < 0xA0) return false; // overlong
            if (b == 0xED && data[i + 1] >= 0xA0) return false; // surrogate
            i += 3;
        } else if ((b & 0xF8) == 0xF0) {
            if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) return false;
            if (b == 0xF0 && data[i + 1] < 0x90) return false; // overlong
            if (b > 0xF4 || (b == 0xF4 && data[i + 1] > 0x8F)) return false; // > 0x10FFFF
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

}

oae_result BankReader::parse(const uint8_t* data, size_t size, BankParseResult& out_result) noexcept
{
    if (!data || size < sizeof(BankHeaderWire)) {
        return OAE_ERR_FORMAT;
    }

    ByteReader file_r(data, size);
    BankHeaderWire header{};
    if (!file_r.read(header)) {
        return OAE_ERR_FORMAT;
    }

    if (header.magic[0] != 'O' || header.magic[1] != 'A' || header.magic[2] != 'E' || header.magic[3] != 'B') {
        return OAE_ERR_FORMAT;
    }
    if (header.version != 1 || header.flags != 0) {
        return OAE_ERR_FORMAT;
    }

    const size_t dir_bytes = static_cast<size_t>(header.chunk_count) * sizeof(ChunkDirEntryWire);
    if (!file_r.can_read(dir_bytes)) {
        return OAE_ERR_FORMAT;
    }

    std::vector<ChunkDirEntryWire> entries(header.chunk_count);
    std::unordered_set<uint32_t> seen_tags;
    uint64_t max_end = sizeof(BankHeaderWire) + dir_bytes;

    for (uint32_t i = 0; i < header.chunk_count; ++i) {
        if (!file_r.read(entries[i])) return OAE_ERR_FORMAT;
        uint32_t tag_u32 = 0;
        std::memcpy(&tag_u32, entries[i].tag, 4);
        if (seen_tags.find(tag_u32) != seen_tags.end()) {
            return OAE_ERR_FORMAT; // Duplicate chunk tag
        }
        seen_tags.insert(tag_u32);

        // Validation of offsets and sizes
        if (entries[i].offset % 16 != 0) return OAE_ERR_FORMAT;
        if (entries[i].offset < sizeof(BankHeaderWire) + dir_bytes) return OAE_ERR_FORMAT;
        if (entries[i].offset > size || entries[i].length > size - entries[i].offset) {
            return OAE_ERR_FORMAT;
        }

        const uint64_t chunk_end = entries[i].offset + entries[i].length;
        if (chunk_end > max_end) {
            max_end = chunk_end;
        }

        // CRC32 check
        const uint32_t calculated_crc = calc_crc32(data + entries[i].offset, static_cast<size_t>(entries[i].length));
        if (calculated_crc != entries[i].crc32) {
            return OAE_ERR_FORMAT;
        }
    }

    if (size > max_end) {
        return OAE_ERR_FORMAT;
    }

    // Helper to find a chunk
    const auto find_chunk = [&](const char* tag) -> const ChunkDirEntryWire* {
        for (const auto& e : entries) {
            if (std::memcmp(e.tag, tag, 4) == 0) return &e;
        }
        return nullptr;
    };

    // Core chunks must be present
    const auto* strs_entry = find_chunk("STRS");
    const auto* buss_entry = find_chunk("BUSS");
    const auto* evts_entry = find_chunk("EVTS");
    const auto* smpl_entry = find_chunk("SMPL");
    const auto* data_entry = find_chunk("DATA");

    if (!strs_entry || !buss_entry || !evts_entry || !smpl_entry || !data_entry) {
        return OAE_ERR_FORMAT;
    }

    // Parse STRS
    ByteReader strs_r(data + strs_entry->offset, static_cast<size_t>(strs_entry->length));
    uint32_t string_count = 0;
    if (!strs_r.read(string_count)) return OAE_ERR_FORMAT;

    std::vector<std::string> string_table;
    string_table.reserve(string_count);

    for (uint32_t i = 0; i < string_count; ++i) {
        uint32_t len = 0;
        if (!strs_r.read(len)) return OAE_ERR_FORMAT;
        if (!strs_r.can_read(len)) return OAE_ERR_FORMAT;
        const auto* s_ptr = strs_r.current_ptr();
        if (len > 0 && !is_valid_utf8(s_ptr, len)) {
            return OAE_ERR_FORMAT;
        }
        string_table.emplace_back(reinterpret_cast<const char*>(s_ptr), len);
        strs_r.skip(len);
    }
    if (strs_r.remaining() != 0) return OAE_ERR_FORMAT;

    const auto get_string = [&](uint32_t idx, std::string& out) -> bool {
        if (idx >= string_table.size()) return false;
        out = string_table[idx];
        return true;
    };

    // Parse BUSS
    ByteReader buss_r(data + buss_entry->offset, static_cast<size_t>(buss_entry->length));
    uint32_t bus_count = 0;
    if (!buss_r.read(bus_count)) return OAE_ERR_FORMAT;

    std::vector<BusEntry> parsed_buses;
    parsed_buses.reserve(bus_count);

    for (uint32_t i = 0; i < bus_count; ++i) {
        BusEntry b;
        uint32_t name_idx = 0;
        if (!buss_r.read(name_idx) || !get_string(name_idx, b.name)) return OAE_ERR_FORMAT;
        if (b.name.empty()) return OAE_ERR_FORMAT;
        if (!buss_r.read(b.parent_index)) return OAE_ERR_FORMAT;
        if (b.parent_index != -1 && (b.parent_index < 0 || static_cast<uint32_t>(b.parent_index) >= i)) {
            return OAE_ERR_FORMAT;
        }
        if (!buss_r.read(b.volume) || !std::isfinite(b.volume) || b.volume < 0.0f) return OAE_ERR_FORMAT;

        uint32_t fx_count = 0;
        if (!buss_r.read(fx_count)) return OAE_ERR_FORMAT;
        b.effects.reserve(fx_count);

        for (uint32_t fi = 0; fi < fx_count; ++fi) {
            BusEntry::EffectDesc fx;
            if (!buss_r.read(fx.position) || (fx.position != 0 && fx.position != 1)) return OAE_ERR_FORMAT;
            if (!buss_r.read(fx.type) || fx.type < 0 || fx.type > 6) return OAE_ERR_FORMAT;
            if (!buss_r.read_bool(fx.bypass)) return OAE_ERR_FORMAT;
            if (!buss_r.read(fx.sidechain_bus)) return OAE_ERR_FORMAT;
            uint32_t pcount = 0;
            if (!buss_r.read(pcount)) return OAE_ERR_FORMAT;
            fx.params.resize(pcount);
            for (uint32_t pi = 0; pi < pcount; ++pi) {
                if (!buss_r.read(fx.params[pi]) || !std::isfinite(fx.params[pi])) return OAE_ERR_FORMAT;
            }
            if (!buss_r.read(fx.wet) || !std::isfinite(fx.wet) || fx.wet < 0.0f || fx.wet > 1.0f) return OAE_ERR_FORMAT;
            if (!buss_r.read(fx.dry) || !std::isfinite(fx.dry) || fx.dry < 0.0f || fx.dry > 1.0f) return OAE_ERR_FORMAT;
            b.effects.push_back(std::move(fx));
        }
        parsed_buses.push_back(std::move(b));
    }
    if (buss_r.remaining() != 0) return OAE_ERR_FORMAT;

    // Parse SMPL
    ByteReader smpl_r(data + smpl_entry->offset, static_cast<size_t>(smpl_entry->length));
    uint32_t sample_count = 0;
    if (!smpl_r.read(sample_count)) return OAE_ERR_FORMAT;

    std::vector<SampleEntry> parsed_samples;
    parsed_samples.reserve(sample_count);

    for (uint32_t i = 0; i < sample_count; ++i) {
        uint64_t d_offset = 0, d_len = 0, frames = 0;
        uint32_t rate = 0;
        if (!smpl_r.read(d_offset) || !smpl_r.read(d_len) || !smpl_r.read(frames) || !smpl_r.read(rate)) {
            return OAE_ERR_FORMAT;
        }
        if (d_offset % 16 != 0 || d_offset + d_len > data_entry->length) return OAE_ERR_FORMAT;
        SampleEntry se;
        se.data.resize(static_cast<size_t>(d_len));
        if (d_len > 0) {
            std::memcpy(se.data.data(), data + data_entry->offset + d_offset, static_cast<size_t>(d_len));
        }
        se.length_frames = frames;
        se.sample_rate = rate;
        parsed_samples.push_back(std::move(se));
    }
    if (smpl_r.remaining() != 0) return OAE_ERR_FORMAT;

    const auto* strm_entry = find_chunk("STRM");
    if (strm_entry) {
        ByteReader strm_r(data + strm_entry->offset, static_cast<size_t>(strm_entry->length));
        uint32_t strm_count = 0;
        if (!strm_r.read(strm_count) || strm_count != sample_count) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < strm_count; ++i) {
            if (!strm_r.read_bool(parsed_samples[i].stream)) return OAE_ERR_FORMAT;
        }
        if (strm_r.remaining() != 0) return OAE_ERR_FORMAT;
    }

    // Parse EVTS
    ByteReader evts_r(data + evts_entry->offset, static_cast<size_t>(evts_entry->length));
    uint32_t event_count = 0;
    if (!evts_r.read(event_count)) return OAE_ERR_FORMAT;

    std::vector<EventDescriptionModel> parsed_events(event_count);
    for (uint32_t i = 0; i < event_count; ++i) {
        auto& ev = parsed_events[i];
        if (!evts_r.read(ev.id.hi) || !evts_r.read(ev.id.lo)) return OAE_ERR_FORMAT;
        uint32_t path_idx = 0, bus_idx = 0;
        if (!evts_r.read(path_idx) || !get_string(path_idx, ev.path)) return OAE_ERR_FORMAT;
        if (!evts_r.read(bus_idx) || !get_string(bus_idx, ev.bus_path)) return OAE_ERR_FORMAT;
        if (!evts_r.read(ev.volume_db) || !std::isfinite(ev.volume_db) || std::abs(ev.volume_db) > 200.0f) return OAE_ERR_FORMAT;
        if (!evts_r.read(ev.pitch_semitones) || !std::isfinite(ev.pitch_semitones) || std::abs(ev.pitch_semitones) > 200.0f) return OAE_ERR_FORMAT;
        if (!evts_r.read(ev.pan) || !std::isfinite(ev.pan) || ev.pan < -1.0f || ev.pan > 1.0f) return OAE_ERR_FORMAT;
        if (!evts_r.read(ev.max_instances)) return OAE_ERR_FORMAT;
        uint8_t steal_u8 = 0;
        if (!evts_r.read(steal_u8) || steal_u8 > 2) return OAE_ERR_FORMAT;
        ev.steal = static_cast<StealMode>(steal_u8);
        if (!evts_r.read(ev.cooldown_ms) || !evts_r.read(ev.priority) || !evts_r.read(ev.stop_fadeout_ms)) return OAE_ERR_FORMAT;

        uint32_t track_count = 0;
        if (!evts_r.read(track_count)) return OAE_ERR_FORMAT;
        ev.tracks.resize(track_count);

        for (uint32_t ti = 0; ti < track_count; ++ti) {
            auto& trk = ev.tracks[ti];
            if (!evts_r.read(trk.id.hi) || !evts_r.read(trk.id.lo)) return OAE_ERR_FORMAT;
            if (!evts_r.read(trk.volume_db) || !std::isfinite(trk.volume_db) || std::abs(trk.volume_db) > 200.0f) return OAE_ERR_FORMAT;
            if (!evts_r.read(trk.pan) || !std::isfinite(trk.pan) || trk.pan < -1.0f || trk.pan > 1.0f) return OAE_ERR_FORMAT;

            uint32_t inst_count = 0;
            if (!evts_r.read(inst_count)) return OAE_ERR_FORMAT;
            trk.instruments.resize(inst_count);

            for (uint32_t ii = 0; ii < inst_count; ++ii) {
                auto& inst = trk.instruments[ii];
                if (!evts_r.read(inst.id.hi) || !evts_r.read(inst.id.lo)) return OAE_ERR_FORMAT;
                uint8_t kind_u8 = 0;
                if (!evts_r.read(kind_u8) || kind_u8 > 6) return OAE_ERR_FORMAT;
                inst.kind = static_cast<InstrumentKind>(kind_u8);
                if (!evts_r.read(inst.offset_ms) || !evts_r.read(inst.length_ms) || !evts_r.read(inst.start_offset_ms)) return OAE_ERR_FORMAT;
                uint8_t inst_flags = 0;
                if (!evts_r.read(inst_flags) || inst_flags > 3) return OAE_ERR_FORMAT;
                inst.loop = (inst_flags & 1u) != 0;
                inst.async = (inst_flags & 2u) != 0;
                if (!evts_r.read(inst.fade_in_ms) || !evts_r.read(inst.fade_out_ms)) return OAE_ERR_FORMAT;
                if (!evts_r.read(inst.volume_db_base) || !evts_r.read(inst.volume_db_random_range)) return OAE_ERR_FORMAT;
                if (!evts_r.read(inst.pitch_st_base) || !evts_r.read(inst.pitch_st_random_range)) return OAE_ERR_FORMAT;
                if (!evts_r.read(inst.sample_index)) return OAE_ERR_FORMAT;
                if (!evts_r.read(inst.event_ref.hi) || !evts_r.read(inst.event_ref.lo)) return OAE_ERR_FORMAT;
                uint8_t cmd_u8 = 0;
                if (!evts_r.read(cmd_u8) || cmd_u8 > 3) return OAE_ERR_FORMAT;
                inst.command_type = static_cast<CommandType>(cmd_u8);
                if (!evts_r.read(inst.command_target.hi) || !evts_r.read(inst.command_target.lo)) return OAE_ERR_FORMAT;
                uint8_t pmode_u8 = 0, pscope_u8 = 0;
                if (!evts_r.read(pmode_u8) || pmode_u8 > 3) return OAE_ERR_FORMAT;
                inst.playlist_mode = static_cast<PlaylistMode>(pmode_u8);
                if (!evts_r.read(pscope_u8) || pscope_u8 > 1) return OAE_ERR_FORMAT;
                inst.playlist_scope = static_cast<PlaylistScope>(pscope_u8);

                uint32_t pentries = 0;
                if (!evts_r.read(pentries)) return OAE_ERR_FORMAT;
                inst.playlist.resize(pentries);
                for (uint32_t pi = 0; pi < pentries; ++pi) {
                    auto& pe = inst.playlist[pi];
                    if (!evts_r.read(pe.sample_index)) return OAE_ERR_FORMAT;
                    if (!evts_r.read(pe.event_guid.hi) || !evts_r.read(pe.event_guid.lo)) return OAE_ERR_FORMAT;
                    if (!evts_r.read(pe.weight) || !std::isfinite(pe.weight) || pe.weight <= 0.0f) return OAE_ERR_FORMAT;
                }
            }
        }
        uint32_t u_props = 0;
        if (!evts_r.read(u_props) || u_props > evts_r.remaining() / 8) return OAE_ERR_FORMAT;
        for (uint32_t up = 0; up < u_props; ++up) {
            uint32_t k = 0, v = 0;
            if (!evts_r.read(k) || !evts_r.read(v)) return OAE_ERR_FORMAT;
            std::string key, value;
            if (!get_string(k, key) || !get_string(v, value)) return OAE_ERR_FORMAT;
            ev.user_props.emplace_back(std::move(key), std::move(value));
        }
    }
    if (evts_r.remaining() != 0) return OAE_ERR_FORMAT;

    // Parse LOGC (optional)
    const auto* logc_entry = find_chunk("LOGC");
    if (logc_entry) {
        ByteReader logc_r(data + logc_entry->offset, static_cast<size_t>(logc_entry->length));
        uint32_t l_event_count = 0;
        if (!logc_r.read(l_event_count) || l_event_count != event_count) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < event_count; ++i) {
            Guid id;
            if (!logc_r.read(id.hi) || !logc_r.read(id.lo)) return OAE_ERR_FORMAT;
            if (id != parsed_events[i].id) return OAE_ERR_FORMAT;
            uint32_t logic_count = 0;
            if (!logc_r.read(logic_count)) return OAE_ERR_FORMAT;
            parsed_events[i].logic_items.resize(logic_count);
            for (uint32_t li = 0; li < logic_count; ++li) {
                auto& item = parsed_events[i].logic_items[li];
                uint8_t kind_u8 = 0;
                if (!logc_r.read(kind_u8) || kind_u8 > 5) return OAE_ERR_FORMAT;
                item.kind = static_cast<LogicItemKind>(kind_u8);
                if (!logc_r.read(item.id.hi) || !logc_r.read(item.id.lo)) return OAE_ERR_FORMAT;
                if (!logc_r.read(item.position_ms) || !logc_r.read(item.end_ms)) return OAE_ERR_FORMAT;
                uint32_t name_idx = 0;
                if (!logc_r.read(name_idx) || !get_string(name_idx, item.name)) return OAE_ERR_FORMAT;
                if (!logc_r.read(item.bpm) || !logc_r.read(item.ts_upper) || !logc_r.read(item.ts_lower)) return OAE_ERR_FORMAT;
                if (!logc_r.read(item.repeat_count) || !logc_r.read_bool(item.until_key_off)) return OAE_ERR_FORMAT;
                uint32_t dest_idx = 0;
                if (!logc_r.read(dest_idx) || !get_string(dest_idx, item.destination)) return OAE_ERR_FORMAT;
                uint8_t q_u8 = 0;
                if (!logc_r.read(q_u8) || q_u8 > 2) return OAE_ERR_FORMAT;
                item.quantization = static_cast<LogicQuantization>(q_u8);
                if (!logc_r.read(item.crossfade_ms) || !logc_r.read(item.condition_index)) return OAE_ERR_FORMAT;
            }
        }
        if (logc_r.remaining() != 0) return OAE_ERR_FORMAT;
    }

    std::vector<ParameterDefinition> parsed_global_params;
    std::vector<EffectAutomation> parsed_effect_autos;

    if (const auto* track_effects = find_chunk("TFXS")) {
        ByteReader reader(data + track_effects->offset, static_cast<size_t>(track_effects->length));
        if (!bank_track_effects::read(reader, parsed_events)) return OAE_ERR_FORMAT;
    }

    // Parse PARM (optional)
    const auto* parm_entry = find_chunk("PARM");
    if (parm_entry) {
        ByteReader parm_r(data + parm_entry->offset, static_cast<size_t>(parm_entry->length));
        uint32_t g_count = 0;
        if (!parm_r.read(g_count) || g_count > parm_r.remaining() / 45) return OAE_ERR_FORMAT;
        parsed_global_params.resize(g_count);

        for (uint32_t gi = 0; gi < g_count; ++gi) {
            auto& gp = parsed_global_params[gi];
            if (!parm_r.read(gp.id.hi) || !parm_r.read(gp.id.lo)) return OAE_ERR_FORMAT;
            uint32_t n_idx = 0;
            if (!parm_r.read(n_idx) || !get_string(n_idx, gp.name)) return OAE_ERR_FORMAT;
            uint8_t k_u8 = 0;
            if (!parm_r.read(k_u8) || k_u8 > 2) return OAE_ERR_FORMAT;
            gp.kind = static_cast<ParameterKind>(k_u8);
            if (!parm_r.read(gp.min_value) || !parm_r.read(gp.max_value) || !parm_r.read(gp.default_value)) return OAE_ERR_FORMAT;
            if (!parm_r.read(gp.seek_speed) || !parm_r.read(gp.seek_speed_down)) return OAE_ERR_FORMAT;
            uint32_t l_count = 0;
            if (!parm_r.read(l_count) || l_count > 64 || l_count > parm_r.remaining() / 4) return OAE_ERR_FORMAT;
            gp.labels.resize(l_count);
            for (uint32_t li = 0; li < l_count; ++li) {
                uint32_t ls_idx = 0;
                if (!parm_r.read(ls_idx) || !get_string(ls_idx, gp.labels[li])) return OAE_ERR_FORMAT;
            }
        }

        for (const auto& p : parsed_global_params) if (!bank_parm::valid_parameter(p) || p.id.is_null()) return OAE_ERR_FORMAT;
        uint32_t p_event_count = 0;
        if (!parm_r.read(p_event_count) || p_event_count != event_count) return OAE_ERR_FORMAT;

        for (uint32_t ei = 0; ei < event_count; ++ei) {
            Guid id;
            if (!parm_r.read(id.hi) || !parm_r.read(id.lo)) return OAE_ERR_FORMAT;
            if (id != parsed_events[ei].id) return OAE_ERR_FORMAT;

            uint32_t l_count = 0;
            if (!parm_r.read(l_count) || l_count > parm_r.remaining() / 45) return OAE_ERR_FORMAT;
            parsed_events[ei].parameters.resize(l_count);
            for (uint32_t li = 0; li < l_count; ++li) {
                auto& lp = parsed_events[ei].parameters[li];
                if (!parm_r.read(lp.id.hi) || !parm_r.read(lp.id.lo)) return OAE_ERR_FORMAT;
                uint32_t n_idx = 0;
                if (!parm_r.read(n_idx) || !get_string(n_idx, lp.name)) return OAE_ERR_FORMAT;
                uint8_t k_u8 = 0;
                if (!parm_r.read(k_u8) || k_u8 > 2) return OAE_ERR_FORMAT;
                lp.kind = static_cast<ParameterKind>(k_u8);
                if (!parm_r.read(lp.min_value) || !parm_r.read(lp.max_value) || !parm_r.read(lp.default_value)) return OAE_ERR_FORMAT;
                if (!parm_r.read(lp.seek_speed) || !parm_r.read(lp.seek_speed_down)) return OAE_ERR_FORMAT;
                uint32_t lbl_count = 0;
                if (!parm_r.read(lbl_count) || lbl_count > 64 || lbl_count > parm_r.remaining() / 4) return OAE_ERR_FORMAT;
                lp.labels.resize(lbl_count);
                for (uint32_t lbi = 0; lbi < lbl_count; ++lbi) {
                    uint32_t ls_idx = 0;
                    if (!parm_r.read(ls_idx) || !get_string(ls_idx, lp.labels[lbi])) return OAE_ERR_FORMAT;
                }
            }

            auto& event = parsed_events[ei];
            for (const auto& p : event.parameters) if (!bank_parm::valid_parameter(p)) return OAE_ERR_FORMAT;
            std::vector<Instrument*> instruments;
            for (auto& track : event.tracks) for (auto& inst : track.instruments) instruments.push_back(&inst);
            uint32_t cond_count = 0;
            if (!parm_r.read(cond_count) || cond_count > 64 || cond_count > parm_r.remaining() / 8) return OAE_ERR_FORMAT;
            std::vector<Condition> conditions(cond_count);
            for (auto& c : conditions) {
                uint32_t count = 0;
                if (!parm_r.read(c.probability_pct) || !parm_r.read(count) || count > 4 || count > parm_r.remaining() / 29) return OAE_ERR_FORMAT;
                c.terms.resize(count);
                for (auto& t : c.terms) {
                    if (!bank_parm::read_ref(parm_r, t.param, event.parameters, parsed_global_params)
                        || !parm_r.read(t.min_value) || !parm_r.read(t.max_value)) return OAE_ERR_FORMAT;
                }
                if (!bank_parm::valid_condition(c)) return OAE_ERR_FORMAT;
            }
            const auto restore_condition = [&](int32_t index, Condition& c) {
                if (index == -1) { c = {}; return true; }
                if (index < 0 || static_cast<size_t>(index) >= conditions.size()) return false;
                c = conditions[static_cast<size_t>(index)]; return true;
            };
            for (auto& item : event.logic_items)
                if (!restore_condition(item.condition_index, item.condition)) return OAE_ERR_FORMAT;
            uint32_t mod_count = 0;
            if (!parm_r.read(mod_count) || mod_count > parm_r.remaining() / 32) return OAE_ERR_FORMAT;
            for (uint32_t mi = 0; mi < mod_count; ++mi) {
                Modulator m;
                uint8_t kind = 0, owner = 0, prop = 0;
                if (!parm_r.read(kind) || !parm_r.read(owner) || !parm_r.read(m.owner_index) || !parm_r.read(prop)
                    || kind > 1 || (owner != 0 && owner != 2) || prop > 2) return OAE_ERR_FORMAT;
                m.kind = static_cast<ModulatorKind>(kind); m.owner = static_cast<ModulatorOwner>(owner);
                m.property = static_cast<ModulatorProperty>(prop);
                float range = 0;
                if (!parm_r.read(range) || !parm_r.read(m.attack_ms) || !parm_r.read(m.hold_ms)
                    || !parm_r.read(m.decay_ms) || !parm_r.read(m.sustain) || !parm_r.read(m.release_ms)
                    || !parm_r.read_bool(m.release_on_key_off)) return OAE_ERR_FORMAT;
                if (m.kind == ModulatorKind::Random) { m.random_range_max = range; m.random_range_min = -range; }
                if (!bank_parm::valid_modulator(m)) return OAE_ERR_FORMAT;
                if (owner == 0) {
                    if (m.owner_index != 0) return OAE_ERR_FORMAT;
                    event.modulators.push_back(m);
                } else {
                    if (m.owner_index >= instruments.size()) return OAE_ERR_FORMAT;
                    instruments[m.owner_index]->modulators.push_back(m);
                }
            }
            uint32_t auto_count = 0;
            if (!parm_r.read(auto_count) || auto_count > parm_r.remaining() / 41) return OAE_ERR_FORMAT;
            for (uint32_t ai = 0; ai < auto_count; ++ai) {
                Automation a;
                uint8_t owner = 0, prop = 0, source = 0;
                if (!parm_r.read(owner) || !parm_r.read(a.owner_index) || !parm_r.read(prop)
                    || !parm_r.read(source) || owner > 1 || prop > 2 || source > 1) return OAE_ERR_FORMAT;
                a.owner = static_cast<AutomationOwner>(owner); a.target = static_cast<AutomationTarget>(prop);
                a.is_timeline = source == 1;
                if (!bank_parm::read_ref(parm_r, a.param, event.parameters, parsed_global_params, !a.is_timeline)
                    || !bank_parm::read_curve(parm_r, a.curve)) return OAE_ERR_FORMAT;
                if (owner == 0) {
                    if (a.owner_index != 0) return OAE_ERR_FORMAT;
                    event.automations.push_back(std::move(a));
                } else {
                    if (a.owner_index >= event.tracks.size()) return OAE_ERR_FORMAT;
                    event.tracks[a.owner_index].automations.push_back(std::move(a));
                }
            }
            uint32_t flat_count = 0;
            if (!parm_r.read(flat_count) || flat_count != instruments.size() || flat_count > parm_r.remaining() / 4) return OAE_ERR_FORMAT;
            for (uint32_t fi = 0; fi < flat_count; ++fi) {
                int32_t index = -1;
                if (!parm_r.read(index) || !restore_condition(index, instruments[fi]->condition)) return OAE_ERR_FORMAT;
            }
        }

        uint32_t eff_auto_count = 0;
        if (!parm_r.read(eff_auto_count) || eff_auto_count > parm_r.remaining() / 49) return OAE_ERR_FORMAT;
        parsed_effect_autos.resize(eff_auto_count);
        for (uint32_t eai = 0; eai < eff_auto_count; ++eai) {
            auto& ea = parsed_effect_autos[eai];
            uint32_t b_idx = 0;
            if (!parm_r.read(b_idx) || !get_string(b_idx, ea.bus_path)) return OAE_ERR_FORMAT;
            uint32_t fx_idx = 0;
            if (!parm_r.read(ea.position) || !parm_r.read(fx_idx) || !parm_r.read(ea.effect_type) || !parm_r.read(ea.param_index)) return OAE_ERR_FORMAT;
            ea.effect_index = fx_idx;
            if (!parm_r.read(ea.param.global_id.hi) || !parm_r.read(ea.param.global_id.lo)) return OAE_ERR_FORMAT;
            ea.param.is_global = true;
            if (!bank_parm::resolve(ea.param, {}, parsed_global_params, false)
                || !bank_parm::read_curve(parm_r, ea.curve)) return OAE_ERR_FORMAT;
        }
        if (parm_r.remaining() != 0) return OAE_ERR_FORMAT;
    }

    // Parse SNAP (optional)
    const auto* snap_entry = find_chunk("SNAP");
    if (snap_entry) {
        ByteReader snap_r(data + snap_entry->offset, static_cast<size_t>(snap_entry->length));
        uint32_t s_event_count = 0;
        if (!snap_r.read(s_event_count) || s_event_count != event_count) return OAE_ERR_FORMAT;

        for (uint32_t i = 0; i < event_count; ++i) {
            Guid id;
            if (!snap_r.read(id.hi) || !snap_r.read(id.lo)) return OAE_ERR_FORMAT;
            if (id != parsed_events[i].id) return OAE_ERR_FORMAT;
            uint8_t kind_u8 = 0, mode_u8 = 0;
            if (!snap_r.read(kind_u8) || kind_u8 > 1) return OAE_ERR_FORMAT;
            parsed_events[i].kind = static_cast<EventKind>(kind_u8);
            if (!snap_r.read(mode_u8) || mode_u8 > 1) return OAE_ERR_FORMAT;
            parsed_events[i].snapshot_mode = static_cast<SnapshotMode>(mode_u8);
            if (!snap_r.read(parsed_events[i].fade_in_ms)) return OAE_ERR_FORMAT;
            int32_t int_idx = -1;
            if (!snap_r.read(int_idx)) return OAE_ERR_FORMAT;
            if (int_idx >= 0 && static_cast<size_t>(int_idx) < parsed_events[i].parameters.size()) {
                parsed_events[i].intensity_param = parsed_events[i].parameters[int_idx].name;
            }
            uint32_t t_count = 0;
            if (!snap_r.read(t_count)) return OAE_ERR_FORMAT;
            parsed_events[i].snapshot_targets.resize(t_count);
            for (uint32_t ti = 0; ti < t_count; ++ti) {
                auto& st = parsed_events[i].snapshot_targets[ti];
                uint32_t bp_idx = 0;
                if (!snap_r.read(bp_idx) || !get_string(bp_idx, st.bus_path)) return OAE_ERR_FORMAT;
                if (!snap_r.read(st.value) || !snap_r.read(st.param_id) || !snap_r.read(st.position)) return OAE_ERR_FORMAT;
                uint32_t idx = 0;
                int32_t fx_type = 0;
                if (!snap_r.read(idx) || !snap_r.read(fx_type)) return OAE_ERR_FORMAT;
                st.effect_index = idx;
                st.effect_type = st.param_id == -1 ? -1 : fx_type;
            }
        }
        if (snap_r.remaining() != 0) return OAE_ERR_FORMAT;
    }

    std::vector<VcaModel> parsed_vcas;

    // Parse VCAS (optional)
    const auto* vcas_entry = find_chunk("VCAS");
    if (vcas_entry) {
        ByteReader vcas_r(data + vcas_entry->offset, static_cast<size_t>(vcas_entry->length));
        uint32_t vca_count = 0;
        if (!vcas_r.read(vca_count)) return OAE_ERR_FORMAT;
        parsed_vcas.resize(vca_count);

        for (uint32_t vi = 0; vi < vca_count; ++vi) {
            auto& v = parsed_vcas[vi];
            uint32_t p_idx = 0;
            if (!vcas_r.read(p_idx) || !get_string(p_idx, v.path)) return OAE_ERR_FORMAT;
            uint32_t b_count = 0;
            if (!vcas_r.read(b_count)) return OAE_ERR_FORMAT;
            v.bus_paths.resize(b_count);
            for (uint32_t bi = 0; bi < b_count; ++bi) {
                uint32_t bp_idx = 0;
                if (!vcas_r.read(bp_idx) || !get_string(bp_idx, v.bus_paths[bi])) return OAE_ERR_FORMAT;
            }
            uint32_t e_count = 0;
            if (!vcas_r.read(e_count)) return OAE_ERR_FORMAT;
            v.event_paths.resize(e_count);
            for (uint32_t ei = 0; ei < e_count; ++ei) {
                uint32_t ep_idx = 0;
                if (!vcas_r.read(ep_idx) || !get_string(ep_idx, v.event_paths[ei])) return OAE_ERR_FORMAT;
            }
        }
        if (vcas_r.remaining() != 0) return OAE_ERR_FORMAT;
    }

    if (const auto* entry = find_chunk("IDLY")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count) || count != reader.remaining() / 16) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t event = 0, track = 0, instrument = 0, delay = 0;
            if (!reader.read(event) || !reader.read(track) || !reader.read(instrument) ||
                !reader.read(delay) || delay == 0 || delay > 5000 || event >= parsed_events.size()) return OAE_ERR_FORMAT;
            auto& tracks = parsed_events[event].tracks;
            if (track >= tracks.size() || instrument >= tracks[track].instruments.size()) return OAE_ERR_FORMAT;
            auto& inst = tracks[track].instruments[instrument];
            if (inst.kind != InstrumentKind::Multi || inst.delay_ms_random_range != 0) return OAE_ERR_FORMAT;
            inst.delay_ms_random_range = delay;
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    if (const auto* entry = find_chunk("SCTR")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count)) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t event = 0, track = 0, instrument = 0;
            double min_int = 0.0, max_int = 0.0;
            uint32_t polyphony = 0, total_sounds = 0;
            float min_dist = 0.0f, max_dist = 0.0f;
            if (!reader.read(event) || !reader.read(track) || !reader.read(instrument) ||
                !reader.read(min_int) || !reader.read(max_int) ||
                !reader.read(polyphony) || !reader.read(total_sounds) ||
                !reader.read(min_dist) || !reader.read(max_dist) ||
                event >= parsed_events.size()) {
                return OAE_ERR_FORMAT;
            }
            auto& tracks = parsed_events[event].tracks;
            if (track >= tracks.size() || instrument >= tracks[track].instruments.size()) return OAE_ERR_FORMAT;
            auto& inst = tracks[track].instruments[instrument];
            if (inst.kind != InstrumentKind::Scatterer) return OAE_ERR_FORMAT;
            inst.spawn_interval_min_ms = min_int;
            inst.spawn_interval_max_ms = max_int;
            inst.polyphony = polyphony;
            inst.total_sounds = total_sounds;
            inst.min_scatter_distance = min_dist;
            inst.max_scatter_distance = max_dist;
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    if (const auto* entry = find_chunk("PRGM")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count)) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t event = 0, track = 0, instrument = 0, key_idx = 0;
            if (!reader.read(event) || !reader.read(track) || !reader.read(instrument) ||
                !reader.read(key_idx) || event >= parsed_events.size()) {
                return OAE_ERR_FORMAT;
            }
            auto& tracks = parsed_events[event].tracks;
            if (track >= tracks.size() || instrument >= tracks[track].instruments.size()) return OAE_ERR_FORMAT;
            auto& inst = tracks[track].instruments[instrument];
            if (inst.kind != InstrumentKind::Programmer) return OAE_ERR_FORMAT;
            if (!get_string(key_idx, inst.programmer_key)) return OAE_ERR_FORMAT;
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    std::vector<AudioTableEntry> parsed_audio_table;
    if (const auto* entry = find_chunk("ATBL")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count)) return OAE_ERR_FORMAT;
        parsed_audio_table.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t key_idx = 0;
            int32_t sample_index = -1;
            if (!reader.read(key_idx) || !reader.read(sample_index)) return OAE_ERR_FORMAT;
            AudioTableEntry item;
            if (!get_string(key_idx, item.key) || item.key.empty()) return OAE_ERR_FORMAT;
            if (sample_index < 0 || static_cast<size_t>(sample_index) >= parsed_samples.size()) return OAE_ERR_FORMAT;
            item.sample_index = sample_index;
            parsed_audio_table.push_back(std::move(item));
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    if (const auto* entry = find_chunk("PSHT")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count)) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t event = 0, track = 0, instrument = 0, param_str_idx = 0;
            double param_min = 0.0, param_max = 0.0;
            if (!reader.read(event) || !reader.read(track) || !reader.read(instrument) ||
                !reader.read(param_str_idx) || !reader.read(param_min) || !reader.read(param_max) ||
                event >= parsed_events.size()) {
                return OAE_ERR_FORMAT;
            }
            auto& tracks = parsed_events[event].tracks;
            if (track >= tracks.size() || instrument >= tracks[track].instruments.size()) return OAE_ERR_FORMAT;
            auto& inst = tracks[track].instruments[instrument];
            if (!get_string(param_str_idx, inst.sheet_parameter) || inst.sheet_parameter.empty()) return OAE_ERR_FORMAT;
            if (!std::isfinite(param_min) || !std::isfinite(param_max) || param_min > param_max) return OAE_ERR_FORMAT;
            inst.param_min = param_min;
            inst.param_max = param_max;
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    if (const auto* entry = find_chunk("PMOD")) {
        ByteReader reader(data + entry->offset, static_cast<size_t>(entry->length));
        uint32_t count = 0;
        if (!reader.read(count)) return OAE_ERR_FORMAT;
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t is_global = 0;
            uint32_t event_idx = 0, param_idx = 0;
            float velocity = 0.0f;
            bool hold = false;
            if (!reader.read(is_global) || !reader.read(event_idx) || !reader.read(param_idx) ||
                !reader.read(velocity) || !reader.read_bool(hold) || !std::isfinite(velocity) || is_global > 1) {
                return OAE_ERR_FORMAT;
            }
            if (is_global == 1) {
                if (param_idx >= parsed_global_params.size()) return OAE_ERR_FORMAT;
                parsed_global_params[param_idx].velocity = velocity;
                parsed_global_params[param_idx].hold_during_playback = hold;
            } else {
                if (event_idx >= parsed_events.size() || param_idx >= parsed_events[event_idx].parameters.size()) {
                    return OAE_ERR_FORMAT;
                }
                parsed_events[event_idx].parameters[param_idx].velocity = velocity;
                parsed_events[event_idx].parameters[param_idx].hold_during_playback = hold;
            }
        }
        if (reader.remaining() != 0) return OAE_ERR_FORMAT;
    }

    // Build ModelSet
    ModelSetBuilder builder;
    for (auto& ev : parsed_events) {
        builder.add_event(std::move(ev));
    }
    for (auto& gp : parsed_global_params) {
        builder.add_global_parameter(std::move(gp));
    }
    for (auto& ea : parsed_effect_autos) {
        builder.add_effect_automation(std::move(ea));
    }
    for (auto& v : parsed_vcas) {
        builder.add_vca(std::move(v));
    }
    for (auto& at_entry : parsed_audio_table) {
        builder.add_audio_table_entry(std::move(at_entry));
    }

    out_result.model_set = builder.build();
    out_result.samples = std::move(parsed_samples);
    out_result.buses = std::move(parsed_buses);

    return OAE_OK;
}

oae_result BankReader::parse_file(const std::filesystem::path& path, BankParseResult& out_result) noexcept
{
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return OAE_ERR_FILE_NOT_FOUND;
        }
        const auto file_pos = file.tellg();
        if (file_pos < static_cast<std::streamoff>(sizeof(BankHeaderWire))) {
            return OAE_ERR_FORMAT;
        }
        const uint64_t file_size = static_cast<uint64_t>(file_pos);
        file.seekg(0, std::ios::beg);

        BankHeaderWire header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || header.magic[0] != 'O' || header.magic[1] != 'A' || header.magic[2] != 'E' || header.magic[3] != 'B') {
            return OAE_ERR_FORMAT;
        }
        if (header.version != 1 || header.flags != 0 || header.chunk_count == 0 || header.chunk_count > 64) {
            return OAE_ERR_FORMAT;
        }
        const uint64_t dir_bytes = static_cast<uint64_t>(header.chunk_count) * sizeof(ChunkDirEntryWire);
        if (sizeof(BankHeaderWire) + dir_bytes > file_size) {
            return OAE_ERR_FORMAT;
        }

        std::vector<ChunkDirEntryWire> entries(header.chunk_count);
        file.read(reinterpret_cast<char*>(entries.data()), static_cast<std::streamsize>(dir_bytes));
        if (!file) return OAE_ERR_FORMAT;

        for (uint32_t i = 0; i < header.chunk_count; ++i) {
            if (entries[i].offset % 16 != 0) return OAE_ERR_FORMAT;
            if (entries[i].offset > file_size || entries[i].length > file_size - entries[i].offset) {
                return OAE_ERR_FORMAT;
            }
            for (uint32_t j = i + 1; j < header.chunk_count; ++j) {
                if (std::memcmp(entries[i].tag, entries[j].tag, 4) == 0) return OAE_ERR_FORMAT;
            }
        }

        auto find_entry = [&](const char tag[4]) -> const ChunkDirEntryWire* {
            for (const auto& e : entries) {
                if (std::memcmp(e.tag, tag, 4) == 0) return &e;
            }
            return nullptr;
        };

        const auto* strs_entry = find_entry("STRS");
        const auto* buss_entry = find_entry("BUSS");
        const auto* evts_entry = find_entry("EVTS");
        const auto* smpl_entry = find_entry("SMPL");
        const auto* data_entry = find_entry("DATA");
        if (!strs_entry || !buss_entry || !evts_entry || !smpl_entry || !data_entry) {
            return OAE_ERR_FORMAT;
        }

        // Stream DATA chunk through a 64 KB buffer to verify CRC-32 without loading DATA into RAM
        {
            uint32_t crc = 0xFFFFFFFFu;
            file.seekg(static_cast<std::streamoff>(data_entry->offset), std::ios::beg);
            std::array<uint8_t, 65536> buf{};
            uint64_t rem = data_entry->length;
            while (rem > 0) {
                const size_t to_read = static_cast<size_t>(std::min<uint64_t>(rem, buf.size()));
                file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(to_read));
                if (!file) return OAE_ERR_FORMAT;
                for (size_t i = 0; i < to_read; ++i) {
                    crc ^= buf[i];
                    for (int k = 0; k < 8; ++k) {
                        crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
                    }
                }
                rem -= to_read;
            }
            if ((crc ^ 0xFFFFFFFFu) != data_entry->crc32) {
                return OAE_ERR_FORMAT;
            }
        }

        // Check if STRM chunk exists and has any streaming samples
        std::vector<uint8_t> stream_flags;
        bool has_any_streaming = false;
        if (const auto* strm_entry = find_entry("STRM")) {
            std::vector<uint8_t> strm_buf(static_cast<size_t>(strm_entry->length));
            if (!strm_buf.empty()) {
                file.seekg(static_cast<std::streamoff>(strm_entry->offset), std::ios::beg);
                file.read(reinterpret_cast<char*>(strm_buf.data()), static_cast<std::streamsize>(strm_buf.size()));
                if (!file) return OAE_ERR_FORMAT;
            }
            if (calc_crc32(strm_buf.data(), strm_buf.size()) != strm_entry->crc32) return OAE_ERR_FORMAT;
            ByteReader strm_r(strm_buf.data(), strm_buf.size());
            uint32_t count = 0;
            if (!strm_r.read(count)) return OAE_ERR_FORMAT;
            stream_flags.resize(count, 0);
            for (uint32_t i = 0; i < count; ++i) {
                bool s = false;
                if (!strm_r.read_bool(s)) return OAE_ERR_FORMAT;
                stream_flags[i] = s ? 1 : 0;
                if (s) has_any_streaming = true;
            }
            if (strm_r.remaining() != 0) return OAE_ERR_FORMAT;
        }

        // If no streaming samples exist, we can read the file once and reuse BankReader::parse
        if (!has_any_streaming) {
            std::vector<uint8_t> full_buf(static_cast<size_t>(file_size));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(full_buf.data()), static_cast<std::streamsize>(file_size));
            if (!file) return OAE_ERR_FORMAT;
            return parse(full_buf.data(), full_buf.size(), out_result);
        }

        // Build a compact synthetic buffer with all metadata chunks + empty DATA chunk for parse(),
        // then populate non-streaming sample payloads directly from file offsets!
        struct SampleSlice {
            uint64_t offset = 0;
            uint64_t length = 0;
        };
        std::vector<SampleSlice> slices;
        {
            std::vector<uint8_t> smpl_buf(static_cast<size_t>(smpl_entry->length));
            file.seekg(static_cast<std::streamoff>(smpl_entry->offset), std::ios::beg);
            file.read(reinterpret_cast<char*>(smpl_buf.data()), static_cast<std::streamsize>(smpl_buf.size()));
            if (!file) return OAE_ERR_FORMAT;
            if (calc_crc32(smpl_buf.data(), smpl_buf.size()) != smpl_entry->crc32) return OAE_ERR_FORMAT;
            ByteReader sr(smpl_buf.data(), smpl_buf.size());
            uint32_t sc = 0;
            if (!sr.read(sc) || sc != stream_flags.size()) return OAE_ERR_FORMAT;
            slices.resize(sc);
            for (uint32_t i = 0; i < sc; ++i) {
                uint64_t f = 0; uint32_t r = 0;
                if (!sr.read(slices[i].offset) || !sr.read(slices[i].length) || !sr.read(f) || !sr.read(r)) {
                    return OAE_ERR_FORMAT;
                }
                if (slices[i].offset % 16 != 0 || slices[i].offset + slices[i].length > data_entry->length) {
                    return OAE_ERR_FORMAT;
                }
            }
            if (sr.remaining() != 0) return OAE_ERR_FORMAT;
        }

        // Read all metadata chunks (everything except DATA) and parse metadata via a compact buffer
        // where DATA length is 0 and SMPL offsets/lengths are zeroed temporarily.
        std::vector<std::vector<uint8_t>> chunk_payloads(header.chunk_count);
        for (uint32_t i = 0; i < header.chunk_count; ++i) {
            if (std::memcmp(entries[i].tag, "DATA", 4) == 0) {
                continue; // Do NOT read DATA into RAM
            }
            chunk_payloads[i].resize(static_cast<size_t>(entries[i].length));
            if (!chunk_payloads[i].empty()) {
                file.seekg(static_cast<std::streamoff>(entries[i].offset), std::ios::beg);
                file.read(reinterpret_cast<char*>(chunk_payloads[i].data()), static_cast<std::streamsize>(chunk_payloads[i].size()));
                if (!file) return OAE_ERR_FORMAT;
            }
            if (calc_crc32(chunk_payloads[i].data(), chunk_payloads[i].size()) != entries[i].crc32) {
                return OAE_ERR_FORMAT;
            }
            if (std::memcmp(entries[i].tag, "SMPL", 4) == 0) {
                // Zero out d_offset and d_len in the synthetic SMPL copy so parse() passes DATA length==0 check
                ByteReader sr(chunk_payloads[i].data(), chunk_payloads[i].size());
                uint32_t sc = 0;
                sr.read(sc);
                for (uint32_t si = 0; si < sc; ++si) {
                    const size_t entry_pos = 4 + si * (sizeof(uint64_t) * 3 + sizeof(uint32_t));
                    std::memset(chunk_payloads[i].data() + entry_pos, 0, sizeof(uint64_t) * 2);
                }
            }
        }

        ByteWriter meta_w;
        meta_w.write(header);
        const size_t dir_pos = meta_w.size();
        for (uint32_t i = 0; i < header.chunk_count; ++i) {
            ChunkDirEntryWire dummy{};
            meta_w.write(dummy);
        }
        std::vector<ChunkDirEntryWire> meta_dir(header.chunk_count);
        for (uint32_t i = 0; i < header.chunk_count; ++i) {
            meta_w.align_to(16);
            std::memcpy(meta_dir[i].tag, entries[i].tag, 4);
            meta_dir[i].offset = static_cast<uint64_t>(meta_w.size());
            meta_dir[i].length = static_cast<uint64_t>(chunk_payloads[i].size());
            meta_dir[i].crc32 = calc_crc32(chunk_payloads[i].data(), chunk_payloads[i].size());
            meta_w.write_bytes(chunk_payloads[i].data(), chunk_payloads[i].size());
        }
        std::memcpy(meta_w.buffer().data() + dir_pos, meta_dir.data(), meta_dir.size() * sizeof(ChunkDirEntryWire));

        const auto meta_res = parse(meta_w.data(), meta_w.size(), out_result);
        if (meta_res != OAE_OK) {
            return meta_res;
        }

        // Now populate preloaded sample data and streaming file slice coordinates
        for (size_t i = 0; i < out_result.samples.size(); ++i) {
            auto& se = out_result.samples[i];
            se.file_offset = data_entry->offset + slices[i].offset;
            se.file_length = slices[i].length;
            if (!se.stream && slices[i].length > 0) {
                se.data.resize(static_cast<size_t>(slices[i].length));
                file.seekg(static_cast<std::streamoff>(se.file_offset), std::ios::beg);
                file.read(reinterpret_cast<char*>(se.data.data()), static_cast<std::streamsize>(se.data.size()));
                if (!file) return OAE_ERR_FORMAT;
            }
        }

        return OAE_OK;
    } catch (...) {
        return OAE_ERR_INTERNAL;
    }
}

}
