#include "../src/common/spsc_queue.h"
#include <doctest.h>

#include <cstdint>
#include <thread>

TEST_CASE("SPSC queue reports empty and full without losing data")
{
    oae::SpscQueue<int, 2> queue;
    int value = -1;
    CHECK_FALSE(queue.try_pop(value));
    CHECK(value == -1);
    REQUIRE(queue.try_push(10));
    REQUIRE(queue.try_push(20));
    CHECK_FALSE(queue.try_push(30));
    REQUIRE(queue.try_pop(value));
    CHECK(value == 10);
    REQUIRE(queue.try_push(30));
    REQUIRE(queue.try_pop(value));
    CHECK(value == 20);
    REQUIRE(queue.try_pop(value));
    CHECK(value == 30);
    CHECK_FALSE(queue.try_pop(value));
}

TEST_CASE("SPSC queue preserves one million items between two threads")
{
    constexpr std::uint32_t count = 1'000'000;
    oae::SpscQueue<std::uint32_t, 1024> queue;
    bool ordered = true;
    std::thread producer([&] {
        for (std::uint32_t value = 0; value < count; ++value) {
            while (!queue.try_push(value)) {
                std::this_thread::yield();
            }
        }
    });

    for (std::uint32_t expected = 0; expected < count; ++expected) {
        std::uint32_t actual = 0;
        while (!queue.try_pop(actual)) {
            std::this_thread::yield();
        }
        ordered = ordered && actual == expected;
    }
    producer.join();
    CHECK(ordered);
}
