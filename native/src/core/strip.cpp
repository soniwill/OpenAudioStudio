#include "strip.h"
#include "system.h"
#include "effect.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <type_traits>

namespace oae {

StripNode::StripNode(System& owner) noexcept : owner_(&owner)
{
    static_assert(std::is_standard_layout_v<StripNode>);
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    static_assert(std::atomic<int>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
}

StripNode::~StripNode()
{
    if (ready_) {
        ma_node_uninit(&base_, nullptr);
    }
}

oae_result StripNode::init(BusDsp* dsp) noexcept
{
    static constexpr ma_node_vtable voice{process, nullptr, 1, 1, MA_NODE_FLAG_PASSTHROUGH};
    static constexpr ma_node_vtable bus{process, nullptr, 1, 1 + static_cast<ma_uint8>(BusDsp::send_limit),
        MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_ALLOW_NULL_INPUT};
    dsp_ = dsp;
    channels_ = ma_engine_get_channels(owner_->engine());
    rampFrames_ = declick_frames(owner_->sample_rate());
    auto config = ma_node_config_init();
    config.vtable = dsp_ ? &bus : &voice;
    config.pInputChannels = &channels_;
    std::array<ma_uint32, 1 + BusDsp::send_limit> outChannels{};
    outChannels.fill(channels_);
    config.pOutputChannels = dsp_ ? outChannels.data() : &channels_;
    const auto result = ma_node_init(ma_engine_get_node_graph(owner_->engine()), &config, nullptr, &base_);
    ready_ = result == MA_SUCCESS;
    if (ready_) {
        return OAE_OK;
    }
    return result == MA_OUT_OF_MEMORY ? OAE_ERR_OUT_OF_MEMORY : OAE_ERR_INTERNAL;
}

void StripNode::Ramp::set(float value, std::uint32_t frames) noexcept
{
    target = value;
    remaining = value == current ? 0 : frames;
    if (remaining == 0) {
        current = target;
    }
}

float StripNode::Ramp::advance() noexcept
{
    if (remaining > 1) {
        current += (target - current) / static_cast<float>(remaining--);
    } else if (remaining == 1) {
        current = target;
        remaining = 0;
    }
    return current;
}

void StripNode::fade_to(float target, std::uint64_t duration, FadeCurve curve, std::uint64_t now) noexcept
{
    controlFade_ = {controlFade_.gain(now), target, now, duration, curve};
    fadeSequence_.fetch_add(1, std::memory_order_acq_rel);
    fadeFrom_.store(controlFade_.from, std::memory_order_relaxed);
    fadeTo_.store(target, std::memory_order_relaxed);
    fadeStart_.store(now, std::memory_order_relaxed);
    fadeDuration_.store(duration, std::memory_order_relaxed);
    fadeCurve_.store(static_cast<int>(curve), std::memory_order_relaxed);
    fadeSequence_.fetch_add(1, std::memory_order_release);
}

void StripNode::set_ahdsr(const AhdsrSpec& spec) noexcept
{
    controlAhdsr_ = spec;
    ahdsrSequence_.fetch_add(1, std::memory_order_release);
}

void StripNode::trigger_ahdsr_release(std::uint64_t rel_frame) noexcept
{
    ahdsrRelFrame_.store(rel_frame, std::memory_order_release);
}

void StripNode::read_ahdsr() noexcept
{
    const auto seq = ahdsrSequence_.load(std::memory_order_acquire);
    if (seq != audioAhdsrSequence_) {
        audioAhdsrSequence_ = seq;
        audioAhdsr_ = controlAhdsr_;
        hasAhdsr_ = (audioAhdsr_.t_start != 0 || audioAhdsr_.attack_frames != 0 ||
                     audioAhdsr_.hold_frames != 0 || audioAhdsr_.decay_frames != 0 ||
                     audioAhdsr_.release_frames != 0);
    }
    const auto rel = ahdsrRelFrame_.load(std::memory_order_acquire);
    if (rel != UINT64_MAX) {
        audioAhdsr_.t_rel = rel;
    }
}

void StripNode::read_fade() noexcept
{
    const auto begin = fadeSequence_.load(std::memory_order_acquire);
    if ((begin & 1u) != 0 || begin == audioSequence_) {
        return;
    }
    const FadeSpec snapshot{
        fadeFrom_.load(std::memory_order_relaxed),
        fadeTo_.load(std::memory_order_relaxed),
        fadeStart_.load(std::memory_order_relaxed),
        fadeDuration_.load(std::memory_order_relaxed),
        static_cast<FadeCurve>(fadeCurve_.load(std::memory_order_relaxed))
    };
    std::atomic_thread_fence(std::memory_order_acquire);
    if (begin == fadeSequence_.load(std::memory_order_relaxed)) {
        audioFade_ = snapshot;
        audioSequence_ = begin;
    }
}

void StripNode::update_ramps() noexcept
{
    const auto gain = volume() * vca_gain() * snapshot_gain();
    if (first_ || gain != audioVolume_.target) {
        audioVolume_.set(gain, first_ || !dsp_ ? 0 : rampFrames_);
    }
    const auto targetPan = pan();
    if (first_ || targetPan != audioPan_) {
        const auto gains = pan_gains(targetPan);
        left_.set(gains.left, first_ ? 0 : rampFrames_);
        right_.set(gains.right, first_ ? 0 : rampFrames_);
        audioPan_ = targetPan;
    }
    first_ = false;
}

void StripNode::process(ma_node* node, const float** input, ma_uint32* inputFrames,
    float** output, ma_uint32* outputFrames) noexcept
{
    auto& strip = *static_cast<StripNode*>(node);
    auto* samples = output[0];
    const auto count = strip.dsp_ ? *outputFrames : std::min(*inputFrames, *outputFrames);
    const auto used = std::min(*inputFrames, count);
    *inputFrames = used;
    *outputFrames = count;
    const auto block = strip.owner_->mix_frame();
    if (strip.first_ || strip.block_ != block) {
        strip.block_ = block;
        strip.frame_ = block;
    }

    strip.read_fade();
    strip.read_ahdsr();
    strip.update_ramps();
    const auto frame = strip.frame_;
    strip.frame_ += count;
    auto* dsp = strip.dsp_;
    if (!dsp) {
        strip.apply_fader(samples, count, frame);
        return;
    }

    const auto sampleCount = static_cast<std::size_t>(count) * strip.channels_;
    const auto copied = input && input[0] ? static_cast<std::size_t>(used) * strip.channels_ : 0;
    if (copied != 0 && samples != input[0]) {
        std::memmove(samples, input[0], copied * sizeof(float));
    }
    std::fill(samples + copied, samples + sampleCount, 0.0f);

    auto write_send = [sampleCount](float* dst, const float* src, float level, bool zero) noexcept {
        if (!dst) {
            return;
        }
        if (zero || level == 0.0f) {
            std::fill_n(dst, sampleCount, 0.0f);
        } else if (level == 1.0f) {
            std::memcpy(dst, src, sampleCount * sizeof(float));
        } else {
            for (std::size_t i = 0; i < sampleCount; ++i) {
                dst[i] = src[i] * level;
            }
        }
    };

    if (!dsp->paused()) {
        const auto& layout = dsp->enter();
        const auto previous = strip.owner_->previous_mix_frame();
        const auto apply = [&](std::size_t position) {
            for (std::size_t i = 0; i < layout.sizes[position]; ++i) {
                layout.chains[position][i]->process(samples, count, previous);
            }
        };
        apply(OAE_EFFECT_PRE_FADER);
        for (std::size_t k = 0; k < BusDsp::send_limit; ++k) {
            if (layout.sends[k].active && layout.sends[k].position == OAE_EFFECT_PRE_FADER) {
                write_send(output[1 + k], samples, dsp->send_level(k), dsp->send_muted(k));
            }
        }
        strip.apply_fader(samples, count, frame);
        apply(OAE_EFFECT_POST_FADER);
        if (strip.muted()) {
            std::fill_n(samples, sampleCount, 0.0f);
        }
        for (std::size_t k = 0; k < BusDsp::send_limit; ++k) {
            if (layout.sends[k].active && layout.sends[k].position == OAE_EFFECT_POST_FADER) {
                write_send(output[1 + k], samples, dsp->send_level(k), strip.muted() || dsp->send_muted(k));
            } else if (!layout.sends[k].active && output[1 + k]) {
                std::fill_n(output[1 + k], sampleCount, 0.0f);
            }
        }
        dsp->leave();
    } else {
        std::fill_n(samples, sampleCount, 0.0f);
        for (std::size_t k = 0; k < BusDsp::send_limit; ++k) {
            if (output[1 + k]) {
                std::fill_n(output[1 + k], sampleCount, 0.0f);
            }
        }
    }
    const auto peak = dsp->meter.add(samples, count, strip.channels_);
    dsp->tap->capture(block, peak);
}

void StripNode::apply_fader(float* samples, ma_uint32 count, std::uint64_t frame) noexcept
{
    const auto stop = stopFrame_.load(std::memory_order_acquire);
    const auto fade = audioFade_;
    if (!hasAhdsr_ && frame + count <= stop && audioVolume_.remaining == 0 && audioVolume_.current == 1.0f
        && (channels_ < 2 || (left_.remaining == 0 && right_.remaining == 0
            && left_.current == 1.0f && right_.current == 1.0f))
        && fade.gain(frame) == 1.0f && fade.gain(frame + count) == 1.0f) {
        return;
    }

    for (ma_uint32 i = 0; i < count; ++i) {
        const auto volume = audioVolume_.advance();
        const auto left = left_.advance();
        const auto right = right_.advance();
        const float ahdsrGain = hasAhdsr_ ? ahdsr_eval(audioAhdsr_, frame + i) : 1.0f;
        const auto gain = (frame + i >= stop ? 0.0f : volume * fade.gain(frame + i)) * ahdsrGain;
        for (ma_uint32 channel = 0; channel < channels_; ++channel) {
            const auto panGain = channels_ < 2 || channel > 1 ? 1.0f : (channel == 0 ? left : right);
            if (gain == 0.0f || panGain == 0.0f) {
                samples[channel] = 0.0f;
            } else {
                const auto scale = gain * panGain;
                if (scale != 1.0f) {
                    samples[channel] *= scale;
                }
            }
        }
        samples += channels_;
    }
}

}
