#pragma once

#include <oae_audio.h>

#include <cmath>

namespace oae {

inline bool valid_pan(float value) noexcept
{
    return value >= -1.0f && value <= 1.0f;
}

inline bool valid_fade(float target, int curve) noexcept
{
    return std::isfinite(target) && target >= 0.0f
        && curve >= OAE_FADE_LINEAR && curve <= OAE_FADE_SCURVE;
}

inline bool valid_vec3(const oae_vec3& v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline bool valid_3d_attributes(const oae_3d_attributes& a) noexcept
{
    return valid_vec3(a.position) && valid_vec3(a.velocity)
        && valid_vec3(a.forward) && valid_vec3(a.up);
}

inline bool valid_spatializer_settings(const oae_spatializer_settings& s) noexcept
{
    return s.rolloff >= OAE_ROLLOFF_LINEAR && s.rolloff <= OAE_ROLLOFF_LINEAR_SQUARED
        && std::isfinite(s.min_distance) && s.min_distance >= 0.0f
        && std::isfinite(s.max_distance) && s.max_distance >= s.min_distance
        && std::isfinite(s.sound_size) && s.sound_size >= 0.0f
        && std::isfinite(s.min_extent_deg) && s.min_extent_deg >= 0.0f && s.min_extent_deg <= 360.0f
        && std::isfinite(s.cone_inner_angle_deg) && s.cone_inner_angle_deg >= 0.0f && s.cone_inner_angle_deg <= 360.0f
        && std::isfinite(s.cone_outer_angle_deg) && s.cone_outer_angle_deg >= 0.0f && s.cone_outer_angle_deg <= 360.0f
        && std::isfinite(s.cone_outer_gain) && s.cone_outer_gain >= 0.0f && s.cone_outer_gain <= 1.0f;
}

inline bool valid_utf8_argument(const char* path) noexcept
{
    if (!path || !*path) {
        return false;
    }
    auto* text = reinterpret_cast<const unsigned char*>(path);
    while (*text) {
        const unsigned int first = *text++;
        if (first < 0x80) {
            continue;
        }
        unsigned int codepoint = 0;
        unsigned int minimum = 0;
        unsigned int count = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            codepoint = first & 0x1F;
            minimum = 0x80;
            count = 1;
        } else if (first >= 0xE0 && first <= 0xEF) {
            codepoint = first & 0x0F;
            minimum = 0x800;
            count = 2;
        } else if (first >= 0xF0 && first <= 0xF4) {
            codepoint = first & 0x07;
            minimum = 0x10000;
            count = 3;
        } else {
            return false;
        }
        for (unsigned int i = 0; i < count; ++i) {
            const unsigned int next = *text++;
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return false;
        }
    }
    return true;
}

}
