#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace oae {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 0 && Capacity < (std::numeric_limits<std::size_t>::max)());
    static_assert(std::is_trivially_copyable_v<T> && std::is_nothrow_copy_assignable_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);

public:
    bool try_push(const T& value) noexcept
    {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = advance(write);
        if (next == read_.load(std::memory_order_acquire)) {
            return false;
        }
        values_[write] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& value) noexcept
    {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) {
            return false;
        }
        value = values_[read];
        read_.store(advance(read), std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t advance(std::size_t index) noexcept
    {
        return index == Capacity ? 0 : index + 1;
    }

    std::array<T, Capacity + 1> values_{};
    std::atomic<std::size_t> read_{0};
    std::atomic<std::size_t> write_{0};
};

}
