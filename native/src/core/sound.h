#pragma once

#include <oae_audio.h>
#include <miniaudio.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace oae {

class System;

struct SoundData {
    std::vector<float> samples;
    std::filesystem::path streamPath;
    std::uint64_t streamOffset = 0;
    std::uint64_t streamLength = 0;
    ma_uint64 frames = 0;
    ma_uint32 rate = 0;
    ma_uint32 channels = 0;
    bool loop = false;
};

struct Sound {
    System* owner;
    std::shared_ptr<const SoundData> data;
};

oae_result load_file(const std::filesystem::path& path, bool stream, SoundData& output);
oae_result load_bank_slice(const std::filesystem::path& path, std::uint64_t offset, std::uint64_t length, SoundData& output);
oae_result load_memory(const void* bytes, std::size_t size, SoundData& output);
oae_result sound_error(ma_result result) noexcept;
std::uint32_t frames_to_ms(ma_uint64 frames, ma_uint32 rate) noexcept;

}
