#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace oae {

inline constexpr std::uint32_t generation_max = 0x00FFFFFFu;

constexpr std::uint32_t next_generation(std::uint32_t generation) noexcept
{
    return generation == generation_max ? 1u : generation + 1u;
}

template <typename T, std::uint8_t TypeTag, std::size_t Capacity>
class HandlePool {
    static_assert(Capacity > 0 && Capacity <= UINT32_MAX);
    static_assert(TypeTag != 0);
    static_assert(std::atomic<T*>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    struct Slot {
        std::atomic<T*> value{nullptr};
        std::atomic<std::uint32_t> generation{1};
    };

public:
    ~HandlePool()
    {
        for (const auto& slot : slots_) {
            delete slot.value.load(std::memory_order_relaxed);
        }
    }

    std::uint64_t insert(std::unique_ptr<T> value) noexcept
    {
        if (!value) {
            return 0;
        }
        for (std::size_t index = 0; index < Capacity; ++index) {
            auto& slot = slots_[index];
            if (slot.value.load(std::memory_order_relaxed) == nullptr) {
                const auto generation = slot.generation.load(std::memory_order_relaxed);
                slot.value.store(value.release(), std::memory_order_release);
                return encode(index, generation);
            }
        }
        return 0;
    }

    T* get(std::uint64_t handle) const noexcept
    {
        const auto index = static_cast<std::uint32_t>(handle);
        if ((handle >> 56) != TypeTag || index >= Capacity) {
            return nullptr;
        }
        const auto& slot = slots_[index];
        const auto value = slot.value.load(std::memory_order_acquire);
        const auto generation = static_cast<std::uint32_t>(handle >> 32) & generation_max;
        return generation == slot.generation.load(std::memory_order_relaxed) ? value : nullptr;
    }

    bool erase(std::uint64_t handle) noexcept
    {
        if (!get(handle)) {
            return false;
        }
        remove(slots_[static_cast<std::uint32_t>(handle)]);
        return true;
    }

    template <typename Predicate>
    void erase_if(Predicate predicate) noexcept
    {
        for (auto& slot : slots_) {
            const auto* value = slot.value.load(std::memory_order_relaxed);
            if (value && predicate(*value)) {
                remove(slot);
            }
        }
    }

    template <typename Predicate>
    std::uint64_t find_if(Predicate predicate) const noexcept
    {
        for (std::size_t index = 0; index < Capacity; ++index) {
            const auto& slot = slots_[index];
            const auto* value = slot.value.load(std::memory_order_relaxed);
            if (value && predicate(*value)) {
                return encode(index, slot.generation.load(std::memory_order_relaxed));
            }
        }
        return 0;
    }

    template <typename Visitor>
    void for_each(Visitor visitor) noexcept
    {
        for (auto& slot : slots_) {
            if (auto* value = slot.value.load(std::memory_order_relaxed)) {
                visitor(*value);
            }
        }
    }

private:
    static std::uint64_t encode(std::size_t index, std::uint32_t generation) noexcept
    {
        return (std::uint64_t{TypeTag} << 56) | (std::uint64_t{generation} << 32) | index;
    }

    static void remove(Slot& slot) noexcept
    {
        const std::unique_ptr<T> value(slot.value.exchange(nullptr, std::memory_order_acq_rel));
        const auto generation = slot.generation.load(std::memory_order_relaxed);
        slot.generation.store(next_generation(generation), std::memory_order_relaxed);
    }

    std::array<Slot, Capacity> slots_{};
};

}
