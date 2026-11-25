#include "AcousticMeterProcessor.h"
#include <algorithm>
#include <cmath>

namespace oae::studio {

namespace {

constexpr double kPi = 3.14159265358979323846;

// ITU-R BS.1770-4 Annex 2 4x oversampling polyphase FIR filter (4 phases x 12 taps).
// Each phase sums to 1.0 for unity DC gain and accurate inter-sample peak reconstruction.
constexpr std::array<std::array<float, EbuR128Processor::kFirTapsPerPhase>, 4> kTruePeakFir = {{
    // Phase 0 (integer sample alignment at tap index 5)
    {{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    // Phase 1 (+0.25 sample)
    {{ 0.001708984f, -0.009765625f, 0.029296875f, -0.072265625f, 0.181640625f, 0.903320313f,
       -0.111328125f, 0.049804688f, -0.021484375f, 0.007812500f, -0.001953125f, 0.000244141f }},
    // Phase 2 (+0.50 sample)
    {{ -0.002929688f, 0.014648438f, -0.041015625f, 0.097656250f, -0.228515625f, 0.660156250f,
       0.660156250f, -0.228515625f, 0.097656250f, -0.041015625f, 0.014648438f, -0.002929688f }},
    // Phase 3 (+0.75 sample)
    {{ 0.000244141f, -0.001953125f, 0.007812500f, -0.021484375f, 0.049804688f, -0.111328125f,
       0.903320313f, 0.181640625f, -0.072265625f, 0.029296875f, -0.009765625f, 0.001708984f }}
}};

inline float channel_weight(uint32_t channel, uint32_t total_channels) noexcept
{
    if (total_channels >= 5 && (channel == 3)) {
        return 0.0f; // LFE excluded in ITU-R BS.1770-4
    }
    if (total_channels >= 5 && (channel == 4 || channel == 5)) {
        return 1.4125375446227544f; // +1.5 dB for surround channels
    }
    return 1.0f;
}

inline float energy_to_lufs(double mean_square) noexcept
{
    if (mean_square <= 1.0e-12) {
        return -80.0f;
    }
    return static_cast<float>(-0.691 + 10.0 * std::log10(mean_square));
}

} // namespace

const char* meter_scale_name(MeterScale scale) noexcept
{
    switch (scale) {
        case MeterScale::DbfsNominal: return "dBFS Nominal";
        case MeterScale::K12:         return "K-12";
        case MeterScale::K14:         return "K-14";
        case MeterScale::K20:         return "K-20";
        case MeterScale::EbuR128Lufs: return "EBU R128 LUFS";
    }
    return "dBFS Nominal";
}

float meter_scale_offset_db(MeterScale scale) noexcept
{
    switch (scale) {
        case MeterScale::DbfsNominal: return 0.0f;
        case MeterScale::K12:         return 12.0f;
        case MeterScale::K14:         return 14.0f;
        case MeterScale::K20:         return 20.0f;
        case MeterScale::EbuR128Lufs: return 23.0f; // 0 LU = -23 LUFS
    }
    return 0.0f;
}

void meter_scale_range(MeterScale scale, float& out_min, float& out_max) noexcept
{
    switch (scale) {
        case MeterScale::DbfsNominal:
            out_min = -60.0f;
            out_max = 0.0f;
            break;
        case MeterScale::K12:
            out_min = -48.0f;
            out_max = 12.0f;
            break;
        case MeterScale::K14:
            out_min = -46.0f;
            out_max = 14.0f;
            break;
        case MeterScale::K20:
            out_min = -40.0f;
            out_max = 20.0f;
            break;
        case MeterScale::EbuR128Lufs:
            out_min = -59.0f;
            out_max = -5.0f;
            break;
    }
}

float linear_to_dbfs(float linear) noexcept
{
    if (!std::isfinite(linear) || linear <= 0.0001f) {
        return -80.0f;
    }
    return std::max(-80.0f, 20.0f * std::log10(linear));
}

float dbfs_to_scale(float dbfs, MeterScale scale) noexcept
{
    if (scale == MeterScale::EbuR128Lufs) {
        return dbfs; // LUFS values are already in absolute LUFS
    }
    return dbfs + meter_scale_offset_db(scale);
}

// ---------------- BallisticMeter ----------------

void BallisticMeter::update(float peak_linear, float rms_linear, float dt_seconds) noexcept
{
    const float dt = std::clamp(dt_seconds, 0.001f, 0.5f);
    const float decay_db = kDecayRateDbPerSec * dt;

    // Peak: instantaneous attack, 20 dB/s decay
    const float new_peak_db = linear_to_dbfs(peak_linear);
    if (new_peak_db >= peak_dbfs_) {
        peak_dbfs_ = new_peak_db;
    } else {
        peak_dbfs_ = std::max(kFloorDb, peak_dbfs_ - decay_db);
    }

    // RMS: 300 ms integration time constant on squared energy, with 20 dB/s release cap
    const float clamped_rms = (std::isfinite(rms_linear) && rms_linear > 0.0f) ? rms_linear : 0.0f;
    const float target_energy = clamped_rms * clamped_rms;
    const float alpha = 1.0f - std::exp(-dt / kRmsIntegrationSec);
    rms_energy_ += alpha * (target_energy - rms_energy_);
    const float integrated_rms_db = linear_to_dbfs(std::sqrt(std::max(0.0f, rms_energy_)));
    if (integrated_rms_db >= rms_dbfs_) {
        rms_dbfs_ = integrated_rms_db;
    } else {
        rms_dbfs_ = std::max(integrated_rms_db, rms_dbfs_ - decay_db);
    }

    // Peak Hold: 1.2 s hold duration, then 20 dB/s decay
    if (new_peak_db >= peak_hold_dbfs_) {
        peak_hold_dbfs_ = new_peak_db;
        hold_timer_sec_ = kPeakHoldDurationSec;
    } else if (hold_timer_sec_ > 0.0f) {
        hold_timer_sec_ = std::max(0.0f, hold_timer_sec_ - dt);
    } else {
        peak_hold_dbfs_ = std::max(kFloorDb, peak_hold_dbfs_ - decay_db);
    }
}

void BallisticMeter::reset() noexcept
{
    peak_dbfs_ = kFloorDb;
    rms_dbfs_ = kFloorDb;
    rms_energy_ = 0.0f;
    peak_hold_dbfs_ = kFloorDb;
    hold_timer_sec_ = 0.0f;
}

// ---------------- EbuR128Processor ----------------

double EbuR128Processor::BiquadState::process(double x) noexcept
{
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

void EbuR128Processor::configure_k_weighting(uint32_t sample_rate) noexcept
{
    const double fs = static_cast<double>(std::max<uint32_t>(8000, sample_rate));

    // Stage 1: High-shelf (+4 dB @ 1681.974 Hz, Q = 0.707175)
    double s1_b0, s1_b1, s1_b2, s1_a1, s1_a2;
    if (sample_rate == 48000) {
        s1_b0 = 1.53512485958697;
        s1_b1 = -2.69169618940638;
        s1_b2 = 1.19839281085285;
        s1_a1 = -1.69065929318241;
        s1_a2 = 0.73248077421585;
    } else {
        const double db = 3.999843853973347;
        const double f0 = 1681.974450955533;
        const double Q = 0.7071752369554196;
        const double K = std::tan(kPi * f0 / fs);
        const double Vh = std::pow(10.0, db / 20.0);
        const double Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        s1_b0 = (Vh + Vb * K / Q + K * K) / a0;
        s1_b1 = 2.0 * (K * K - Vh) / a0;
        s1_b2 = (Vh - Vb * K / Q + K * K) / a0;
        s1_a1 = 2.0 * (K * K - 1.0) / a0;
        s1_a2 = (1.0 - K / Q + K * K) / a0;
    }

    // Stage 2: RLB High-pass (2nd-order Butterworth ~38.135 Hz)
    double s2_b0, s2_b1, s2_b2, s2_a1, s2_a2;
    if (sample_rate == 48000) {
        s2_b0 = 1.0;
        s2_b1 = -2.0;
        s2_b2 = 1.0;
        s2_a1 = -1.99004745483398;
        s2_a2 = 0.99007225036621;
    } else {
        const double f0 = 38.13547087602444;
        const double Q = 0.5003270373238773;
        const double K = std::tan(kPi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        s2_b0 = 1.0 / a0;
        s2_b1 = -2.0 / a0;
        s2_b2 = 1.0 / a0;
        s2_a1 = 2.0 * (K * K - 1.0) / a0;
        s2_a2 = (1.0 - K / Q + K * K) / a0;
    }

    for (auto& ch : filters_) {
        ch.pre_shelf.b0 = s1_b0;
        ch.pre_shelf.b1 = s1_b1;
        ch.pre_shelf.b2 = s1_b2;
        ch.pre_shelf.a1 = s1_a1;
        ch.pre_shelf.a2 = s1_a2;

        ch.rlb_highpass.b0 = s2_b0;
        ch.rlb_highpass.b1 = s2_b1;
        ch.rlb_highpass.b2 = s2_b2;
        ch.rlb_highpass.a1 = s2_a1;
        ch.rlb_highpass.a2 = s2_a2;
    }
}

void EbuR128Processor::prepare(uint32_t sample_rate, uint32_t channels)
{
    sample_rate_ = std::max<uint32_t>(8000, sample_rate);
    channels_ = std::clamp<uint32_t>(channels, 1, kMaxChannels);
    block_size_frames_ = std::max<uint32_t>(1, sample_rate_ / 10); // 100 ms sub-blocks
    gating_block_energies_.assign(kMaxGatingBlocks, 0.0);
    configure_k_weighting(sample_rate_);
    reset();
}

void EbuR128Processor::reset() noexcept
{
    block_frame_pos_ = 0;
    for (auto& ch : filters_) {
        ch.pre_shelf.reset();
        ch.rlb_highpass.reset();
        ch.fir_history.fill(0.0f);
        ch.fir_pos = 0;
    }
    current_block_energy_.fill(0.0);
    recent_subblocks_.fill(0.0);
    recent_subblock_count_ = 0;
    recent_subblock_head_ = 0;
    gating_block_count_ = 0;
    gating_block_head_ = 0;
    max_true_peak_linear_ = 0.0f;
    momentary_lufs_ = -80.0f;
    short_term_lufs_ = -80.0f;
    integrated_lufs_ = -80.0f;
}

void EbuR128Processor::process(const float* interleaved_samples, uint32_t frames) noexcept
{
    if (!interleaved_samples || frames == 0) {
        return;
    }
    if (gating_block_energies_.empty()) {
        return;
    }

    for (uint32_t f = 0; f < frames; ++f) {
        for (uint32_t c = 0; c < channels_; ++c) {
            const float raw = interleaved_samples[f * channels_ + c];
            const float sample = std::isfinite(raw) ? raw : 0.0f;

            auto& ch = filters_[c];

            // 4x Polyphase FIR True Peak
            ch.fir_history[ch.fir_pos] = sample;
            for (size_t phase = 0; phase < kOversampleFactor; ++phase) {
                float acc = 0.0f;
                for (size_t k = 0; k < kFirTapsPerPhase; ++k) {
                    const size_t idx = (ch.fir_pos + kFirTapsPerPhase - k) % kFirTapsPerPhase;
                    acc += ch.fir_history[idx] * kTruePeakFir[phase][k];
                }
                max_true_peak_linear_ = std::max(max_true_peak_linear_, std::fabs(acc));
            }
            ch.fir_pos = (ch.fir_pos + 1) % kFirTapsPerPhase;

            // K-Weighting cascade
            const double s1 = ch.pre_shelf.process(static_cast<double>(sample));
            const double s2 = ch.rlb_highpass.process(s1);
            current_block_energy_[c] += s2 * s2;
        }

        ++block_frame_pos_;
        if (block_frame_pos_ >= block_size_frames_) {
            commit_100ms_block();
        }
    }
}

void EbuR128Processor::commit_100ms_block() noexcept
{
    const double inv_frames = 1.0 / static_cast<double>(block_size_frames_);
    double weighted_sum = 0.0;
    for (uint32_t c = 0; c < channels_; ++c) {
        const double z_i = current_block_energy_[c] * inv_frames;
        weighted_sum += static_cast<double>(channel_weight(c, channels_)) * z_i;
        current_block_energy_[c] = 0.0;
    }
    block_frame_pos_ = 0;

    recent_subblocks_[recent_subblock_head_] = weighted_sum;
    recent_subblock_head_ = (recent_subblock_head_ + 1) % recent_subblocks_.size();
    if (recent_subblock_count_ < recent_subblocks_.size()) {
        ++recent_subblock_count_;
    }

    // Momentary (400 ms = 4 x 100 ms sub-blocks)
    const size_t m_count = std::min<size_t>(4, recent_subblock_count_);
    double m_sum = 0.0;
    for (size_t i = 0; i < m_count; ++i) {
        const size_t idx = (recent_subblock_head_ + recent_subblocks_.size() - 1 - i) % recent_subblocks_.size();
        m_sum += recent_subblocks_[idx];
    }
    const double m_mean = (m_count > 0) ? (m_sum / static_cast<double>(m_count)) : 0.0;
    momentary_lufs_ = energy_to_lufs(m_mean);

    // Short-Term (3 s = 30 x 100 ms sub-blocks)
    double s_sum = 0.0;
    for (size_t i = 0; i < recent_subblock_count_; ++i) {
        const size_t idx = (recent_subblock_head_ + recent_subblocks_.size() - 1 - i) % recent_subblocks_.size();
        s_sum += recent_subblocks_[idx];
    }
    const double s_mean = (recent_subblock_count_ > 0) ? (s_sum / static_cast<double>(recent_subblock_count_)) : 0.0;
    short_term_lufs_ = energy_to_lufs(s_mean);

    // Record 400-ms gating block once at least 400 ms have elapsed
    if (recent_subblock_count_ >= 4 && !gating_block_energies_.empty()) {
        gating_block_energies_[gating_block_head_] = m_mean;
        gating_block_head_ = (gating_block_head_ + 1) % gating_block_energies_.size();
        if (gating_block_count_ < gating_block_energies_.size()) {
            ++gating_block_count_;
        }
        integrated_lufs_ = compute_integrated_lufs();
    }
}

float EbuR128Processor::compute_integrated_lufs() const noexcept
{
    if (gating_block_count_ == 0) {
        return -80.0f;
    }

    // Stage 1: Absolute gate (-70.0 LUFS)
    constexpr double kAbsThresholdEnergy = 1.0e-7; // 10^((-70 + 0.691) / 10) ~ 1.17e-7
    double abs_sum = 0.0;
    size_t abs_count = 0;
    for (size_t i = 0; i < gating_block_count_; ++i) {
        const double e = gating_block_energies_[i];
        if (energy_to_lufs(e) > -70.0f && e > kAbsThresholdEnergy * 0.5) {
            abs_sum += e;
            ++abs_count;
        }
    }
    if (abs_count == 0) {
        return -80.0f;
    }

    const float ungated_lufs = energy_to_lufs(abs_sum / static_cast<double>(abs_count));
    const float rel_threshold_lufs = ungated_lufs - 10.0f;

    // Stage 2: Relative gate (L_ungated - 10 dB)
    double rel_sum = 0.0;
    size_t rel_count = 0;
    for (size_t i = 0; i < gating_block_count_; ++i) {
        const double e = gating_block_energies_[i];
        const float lufs = energy_to_lufs(e);
        if (lufs > -70.0f && lufs > rel_threshold_lufs) {
            rel_sum += e;
            ++rel_count;
        }
    }
    if (rel_count == 0) {
        return -80.0f;
    }

    return energy_to_lufs(rel_sum / static_cast<double>(rel_count));
}

LoudnessSnapshot EbuR128Processor::snapshot() const noexcept
{
    LoudnessSnapshot snap;
    snap.momentary_lufs = momentary_lufs_;
    snap.short_term_lufs = short_term_lufs_;
    snap.integrated_lufs = integrated_lufs_;
    snap.true_peak_dbtp = linear_to_dbfs(max_true_peak_linear_);
    return snap;
}

} // namespace oae::studio
