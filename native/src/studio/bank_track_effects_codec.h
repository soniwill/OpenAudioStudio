#pragma once

#include "bank_format.h"
#include "../core/effect.h"
#include "../core/studio_model.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace oae::bank_track_effects {

/** Validates one effect and the two independent insert-chain limits. */
inline bool validate(const TrackEffectSpec& effect, unsigned (&counts)[2])
{
    if (effect.position < 0 || effect.position > 1 || ++counts[effect.position] > 8 ||
        effect_parameter(effect.type, 0) == nullptr ||
        !(effect.wet >= 0.0f && effect.wet <= 1.0f) ||
        !(effect.dry >= 0.0f && effect.dry <= 1.0f) || effect.params.size() > 5) {
        return false;
    }

    unsigned seen = 0;
    float low = 400.0f;
    float high = 4000.0f;
    for (const auto& [id, value] : effect.params) {
        const auto* range = effect_parameter(effect.type, id);
        if (range == nullptr || !(value >= range->low && value <= range->high) ||
            (seen & (1u << id)) != 0) {
            return false;
        }
        seen |= 1u << id;
        if (effect.type == OAE_EFFECT_EQ3) {
            if (id == OAE_EQ3_LOW_XOVER_HZ) low = value;
            if (id == OAE_EQ3_HIGH_XOVER_HZ) high = value;
        }
    }
    return low < high;
}

/** Writes a sparse TFXS chunk, leaving the output empty for banks without track effects. */
inline void write(ByteWriter& writer, const std::vector<EventDescriptionModel>& events)
{
    uint32_t records = 0;
    for (const auto& event : events) {
        for (const auto& track : event.tracks) {
            if (!track.effects.empty()) {
                if (records == std::numeric_limits<uint32_t>::max()) {
                    throw std::invalid_argument("Too many track effect records");
                }
                ++records;
            }
        }
    }
    if (records == 0) return;

    writer.write(records);
    for (size_t ei = 0; ei < events.size(); ++ei) {
        const auto& tracks = events[ei].tracks;
        for (size_t ti = 0; ti < tracks.size(); ++ti) {
            const auto& effects = tracks[ti].effects;
            if (effects.empty()) continue;
            if (effects.size() > 16 || ei > std::numeric_limits<uint32_t>::max() ||
                ti > std::numeric_limits<uint32_t>::max()) {
                throw std::invalid_argument("Invalid track effect chain");
            }

            writer.write(static_cast<uint32_t>(ei));
            writer.write(static_cast<uint32_t>(ti));
            writer.write(static_cast<uint8_t>(effects.size()));
            unsigned counts[2]{};
            for (const auto& effect : effects) {
                if (!validate(effect, counts)) throw std::invalid_argument("Invalid track effect");
                writer.write(static_cast<uint8_t>(effect.type));
                writer.write(static_cast<uint8_t>(effect.position));
                writer.write_bool(effect.bypass);
                writer.write(effect.wet);
                writer.write(effect.dry);
                writer.write(static_cast<uint8_t>(effect.params.size()));
                for (const auto& [id, value] : effect.params) {
                    writer.write(static_cast<uint8_t>(id));
                    writer.write(value);
                }
            }
        }
    }
}

/** Applies a TFXS payload to events already decoded from EVTS. */
inline bool read(ByteReader& reader, std::vector<EventDescriptionModel>& events)
{
    uint32_t records = 0;
    if (!reader.read(records) || records == 0 || records > reader.remaining() / 21) return false;

    std::unordered_set<uint64_t> seen;
    for (uint32_t record = 0; record < records; ++record) {
        uint32_t event = 0;
        uint32_t track = 0;
        uint8_t count = 0;
        if (!reader.read(event) || !reader.read(track) || !reader.read(count) ||
            event >= events.size() || track >= events[event].tracks.size() ||
            count == 0 || count > 16 || count > reader.remaining() / 12 ||
            !seen.insert((static_cast<uint64_t>(event) << 32) | track).second) {
            return false;
        }
        auto& effects = events[event].tracks[track].effects;
        effects.reserve(count);
        unsigned counts[2]{};
        for (unsigned index = 0; index < count; ++index) {
            TrackEffectSpec effect;
            uint8_t type = 0;
            uint8_t position = 0;
            uint8_t parameters = 0;
            if (!reader.read(type) || !reader.read(position) || !reader.read_bool(effect.bypass) ||
                !reader.read(effect.wet) || !reader.read(effect.dry) || !reader.read(parameters) ||
                parameters > 5 || parameters > reader.remaining() / 5) {
                return false;
            }
            effect.type = type;
            effect.position = position;
            for (unsigned pi = 0; pi < parameters; ++pi) {
                uint8_t id = 0;
                float value = 0;
                if (!reader.read(id) || !reader.read(value)) return false;
                effect.params.emplace_back(id, value);
            }
            if (!validate(effect, counts)) return false;
            effects.push_back(std::move(effect));
        }
    }
    return reader.remaining() == 0;
}

} // namespace oae::bank_track_effects

