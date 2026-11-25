#include "HeadlessAudioProbe.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace oae::studio::testing {

namespace {

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kBlockFrames = 256;

double linear_to_db(double amp) noexcept
{
    if (amp <= 1.0e-6) {
        return -120.0;
    }
    return 20.0 * std::log10(amp);
}

double goertzel_power(const std::vector<float>& monoSamples, double targetHz) noexcept
{
    if (monoSamples.empty()) {
        return 0.0;
    }
    const double n = static_cast<double>(monoSamples.size());
    const double omega = 2.0 * kPi * targetHz / kSampleRate;
    const double coeff = 2.0 * std::cos(omega);
    double sPrev = 0.0;
    double sPrev2 = 0.0;
    for (float x : monoSamples) {
        const double s = static_cast<double>(x) + coeff * sPrev - sPrev2;
        sPrev2 = sPrev;
        sPrev = s;
    }
    const double power = (sPrev2 * sPrev2 + sPrev * sPrev - coeff * sPrev * sPrev2) / (n * n);
    return std::max(0.0, power);
}

} // namespace

ProbeMetrics HeadlessAudioProbe::capture(StudioEngine& engine,
                                         uint32_t measureFrames,
                                         uint32_t warmupFrames,
                                         const std::string& busPath)
{
    ProbeMetrics out;
    std::vector<float> block(static_cast<size_t>(kBlockFrames) * 2u, 0.0f);

    uint32_t remainingWarmup = warmupFrames;
    while (remainingWarmup > 0) {
        const uint32_t chunk = std::min(kBlockFrames, remainingWarmup);
        const oae_result upRes = engine.update();
        if (upRes != OAE_OK) {
            out.lastResult = upRes;
            return out;
        }
        std::fill(block.begin(), block.end(), 0.0f);
        const oae_result renRes = engine.render(block.data(), chunk);
        if (renRes != OAE_OK) {
            out.lastResult = renRes;
            return out;
        }
        remainingWarmup -= chunk;
    }

    std::vector<float> monoSamples;
    monoSamples.reserve(measureFrames);
    double sumSqL = 0.0;
    double sumSqR = 0.0;
    double peak = 0.0;
    uint32_t totalFrames = 0;

    uint32_t remainingMeasure = measureFrames;
    while (remainingMeasure > 0) {
        const uint32_t chunk = std::min(kBlockFrames, remainingMeasure);
        const oae_result upRes = engine.update();
        if (upRes != OAE_OK) {
            out.lastResult = upRes;
            return out;
        }
        std::fill(block.begin(), block.end(), 0.0f);
        const oae_result renRes = engine.render(block.data(), chunk);
        if (renRes != OAE_OK) {
            out.lastResult = renRes;
            return out;
        }
        for (uint32_t i = 0; i < chunk; ++i) {
            const double l = static_cast<double>(block[static_cast<size_t>(i) * 2u]);
            const double r = static_cast<double>(block[static_cast<size_t>(i) * 2u + 1u]);
            sumSqL += l * l;
            sumSqR += r * r;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            monoSamples.push_back(static_cast<float>(0.5 * (l + r)));
        }
        totalFrames += chunk;
        remainingMeasure -= chunk;
    }

    if (totalFrames > 0) {
        const double denom = static_cast<double>(totalFrames);
        out.rmsL = std::sqrt(sumSqL / denom);
        out.rmsR = std::sqrt(sumSqR / denom);
        out.rmsTotal = std::sqrt(0.5 * (out.rmsL * out.rmsL + out.rmsR * out.rmsR));
        out.rmsDb = linear_to_db(out.rmsTotal);
        out.peakDb = linear_to_db(peak);
        out.lrRatio = (out.rmsR > 1.0e-6) ? (out.rmsL / out.rmsR) : ((out.rmsL > 1.0e-6) ? 100.0 : 1.0);
        out.energy220 = goertzel_power(monoSamples, 220.0);
        out.energy440 = goertzel_power(monoSamples, 440.0);
        out.energy880 = goertzel_power(monoSamples, 880.0);
        out.energy4000 = goertzel_power(monoSamples, 4000.0);
        out.hfToLfRatio = (out.energy220 > 1.0e-12) ? (out.energy4000 / out.energy220) : 0.0;
        out.ratio880To440 = (out.energy440 > 1.0e-12) ? (out.energy880 / out.energy440) : 0.0;
    }

    oae_bus busHandle = 0;
    if (!busPath.empty() && engine.get_bus(busPath, &busHandle) == OAE_OK && busHandle != 0) {
        oae_meter meter{};
        if (engine.get_bus_meter(busHandle, &meter) == OAE_OK && meter.channel_count > 0) {
            float maxPeak = 0.0f;
            float maxRms = 0.0f;
            const uint32_t chCount = std::min<uint32_t>(meter.channel_count, OAE_METER_MAX_CHANNELS);
            for (uint32_t ch = 0; ch < chCount; ++ch) {
                maxPeak = std::max(maxPeak, meter.peak[ch]);
                maxRms = std::max(maxRms, meter.rms[ch]);
            }
            out.busMeterPeak = maxPeak;
            out.busMeterRms = maxRms;
            out.busMeterOk = true;
        }
    }

    return out;
}

} // namespace oae::studio::testing
