#pragma once

#include <oae_audio.h>
#include <array>
#include <cstdint>
#include <vector>

namespace oae::studio {

/** Acoustic meter scale presets supported by the Mixer Deck. */
enum class MeterScale {
    DbfsNominal = 0,
    K12         = 1,
    K14         = 2,
    K20         = 3,
    EbuR128Lufs = 4
};

/** Returns human-readable label for a MeterScale. */
const char* meter_scale_name(MeterScale scale) noexcept;

/** Returns the 0-reference offset in dB for a given scale (e.g. +14.0f for K-14). */
float meter_scale_offset_db(MeterScale scale) noexcept;

/** Returns display [min, max] range in the target scale's units. */
void meter_scale_range(MeterScale scale, float& out_min, float& out_max) noexcept;

/** Converts linear amplitude to dBFS, clamping floor at -80.0 dBFS. */
float linear_to_dbfs(float linear) noexcept;

/** Converts a dBFS value into the target MeterScale display unit. */
float dbfs_to_scale(float dbfs, MeterScale scale) noexcept;

/**
 * IEC 60268-17 ballistic meter state for a single strip.
 * - Instantaneous attack
 * - 20 dB/s release decay
 * - 300 ms RMS integration window
 * - 1.2 s Peak Hold before 20 dB/s decay
 */
class BallisticMeter {
public:
    static constexpr float kFloorDb = -80.0f;
    static constexpr float kDecayRateDbPerSec = 20.0f;
    static constexpr float kRmsIntegrationSec = 0.300f;
    static constexpr float kPeakHoldDurationSec = 1.2f;

    BallisticMeter() = default;

    void update(float peak_linear, float rms_linear, float dt_seconds) noexcept;
    void reset() noexcept;

    float peak_dbfs() const noexcept { return peak_dbfs_; }
    float rms_dbfs() const noexcept { return rms_dbfs_; }
    float peak_hold_dbfs() const noexcept { return peak_hold_dbfs_; }

private:
    float peak_dbfs_{ kFloorDb };
    float rms_dbfs_{ kFloorDb };
    float rms_energy_{ 0.0f };
    float peak_hold_dbfs_{ kFloorDb };
    float hold_timer_sec_{ 0.0f };
};

/** Snapshot of ITU-R BS.1770-4 / EBU R128 loudness and True Peak measurements. */
struct LoudnessSnapshot {
    float momentary_lufs{ -80.0f };
    float short_term_lufs{ -80.0f };
    float integrated_lufs{ -80.0f };
    float true_peak_dbtp{ -80.0f };
};

/**
 * Real-time-safe ITU-R BS.1770-4 / EBU R128 loudness & 4x True Peak processor.
 * Uses K-weighting biquad cascade, 100 ms overlapping 400 ms gating blocks,
 * 3 s short-term window, dual-gate integrated loudness (-70 LUFS / -10 LU),
 * and 4x polyphase FIR oversampling for True Peak (dBTP).
 */
class EbuR128Processor {
public:
    static constexpr uint32_t kMaxChannels = OAE_METER_MAX_CHANNELS;
    static constexpr size_t kFirTapsPerPhase = 12;
    static constexpr size_t kOversampleFactor = 4;

    EbuR128Processor() = default;

    void prepare(uint32_t sample_rate, uint32_t channels);
    void reset() noexcept;

    /** Processes interleaved audio frames without heap allocations. */
    void process(const float* interleaved_samples, uint32_t frames) noexcept;

    LoudnessSnapshot snapshot() const noexcept;

private:
    struct BiquadState {
        double b0{ 1.0 }, b1{ 0.0 }, b2{ 0.0 };
        double a1{ 0.0 }, a2{ 0.0 };
        double z1{ 0.0 }, z2{ 0.0 };

        double process(double x) noexcept;
        void reset() noexcept { z1 = 0.0; z2 = 0.0; }
    };

    struct ChannelFilter {
        BiquadState pre_shelf;
        BiquadState rlb_highpass;
        std::array<float, kFirTapsPerPhase> fir_history{};
        size_t fir_pos{ 0 };
    };

    void configure_k_weighting(uint32_t sample_rate) noexcept;
    void commit_100ms_block() noexcept;
    float compute_integrated_lufs() const noexcept;

    uint32_t sample_rate_{ 48000 };
    uint32_t channels_{ 2 };
    uint32_t block_size_frames_{ 4800 };
    uint32_t block_frame_pos_{ 0 };

    std::array<ChannelFilter, kMaxChannels> filters_{};
    std::array<double, kMaxChannels> current_block_energy_{};

    // Ring buffer of last 30 100-ms sub-blocks (3.0 seconds)
    std::array<double, 30> recent_subblocks_{};
    size_t recent_subblock_count_{ 0 };
    size_t recent_subblock_head_{ 0 };

    // Pre-allocated storage for 400-ms overlapping gating block energies (up to 1 hour = 36000 blocks)
    static constexpr size_t kMaxGatingBlocks = 36000;
    std::vector<double> gating_block_energies_;
    size_t gating_block_count_{ 0 };
    size_t gating_block_head_{ 0 };

    float max_true_peak_linear_{ 0.0f };
    float momentary_lufs_{ -80.0f };
    float short_term_lufs_{ -80.0f };
    float integrated_lufs_{ -80.0f };
};

} // namespace oae::studio
