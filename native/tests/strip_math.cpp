#include "../src/common/strip_math.h"

#include <doctest.h>

#include <limits>

TEST_CASE("Stereo pan preserves unity center and exact hard-pan silence")
{
    const auto center = oae::pan_gains(0.0f);
    CHECK(center.left == 1.0f);
    CHECK(center.right == 1.0f);
    CHECK(oae::pan_gains(-1.0f).right == 0.0f);
    CHECK(oae::pan_gains(1.0f).left == 0.0f);
    const auto mixed = oae::pan_gains(0.5f);
    CHECK(mixed.left * mixed.left + mixed.right * mixed.right == doctest::Approx(2.0));
    CHECK(oae::declick_frames(48000) == 128);
    CHECK(oae::declick_frames(96000) == 256);
    CHECK(oae::declick_frames(1000) == 16);
}

TEST_CASE("Fades use exact endpoints and tolerate DSP clocks near overflow")
{
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    const oae::FadeSpec linear{1.0f, 0.0f, maximum - 8, 16, oae::FadeCurve::linear};
    CHECK(linear.gain(maximum - 4) == 0.75f);
    const oae::FadeSpec log{1.0f, 0.0f, 100, 100, oae::FadeCurve::logarithmic};
    CHECK(log.gain(100) == 1.0f);
    CHECK(log.gain(150) == doctest::Approx(0.01));
    CHECK(log.gain(200) == 0.0f);
    const oae::FadeSpec silence{0.0f, 0.0f, 100, 100, oae::FadeCurve::logarithmic};
    CHECK(silence.gain(150) == 0.0f);
    const oae::FadeSpec smooth{0.0f, 1.0f, 0, 100, oae::FadeCurve::scurve};
    CHECK(smooth.gain(25) == doctest::Approx(0.1464466));
    CHECK(smooth.gain(100) == 1.0f);
}
