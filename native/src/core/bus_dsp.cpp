#include "bus_dsp.h"

#include <algorithm>
#include <cmath>

namespace oae {

float measured_magnitude(float sample) noexcept
{
    return std::isfinite(sample) ? std::min(std::fabs(sample), 1.0e9f) : 0.0f;
}

float Meter::add(const float* data, std::uint32_t frames, std::uint32_t channels) noexcept
{
    auto& window = windows_[slots_.enter()];
    float blockPeak = 0.0f;
    for (std::uint32_t f = 0; f < frames; ++f) {
        for (std::uint32_t c = 0; c < channels; ++c) {
            const auto sample = measured_magnitude(*data++);
            window.peak[c] = std::max(window.peak[c], sample);
            blockPeak = std::max(blockPeak, sample);
            window.squares[c] += static_cast<double>(sample) * sample;
        }
    }
    window.frames += frames;
    slots_.leave();
    return blockPeak;
}

void Meter::close(std::uint32_t channels) noexcept
{
    auto& previous = windows_[slots_.swap()];
    closed_ = {};
    closed_.channel_count = channels;
    closed_.frame_count = static_cast<std::uint32_t>(std::min<std::uint64_t>(UINT32_MAX, previous.frames));
    if (previous.frames != 0) {
        for (std::uint32_t c = 0; c < channels; ++c) {
            closed_.peak[c] = previous.peak[c];
            closed_.rms[c] = static_cast<float>(std::sqrt(previous.squares[c] / static_cast<double>(previous.frames)));
        }
    }
    previous = {};
}

void SidechainTap::capture(std::uint64_t block, float peak) noexcept
{
    if (blocks_[current_].frame != block) {
        current_ ^= 1;
        blocks_[current_] = {block, 0};
    }
    auto& value = blocks_[current_].peak;
    value = std::max(value, peak);
}

float SidechainTap::peak(std::uint64_t block) const noexcept
{
    for (const auto& slot : blocks_) {
        if (slot.frame == block) {
            return slot.peak;
        }
    }
    return 0.0f;
}

std::size_t BusDsp::size(int position) const noexcept
{
    return layouts_[slots_.current()].sizes[static_cast<std::size_t>(position)];
}

Effect* BusDsp::effect_at(int position, std::size_t index) noexcept
{
    if (position >= 0 && position < 2 && index < size(position)) {
        return layouts_[slots_.current()].chains[static_cast<std::size_t>(position)][index];
    }
    return nullptr;
}

bool BusDsp::can_insert(int position, int index) const noexcept
{
    const auto count = size(position);
    return count < chain_limit && (index == OAE_EFFECT_INDEX_LAST || static_cast<std::size_t>(index) <= count);
}

void BusDsp::publish(const Layout& layout) noexcept
{
    layouts_[slots_.current() ^ 1u] = layout;
    slots_.swap();
}

void BusDsp::synchronize() noexcept
{
    publish(layouts_[slots_.current()]);
}

void BusDsp::insert(Effect& effect, int position, int index) noexcept
{
    auto layout = layouts_[slots_.current()];
    const auto p = static_cast<std::size_t>(position);
    auto& chain = layout.chains[p];
    auto& size = layout.sizes[p];
    const auto at = index == OAE_EFFECT_INDEX_LAST ? size : static_cast<std::size_t>(index);
    for (auto i = size; i > at; --i) {
        chain[i] = chain[i - 1];
    }
    chain[at] = &effect;
    ++size;
    publish(layout);
}

void BusDsp::remove(const Effect& effect) noexcept
{
    auto layout = layouts_[slots_.current()];
    for (std::size_t p = 0; p < layout.chains.size(); ++p) {
        auto& chain = layout.chains[p];
        auto& size = layout.sizes[p];
        for (std::size_t i = 0; i < size; ++i) {
            if (chain[i] == &effect) {
                for (auto next = i + 1; next < size; ++next) {
                    chain[next - 1] = chain[next];
                }
                chain[--size] = nullptr;
                publish(layout);
                return;
            }
        }
    }
}

void BusDsp::clear() noexcept
{
    publish({});
}

int BusDsp::find_send(const Bus* target) const noexcept
{
    const auto& sends = layouts_[slots_.current()].sends;
    for (std::size_t i = 0; i < send_limit; ++i) {
        if (sends[i].active && sends[i].target == target) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int BusDsp::allocate_send(Bus& target, float level, int position) noexcept
{
    auto layout = layouts_[slots_.current()];
    for (std::size_t i = 0; i < send_limit; ++i) {
        if (!layout.sends[i].active) {
            sendLevels_[i].store(level, std::memory_order_relaxed);
            sendMuted_[i].store(false, std::memory_order_relaxed);
            layout.sends[i] = {&target, position, true};
            publish(layout);
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool BusDsp::remove_send(const Bus* target) noexcept
{
    auto layout = layouts_[slots_.current()];
    for (std::size_t i = 0; i < send_limit; ++i) {
        if (layout.sends[i].active && layout.sends[i].target == target) {
            layout.sends[i] = {};
            publish(layout);
            sendLevels_[i].store(0.0f, std::memory_order_relaxed);
            sendMuted_[i].store(false, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void BusDsp::clear_sends() noexcept
{
    auto layout = layouts_[slots_.current()];
    layout.sends = {};
    publish(layout);
    for (std::size_t i = 0; i < send_limit; ++i) {
        sendLevels_[i].store(0.0f, std::memory_order_relaxed);
        sendMuted_[i].store(false, std::memory_order_relaxed);
    }
}

}
