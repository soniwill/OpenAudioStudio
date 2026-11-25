#pragma once

#include <cmath>
#include <cstdint>

namespace oae {

inline constexpr float pi = 3.14159265358979323846f;
inline constexpr float sqrt2 = 1.41421356237309504880f;

inline float db_to_linear(float db) noexcept
{
    if (db <= -80.0f) {
        return 0.0f;
    }
    return std::pow(10.0f, db / 20.0f);
}

inline float semitones_to_pitch(float st) noexcept
{
    return std::pow(2.0f, st / 12.0f);
}

struct PanGains {
    float left;
    float right;
};

inline PanGains pan_gains(float pan) noexcept
{
    if (pan == 0.0f) {
        return {1.0f, 1.0f};
    }
    if (pan <= -1.0f) {
        return {sqrt2, 0.0f};
    }
    if (pan >= 1.0f) {
        return {0.0f, sqrt2};
    }
    const auto angle = (pan + 1.0f) * (pi * 0.25f);
    return {sqrt2 * std::cos(angle), sqrt2 * std::sin(angle)};
}

constexpr std::uint32_t declick_frames(std::uint32_t rate) noexcept
{
    const auto scaled = static_cast<std::uint32_t>((128ull * rate + 24000ull) / 48000ull);
    return scaled < 16u ? 16u : scaled;
}

constexpr std::uint64_t milliseconds_to_frames(std::uint32_t ms, std::uint32_t rate) noexcept
{
    return (std::uint64_t{ms} * rate + 500u) / 1000u;
}

enum class FadeCurve {
    linear = 0,
    logarithmic = 1,
    scurve = 2
};

struct FadeSpec {
    float from = 1.0f;
    float to = 1.0f;
    std::uint64_t start = 0;
    std::uint64_t duration = 0;
    FadeCurve curve = FadeCurve::linear;

    float gain(std::uint64_t frame) const noexcept
    {
        if (duration == 0) {
            return to;
        }
        if (frame <= start) {
            return from;
        }
        const auto elapsed = frame - start;
        if (elapsed >= duration) {
            return to;
        }
        if (from == to) {
            return from;
        }
        const auto t = static_cast<double>(elapsed) / static_cast<double>(duration);
        if (curve == FadeCurve::logarithmic) {
            constexpr double floor = 1.0e-4;
            const auto a = from < floor ? floor : static_cast<double>(from);
            const auto b = to < floor ? floor : static_cast<double>(to);
            return static_cast<float>(a * std::pow(b / a, t));
        }
        const auto fraction = curve == FadeCurve::scurve ? 0.5 * (1.0 - std::cos(pi * t)) : t;
        return static_cast<float>(from + (static_cast<double>(to) - from) * fraction);
    }
};

}
