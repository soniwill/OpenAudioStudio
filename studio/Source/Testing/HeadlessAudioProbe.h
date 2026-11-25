#pragma once

#include "../StudioEngine.h"
#include <cstdint>
#include <string>

namespace oae::studio::testing {

/** Measured acoustic and meter telemetry from headless PCM rendering. */
struct ProbeMetrics {
    oae_result lastResult{ OAE_OK };
    double rmsL{ 0.0 };
    double rmsR{ 0.0 };
    double rmsTotal{ 0.0 };
    double rmsDb{ -120.0 };
    double peakDb{ -120.0 };
    double lrRatio{ 1.0 };
    double energy220{ 0.0 };
    double energy440{ 0.0 };
    double energy880{ 0.0 };
    double energy4000{ 0.0 };
    double hfToLfRatio{ 0.0 };
    double ratio880To440{ 0.0 };
    float busMeterPeak{ 0.0f };
    float busMeterRms{ 0.0f };
    bool busMeterOk{ false };
};

/** Captures deterministic stereo PCM buffers from StudioEngine in headless mode and computes RMS/Goertzel metrics. */
class HeadlessAudioProbe {
public:
    /** Advances engine updates, renders warmup + measurement frames at 48 kHz, and reads bus meter telemetry. */
    static ProbeMetrics capture(StudioEngine& engine,
                                uint32_t measureFrames = 4096,
                                uint32_t warmupFrames = 1024,
                                const std::string& busPath = "bus:/");
};

} // namespace oae::studio::testing
