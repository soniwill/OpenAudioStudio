#pragma once
#include <oae_audio.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace oae {

class Effect;
class Bus;

class AudioSlots {
public:
    unsigned enter() noexcept { return state_.fetch_or(2u, std::memory_order_acq_rel) & 1u; }
    void leave() noexcept { state_.fetch_and(1u, std::memory_order_release); }
    unsigned current() const noexcept { return state_.load(std::memory_order_acquire) & 1u; }

    unsigned swap() noexcept
    {
        auto expected = current();
        for (;;) {
            if ((expected & 2u) == 0 && state_.compare_exchange_weak(expected, expected ^ 1u,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return expected;
            }
            std::this_thread::yield();
            expected = state_.load(std::memory_order_acquire);
        }
    }

private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::atomic<unsigned> state_{0};
};

float measured_magnitude(float sample) noexcept;

class Meter {
public:
    float add(const float* data, std::uint32_t frames, std::uint32_t channels) noexcept;
    void close(std::uint32_t channels) noexcept;
    const oae_meter& snapshot() const noexcept { return closed_; }

private:
    struct Window {
        std::uint64_t frames = 0;
        std::array<float, OAE_METER_MAX_CHANNELS> peak{};
        std::array<double, OAE_METER_MAX_CHANNELS> squares{};
    };

    AudioSlots slots_;
    std::array<Window, 2> windows_{};
    oae_meter closed_{};
};

class SidechainTap {
public:
    void capture(std::uint64_t block, float peak) noexcept;
    float peak(std::uint64_t block) const noexcept;

private:
    struct Block {
        std::uint64_t frame = UINT64_MAX;
        float peak = 0;
    };
    std::array<Block, 2> blocks_{};
    unsigned current_ = 0;
};

class BusDsp {
public:
    static constexpr std::size_t chain_limit = 8;
    static constexpr std::size_t send_limit = 8;
    using Chain = std::array<Effect*, chain_limit>;
    struct SendSlot {
        Bus* target = nullptr;
        int position = OAE_EFFECT_POST_FADER;
        bool active = false;
    };
    struct Layout {
        std::array<Chain, 2> chains{};
        std::array<std::size_t, 2> sizes{};
        std::array<SendSlot, send_limit> sends{};
    };

    bool can_insert(int position, int index) const noexcept;
    void insert(Effect& effect, int position, int index) noexcept;
    void remove(const Effect& effect) noexcept;
    void clear() noexcept;
    std::size_t size(int position) const noexcept;
    class Effect* effect_at(int position, std::size_t index) noexcept;

    int find_send(const Bus* target) const noexcept;
    int allocate_send(Bus& target, float level, int position) noexcept;
    bool remove_send(const Bus* target) noexcept;
    void clear_sends() noexcept;
    const SendSlot& send_slot(std::size_t slot) const noexcept { return layouts_[slots_.current()].sends[slot]; }
    void set_send_level(std::size_t slot, float level) noexcept { sendLevels_[slot].store(level, std::memory_order_relaxed); }
    float send_level(std::size_t slot) const noexcept { return sendLevels_[slot].load(std::memory_order_relaxed); }
    void set_send_muted(std::size_t slot, bool muted) noexcept { sendMuted_[slot].store(muted, std::memory_order_relaxed); }
    bool send_muted(std::size_t slot) const noexcept { return sendMuted_[slot].load(std::memory_order_relaxed); }

    const Layout& enter() noexcept { return layouts_[slots_.enter()]; }
    void leave() noexcept { slots_.leave(); }
    void synchronize() noexcept;
    void set_paused(bool paused) noexcept { paused_.store(paused, std::memory_order_relaxed); }
    bool paused() const noexcept { return paused_.load(std::memory_order_relaxed); }

    Meter meter;
    std::shared_ptr<SidechainTap> tap = std::make_shared<SidechainTap>();

private:
    void publish(const Layout& layout) noexcept;
    AudioSlots slots_;
    std::array<Layout, 2> layouts_{};
    std::array<std::atomic<float>, send_limit> sendLevels_{};
    std::array<std::atomic<bool>, send_limit> sendMuted_{};
    std::atomic<bool> paused_{false};
};

}
