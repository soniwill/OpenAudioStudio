#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <filesystem>

namespace fs = std::filesystem;

namespace oae::studio {

/** Multi-resolution Peak and RMS waveform envelope data for an audio sample. */
struct WaveformEnvelope {
    double duration_ms{ 0.0 };
    uint32_t sample_rate{ 48000 };
    uint32_t channels{ 2 };
    double points_per_second{ 200.0 }; // 1 point per 5 ms

    std::vector<float> min_peaks;
    std::vector<float> max_peaks;
    std::vector<float> rms_levels;

    bool is_empty() const noexcept { return min_peaks.empty(); }

    void get_envelope_at(double time_ms, float& out_min, float& out_max, float& out_rms) const;

    /** Renders the transformed waveform envelope inside the given clip bounds. */
    void render_clip(juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     double clip_offset_ms,
                     double clip_length_ms,
                     double start_offset_ms,
                     double pitch_semitones,
                     bool loop,
                     double fade_in_ms,
                     double fade_out_ms,
                     juce::Colour peak_colour,
                     juce::Colour rms_colour) const;
};

/** Thread-safe cache and asynchronous background decoder for audio waveforms. */
class WaveformCache : public juce::ChangeBroadcaster {
public:
    WaveformCache();
    ~WaveformCache();

    /** Queries or queues background loading of waveform envelope for the given audio file. */
    std::shared_ptr<const WaveformEnvelope> get_or_load(const fs::path& file_path);

    /** Queries or queues background synthesis of a composite envelope across multiple audio files. */
    std::shared_ptr<const WaveformEnvelope> get_or_load_composite(const std::string& cache_key,
                                                                  const std::vector<fs::path>& file_paths);

    /** Builds a composite envelope synchronously across multiple audio files on the calling thread. */
    std::shared_ptr<const WaveformEnvelope> load_composite_synchronously(const std::string& cache_key,
                                                                         const std::vector<fs::path>& file_paths);

    /** Returns a deterministic synthetic envelope for keyed programmer instruments. */
    std::shared_ptr<const WaveformEnvelope> get_or_build_synthetic(const std::string& key_name);

    /** Loads and decodes waveform envelope synchronously on the calling thread. */
    std::shared_ptr<const WaveformEnvelope> load_synchronously(const fs::path& file_path);

    /** Clears all cached waveforms. */
    void clear();

private:
    std::shared_ptr<WaveformEnvelope> decode_file(const fs::path& file_path);
    std::shared_ptr<WaveformEnvelope> build_composite_envelope(const std::vector<fs::path>& file_paths);

    juce::AudioFormatManager formatManager_;
    juce::ThreadPool threadPool_{ 2 };

    mutable std::mutex cacheMutex_;
    std::unordered_map<std::string, std::shared_ptr<WaveformEnvelope>> cache_;
    std::unordered_map<std::string, bool> pending_;
};

} // namespace oae::studio
