#include "sound.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>

namespace oae {
namespace {

class Decoder {
public:
    ~Decoder()
    {
        if (active) {
            ma_decoder_uninit(&value);
        }
    }

    ma_decoder value{};
    bool active = false;
};

oae_result read_sound(Decoder& decoder, ma_result result, bool stream, SoundData& output)
{
    if (result != MA_SUCCESS) {
        return sound_error(result);
    }
    decoder.active = true;
    result = ma_decoder_get_data_format(&decoder.value, nullptr, &output.channels, &output.rate, nullptr, 0);
    if (result != MA_SUCCESS || output.channels == 0 || output.channels > MA_MAX_CHANNELS || output.rate == 0) {
        return OAE_ERR_FORMAT;
    }
    if (stream) {
        result = ma_decoder_get_length_in_pcm_frames(&decoder.value, &output.frames);
        if (result == MA_SUCCESS && output.frames > 0) {
            return OAE_OK;
        }
        if (result != MA_SUCCESS && result != MA_NOT_IMPLEMENTED) {
            return sound_error(result);
        }
        output.frames = 0;
    }

    std::array<float, 4096> block{};
    const auto blockFrames = block.size() / output.channels;
    for (;;) {
        ma_uint64 framesRead = 0;
        result = ma_decoder_read_pcm_frames(&decoder.value, block.data(), blockFrames, &framesRead);
        if (result != MA_SUCCESS && result != MA_AT_END) {
            return sound_error(result);
        }
        const auto samplesRead = static_cast<std::size_t>(framesRead) * output.channels;
        if (!stream) {
            if (samplesRead > output.samples.max_size() - output.samples.size()) {
                return OAE_ERR_OUT_OF_MEMORY;
            }
            output.samples.insert(output.samples.end(), block.data(), block.data() + samplesRead);
        }
        output.frames += framesRead;
        if (framesRead == 0 || result == MA_AT_END) {
            return output.frames > 0 ? OAE_OK : OAE_ERR_FORMAT;
        }
    }
}

}

oae_result sound_error(ma_result result) noexcept
{
    switch (result) {
        case MA_SUCCESS: return OAE_OK;
        case MA_OUT_OF_MEMORY: return OAE_ERR_OUT_OF_MEMORY;
        case MA_DOES_NOT_EXIST: return OAE_ERR_FILE_NOT_FOUND;
        default: return OAE_ERR_FORMAT;
    }
}

oae_result load_file(const std::filesystem::path& path, bool stream, SoundData& output)
{
    Decoder decoder;
    const auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
#if defined(_WIN32)
    const auto result = ma_decoder_init_file_w(path.c_str(), &config, &decoder.value);
#else
    const auto result = ma_decoder_init_file(path.c_str(), &config, &decoder.value);
#endif
    const auto status = read_sound(decoder, result, stream, output);
    if (status == OAE_OK && stream) {
        output.streamPath = path;
    }
    return status;
}

namespace {
struct SliceReader {
    std::ifstream file;
    std::uint64_t base = 0;
    std::uint64_t length = 0;
    std::uint64_t pos = 0;
};

ma_result slice_on_read(ma_decoder* dec, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead)
{
    auto* ctx = static_cast<SliceReader*>(dec->pUserData);
    if (!ctx || ctx->pos >= ctx->length) {
        if (pBytesRead) *pBytesRead = 0;
        return MA_AT_END;
    }
    const std::uint64_t avail = ctx->length - ctx->pos;
    const size_t n = static_cast<size_t>(std::min<std::uint64_t>(bytesToRead, avail));
    ctx->file.read(static_cast<char*>(pBufferOut), static_cast<std::streamsize>(n));
    const size_t got = static_cast<size_t>(ctx->file.gcount());
    ctx->pos += got;
    if (pBytesRead) *pBytesRead = got;
    return got > 0 ? MA_SUCCESS : MA_AT_END;
}

ma_result slice_on_seek(ma_decoder* dec, ma_int64 byteOffset, ma_seek_origin origin)
{
    auto* ctx = static_cast<SliceReader*>(dec->pUserData);
    if (!ctx) return MA_ERROR;
    std::int64_t target = 0;
    if (origin == ma_seek_origin_start) target = byteOffset;
    else if (origin == ma_seek_origin_current) target = static_cast<std::int64_t>(ctx->pos) + byteOffset;
    else if (origin == ma_seek_origin_end) target = static_cast<std::int64_t>(ctx->length) + byteOffset;
    if (target < 0 || static_cast<std::uint64_t>(target) > ctx->length) return MA_ERROR;
    ctx->pos = static_cast<std::uint64_t>(target);
    ctx->file.clear();
    ctx->file.seekg(static_cast<std::streamoff>(ctx->base + ctx->pos), std::ios::beg);
    return ctx->file ? MA_SUCCESS : MA_ERROR;
}
}

oae_result load_bank_slice(const std::filesystem::path& path, std::uint64_t offset, std::uint64_t length, SoundData& output)
{
    SliceReader ctx;
    ctx.file.open(path, std::ios::binary);
    if (!ctx.file.is_open()) {
        return OAE_ERR_FILE_NOT_FOUND;
    }
    ctx.base = offset;
    ctx.length = length;
    ctx.file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!ctx.file) {
        return OAE_ERR_FORMAT;
    }

    Decoder decoder;
    const auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
    const auto result = ma_decoder_init(slice_on_read, slice_on_seek, &ctx, &config, &decoder.value);
    const auto status = read_sound(decoder, result, true, output);
    if (status == OAE_OK) {
        output.streamPath = path;
        output.streamOffset = offset;
        output.streamLength = length;
    }
    return status;
}

oae_result load_memory(const void* bytes, std::size_t size, SoundData& output)
{
    Decoder decoder;
    const auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
    const auto result = ma_decoder_init_memory(bytes, size, &config, &decoder.value);
    return read_sound(decoder, result, false, output);
}

std::uint32_t frames_to_ms(ma_uint64 frames, ma_uint32 rate) noexcept
{
    constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
    const auto seconds = frames / rate;
    if (seconds > maximum / 1000u) {
        return maximum;
    }
    const auto ms = seconds * 1000u + (frames % rate) * 1000u / rate;
    return ms > maximum ? maximum : static_cast<std::uint32_t>(ms);
}

}
