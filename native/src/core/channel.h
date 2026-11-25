#pragma once

#include "sound.h"
#include "strip.h"

#include <atomic>
#include <fstream>

namespace oae {

class Bus;

class Channel {
public:
    Channel(Bus& bus, std::shared_ptr<const SoundData> data, std::uint64_t order, std::uint64_t start) noexcept;
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    oae_result init() noexcept;
    System* owner() const noexcept;
    const Bus& bus() const noexcept { return *bus_; }
    bool finished() const noexcept;
    bool finished_at(std::uint64_t now) const noexcept;
    bool physical() const noexcept { return physical_; }
    oae_result set_physical(bool physical, std::uint64_t now) noexcept;
    void sync_virtual(std::uint64_t now) noexcept;
    double audibility() const noexcept;
    int priority() const noexcept { return priority_; }
    void set_priority(int value) noexcept { priority_ = value; }
    std::uint64_t order() const noexcept { return order_; }
    bool paused() const noexcept;
    bool playing() const noexcept;
    oae_result set_paused(bool paused) noexcept;
    oae_result stop() noexcept;
    oae_result stop_with_fade(std::uint64_t duration) noexcept;
    void set_ahdsr(const AhdsrSpec& spec) noexcept;
    bool has_ahdsr() const noexcept { return hasAhdsr_; }
    void trigger_ahdsr_release(std::uint64_t rel_frame) noexcept;
    void set_loop(bool loop) noexcept;
    bool is_loop() const noexcept { return loop_; }
    StripNode& strip() noexcept { return strip_; }
    const StripNode& strip() const noexcept { return strip_; }
    void set_volume(float volume) noexcept;
    float volume() const noexcept;
    oae_result set_pitch(float pitch) noexcept;
    float pitch() const noexcept;
    oae_result set_position_ms(std::uint32_t position) noexcept;
    oae_result position_ms(std::uint32_t& position) const noexcept;

private:
    struct PcmSource {
        ma_data_source_base base{};
        const SoundData* data = nullptr;
        std::atomic<ma_uint64> cursor{0};

        static ma_result read(ma_data_source*, void*, ma_uint64, ma_uint64*) noexcept;
        static ma_result seek(ma_data_source*, ma_uint64) noexcept;
        static ma_result format(ma_data_source*, ma_format*, ma_uint32*, ma_uint32*, ma_channel*, size_t) noexcept;
        static ma_result position(ma_data_source*, ma_uint64*) noexcept;
        static ma_result length(ma_data_source*, ma_uint64*) noexcept;
    };

    struct SliceSource {
        ma_data_source_base base{};
        const SoundData* data = nullptr;
        std::ifstream file;
        std::uint64_t pos = 0;
        ma_decoder decoder{};
        bool decoderReady = false;

        static ma_result read(ma_data_source*, void*, ma_uint64, ma_uint64*) noexcept;
        static ma_result seek(ma_data_source*, ma_uint64) noexcept;
        static ma_result format(ma_data_source*, ma_format*, ma_uint32*, ma_uint32*, ma_channel*, size_t) noexcept;
        static ma_result position(ma_data_source*, ma_uint64*) noexcept;
        static ma_result length(ma_data_source*, ma_uint64*) noexcept;
    };

    double virtual_cursor(std::uint64_t now) const noexcept;

    Bus* bus_;
    std::shared_ptr<const SoundData> data_;
    PcmSource source_;
    SliceSource sliceSource_;
    ma_sound sound_{};
    StripNode strip_;
    ma_uint64 frames_ = 0;
    std::uint64_t order_;
    std::uint64_t startClock_;
    std::uint64_t stopClock_ = 0;
    bool fadeStop_ = false;
    std::uint64_t virtualSince_ = 0;
    double virtualFrame_ = 0.0;
    int priority_ = 128;
    bool physical_ = false;
    ma_uint32 rate_ = 0;
    bool sourceReady_ = false;
    bool sliceReady_ = false;
    bool soundReady_ = false;
    bool paused_ = true;
    bool loop_ = false;
    bool stopped_ = false;
    bool hasAhdsr_ = false;
    AhdsrSpec ahdsr_{};
    std::uint64_t ahdsrRel_ = UINT64_MAX;
};

}
