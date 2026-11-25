#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace oae {

enum class CurveShape : std::uint8_t {
    LINEAR = 0,
    HOLD = 1,
    EASE_IN = 2,
    EASE_OUT = 3,
    S_CURVE = 4
};

struct CurvePoint {
    float x = 0.0f;
    float y = 0.0f;
    CurveShape shape = CurveShape::LINEAR;
};

struct Curve {
    std::vector<CurvePoint> points;

    void add_point(float x, float y, CurveShape shape = CurveShape::LINEAR) {
        CurvePoint pt;
        pt.x = x;
        pt.y = y;
        pt.shape = shape;
        points.push_back(pt);
    }
};

inline float curve_eval(const Curve& c, float x) noexcept
{
    if (c.points.empty()) return 0.0f;
    if (std::isnan(x) || x <= c.points.front().x) return c.points.front().y;
    if (x >= c.points.back().x) return c.points.back().y;
    for (std::size_t i = 0; i + 1 < c.points.size(); ++i) {
        const auto& p0 = c.points[i];
        const auto& p1 = c.points[i + 1];
        if (x >= p0.x && x <= p1.x) {
            if (p0.shape == CurveShape::HOLD) return p0.y;
            const float dx = p1.x - p0.x;
            const float t = dx > 0.0f ? (x - p0.x) / dx : 0.0f;
            float u = t;
            switch (p0.shape) {
                case CurveShape::LINEAR:   u = t; break;
                case CurveShape::EASE_IN:  u = t * t; break;
                case CurveShape::EASE_OUT: u = 1.0f - (1.0f - t) * (1.0f - t); break;
                case CurveShape::S_CURVE:  u = t * t * (3.0f - 2.0f * t); break;
                case CurveShape::HOLD:     return p0.y;
                default: break;
            }
            return p0.y + (p1.y - p0.y) * u;
        }
    }
    return c.points.back().y;
}

}
