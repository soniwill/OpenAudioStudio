#include "heap_guard.h"
#include "../src/common/handle_pool.h"

#include <oae_audio.h>
#include <doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string_view>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

class ScopedSystem {
public:
    ~ScopedSystem()
    {
        if (handle) {
            static_cast<void>(oae_system_release(handle));
        }
    }

    oae_system handle = 0;
};

}

TEST_CASE("System API reports defaults and rejects invalid inputs before invalid handles")
{
    CHECK(oae_get_version() == 1);
    CHECK(std::string_view(oae_result_string(OAE_OK)) == "OK");
    CHECK(oae_init_settings_default(nullptr) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_create(nullptr) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_init(0, nullptr) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_render(0, nullptr, 1) == OAE_ERR_INVALID_PARAM);

    oae_init_settings settings{};
    REQUIRE(oae_init_settings_default(&settings) == OAE_OK);
    CHECK(settings.sample_rate == 48000);
    CHECK(settings.channels == 2);
    CHECK(settings.max_physical_voices == 32);
    CHECK(settings.flags == OAE_INIT_DEFAULT);
    CHECK(oae_system_init(0, &settings) == OAE_ERR_INVALID_HANDLE);
    settings.channels = 0;
    CHECK(oae_system_init(0, &settings) == OAE_ERR_INVALID_PARAM);
    settings.channels = 2;
    settings.flags = 2;
    CHECK(oae_system_init(0, &settings) == OAE_ERR_INVALID_PARAM);
    CHECK(oae_system_update(0) == OAE_ERR_INVALID_HANDLE);
}

TEST_CASE("Headless system renders exact silence without C++ heap operations")
{
    ScopedSystem system;
    REQUIRE(oae_system_create(&system.handle) == OAE_OK);

    std::array<float, 512> samples{};
    samples.fill(123.0f);
    CHECK(oae_system_update(system.handle) == OAE_ERR_NOT_INITIALIZED);
    CHECK(oae_system_render(system.handle, samples.data(), 256) == OAE_ERR_NOT_INITIALIZED);

    oae_init_settings settings{};
    REQUIRE(oae_init_settings_default(&settings) == OAE_OK);
    settings.flags = OAE_INIT_NO_DEVICE;
    REQUIRE(oae_system_init(system.handle, &settings) == OAE_OK);
    CHECK(oae_system_init(system.handle, &settings) == OAE_ERR_ALREADY_INITIALIZED);
    CHECK(oae_system_update(system.handle) == OAE_OK);

    oae_result result = OAE_ERR_INTERNAL;
    std::size_t heapOperations = 0;
    {
        oae::test::HeapGuard guard;
        result = oae_system_render(system.handle, samples.data(), 256);
        heapOperations = guard.operations();
    }
    CHECK(result == OAE_OK);
    CHECK(heapOperations == 0);
    CHECK(samples == std::array<float, 512>{});
}

TEST_CASE("System can complete one hundred headless lifecycles and invalidate released handles")
{
    oae_init_settings settings{};
    REQUIRE(oae_init_settings_default(&settings) == OAE_OK);
    settings.flags = OAE_INIT_NO_DEVICE;

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState before{};
    _CrtMemCheckpoint(&before);
#endif
    oae_system previous = 0;
    for (int cycle = 0; cycle < 100; ++cycle) {
        ScopedSystem system;
        REQUIRE(oae_system_create(&system.handle) == OAE_OK);
        REQUIRE(oae_system_init(system.handle, &settings) == OAE_OK);
        CHECK(oae_system_update(previous) == OAE_ERR_INVALID_HANDLE);
        previous = system.handle;
        REQUIRE(oae_system_release(system.handle) == OAE_OK);
        system.handle = 0;
        CHECK(oae_system_release(previous) == OAE_ERR_INVALID_HANDLE);
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState after{};
    _CrtMemCheckpoint(&after);
    CHECK(after.lCounts[_NORMAL_BLOCK] == before.lCounts[_NORMAL_BLOCK]);
    CHECK(after.lSizes[_NORMAL_BLOCK] == before.lSizes[_NORMAL_BLOCK]);
#endif
}

TEST_CASE("Handle pool rejects exhausted, stale, zero and wrong-type handles")
{
    oae::HandlePool<int, 7, 1> pool;
    const auto first = pool.insert(std::make_unique<int>(10));
    REQUIRE(first != 0);
    REQUIRE(pool.get(first));
    CHECK(*pool.get(first) == 10);
    CHECK(pool.insert(std::make_unique<int>(20)) == 0);
    CHECK(pool.get(0) == nullptr);
    CHECK(pool.get(first ^ (std::uint64_t{1} << 56)) == nullptr);
    REQUIRE(pool.erase(first));

    const auto second = pool.insert(std::make_unique<int>(30));
    REQUIRE(second != 0);
    CHECK(second != first);
    CHECK(pool.get(first) == nullptr);
    REQUIRE(pool.get(second));
    CHECK(*pool.get(second) == 30);
    CHECK(oae::next_generation(oae::generation_max) == 1);
}
