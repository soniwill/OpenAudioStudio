#pragma once

#include <cstdint>

namespace oae {

/**
 * Deterministic pseudo-random number generator based on PCG-XSH-RR (64-bit state, 32-bit output).
 */
class Pcg32 {
public:
    Pcg32() noexcept : state_(0x853c49e6748fea9bull), inc_(0xda3e39cb94b95bdbull | 1ull) {}

    explicit Pcg32(std::uint64_t state, std::uint64_t seq = 1ull) noexcept
    {
        seed(state, seq);
    }

    void seed(std::uint64_t state, std::uint64_t seq = 1ull) noexcept
    {
        state_ = 0;
        inc_ = (seq << 1u) | 1ull;
        state_ = (state + inc_) * 6364136223846793005ull + inc_;
        step();
    }

    std::uint32_t next() noexcept
    {
        const std::uint64_t oldstate = state_;
        step();
        const auto xorshifted = static_cast<std::uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
        const auto rot = static_cast<std::uint32_t>(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-static_cast<std::int32_t>(rot)) & 31));
    }

    std::uint32_t next_bounded(std::uint32_t bound) noexcept
    {
        if (bound <= 1u) {
            return 0;
        }
        const std::uint32_t threshold = (0u - bound) % bound;
        for (;;) {
            const std::uint32_t r = next();
            if (r >= threshold) {
                return r % bound;
            }
        }
    }

    float next_float() noexcept
    {
        return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f);
    }

    float next_range(float min_val, float max_val) noexcept
    {
        return min_val + next_float() * (max_val - min_val);
    }

private:
    void step() noexcept
    {
        state_ = state_ * 6364136223846793005ull + inc_;
    }

    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

}
