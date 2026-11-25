#pragma once

#include "../common/ahdsr.h"
#include "../common/strip_math.h"

#include <oae_audio.h>
#include <miniaudio.h>

#include <atomic>

namespace oae {

class System;
class BusDsp;

class StripNode {
public:
    explicit StripNode(System& owner) noexcept;
    ~StripNode();
    StripNode(const StripNode&) = delete;
    StripNode& operator=(const StripNode&) = delete;

    oae_result init(BusDsp* dsp = nullptr) noexcept;
    ma_node* node() noexcept { return &base_; }
    void set_volume(float level) noexcept { volume_.store(level, std::memory_order_relaxed); }
    float volume() const noexcept { return volume_.load(std::memory_order_relaxed); }
    void set_vca_gain(float gain) noexcept { vcaGain_.store(gain, std::memory_order_relaxed); }
    float vca_gain() const noexcept { return vcaGain_.load(std::memory_order_relaxed); }
    void set_snapshot_gain(float gain) noexcept { snapshotGain_.store(gain, std::memory_order_relaxed); }
    float snapshot_gain() const noexcept { return snapshotGain_.load(std::memory_order_relaxed); }
    void set_pan(float pan) noexcept { pan_.store(pan, std::memory_order_relaxed); }
    float pan() const noexcept { return pan_.load(std::memory_order_relaxed); }
    void set_muted(bool muted) noexcept { mute_.store(muted, std::memory_order_relaxed); }
    bool muted() const noexcept { return mute_.load(std::memory_order_relaxed); }
    void stop_at(std::uint64_t frame) noexcept { stopFrame_.store(frame, std::memory_order_release); }
    void fade_to(float target, std::uint64_t duration, FadeCurve curve, std::uint64_t now) noexcept;
    float fade_gain(std::uint64_t now) const noexcept { return controlFade_.gain(now); }

    void set_ahdsr(const AhdsrSpec& spec) noexcept;
    void trigger_ahdsr_release(std::uint64_t rel_frame) noexcept;

private:
    struct Ramp {
        float current = 1.0f;
        float target = 1.0f;
        std::uint32_t remaining = 0;
        void set(float value, std::uint32_t frames) noexcept;
        float advance() noexcept;
    };

    static void process(ma_node*, const float**, ma_uint32*, float**, ma_uint32*) noexcept;
    void read_fade() noexcept;
    void read_ahdsr() noexcept;
    void update_ramps() noexcept;
    void apply_fader(float* samples, ma_uint32 count, std::uint64_t frame) noexcept;

    ma_node_base base_{};
    System* owner_;
    ma_uint32 channels_ = 0;
    std::uint32_t rampFrames_ = 0;
    bool ready_ = false;
    BusDsp* dsp_ = nullptr;

    std::atomic<float> volume_{1.0f};
    std::atomic<float> vcaGain_{1.0f};
    std::atomic<float> snapshotGain_{1.0f};
    std::atomic<float> pan_{0.0f};
    std::atomic<bool> mute_{false};
    std::atomic<std::uint64_t> stopFrame_{UINT64_MAX};
    FadeSpec controlFade_{};
    std::atomic<std::uint32_t> fadeSequence_{0};
    std::atomic<float> fadeFrom_{1.0f};
    std::atomic<float> fadeTo_{1.0f};
    std::atomic<std::uint64_t> fadeStart_{0};
    std::atomic<std::uint64_t> fadeDuration_{0};
    std::atomic<int> fadeCurve_{0};

    AhdsrSpec controlAhdsr_{};
    std::atomic<std::uint32_t> ahdsrSequence_{0};
    std::atomic<std::uint64_t> ahdsrRelFrame_{UINT64_MAX};

    FadeSpec audioFade_{};
    std::uint32_t audioSequence_ = 0;
    AhdsrSpec audioAhdsr_{};
    std::uint32_t audioAhdsrSequence_ = 0;
    bool hasAhdsr_ = false;
    Ramp audioVolume_;
    Ramp left_;
    Ramp right_;
    float audioPan_ = 0.0f;
    bool first_ = true;
    std::uint64_t block_ = 0;
    std::uint64_t frame_ = 0;
};

}
