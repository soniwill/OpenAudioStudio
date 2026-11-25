#include "playback_fixture.h"

#include "../src/core/studio_model.h"

#include <cmath>
#include <vector>

// Regression for the fixed-seeded global PlaylistState: every play created a
// new EventDescription whose default-constructed PlaylistState started from
// Pcg32(0x12345678, 0x9abcdef0), making the first pick of every play
// identical. Independent default-constructed states must now draw varied
// picks that converge to the configured weight distribution.
TEST_CASE("Independent plays draw varied playlist picks converging to weights")
{
    std::vector<oae::PlaylistEntry> entries(2);
    entries[0].weight = 0.8f;
    entries[1].weight = 0.2f;

    const int plays = 400;
    int count0 = 0;
    int count1 = 0;
    for (int i = 0; i < plays; ++i) {
        // Default-constructed state, exactly as each independent play does
        // when the runtime creates a new EventDescription.
        oae::PlaylistState state;
        const auto pick = state.pick(entries, oae::PlaylistMode::Random);
        if (pick == 0) {
            ++count0;
        } else if (pick == 1) {
            ++count1;
        }
    }

    // Both entries must be selected at least once across plays.
    REQUIRE(count0 + count1 == plays);
    CHECK(count0 > 0);
    CHECK(count1 > 0);

    // Convergence to the 80/20 weights within 3 binomial standard deviations
    // (sigma = sqrt(N * p * (1 - p)) with p = 0.8, N = 400 -> sigma = 8).
    const double p = 0.8;
    const double expected = plays * p;
    const double sigma = std::sqrt(static_cast<double>(plays) * p * (1.0 - p));
    CHECK(count0 >= expected - 3.0 * sigma);
    CHECK(count0 <= expected + 3.0 * sigma);
}

// A single instance must remain deterministic across its own picks so modes
// such as RandomNoRepeat and tests that pin an explicit seed stay stable.
TEST_CASE("Seeded playlist state stays deterministic within one instance")
{
    std::vector<oae::PlaylistEntry> entries(2);
    entries[0].weight = 0.5f;
    entries[1].weight = 0.5f;

    std::vector<std::size_t> first_run;
    std::vector<std::size_t> second_run;
    for (int rep = 0; rep < 2; ++rep) {
        oae::PlaylistState state(oae::Pcg32(0x12345678ull, 0x9abcdef0ull));
        std::vector<std::size_t> picks;
        for (int i = 0; i < 32; ++i) {
            picks.push_back(state.pick(entries, oae::PlaylistMode::Random));
        }
        if (rep == 0) first_run = picks; else second_run = picks;
    }
    CHECK(first_run == second_run);
}
