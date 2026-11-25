#pragma once

#include "bus_dsp.h"

#include <vector>

namespace oae {

class Bus;

struct ParamRange {
    float low;
    float high;
    float initial;
};

const ParamRange* effect_parameter(int type, int id) noexcept;

class Effect {
public:
    Effect(Bus& bus, int type, std::uint32_t rate, std::uint32_t channels);
    Bus& bus() const noexcept { return *bus_; }
    int type() const noexcept { return type_; }
    float parameter(int id) const noexcept { return control_.params[static_cast<std::size_t>(id)]; }
    bool set_parameter(int id, float value) noexcept;
    void set_bypass(bool bypass) noexcept;
    bool bypass() const noexcept { return control_.bypass; }
    void set_mix(float wet, float dry) noexcept;
    float wet() const noexcept { return control_.wet; }
    float dry() const noexcept { return control_.dry; }
    void set_sidechain(std::shared_ptr<SidechainTap> tap) noexcept;
    void process(float* output, std::uint32_t frames, std::uint64_t previousBlock) noexcept;

private:
    using Frame = std::array<float, OAE_METER_MAX_CHANNELS>;
    struct Settings {
        std::array<float, 5> params{};
        float wet = 1.0f;
        float dry = 0.0f;
        bool bypass = false;
    };

    struct Coefficients {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    struct Biquad {
        double z1 = 0, z2 = 0;
        float process(float input, const Coefficients& coeff) noexcept;
    };

    struct ReverbLine {
        std::vector<float> samples;
        std::size_t head = 0;
        float memory = 0;
        float comb(float input, float feedback, float damping) noexcept;
        float allpass(float input) noexcept;
        void push(float value) noexcept;
    };

    struct ReverbChannel {
        std::array<ReverbLine, 8> combs;
        std::array<ReverbLine, 4> allpasses;
    };

    struct Peak {
        std::uint64_t frame = 0;
        float level = 0;
    };

    void publish() noexcept;
    void read_settings() noexcept;
    void update_filter() noexcept;
    static Coefficients biquad(float frequency, float q, std::uint32_t rate, bool high) noexcept;
    float cascade(std::size_t lane, std::uint32_t channel, float sample, const Coefficients& coeff) noexcept;
    void filters(Frame& data) noexcept;
    void reverb(Frame& data) noexcept;
    void delay(Frame& data, std::size_t distance) noexcept;
    void limiter(Frame& data, float inputGain, float ceiling, float release) noexcept;
    void compressor(Frame& data, float keyGain, float threshold, float exponent,
        float attack, float release, float makeup) noexcept;

    Bus* bus_;
    int type_;
    std::uint32_t rate_;
    std::uint32_t channels_;

    Settings control_{};
    Settings audio_{};
    std::atomic<unsigned> seq_{0};
    unsigned seen_ = 0;
    std::array<std::atomic<float>, 5> params_{};
    std::atomic<float> wet_{1}, dry_{0};
    std::atomic<bool> bypass_{false};
    std::shared_ptr<SidechainTap> controlTap_;
    std::atomic<SidechainTap*> tap_{nullptr};

    std::array<float, 5> filtered_{};
    std::array<Coefficients, 4> coeffs_{};
    std::array<std::array<std::array<Biquad, 2>, OAE_METER_MAX_CHANNELS>, 6> filterState_{};
    std::array<float, 3> bandGains_{};
    float filterStep_;
    unsigned untilFilter_ = 0;
    bool firstFilter_ = true;

    std::vector<float> delay_;
    std::size_t delayFrames_ = 0;
    std::size_t write_ = 0;
    std::vector<Peak> peaks_;
    std::size_t front_ = 0, back_ = 0;
    std::uint64_t frame_ = 0;
    float dynamicsGain_ = 1;
    std::vector<ReverbChannel> reverb_;
};

}
