#pragma once

#include "studio_model.h"
#include <oae_audio.h>
#include <miniaudio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace oae {

class System {
public:
    System() = default;
    ~System();
    System(const System&) = delete;
    System& operator=(const System&) = delete;

    oae_result init(const oae_init_settings& settings) noexcept;
    oae_result update() const noexcept;
    oae_result render(float* out, std::uint32_t frames) noexcept;
    ma_engine* engine() noexcept { return &engine_; }
    void shutdown() noexcept;
    std::uint64_t mix_frame() const noexcept { return mixFrame_; }
    std::uint64_t previous_mix_frame() const noexcept { return previousMixFrame_; }
    std::uint32_t channels() const noexcept { return ma_engine_get_channels(&engine_); }
    std::uint64_t clock() const noexcept { return clock_.load(std::memory_order_acquire); }
    std::uint32_t sample_rate() const noexcept { return ma_engine_get_sample_rate(&engine_); }
    std::uint32_t max_voices() const noexcept { return maxVoices_; }
    oae_bus master_bus() const noexcept { return master_; }
    void set_master_bus(oae_bus handle) noexcept { master_ = handle; }
    oae_system handle() const noexcept { return handle_; }
    void set_handle(oae_system h) noexcept { handle_ = h; }

    void register_model_set(std::shared_ptr<const class ModelSet> set);
    void unregister_model_set(const std::shared_ptr<const class ModelSet>& set);
    const struct EventDescriptionModel* find_event_model(const std::string& path, std::shared_ptr<const class ModelSet>& out_set) const noexcept;
    const struct EventDescriptionModel* find_event_model_by_guid(const Guid& guid, std::shared_ptr<const class ModelSet>& out_set) const noexcept;
    void add_active_instance(class EventInstance* inst);
    void remove_active_instance(class EventInstance* inst);
    const std::vector<class EventInstance*>& active_instances() const noexcept { return activeInstances_; }
    void update_instances(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept;

    oae_result set_global_parameter(const std::string& name, float value, bool ignore_seek_speed);
    oae_result set_global_parameter_with_label(const std::string& name, const std::string& label, bool ignore_seek_speed);
    oae_result get_global_parameter(const std::string& name, float* out_value, float* out_final_value) const noexcept;
    bool find_global_parameter_value(const std::string& name, float& out_value) const noexcept;
    void register_global_parameter(const ParameterDefinition& def);
    const std::unordered_map<std::string, ParameterInstance>& global_parameters() const noexcept { return globalParameters_; }

    void set_listener_3d_attributes(const oae_3d_attributes& attrs) noexcept;
    const oae_3d_attributes& listener_3d_attributes() const noexcept { return listenerAttributes_; }

    oae_result find_vca(const std::string& path, oae_vca* out_vca) const noexcept;
    void update_vca_multipliers() noexcept;

    void invalidate_bus_cache() noexcept { ++busEpoch_; }
    oae_bus lookup_bus_cached(const std::string& path) const noexcept;

    class CallbackQueue& callback_queue() noexcept;
    void dispatch_callbacks() noexcept;

    bool in_callback() const noexcept { return in_callback_; }
    void set_in_callback(bool v) noexcept { in_callback_ = v; }

    void register_bank(class Bank* bank);
    void unregister_bank(class Bank* bank);
    class Bank* find_bank_for_model_set(const class ModelSet* ms) const noexcept;
    bool owns_bank_bus(const std::string& path) const noexcept;

private:
    friend class Bank;
    friend struct BankBus;
    ma_result mix(float* out, ma_uint32 frames, bool device) noexcept;
    static void render_device(ma_device* device, void* output, const void*, ma_uint32 frames) noexcept;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> clock_{0};
    std::uint64_t mixFrame_ = UINT64_MAX;
    std::uint64_t previousMixFrame_ = UINT64_MAX;
    std::uint64_t lastInstanceUpdateClock_ = 0;
    std::uint32_t busEpoch_ = 1;
    mutable std::unordered_map<std::string, std::pair<std::uint32_t, oae_bus>> busLookupCache_;
    std::uint32_t maxVoices_ = 0;
    ma_engine engine_{};
    oae_system handle_ = 0;
    oae_bus master_ = 0;
    std::vector<std::shared_ptr<const class ModelSet>> modelSets_;
    std::vector<class EventInstance*> activeInstances_;
    std::unordered_map<std::string, ParameterInstance> globalParameters_;
    oae_3d_attributes listenerAttributes_{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    std::unordered_map<std::string, oae_vca> vcaHandles_;
    mutable std::unordered_map<std::string, float> lastSnapshotBusDb_;
    std::unique_ptr<class CallbackQueue> callbackQueue_;
    std::vector<class Bank*> registeredBanks_;
    std::unordered_map<std::string, std::weak_ptr<struct BankBus>> bankMixer_;
    bool in_callback_ = false;
    bool initialized_ = false;
    bool headless_ = false;
};

}
