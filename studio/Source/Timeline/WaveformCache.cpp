#include "WaveformCache.h"
#include <miniaudio.h>
#include <cmath>
#include <algorithm>

namespace oae::studio {

void WaveformEnvelope::get_envelope_at(double time_ms, float& out_min, float& out_max, float& out_rms) const
{
    if (min_peaks.empty()) {
        out_min = 0.0f;
        out_max = 0.0f;
        out_rms = 0.0f;
        return;
    }

    const double sec = std::max(0.0, time_ms / 1000.0);
    size_t idx = static_cast<size_t>(std::floor(sec * points_per_second));
    idx = std::clamp(idx, size_t(0), min_peaks.size() - 1);

    out_min = min_peaks[idx];
    out_max = max_peaks[idx];
    out_rms = rms_levels[idx];
}

void WaveformEnvelope::render_clip(juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   double /*clip_offset_ms*/,
                                   double clip_length_ms,
                                   double start_offset_ms,
                                   double pitch_semitones,
                                   bool loop,
                                   double fade_in_ms,
                                   double fade_out_ms,
                                   juce::Colour peak_colour,
                                   juce::Colour rms_colour) const
{
    if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 2.0f || clip_length_ms <= 0.0 || is_empty()) {
        return;
    }

    const float centerY = bounds.getCentreY();
    const float halfHeight = std::max(1.0f, (bounds.getHeight() - 4.0f) * 0.5f);
    const double pitch_ratio = std::pow(2.0, pitch_semitones / 12.0);
    const int pixelWidth = std::max(2, static_cast<int>(std::ceil(bounds.getWidth())));

    g.setColour(juce::Colour(0x38ffffff));
    g.drawHorizontalLine(static_cast<int>(std::round(centerY)), bounds.getX(), bounds.getRight());

    std::vector<float> xCoords;
    std::vector<float> topPeaks;
    std::vector<float> bottomPeaks;
    std::vector<float> topRms;
    std::vector<float> bottomRms;
    xCoords.reserve(static_cast<size_t>(pixelWidth));
    topPeaks.reserve(static_cast<size_t>(pixelWidth));
    bottomPeaks.reserve(static_cast<size_t>(pixelWidth));
    topRms.reserve(static_cast<size_t>(pixelWidth));
    bottomRms.reserve(static_cast<size_t>(pixelWidth));

    for (int i = 0; i < pixelWidth; ++i) {
        const float tNorm = static_cast<float>(i) / static_cast<float>(pixelWidth - 1);
        const float px = bounds.getX() + tNorm * bounds.getWidth();
        const double rel_t_ms = static_cast<double>(tNorm) * clip_length_ms;
        double sample_t_ms = start_offset_ms + rel_t_ms * pitch_ratio;

        if (loop && duration_ms > 0.0) {
            sample_t_ms = std::fmod(sample_t_ms, duration_ms);
        }

        float fade = 1.0f;
        if (fade_in_ms > 0.0 && rel_t_ms < fade_in_ms) {
            fade = std::min(fade, static_cast<float>(rel_t_ms / fade_in_ms));
        }
        if (fade_out_ms > 0.0 && rel_t_ms > (clip_length_ms - fade_out_ms)) {
            fade = std::min(fade, static_cast<float>((clip_length_ms - rel_t_ms) / fade_out_ms));
        }
        fade = std::clamp(fade, 0.0f, 1.0f);

        float minVal = 0.0f;
        float maxVal = 0.0f;
        float rmsVal = 0.0f;
        const double window_ms = (clip_length_ms / static_cast<double>(pixelWidth)) * pitch_ratio;
        const int subSteps = window_ms > 10.0 ? 3 : 1;
        for (int s = 0; s < subSteps; ++s) {
            double st = sample_t_ms + (static_cast<double>(s) / static_cast<double>(subSteps)) * window_ms;
            if (loop && duration_ms > 0.0) {
                st = std::fmod(st, duration_ms);
            }
            float sMin = 0.0f;
            float sMax = 0.0f;
            float sRms = 0.0f;
            get_envelope_at(st, sMin, sMax, sRms);
            if (s == 0) {
                minVal = sMin;
                maxVal = sMax;
                rmsVal = sRms;
            } else {
                minVal = std::min(minVal, sMin);
                maxVal = std::max(maxVal, sMax);
                rmsVal = std::max(rmsVal, sRms);
            }
        }

        minVal *= fade;
        maxVal *= fade;
        rmsVal *= fade;

        xCoords.push_back(px);
        topPeaks.push_back(centerY - (maxVal * halfHeight));
        bottomPeaks.push_back(centerY - (minVal * halfHeight));
        topRms.push_back(centerY - (rmsVal * halfHeight));
        bottomRms.push_back(centerY + (rmsVal * halfHeight));
    }

    juce::Path peakPolygon;
    peakPolygon.startNewSubPath(xCoords.front(), topPeaks.front());
    for (size_t i = 1; i < xCoords.size(); ++i) {
        peakPolygon.lineTo(xCoords[i], topPeaks[i]);
    }
    for (size_t i = xCoords.size(); i-- > 0;) {
        peakPolygon.lineTo(xCoords[i], bottomPeaks[i]);
    }
    peakPolygon.closeSubPath();

    juce::ColourGradient peakGrad(peak_colour.withAlpha(0.55f), bounds.getX(), bounds.getY(),
                                  peak_colour.withAlpha(0.55f), bounds.getX(), bounds.getBottom(), false);
    peakGrad.addColour(0.5, peak_colour.withAlpha(0.18f));
    g.setGradientFill(peakGrad);
    g.fillPath(peakPolygon);

    g.setColour(peak_colour.withAlpha(0.92f));
    g.strokePath(peakPolygon, juce::PathStrokeType(1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path rmsPolygon;
    rmsPolygon.startNewSubPath(xCoords.front(), topRms.front());
    for (size_t i = 1; i < xCoords.size(); ++i) {
        rmsPolygon.lineTo(xCoords[i], topRms[i]);
    }
    for (size_t i = xCoords.size(); i-- > 0;) {
        rmsPolygon.lineTo(xCoords[i], bottomRms[i]);
    }
    rmsPolygon.closeSubPath();

    g.setColour(rms_colour.withAlpha(0.84f));
    g.fillPath(rmsPolygon);
}

WaveformCache::WaveformCache()
{
    formatManager_.registerBasicFormats();
}

WaveformCache::~WaveformCache()
{
    threadPool_.removeAllJobs(true, 2000);
}

std::shared_ptr<const WaveformEnvelope> WaveformCache::get_or_load(const fs::path& file_path)
{
    const std::string key = file_path.lexically_normal().u8string();
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        if (pending_[key]) {
            return nullptr;
        }
        pending_[key] = true;
    }

    threadPool_.addJob([this, file_path, key]() {
        auto env = decode_file(file_path);
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            if (env) {
                cache_[key] = env;
            }
            pending_[key] = false;
        }
        juce::MessageManager::callAsync([this]() {
            sendChangeMessage();
        });
    });

    return nullptr;
}

std::shared_ptr<const WaveformEnvelope> WaveformCache::load_synchronously(const fs::path& file_path)
{
    const std::string key = file_path.lexically_normal().u8string();
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
    }

    auto env = decode_file(file_path);
    if (env) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[key] = env;
    }
    return env;
}

std::shared_ptr<const WaveformEnvelope> WaveformCache::get_or_load_composite(const std::string& cache_key,
                                                                               const std::vector<fs::path>& file_paths)
{
    if (file_paths.empty()) return nullptr;
    if (file_paths.size() == 1) return get_or_load(file_paths.front());

    const std::string full_key = "composite:" + cache_key;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(full_key);
        if (it != cache_.end()) {
            return it->second;
        }
        if (pending_[full_key]) {
            return nullptr;
        }
        pending_[full_key] = true;
    }

    threadPool_.addJob([this, full_key, file_paths]() {
        auto env = build_composite_envelope(file_paths);
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            if (env) {
                cache_[full_key] = env;
            }
            pending_[full_key] = false;
        }
        juce::MessageManager::callAsync([this]() {
            sendChangeMessage();
        });
    });
    return nullptr;
}

std::shared_ptr<const WaveformEnvelope> WaveformCache::load_composite_synchronously(const std::string& cache_key,
                                                                                      const std::vector<fs::path>& file_paths)
{
    if (file_paths.empty()) return nullptr;
    if (file_paths.size() == 1) return load_synchronously(file_paths.front());

    const std::string full_key = "composite:" + cache_key;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(full_key);
        if (it != cache_.end()) {
            return it->second;
        }
    }

    auto env = build_composite_envelope(file_paths);
    if (env) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[full_key] = env;
    }
    return env;
}

std::shared_ptr<const WaveformEnvelope> WaveformCache::get_or_build_synthetic(const std::string& key_name)
{
    if (key_name.empty()) return nullptr;
    const std::string full_key = "synthetic:" + key_name;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(full_key);
        if (it != cache_.end()) {
            return it->second;
        }
    }

    auto env = std::make_shared<WaveformEnvelope>();
    env->duration_ms = 2000.0;
    env->sample_rate = 48000;
    env->channels = 2;
    env->points_per_second = 200.0;
    constexpr size_t kPoints = 400;
    env->min_peaks.resize(kPoints);
    env->max_peaks.resize(kPoints);
    env->rms_levels.resize(kPoints);

    const size_t seed = std::hash<std::string>{}(key_name);
    const double f1 = 2.5 + static_cast<double>(seed % 5) * 0.7;
    const double f2 = 6.0 + static_cast<double>((seed >> 8) % 7) * 0.9;
    const double phase = static_cast<double>((seed >> 16) % 628) * 0.01;
    for (size_t i = 0; i < kPoints; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kPoints);
        const double envShape = std::sin(t * 3.14159265358979323846) * (0.55 + 0.35 * std::sin(t * f1 * 6.283185 + phase));
        const float peak = static_cast<float>(std::clamp(std::abs(envShape) + 0.12 * std::abs(std::cos(t * f2 * 6.283185)), 0.08, 0.92));
        env->max_peaks[i] = peak;
        env->min_peaks[i] = -peak * 0.95f;
        env->rms_levels[i] = peak * 0.62f;
    }

    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_[full_key] = env;
    return env;
}

std::shared_ptr<WaveformEnvelope> WaveformCache::build_composite_envelope(const std::vector<fs::path>& file_paths)
{
    std::vector<std::shared_ptr<const WaveformEnvelope>> parts;
    parts.reserve(file_paths.size());
    size_t max_blocks = 0;
    double max_duration_ms = 0.0;

    for (const auto& fp : file_paths) {
        auto part = load_synchronously(fp);
        if (part && !part->is_empty()) {
            max_blocks = std::max(max_blocks, part->min_peaks.size());
            max_duration_ms = std::max(max_duration_ms, part->duration_ms);
            parts.push_back(std::move(part));
        }
    }
    if (parts.empty() || max_blocks == 0) {
        return nullptr;
    }

    auto combined = std::make_shared<WaveformEnvelope>();
    combined->sample_rate = parts.front()->sample_rate;
    combined->channels = parts.front()->channels;
    combined->points_per_second = parts.front()->points_per_second;
    combined->duration_ms = max_duration_ms;
    combined->min_peaks.assign(max_blocks, 0.0f);
    combined->max_peaks.assign(max_blocks, 0.0f);
    combined->rms_levels.assign(max_blocks, 0.0f);

    for (const auto& part : parts) {
        for (size_t i = 0; i < part->min_peaks.size(); ++i) {
            combined->min_peaks[i] = std::min(combined->min_peaks[i], part->min_peaks[i]);
            combined->max_peaks[i] = std::max(combined->max_peaks[i], part->max_peaks[i]);
            combined->rms_levels[i] = std::max(combined->rms_levels[i], part->rms_levels[i]);
        }
    }
    return combined;
}

void WaveformCache::clear()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
    pending_.clear();
}

std::shared_ptr<WaveformEnvelope> WaveformCache::decode_file(const fs::path& file_path)
{
    if (!fs::exists(file_path)) {
        return nullptr;
    }

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#if defined(_WIN32)
    ma_result initRes = ma_decoder_init_file_w(file_path.c_str(), &config, &decoder);
#else
    ma_result initRes = ma_decoder_init_file(file_path.u8string().c_str(), &config, &decoder);
#endif

    if (initRes == MA_SUCCESS) {
        ma_uint64 totalFrames = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS || totalFrames == 0) {
            ma_uint64 framesCount = 0;
            ma_uint64 read = 0;
            std::vector<float> probeBuf(4096 * (decoder.outputChannels > 0 ? decoder.outputChannels : 2));
            while (ma_decoder_read_pcm_frames(&decoder, probeBuf.data(), 4096, &read) == MA_SUCCESS && read > 0) {
                framesCount += read;
            }
            totalFrames = framesCount;
            ma_decoder_seek_to_pcm_frame(&decoder, 0);
        }

        if (totalFrames > 0 && decoder.outputSampleRate > 0 && decoder.outputChannels > 0) {
            auto env = std::make_shared<WaveformEnvelope>();
            env->sample_rate = static_cast<uint32_t>(decoder.outputSampleRate);
            env->channels = decoder.outputChannels;
            env->duration_ms = (static_cast<double>(totalFrames) / decoder.outputSampleRate) * 1000.0;
            env->points_per_second = 200.0;

            const int samplesPerBlock = std::max(1, static_cast<int>(decoder.outputSampleRate / env->points_per_second));
            const size_t numBlocks = static_cast<size_t>((totalFrames + samplesPerBlock - 1) / samplesPerBlock);

            env->min_peaks.resize(numBlocks, 0.0f);
            env->max_peaks.resize(numBlocks, 0.0f);
            env->rms_levels.resize(numBlocks, 0.0f);

            constexpr ma_uint64 readChunkFrames = 4096;
            std::vector<float> pcmBuffer(readChunkFrames * decoder.outputChannels);

            ma_uint64 frameCursor = 0;
            size_t blockIndex = 0;
            float currentMin = 0.0f;
            float currentMax = 0.0f;
            double currentSumSq = 0.0;
            int currentCount = 0;

            while (frameCursor < totalFrames) {
                const ma_uint64 framesToRead = std::min<ma_uint64>(readChunkFrames, totalFrames - frameCursor);
                ma_uint64 framesRead = 0;
                const ma_result readRes = ma_decoder_read_pcm_frames(&decoder, pcmBuffer.data(), framesToRead, &framesRead);
                if (readRes != MA_SUCCESS || framesRead == 0) {
                    break;
                }

                for (ma_uint64 i = 0; i < framesRead; ++i) {
                    float sampleVal = 0.0f;
                    for (uint32_t ch = 0; ch < decoder.outputChannels; ++ch) {
                        const float s = pcmBuffer[i * decoder.outputChannels + ch];
                        if (std::abs(s) > std::abs(sampleVal)) {
                            sampleVal = s;
                        }
                    }

                    if (currentCount == 0) {
                        currentMin = sampleVal;
                        currentMax = sampleVal;
                        currentSumSq = sampleVal * sampleVal;
                    } else {
                        currentMin = std::min(currentMin, sampleVal);
                        currentMax = std::max(currentMax, sampleVal);
                        currentSumSq += sampleVal * sampleVal;
                    }
                    ++currentCount;

                    if (currentCount >= samplesPerBlock && blockIndex < numBlocks) {
                        env->min_peaks[blockIndex] = currentMin;
                        env->max_peaks[blockIndex] = currentMax;
                        env->rms_levels[blockIndex] = static_cast<float>(std::sqrt(currentSumSq / currentCount));
                        ++blockIndex;
                        currentCount = 0;
                    }
                }

                frameCursor += framesRead;
            }

            if (currentCount > 0 && blockIndex < numBlocks) {
                env->min_peaks[blockIndex] = currentMin;
                env->max_peaks[blockIndex] = currentMax;
                env->rms_levels[blockIndex] = static_cast<float>(std::sqrt(currentSumSq / currentCount));
            }

            ma_decoder_uninit(&decoder);
            return env;
        }

        ma_decoder_uninit(&decoder);
    }

    const juce::File jFile(juce::String::fromUTF8(file_path.u8string().c_str()));
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(jFile));
    if (!reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) {
        return nullptr;
    }

    auto env = std::make_shared<WaveformEnvelope>();
    env->sample_rate = static_cast<uint32_t>(reader->sampleRate);
    env->channels = reader->numChannels;
    env->duration_ms = (static_cast<double>(reader->lengthInSamples) / reader->sampleRate) * 1000.0;
    env->points_per_second = 200.0;

    const int64_t totalSamples = reader->lengthInSamples;
    const int samplesPerBlock = std::max(1, static_cast<int>(reader->sampleRate / env->points_per_second));
    const size_t numBlocks = static_cast<size_t>((totalSamples + samplesPerBlock - 1) / samplesPerBlock);

    env->min_peaks.resize(numBlocks, 0.0f);
    env->max_peaks.resize(numBlocks, 0.0f);
    env->rms_levels.resize(numBlocks, 0.0f);

    constexpr int readChunkSize = 4096;
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), readChunkSize);

    int64_t sampleCursor = 0;
    size_t blockIndex = 0;
    float currentMin = 0.0f;
    float currentMax = 0.0f;
    double currentSumSq = 0.0;
    int currentCount = 0;

    while (sampleCursor < totalSamples) {
        const int samplesToRead = static_cast<int>(std::min<int64_t>(readChunkSize, totalSamples - sampleCursor));
        reader->read(&buffer, 0, samplesToRead, sampleCursor, true, true);

        for (int i = 0; i < samplesToRead; ++i) {
            float sampleVal = 0.0f;
            for (int ch = 0; ch < static_cast<int>(reader->numChannels); ++ch) {
                const float s = buffer.getSample(ch, i);
                if (std::abs(s) > std::abs(sampleVal)) sampleVal = s;
            }

            if (currentCount == 0) {
                currentMin = sampleVal;
                currentMax = sampleVal;
                currentSumSq = sampleVal * sampleVal;
            } else {
                currentMin = std::min(currentMin, sampleVal);
                currentMax = std::max(currentMax, sampleVal);
                currentSumSq += sampleVal * sampleVal;
            }
            ++currentCount;

            if (currentCount >= samplesPerBlock && blockIndex < numBlocks) {
                env->min_peaks[blockIndex] = currentMin;
                env->max_peaks[blockIndex] = currentMax;
                env->rms_levels[blockIndex] = static_cast<float>(std::sqrt(currentSumSq / currentCount));
                ++blockIndex;
                currentCount = 0;
            }
        }

        sampleCursor += samplesToRead;
    }

    if (currentCount > 0 && blockIndex < numBlocks) {
        env->min_peaks[blockIndex] = currentMin;
        env->max_peaks[blockIndex] = currentMax;
        env->rms_levels[blockIndex] = static_cast<float>(std::sqrt(currentSumSq / currentCount));
    }

    return env;
}

} // namespace oae::studio
