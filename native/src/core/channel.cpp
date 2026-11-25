#include "channel.h"
#include "bus.h"
#include "system.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

extern "C" void oae_ma_enable_pitch(ma_sound* sound);

namespace oae {

Channel::Channel(Bus& bus, std::shared_ptr<const SoundData> data, std::uint64_t order, std::uint64_t start) noexcept
    : bus_(&bus), data_(std::move(data)), strip_(*bus.owner()), order_(order), startClock_(start),
      virtualSince_(bus.owner()->clock()), loop_(data_ ? data_->loop : false)
{
    static_assert(std::is_standard_layout_v<PcmSource>);
    static_assert(std::atomic<ma_uint64>::is_always_lock_free);
}

Channel::~Channel()
{
    if (soundReady_) {
        ma_sound_uninit(&sound_);
    }
    if (sourceReady_) {
        ma_data_source_uninit(&source_.base);
    }
    if (sliceReady_) {
        ma_data_source_uninit(&sliceSource_.base);
    }
    if (sliceSource_.decoderReady) {
        ma_decoder_uninit(&sliceSource_.decoder);
    }
}

oae_result Channel::init() noexcept
{
    constexpr ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_PITCH;
    ma_result result;
    if (data_->streamPath.empty()) {
        static constexpr ma_data_source_vtable callbacks{
            PcmSource::read, PcmSource::seek, PcmSource::format, PcmSource::position,
            PcmSource::length, nullptr, 0
        };
        auto config = ma_data_source_config_init();
        config.vtable = &callbacks;
        result = ma_data_source_init(&config, &source_.base);
        if (result != MA_SUCCESS) {
            return sound_error(result);
        }
        sourceReady_ = true;
        source_.data = data_.get();
        result = ma_sound_init_from_data_source(owner()->engine(), &source_.base, flags, nullptr, &sound_);
    } else if (data_->streamLength > 0) {
        sliceSource_.data = data_.get();
        sliceSource_.pos = 0;
        sliceSource_.file.open(data_->streamPath, std::ios::binary);
        if (!sliceSource_.file.is_open()) {
            return OAE_ERR_FILE_NOT_FOUND;
        }
        sliceSource_.file.seekg(static_cast<std::streamoff>(data_->streamOffset), std::ios::beg);
        if (!sliceSource_.file) {
            return OAE_ERR_FORMAT;
        }
        auto on_read = [](ma_decoder* dec, void* out, size_t bytes, size_t* bytes_read) -> ma_result {
            auto* ss = static_cast<SliceSource*>(dec->pUserData);
            if (!ss || ss->pos >= ss->data->streamLength) {
                if (bytes_read) *bytes_read = 0;
                return MA_AT_END;
            }
            const std::uint64_t avail = ss->data->streamLength - ss->pos;
            const size_t n = static_cast<size_t>(std::min<std::uint64_t>(bytes, avail));
            ss->file.read(static_cast<char*>(out), static_cast<std::streamsize>(n));
            const size_t got = static_cast<size_t>(ss->file.gcount());
            ss->pos += got;
            if (bytes_read) *bytes_read = got;
            return got > 0 ? MA_SUCCESS : MA_AT_END;
        };
        auto on_seek = [](ma_decoder* dec, ma_int64 offset, ma_seek_origin origin) -> ma_result {
            auto* ss = static_cast<SliceSource*>(dec->pUserData);
            if (!ss) return MA_ERROR;
            std::int64_t target = 0;
            if (origin == ma_seek_origin_start) target = offset;
            else if (origin == ma_seek_origin_current) target = static_cast<std::int64_t>(ss->pos) + offset;
            else if (origin == ma_seek_origin_end) target = static_cast<std::int64_t>(ss->data->streamLength) + offset;
            if (target < 0 || static_cast<std::uint64_t>(target) > ss->data->streamLength) return MA_ERROR;
            ss->pos = static_cast<std::uint64_t>(target);
            ss->file.clear();
            ss->file.seekg(static_cast<std::streamoff>(ss->data->streamOffset + ss->pos), std::ios::beg);
            return ss->file ? MA_SUCCESS : MA_ERROR;
        };
        const auto dec_cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        result = ma_decoder_init(on_read, on_seek, &sliceSource_, &dec_cfg, &sliceSource_.decoder);
        if (result != MA_SUCCESS) {
            return sound_error(result);
        }
        sliceSource_.decoderReady = true;
        static constexpr ma_data_source_vtable slice_callbacks{
            SliceSource::read, SliceSource::seek, SliceSource::format, SliceSource::position,
            SliceSource::length, nullptr, 0
        };
        auto ds_cfg = ma_data_source_config_init();
        ds_cfg.vtable = &slice_callbacks;
        result = ma_data_source_init(&ds_cfg, &sliceSource_.base);
        if (result != MA_SUCCESS) {
            return sound_error(result);
        }
        sliceReady_ = true;
        result = ma_sound_init_from_data_source(owner()->engine(), &sliceSource_.base, flags | MA_SOUND_FLAG_STREAM, nullptr, &sound_);
    } else {
#if defined(_WIN32)
        result = ma_sound_init_from_file_w(owner()->engine(), data_->streamPath.c_str(),
            flags | MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sound_);
#else
        result = ma_sound_init_from_file(owner()->engine(), data_->streamPath.c_str(),
            flags | MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sound_);
#endif
    }
    if (result != MA_SUCCESS) {
        return sound_error(result);
    }
    soundReady_ = true;
    result = ma_sound_get_data_format(&sound_, nullptr, nullptr, &rate_, nullptr, 0);
    if (result != MA_SUCCESS || rate_ == 0) {
        return OAE_ERR_FORMAT;
    }
    result = ma_sound_get_length_in_pcm_frames(&sound_, &frames_);
    if (result == MA_NOT_IMPLEMENTED && !data_->streamPath.empty()) {
        const auto seconds = data_->frames / data_->rate;
        const auto fractionalFrames = data_->frames % data_->rate * rate_ / data_->rate;
        constexpr auto maxFrames = (std::numeric_limits<ma_uint64>::max)();
        if (seconds > (maxFrames - fractionalFrames) / rate_) {
            return OAE_ERR_FORMAT;
        }
        frames_ = seconds * rate_ + fractionalFrames;
    } else if (result != MA_SUCCESS) {
        return sound_error(result);
    }
    ma_sound_set_looping(&sound_, loop_ ? MA_TRUE : MA_FALSE);
    const auto stripResult = strip_.init();
    if (stripResult != OAE_OK) {
        return stripResult;
    }
    return ma_node_attach_output_bus(&sound_, 0, strip_.node(), 0) == MA_SUCCESS ? OAE_OK : OAE_ERR_INTERNAL;
}

System* Channel::owner() const noexcept { return bus_->owner(); }
bool Channel::finished() const noexcept { return finished_at(owner()->clock()); }

bool Channel::finished_at(std::uint64_t now) const noexcept
{
    if (hasAhdsr_ && ahdsrRel_ != UINT64_MAX) {
        return now >= ahdsrRel_ + ahdsr_.release_frames;
    }
    if (stopped_ || (fadeStop_ && now >= stopClock_)) {
        return true;
    }
    if (physical_) {
        return ma_sound_at_end(&sound_) != MA_FALSE;
    }
    return now >= startClock_ && !paused_ && !bus_->paused_in_tree() && !loop_
        && virtual_cursor(now) >= static_cast<double>(frames_);
}

bool Channel::paused() const noexcept { return paused_; }
bool Channel::playing() const noexcept { return !paused_ && !finished(); }

double Channel::audibility() const noexcept
{
    const auto level = static_cast<double>(volume()) * strip_.fade_gain(owner()->clock());
    return level == 0.0 ? 0.0 : bus_->effective_gain() * level;
}

double Channel::virtual_cursor(std::uint64_t now) const noexcept
{
    const auto from = std::max(virtualSince_, startClock_);
    if (paused_ || stopped_ || bus_->paused_in_tree() || now <= from) {
        return virtualFrame_;
    }
    const double advance = static_cast<double>(now - from) * pitch() * rate_ / owner()->sample_rate();
    const double position = virtualFrame_ + advance;
    const double length = static_cast<double>(frames_);
    return loop_ && length > 0.0 ? std::fmod(position, length) : std::min(position, length);
}

void Channel::set_loop(bool loop) noexcept
{
    loop_ = loop;
    if (soundReady_) {
        ma_sound_set_looping(&sound_, loop_ ? MA_TRUE : MA_FALSE);
    }
}

void Channel::sync_virtual(std::uint64_t now) noexcept
{
    if (!physical_) {
        virtualFrame_ = virtual_cursor(now);
        virtualSince_ = now;
    }
}

oae_result Channel::set_physical(bool physical, std::uint64_t now) noexcept
{
    if (physical_ == physical) {
        return OAE_OK;
    }
    if (physical) {
        sync_virtual(now);
        const auto cursor = virtualFrame_ >= static_cast<double>(frames_)
            ? frames_ : static_cast<ma_uint64>(virtualFrame_);
        auto result = ma_sound_seek_to_pcm_frame(&sound_, cursor);
        ma_sound_set_start_time_in_pcm_frames(&sound_, std::max(startClock_, now));
        if (result == MA_SUCCESS) {
            result = ma_node_attach_output_bus(strip_.node(), 0, bus_->group(), 0);
        }
        if (result != MA_SUCCESS) {
            return OAE_ERR_INTERNAL;
        }
        if (!paused_ && ma_sound_start(&sound_) != MA_SUCCESS) {
            ma_node_detach_output_bus(strip_.node(), 0);
            return OAE_ERR_INTERNAL;
        }
        physical_ = true;
        return OAE_OK;
    }

    if (ma_sound_stop(&sound_) != MA_SUCCESS || ma_node_detach_output_bus(strip_.node(), 0) != MA_SUCCESS) {
        return OAE_ERR_INTERNAL;
    }
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&sound_, &cursor) != MA_SUCCESS) {
        physical_ = false;
        stopped_ = true;
        return OAE_ERR_INTERNAL;
    }
    virtualFrame_ = static_cast<double>(cursor);
    virtualSince_ = owner()->clock();
    physical_ = false;
    return OAE_OK;
}

oae_result Channel::set_paused(bool paused) noexcept
{
    if (finished()) {
        return OAE_ERR_INVALID_STATE;
    }
    if (!physical_) {
        sync_virtual(owner()->clock());
        paused_ = paused;
        return OAE_OK;
    }
    const auto result = paused ? ma_sound_stop(&sound_) : ma_sound_start(&sound_);
    if (result == MA_SUCCESS) {
        paused_ = paused;
    }
    return result == MA_SUCCESS ? OAE_OK : OAE_ERR_INTERNAL;
}

oae_result Channel::stop() noexcept
{
    sync_virtual(owner()->clock());
    if (hasAhdsr_ && ahdsrRel_ == UINT64_MAX && ahdsr_.release_frames > 0) {
        trigger_ahdsr_release(owner()->clock());
        return OAE_OK;
    }
    if (physical_ && ma_sound_stop(&sound_) != MA_SUCCESS) {
        return OAE_ERR_INTERNAL;
    }
    stopped_ = true;
    paused_ = false;
    return OAE_OK;
}

oae_result Channel::stop_with_fade(std::uint64_t duration) noexcept
{
    const auto now = owner()->clock();
    if (finished_at(now)) {
        return OAE_ERR_INVALID_STATE;
    }
    if (hasAhdsr_) {
        trigger_ahdsr_release(now);
        return OAE_OK;
    }
    strip_.fade_to(0.0f, duration, FadeCurve::linear, now);
    if (duration == 0) {
        return stop();
    }
    constexpr auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    stopClock_ = now + std::min(duration, maximum - now);
    fadeStop_ = true;
    strip_.stop_at(stopClock_);
    return OAE_OK;
}

void Channel::set_ahdsr(const AhdsrSpec& spec) noexcept
{
    hasAhdsr_ = true;
    ahdsr_ = spec;
    ahdsrRel_ = UINT64_MAX;
    strip_.set_ahdsr(spec);
}

void Channel::trigger_ahdsr_release(std::uint64_t rel_frame) noexcept
{
    if (hasAhdsr_ && ahdsrRel_ == UINT64_MAX) {
        ahdsrRel_ = rel_frame;
        strip_.trigger_ahdsr_release(rel_frame);
    }
}

void Channel::set_volume(float volume) noexcept { ma_sound_set_volume(&sound_, volume); }
float Channel::volume() const noexcept { return ma_sound_get_volume(&sound_); }
oae_result Channel::set_pitch(float pitch) noexcept
{
    const float baseRatio = static_cast<float>(rate_) / static_cast<float>(ma_engine_get_sample_rate(owner()->engine()));
    const float scaledRatio = baseRatio * pitch * 1'000'000.0f;
    if (!(scaledRatio >= 1.0f && scaledRatio < 4'294'967'296.0f)) {
        return OAE_ERR_INVALID_PARAM;
    }
    sync_virtual(owner()->clock());
    ma_sound_set_pitch(&sound_, pitch);
    if (pitch != 1.0f) {
        oae_ma_enable_pitch(&sound_);
    }
    return OAE_OK;
}
float Channel::pitch() const noexcept { return ma_sound_get_pitch(&sound_); }

oae_result Channel::set_position_ms(std::uint32_t position) noexcept
{
    if (finished()) {
        return OAE_ERR_INVALID_STATE;
    }
    const ma_uint64 raw_frame = ma_uint64{position} * rate_ / 1000u;
    const auto frame = (loop_ && frames_ > 0) ? (raw_frame % frames_) : std::min(frames_, raw_frame);
    if (!physical_) {
        virtualFrame_ = static_cast<double>(frame);
        virtualSince_ = owner()->clock();
        return OAE_OK;
    }
    return ma_sound_seek_to_pcm_frame(&sound_, frame) == MA_SUCCESS ? OAE_OK : OAE_ERR_INTERNAL;
}

oae_result Channel::position_ms(std::uint32_t& position) const noexcept
{
    ma_uint64 cursor = 0;
    if (physical_) {
        if (ma_sound_get_cursor_in_pcm_frames(&sound_, &cursor) != MA_SUCCESS) {
            return OAE_ERR_INTERNAL;
        }
    } else {
        const auto logical = virtual_cursor(owner()->clock());
        cursor = logical >= static_cast<double>(frames_) ? frames_ : static_cast<ma_uint64>(logical);
    }
    position = frames_to_ms(cursor, rate_);
    return OAE_OK;
}

ma_result Channel::PcmSource::read(ma_data_source* source, void* output, ma_uint64 frames, ma_uint64* read) noexcept
{
    auto& pcm = *static_cast<PcmSource*>(source);
    const auto start = pcm.cursor.load(std::memory_order_relaxed);
    const auto count = std::min(frames, pcm.data->frames - start);
    const auto channels = pcm.data->channels;
    if (output && count > 0) {
        const auto* begin = pcm.data->samples.data() + static_cast<std::size_t>(start) * channels;
        std::memcpy(output, begin, static_cast<std::size_t>(count) * channels * sizeof(float));
    }
    pcm.cursor.store(start + count, std::memory_order_relaxed);
    if (read) {
        *read = count;
    }
    return count < frames ? MA_AT_END : MA_SUCCESS;
}

ma_result Channel::PcmSource::seek(ma_data_source* source, ma_uint64 frame) noexcept
{
    auto& pcm = *static_cast<PcmSource*>(source);
    pcm.cursor.store(std::min(frame, pcm.data->frames), std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result Channel::PcmSource::format(ma_data_source* source, ma_format* format,
    ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t mapSize) noexcept
{
    const auto& data = *static_cast<PcmSource*>(source)->data;
    if (format) {
        *format = ma_format_f32;
    }
    if (channels) {
        *channels = data.channels;
    }
    if (rate) {
        *rate = data.rate;
    }
    if (map) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, map, mapSize, data.channels);
    }
    return MA_SUCCESS;
}

ma_result Channel::PcmSource::position(ma_data_source* source, ma_uint64* cursor) noexcept
{
    *cursor = static_cast<PcmSource*>(source)->cursor.load(std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result Channel::SliceSource::read(ma_data_source* source, void* output, ma_uint64 frames, ma_uint64* read) noexcept
{
    auto& ss = *static_cast<SliceSource*>(source);
    ma_uint64 got = 0;
    const ma_result res = ma_decoder_read_pcm_frames(&ss.decoder, output, frames, &got);
    if (read) {
        *read = got;
    }
    return res;
}

ma_result Channel::SliceSource::seek(ma_data_source* source, ma_uint64 frame) noexcept
{
    auto& ss = *static_cast<SliceSource*>(source);
    return ma_decoder_seek_to_pcm_frame(&ss.decoder, frame);
}

ma_result Channel::SliceSource::format(ma_data_source* source, ma_format* format,
    ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t mapSize) noexcept
{
    const auto& data = *static_cast<SliceSource*>(source)->data;
    if (format) *format = ma_format_f32;
    if (channels) *channels = data.channels;
    if (rate) *rate = data.rate;
    if (map) ma_channel_map_init_standard(ma_standard_channel_map_default, map, mapSize, data.channels);
    return MA_SUCCESS;
}

ma_result Channel::SliceSource::position(ma_data_source* source, ma_uint64* cursor) noexcept
{
    auto& ss = *static_cast<SliceSource*>(source);
    return ma_decoder_get_cursor_in_pcm_frames(&ss.decoder, cursor);
}

ma_result Channel::SliceSource::length(ma_data_source* source, ma_uint64* len) noexcept
{
    *len = static_cast<SliceSource*>(source)->data->frames;
    return MA_SUCCESS;
}

ma_result Channel::PcmSource::length(ma_data_source* source, ma_uint64* frames) noexcept
{
    *frames = static_cast<PcmSource*>(source)->data->frames;
    return MA_SUCCESS;
}

}
