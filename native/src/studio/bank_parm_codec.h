#pragma once
#include "bank_format.h"
#include "../core/studio_model.h"
#include <stdexcept>

namespace oae::bank_parm {

inline bool valid_parameter(const ParameterDefinition& p) noexcept
{
    return !p.name.empty() && static_cast<unsigned>(p.kind) <= 2
        && std::isfinite(p.min_value) && std::isfinite(p.max_value)
        && std::isfinite(p.default_value) && p.min_value <= p.default_value
        && p.default_value <= p.max_value && std::isfinite(p.seek_speed)
        && p.seek_speed >= 0 && std::isfinite(p.seek_speed_down)
        && p.seek_speed_down >= 0 && std::isfinite(p.velocity)
        && p.labels.size() <= 64;
}

inline bool resolve(ParamRef& ref, const std::vector<ParameterDefinition>& local,
                    const std::vector<ParameterDefinition>& global, bool by_name)
{
    const auto& defs = ref.is_global ? global : local;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const bool match = by_name && !ref.name.empty() ? defs[i].name == ref.name
            : ref.is_global ? !ref.global_id.is_null() && defs[i].id == ref.global_id
                            : i == ref.local_index;
        if (!match) continue;
        ref.name = defs[i].name;
        ref.local_index = ref.is_global ? 0 : static_cast<uint32_t>(i);
        ref.global_id = ref.is_global ? defs[i].id : Guid{};
        return !ref.is_global || !ref.global_id.is_null();
    }
    return false;
}

inline void write_ref(ByteWriter& w, ParamRef ref,
                      const std::vector<ParameterDefinition>& local,
                      const std::vector<ParameterDefinition>& global, bool active = true)
{
    if (!active) ref = {};
    else if (!resolve(ref, local, global, true)) throw std::invalid_argument("Unresolved parameter reference: " + ref.name);
    w.write_bool(ref.is_global); w.write(ref.local_index);
    w.write(ref.global_id.hi); w.write(ref.global_id.lo);
}

inline bool read_ref(ByteReader& r, ParamRef& ref,
                     const std::vector<ParameterDefinition>& local,
                     const std::vector<ParameterDefinition>& global, bool active = true)
{
    if (!r.read_bool(ref.is_global) || !r.read(ref.local_index)
        || !r.read(ref.global_id.hi) || !r.read(ref.global_id.lo)) return false;
    if (!active) { ref = {}; return true; }
    return resolve(ref, local, global, false);
}

inline bool valid_curve(const Curve& curve) noexcept
{
    if (curve.points.empty() || curve.points.size() > 32) return false;
    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        const auto& p = curve.points[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || static_cast<unsigned>(p.shape) > 4
            || (i && p.x <= curve.points[i - 1].x)) return false;
    }
    return true;
}

inline void write_curve(ByteWriter& w, const Curve& curve)
{
    if (!valid_curve(curve)) throw std::invalid_argument("Invalid automation curve");
    w.write(static_cast<uint32_t>(curve.points.size()));
    for (const auto& p : curve.points) { w.write(p.x); w.write(p.y); w.write(static_cast<uint8_t>(p.shape)); }
}

inline bool read_curve(ByteReader& r, Curve& curve)
{
    uint32_t count = 0;
    if (!r.read(count) || count == 0 || count > 32 || count > r.remaining() / 9) return false;
    curve.points.resize(count);
    for (auto& p : curve.points) {
        uint8_t shape = 0;
        if (!r.read(p.x) || !r.read(p.y) || !r.read(shape)) return false;
        p.shape = static_cast<CurveShape>(shape);
    }
    return valid_curve(curve);
}

inline bool valid_modulator(const Modulator& m) noexcept
{
    if (static_cast<unsigned>(m.kind) > 1 || static_cast<unsigned>(m.property) > 2) return false;
    if (m.kind == ModulatorKind::Random) {
        const float limit = m.property == ModulatorProperty::Volume ? 80.0f
            : m.property == ModulatorProperty::Pitch ? 48.0f : 1.0f;
        return std::isfinite(m.random_range_max) && m.random_range_max >= 0
            && m.random_range_max <= limit && m.random_range_min == -m.random_range_max;
    }
    return std::isfinite(m.attack_ms) && m.attack_ms >= 0 && std::isfinite(m.hold_ms)
        && m.hold_ms >= 0 && std::isfinite(m.decay_ms) && m.decay_ms >= 0
        && std::isfinite(m.release_ms) && m.release_ms >= 0
        && std::isfinite(m.sustain) && m.sustain >= 0 && m.sustain <= 1;
}

inline bool valid_condition(const Condition& c) noexcept
{
    if (!std::isfinite(c.probability_pct) || c.probability_pct < 0 || c.probability_pct > 100 || c.terms.size() > 4) return false;
    for (const auto& t : c.terms) if (!std::isfinite(t.min_value) || !std::isfinite(t.max_value) || t.min_value > t.max_value) return false;
    return true;
}

}
