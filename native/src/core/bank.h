#pragma once

#include <oae_audio.h>
#include "studio_model.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace oae {

class System;
struct BankBus;
struct BankParseResult;
struct BusEntry;

struct BankSampleEntry {
    std::vector<uint8_t> data;
    uint64_t length_frames = 0;
    uint32_t sample_rate = 48000;
    oae_sound sound = 0;
    int32_t pin_count = 0;
    bool stream = false;
    std::filesystem::path stream_path;
    uint64_t stream_offset = 0;
    uint64_t stream_length = 0;
};

class Bank {
public:
    Bank(System& owner, std::shared_ptr<const ModelSet> model_set, std::vector<BankSampleEntry> samples, bool nonblocking);
    ~Bank();

    Bank(const Bank&) = delete;
    Bank& operator=(const Bank&) = delete;

    System* owner() const noexcept { return owner_; }
    std::shared_ptr<const ModelSet> model_set() const noexcept { return model_set_; }
    void set_model_set(std::shared_ptr<const ModelSet> set) noexcept;

    void unload() noexcept;

    int loading_state() const noexcept { return loading_state_.load(std::memory_order_acquire); }
    void set_loading_state(int s) noexcept { loading_state_.store(s, std::memory_order_release); }
    bool is_nonblocking() const noexcept { return nonblocking_; }

    bool is_sample_pinned(int32_t sample_idx) const noexcept;
    oae_sound get_sample_sound(int32_t sample_idx) const noexcept;
    oae_result pin_sample(int32_t sample_idx);
    oae_result unpin_sample(int32_t sample_idx);

    void set_samples(std::vector<BankSampleEntry> samples);
    void set_async_worker(std::thread t);
    void wait_async_load();
    /** Installs or shares the authored mixer on the game thread. */
    oae_result attach_mixer(const std::vector<BusEntry>& buses);
    /** Hands parsed data from the background reader to the game thread. */
    void prepare_async(BankParseResult parsed);
    /** Activates a finished background load during a loading-state query. */
    void complete_async();

private:
    System* owner_;
    std::shared_ptr<const ModelSet> model_set_;
    std::vector<BankSampleEntry> samples_;
    std::atomic<int> loading_state_{OAE_BANK_LOADED};
    bool nonblocking_ = false;
    bool unloaded_ = false;
    mutable std::mutex mutex_;
    std::thread async_worker_;
    std::unique_ptr<BankParseResult> prepared_;
    std::atomic<bool> prepared_ready_{false};
    std::vector<std::shared_ptr<BankBus>> mixer_;
};

}
