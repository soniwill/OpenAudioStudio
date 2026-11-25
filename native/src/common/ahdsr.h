#pragma once

#include <cmath>
#include <cstdint>

namespace oae {

struct AhdsrSpec {
    std::uint64_t t_start{0};
    std::uint32_t attack_frames{0};
    std::uint32_t hold_frames{0};
    std::uint32_t decay_frames{0};
    std::uint32_t release_frames{0};
    float sustain{1.0f};
    std::uint64_t t_rel{~std::uint64_t{0}}; // ~0ULL = release ainda não agendado
};

inline float ahdsr_pre_release(const AhdsrSpec& s, std::uint64_t t) noexcept
{
    if (t < s.t_start) return 0.0f;
    const std::uint64_t dt = t - s.t_start;
    if (dt < s.attack_frames) {
        return s.attack_frames > 0 ? static_cast<float>(dt) / static_cast<float>(s.attack_frames) : 1.0f;
    }
    const std::uint64_t after_a = dt - s.attack_frames;
    if (after_a < s.hold_frames) {
        return 1.0f;
    }
    const std::uint64_t after_h = after_a - s.hold_frames;
    if (after_h < s.decay_frames) {
        const float u = s.decay_frames > 0 ? static_cast<float>(after_h) / static_cast<float>(s.decay_frames) : 0.0f;
        return 1.0f + (s.sustain - 1.0f) * u;
    }
    return s.sustain;
}

inline float ahdsr_eval(const AhdsrSpec& s, std::uint64_t t) noexcept
{
    if (t < s.t_rel) {
        return ahdsr_pre_release(s, t);
    }
    if (s.release_frames == 0 || t >= s.t_rel + s.release_frames) {
        return 0.0f;
    }
    const float level_at_rel = ahdsr_pre_release(s, s.t_rel);
    const float u = static_cast<float>(t - s.t_rel) / static_cast<float>(s.release_frames);
    return level_at_rel * (1.0f - u);
}

}
