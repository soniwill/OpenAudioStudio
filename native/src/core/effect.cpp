#include "effect.h"
#include "bus.h"

#include <algorithm>
#include <cmath>

namespace oae {
namespace {

constexpr ParamRange ranges[][5] = {
    {{-12,0,-0.1f},{1,1000,50},{0,24,0}},
    {{-60,0,-20},{1,20,4},{0.1f,500,10},{1,5000,100},{0,24,0}},
    {{-80,12,0},{-80,12,0},{-80,12,0},{20,20000,400},{20,20000,4000}},
    {{10,20000,20000},{0.1f,10,0.7071f}},
    {{10,20000,10},{0.1f,10,0.7071f}},
    {{0,1,0.5f},{0,1,0.5f},{0,1,1}},
    {{1,2000,300},{0,0.95f,0}}
};
constexpr int counts[] = {3, 5, 5, 2, 2, 3, 2};

float decibels(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

float smoothing(float ms, std::uint32_t rate) noexcept
{
    return std::exp(-1000.0f / (ms * static_cast<float>(rate)));
}

float quiet(float value) noexcept { return std::fabs(value) < 1e-20f ? 0.0f : value; }

float safe_sample(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -1.0e9f, 1.0e9f) : 0.0f;
}

std::size_t frames_for(float ms, std::uint32_t rate) noexcept
{
    return std::max<std::size_t>(1, static_cast<std::size_t>(
        std::llround(static_cast<double>(ms) * rate / 1000.0)));
}

}

const ParamRange* effect_parameter(int type, int id) noexcept
{
    return type >= 0 && type < 7 && id >= 0 && id < counts[type] ? &ranges[type][id] : nullptr;
}

Effect::Effect(Bus& bus, int type, std::uint32_t rate, std::uint32_t channels)
    : bus_(&bus), type_(type), rate_(rate), channels_(channels),
      filterStep_(1.0f - std::exp(-32.0f / (0.02f * static_cast<float>(rate))))
{
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    static_assert(std::atomic<SidechainTap*>::is_always_lock_free);
    for (int id = 0; id < counts[type]; ++id) {
        control_.params[static_cast<std::size_t>(id)] = ranges[type][id].initial;
    }
    audio_ = control_;
    if (type_ == OAE_EFFECT_DELAY || type_ == OAE_EFFECT_LIMITER) {
        delayFrames_ = frames_for(type_ == OAE_EFFECT_DELAY ? 2000.0f : 1.5f, rate_);
        delay_.resize(delayFrames_ * channels_);
        if (type_ == OAE_EFFECT_LIMITER) {
            peaks_.resize(delayFrames_ + 2);
        }
    }
    if (type_ == OAE_EFFECT_REVERB) {
        constexpr std::array combLengths{1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        constexpr std::array allpassLengths{556, 441, 341, 225};
        reverb_.resize(channels_);
        for (std::uint32_t c = 0; c < channels_; ++c) {
            const auto length = [&](int base) {
                return std::max<std::size_t>(1, static_cast<std::size_t>(
                    std::llround(static_cast<double>(base + (c % 2 == 0 ? 0 : 23)) * rate_ / 44100.0)));
            };
            for (std::size_t i = 0; i < combLengths.size(); ++i) {
                reverb_[c].combs[i].samples.resize(length(combLengths[i]));
            }
            for (std::size_t i = 0; i < allpassLengths.size(); ++i) {
                reverb_[c].allpasses[i].samples.resize(length(allpassLengths[i]));
            }
        }
    }
    publish();
}

bool Effect::set_parameter(int id, float value) noexcept
{
    const auto* range = effect_parameter(type_, id);
    if (!range || !(value >= range->low && value <= range->high)) {
        return false;
    }
    if (type_ == OAE_EFFECT_EQ3 &&
        ((id == OAE_EQ3_LOW_XOVER_HZ && value >= parameter(OAE_EQ3_HIGH_XOVER_HZ)) ||
         (id == OAE_EQ3_HIGH_XOVER_HZ && value <= parameter(OAE_EQ3_LOW_XOVER_HZ)))) {
        return false;
    }
    control_.params[static_cast<std::size_t>(id)] = value;
    publish();
    return true;
}

void Effect::set_bypass(bool bypass) noexcept
{
    control_.bypass = bypass;
    publish();
}

void Effect::set_mix(float wet, float dry) noexcept
{
    control_.wet = wet;
    control_.dry = dry;
    publish();
}

void Effect::set_sidechain(std::shared_ptr<SidechainTap> tap) noexcept
{
    auto previous = std::move(controlTap_);
    controlTap_ = std::move(tap);
    tap_.store(controlTap_.get(), std::memory_order_release);
    if (previous) {
        bus_->dsp().synchronize();
    }
}

void Effect::publish() noexcept
{
    seq_.fetch_add(1, std::memory_order_acq_rel);
    for (std::size_t i = 0; i < params_.size(); ++i) {
        params_[i].store(control_.params[i], std::memory_order_relaxed);
    }
    wet_.store(control_.wet, std::memory_order_relaxed);
    dry_.store(control_.dry, std::memory_order_relaxed);
    bypass_.store(control_.bypass, std::memory_order_relaxed);
    seq_.fetch_add(1, std::memory_order_release);
}

void Effect::read_settings() noexcept
{
    const auto seq = seq_.load(std::memory_order_acquire);
    if ((seq & 1u) != 0 || seq == seen_) {
        return;
    }
    Settings settings;
    for (std::size_t i = 0; i < params_.size(); ++i) {
        settings.params[i] = params_[i].load(std::memory_order_relaxed);
    }
    settings.wet = wet_.load(std::memory_order_relaxed);
    settings.dry = dry_.load(std::memory_order_relaxed);
    settings.bypass = bypass_.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (seq == seq_.load(std::memory_order_relaxed)) {
        seen_ = seq;
        audio_ = settings;
    }
}

Effect::Coefficients Effect::biquad(float frequency, float q, std::uint32_t rate, bool high) noexcept
{
    const auto angle = 2.0 * 3.14159265358979323846 *
        std::min(static_cast<double>(frequency) / rate, 0.49);
    const auto cos = std::cos(angle);
    const auto alpha = std::sin(angle) / (2.0 * q);
    const auto inverse = 1.0 / (1.0 + alpha);
    const auto b0 = (high ? 1.0 + cos : 1.0 - cos) * 0.5 * inverse;
    return {b0, (high ? -2.0 : 2.0) * b0, b0, -2.0 * cos * inverse, (1.0 - alpha) * inverse};
}

float Effect::Biquad::process(float input, const Coefficients& c) noexcept
{
    const auto result = c.b0 * input + z1;
    z1 = c.b1 * input - c.a1 * result + z2;
    z2 = c.b2 * input - c.a2 * result;
    if (std::fabs(z1) < 1e-20) {
        z1 = 0;
    }
    if (std::fabs(z2) < 1e-20) {
        z2 = 0;
    }
    return static_cast<float>(result);
}

void Effect::update_filter() noexcept
{
    bool changed = firstFilter_;
    for (int id = 0; id < counts[type_]; ++id) {
        const auto i = static_cast<std::size_t>(id);
        const auto target = audio_.params[i];
        auto& current = filtered_[i];
        const auto previous = current;
        const bool frequency = type_ == OAE_EFFECT_EQ3 ? i >= 3 : i == 0;
        if (firstFilter_ || std::fabs(current - target) <= (frequency ? target * 1e-4f : 1e-4f)) {
            current = target;
        } else if (frequency) {
            current = std::exp(std::log(current) + filterStep_ * (std::log(target) - std::log(current)));
        } else {
            current += filterStep_ * (target - current);
        }
        changed = changed || current != previous;
    }
    firstFilter_ = false;
    if (!changed) {
        return;
    }
    if (type_ == OAE_EFFECT_EQ3) {
        constexpr float butterworth = 0.70710678118f;
        coeffs_ = {biquad(filtered_[3], butterworth, rate_, false), biquad(filtered_[3], butterworth, rate_, true),
            biquad(filtered_[4], butterworth, rate_, false), biquad(filtered_[4], butterworth, rate_, true)};
        for (std::size_t i = 0; i < bandGains_.size(); ++i) {
            bandGains_[i] = filtered_[i] <= -80.0f ? 0.0f : decibels(filtered_[i]);
        }
    } else {
        coeffs_[0] = biquad(filtered_[0], filtered_[1], rate_, type_ == OAE_EFFECT_HIGHPASS);
    }
}

float Effect::cascade(std::size_t lane, std::uint32_t channel, float sample, const Coefficients& coeff) noexcept
{
    auto& stages = filterState_[lane][channel];
    return stages[1].process(stages[0].process(sample, coeff), coeff);
}

void Effect::filters(Frame& data) noexcept
{
    if (untilFilter_ == 0 || firstFilter_) {
        update_filter();
        untilFilter_ = 32;
    }
    --untilFilter_;
    for (std::uint32_t c = 0; c < channels_; ++c) {
        if (type_ != OAE_EFFECT_EQ3) {
            data[c] = filterState_[0][c][0].process(data[c], coeffs_[0]);
            continue;
        }
        const auto low = cascade(0, c, data[c], coeffs_[0]);
        const auto upper = cascade(1, c, data[c], coeffs_[1]);
        const auto lowAligned = cascade(2, c, low, coeffs_[2]) + cascade(3, c, low, coeffs_[3]);
        const auto mid = cascade(4, c, upper, coeffs_[2]);
        const auto high = cascade(5, c, upper, coeffs_[3]);
        data[c] = bandGains_[0] * lowAligned + bandGains_[1] * mid + bandGains_[2] * high;
    }
}

void Effect::ReverbLine::push(float value) noexcept
{
    samples[head] = quiet(value);
    if (++head == samples.size()) {
        head = 0;
    }
}

float Effect::ReverbLine::comb(float input, float feedback, float damping) noexcept
{
    const auto delayed = samples[head];
    memory = quiet(delayed * (1.0f - damping) + memory * damping);
    push(input + memory * feedback);
    return delayed;
}

float Effect::ReverbLine::allpass(float input) noexcept
{
    const auto delayed = samples[head];
    push(input + delayed * 0.5f);
    return delayed - input;
}

void Effect::reverb(Frame& data) noexcept
{
    const auto feedback = 0.7f + audio_.params[0] * 0.28f;
    const auto damping = audio_.params[1] * 0.4f;
    Frame wet{};
    for (std::uint32_t c = 0; c < channels_; ++c) {
        const auto other = c ^ 1u;
        const auto input = (data[c] + (other < channels_ ? data[other] : 0.0f)) * 0.015f;
        auto& channel = reverb_[c];
        auto& sum = wet[c];
        for (auto& comb : channel.combs) {
            sum += comb.comb(input, feedback, damping);
        }
        for (auto& allpass : channel.allpasses) {
            sum = allpass.allpass(sum);
        }
    }
    const auto same = (1.0f + audio_.params[2]) * 0.5f;
    for (std::uint32_t c = 0; c < channels_; ++c) {
        const auto other = c ^ 1u;
        data[c] = other < channels_ ? wet[c] * same + wet[other] * (1.0f - same) : wet[c];
    }
}

void Effect::delay(Frame& data, std::size_t distance) noexcept
{
    const auto read = (write_ + delayFrames_ - distance) % delayFrames_;
    for (std::uint32_t c = 0; c < channels_; ++c) {
        const auto old = delay_[read * channels_ + c];
        delay_[write_ * channels_ + c] = quiet(data[c] + old * audio_.params[1]);
        data[c] = old;
    }
    write_ = (write_ + 1) % delayFrames_;
}

void Effect::limiter(Frame& data, float inputGain, float ceiling, float release) noexcept
{
    float peak = 0;
    for (std::uint32_t c = 0; c < channels_; ++c) {
        data[c] *= inputGain;
        peak = std::max(peak, std::fabs(data[c]));
    }
    const auto size = peaks_.size();
    while (front_ != back_ && frame_ - peaks_[front_].frame > delayFrames_) {
        front_ = (front_ + 1) % size;
    }
    while (front_ != back_) {
        const auto previous = (back_ + size - 1) % size;
        if (peaks_[previous].level > peak) {
            break;
        }
        back_ = previous;
    }
    peaks_[back_] = {frame_++, peak};
    back_ = (back_ + 1) % size;
    const auto maximum = peaks_[front_].level;
    const auto required = maximum > ceiling ? ceiling / maximum : 1.0f;
    dynamicsGain_ = std::min(required, 1.0f + release * (dynamicsGain_ - 1.0f));
    for (std::uint32_t c = 0; c < channels_; ++c) {
        auto& value = delay_[write_ * channels_ + c];
        const auto previous = value;
        value = data[c];
        data[c] = std::clamp(previous * dynamicsGain_, -ceiling, ceiling);
    }
    write_ = (write_ + 1) % delayFrames_;
}

void Effect::compressor(Frame& data, float keyGain, float threshold, float exponent,
    float attack, float release, float makeup) noexcept
{
    auto required = keyGain;
    if (required < 0) {
        float key = 0;
        for (std::uint32_t c = 0; c < channels_; ++c) {
            key = std::max(key, measured_magnitude(data[c]));
        }
        required = key > threshold ? std::pow(threshold / key, exponent) : 1.0f;
    }
    const auto coeff = required < dynamicsGain_ ? attack : release;
    dynamicsGain_ = required + coeff * (dynamicsGain_ - required);
    for (std::uint32_t c = 0; c < channels_; ++c) {
        data[c] *= dynamicsGain_ * makeup;
    }
}

void Effect::process(float* output, std::uint32_t frames, std::uint64_t previousBlock) noexcept
{
    read_settings();
    if (audio_.bypass || (audio_.wet == 0.0f && audio_.dry == 1.0f)) {
        firstFilter_ = true;
        return;
    }

    const auto* tap = tap_.load(std::memory_order_acquire);
    const auto key = tap ? tap->peak(previousBlock) : 0.0f;
    const auto& p = audio_.params;
    const auto limiterInput = type_ == OAE_EFFECT_LIMITER ? decibels(p[2]) : 1.0f;
    const auto limit = type_ == OAE_EFFECT_LIMITER ? decibels(p[0]) : 1.0f;
    const auto threshold = type_ == OAE_EFFECT_COMPRESSOR ? decibels(p[0]) : 1.0f;
    const auto exponent = type_ == OAE_EFFECT_COMPRESSOR ? 1.0f - 1.0f / p[1] : 0.0f;
    const auto keyGain = !tap ? -1.0f : (key > threshold ? std::pow(threshold / key, exponent) : 1.0f);
    const auto makeup = type_ == OAE_EFFECT_COMPRESSOR ? decibels(p[4]) : 1.0f;
    const auto attack = type_ == OAE_EFFECT_COMPRESSOR ? smoothing(p[2], rate_) : 0.0f;
    const auto release = type_ == OAE_EFFECT_LIMITER ? smoothing(p[1], rate_) :
        (type_ == OAE_EFFECT_COMPRESSOR ? smoothing(p[3], rate_) : 0.0f);
    const auto distance = type_ == OAE_EFFECT_DELAY ? frames_for(p[0], rate_) : 0;

    for (std::uint32_t f = 0; f < frames; ++f) {
        Frame frame{};
        for (std::uint32_t c = 0; c < channels_; ++c) {
            frame[c] = safe_sample(output[c]);
        }
        switch (type_) {
            case OAE_EFFECT_LIMITER: limiter(frame, limiterInput, limit, release); break;
            case OAE_EFFECT_COMPRESSOR: compressor(frame, keyGain, threshold, exponent, attack, release, makeup); break;
            case OAE_EFFECT_REVERB: reverb(frame); break;
            case OAE_EFFECT_DELAY: delay(frame, distance); break;
            default: filters(frame); break;
        }
        for (std::uint32_t c = 0; c < channels_; ++c) {
            const auto dry = audio_.dry == 0.0f ? 0.0f : safe_sample(output[c]) * audio_.dry;
            output[c] = dry + (audio_.wet == 0.0f ? 0.0f : frame[c] * audio_.wet);
        }
        output += channels_;
    }
}

}
