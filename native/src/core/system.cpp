#include "system.h"
#include "studio_model.h"
#include "event_instance.h"
#include "effect.h"
#include "vca.h"
#include "bank.h"
#include "../api/runtime.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_set>

namespace oae {

System::~System()
{
    shutdown();
}

void System::shutdown() noexcept
{
    if (initialized_) {
        for (const auto& kv : vcaHandles_) {
            runtime.vcas.erase(kv.second);
        }
        vcaHandles_.clear();
        lastSnapshotBusDb_.clear();

        for (auto* inst : activeInstances_) {
            if (inst) {
                inst->stop(OAE_STOP_IMMEDIATE);
            }
        }
        activeInstances_.clear();
        modelSets_.clear();
        callbackQueue_.reset();

        ma_engine_uninit(&engine_);
        initialized_ = false;
        headless_ = false;
        master_ = 0;
    }
}

oae_result System::init(const oae_init_settings& settings) noexcept
{
    if (initialized_) {
        return OAE_ERR_ALREADY_INITIALIZED;
    }
    auto config = ma_engine_config_init();
    config.sampleRate = settings.sample_rate;
    config.channels = settings.channels;
    config.noDevice = (settings.flags & OAE_INIT_NO_DEVICE) != 0;
    config.dataCallback = render_device;
    config.pProcessUserData = this;
    clock_.store(0, std::memory_order_relaxed);
    mixFrame_ = UINT64_MAX;
    lastInstanceUpdateClock_ = 0;
    listenerAttributes_ = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}
    };
    const auto result = ma_engine_init(&config, &engine_);
    if (result != MA_SUCCESS) {
        return result == MA_OUT_OF_MEMORY ? OAE_ERR_OUT_OF_MEMORY : OAE_ERR_DEVICE;
    }
    maxVoices_ = settings.max_physical_voices;
    initialized_ = true;
    headless_ = config.noDevice != 0;
    return OAE_OK;
}

oae_result System::update() const noexcept
{
    if (in_callback_) {
        return OAE_ERR_INVALID_STATE;
    }
    return initialized_ ? OAE_OK : OAE_ERR_NOT_INITIALIZED;
}

void System::register_bank(Bank* bank)
{
    if (bank && std::find(registeredBanks_.begin(), registeredBanks_.end(), bank) == registeredBanks_.end()) {
        registeredBanks_.push_back(bank);
    }
}

void System::unregister_bank(Bank* bank)
{
    const auto it = std::find(registeredBanks_.begin(), registeredBanks_.end(), bank);
    if (it != registeredBanks_.end()) {
        registeredBanks_.erase(it);
    }
}

Bank* System::find_bank_for_model_set(const ModelSet* ms) const noexcept
{
    if (!ms) return nullptr;
    for (auto* b : registeredBanks_) {
        if (b && b->model_set().get() == ms) {
            return b;
        }
    }
    return nullptr;
}

void System::register_model_set(std::shared_ptr<const ModelSet> set)
{
    if (set && std::find(modelSets_.begin(), modelSets_.end(), set) == modelSets_.end()) {
        for (const auto& gp : set->global_parameters()) {
            register_global_parameter(gp);
        }
        for (const auto& vm : set->vcas()) {
            if (vcaHandles_.find(vm.path) == vcaHandles_.end()) {
                auto vca_obj = std::make_unique<Vca>(*this, vm.path, vm.bus_paths, vm.event_paths, vm.default_volume);
                const auto h = runtime.vcas.insert(std::move(vca_obj));
                if (h != 0) {
                    vcaHandles_[vm.path] = h;
                }
            }
        }
        modelSets_.push_back(std::move(set));
        update_vca_multipliers();
    }
}

void System::unregister_model_set(const std::shared_ptr<const ModelSet>& set)
{
    const auto it = std::find(modelSets_.begin(), modelSets_.end(), set);
    if (it != modelSets_.end()) {
        modelSets_.erase(it);
    }
}

const EventDescriptionModel* System::find_event_model(const std::string& path, std::shared_ptr<const ModelSet>& out_set) const noexcept
{
    for (const auto& set : modelSets_) {
        if (set) {
            const auto* ev = set->find_event_by_path(path);
            if (ev) {
                out_set = set;
                return ev;
            }
        }
    }
    return nullptr;
}

const EventDescriptionModel* System::find_event_model_by_guid(const Guid& guid, std::shared_ptr<const ModelSet>& out_set) const noexcept
{
    for (const auto& set : modelSets_) {
        if (set) {
            const auto* ev = set->find_event_by_guid(guid);
            if (ev) {
                out_set = set;
                return ev;
            }
        }
    }
    return nullptr;
}

void System::register_global_parameter(const ParameterDefinition& def)
{
    if (globalParameters_.find(def.name) == globalParameters_.end()) {
        globalParameters_.emplace(def.name, ParameterInstance(def));
    }
}

oae_result System::set_global_parameter(const std::string& name, float value, bool ignore_seek_speed)
{
    auto it = globalParameters_.find(name);
    if (it == globalParameters_.end()) {
        for (const auto& set : modelSets_) {
            if (set) {
                const auto* def = set->find_global_parameter(name);
                if (def) {
                    it = globalParameters_.emplace(name, ParameterInstance(*def)).first;
                    break;
                }
            }
        }
    }
    if (it == globalParameters_.end()) {
        ParameterDefinition def = ParameterDefinition::continuous(name, -1e9f, 1e9f, value);
        it = globalParameters_.emplace(name, ParameterInstance(def)).first;
    }
    it->second.set_value(value, ignore_seek_speed);
    for (auto* inst : activeInstances_) {
        if (inst && inst->is_active()) {
            inst->evaluate_parameter_sheets();
        }
    }
    return OAE_OK;
}

oae_result System::set_global_parameter_with_label(const std::string& name, const std::string& label, bool ignore_seek_speed)
{
    auto it = globalParameters_.find(name);
    if (it == globalParameters_.end()) {
        for (const auto& set : modelSets_) {
            if (set) {
                const auto* def = set->find_global_parameter(name);
                if (def) {
                    it = globalParameters_.emplace(name, ParameterInstance(*def)).first;
                    break;
                }
            }
        }
    }
    if (it == globalParameters_.end()) {
        return OAE_ERR_INVALID_PARAM;
    }
    const auto res = it->second.set_value_with_label(label, ignore_seek_speed);
    if (res == OAE_OK) {
        for (auto* inst : activeInstances_) {
            if (inst && inst->is_active()) {
                inst->evaluate_parameter_sheets();
            }
        }
    }
    return res;
}

oae_result System::get_global_parameter(const std::string& name, float* out_value, float* out_final_value) const noexcept
{
    const auto it = globalParameters_.find(name);
    if (it == globalParameters_.end()) {
        return OAE_ERR_INVALID_PARAM;
    }
    if (out_value) *out_value = it->second.current_value();
    if (out_final_value) *out_final_value = it->second.target_value();
    return OAE_OK;
}

bool System::find_global_parameter_value(const std::string& name, float& out_value) const noexcept
{
    const auto it = globalParameters_.find(name);
    if (it != globalParameters_.end()) {
        out_value = it->second.current_value();
        return true;
    }
    return false;
}

void System::add_active_instance(EventInstance* inst)
{
    if (inst && std::find(activeInstances_.begin(), activeInstances_.end(), inst) == activeInstances_.end()) {
        activeInstances_.push_back(inst);
    }
}

void System::remove_active_instance(EventInstance* inst)
{
    const auto it = std::find(activeInstances_.begin(), activeInstances_.end(), inst);
    if (it != activeInstances_.end()) {
        activeInstances_.erase(it);
    }
}

void System::set_listener_3d_attributes(const oae_3d_attributes& attrs) noexcept
{
    listenerAttributes_ = attrs;
    for (auto* inst : activeInstances_) {
        if (inst) {
            inst->refresh_3d_state();
        }
    }
}

oae_bus System::lookup_bus_cached(const std::string& path) const noexcept
{
    const auto it = busLookupCache_.find(path);
    if (it != busLookupCache_.end() && it->second.first == busEpoch_) {
        return it->second.second;
    }
    oae_bus bus_h = 0;
    if (handle_ != 0 && oae_system_get_bus(handle_, path.c_str(), &bus_h) == OAE_OK) {
        busLookupCache_[path] = {busEpoch_, bus_h};
        return bus_h;
    }
    busLookupCache_[path] = {busEpoch_, 0};
    return 0;
}

oae_result System::find_vca(const std::string& path, oae_vca* out_vca) const noexcept
{
    const auto it = vcaHandles_.find(path);
    if (it != vcaHandles_.end()) {
        if (out_vca) *out_vca = it->second;
        return OAE_OK;
    }
    return OAE_ERR_NOT_FOUND;
}

void System::update_vca_multipliers() noexcept
{
    std::unordered_map<std::string, float> busVcaGains;
    for (const auto& kv : vcaHandles_) {
        auto* vca = runtime.vcas.get(kv.second);
        if (!vca) continue;
        const float vol = vca->volume();
        for (const auto& bpath : vca->bus_paths()) {
            auto it = busVcaGains.find(bpath);
            if (it == busVcaGains.end()) {
                busVcaGains[bpath] = vol;
            } else {
                it->second *= vol;
            }
        }
    }
    for (const auto& kv : busVcaGains) {
        const auto bus_h = lookup_bus_cached(kv.first);
        if (bus_h != 0) {
            auto* b = runtime.buses.get(bus_h);
            if (b) {
                b->set_vca_gain(kv.second);
            }
        }
    }
}

void System::update_instances(std::uint64_t dsp_clock, std::uint32_t sample_rate) noexcept
{
    const std::uint64_t delta_frames = (dsp_clock >= lastInstanceUpdateClock_) ? (dsp_clock - lastInstanceUpdateClock_) : 0;
    const double delta_sec = static_cast<double>(delta_frames) / static_cast<double>(sample_rate > 0 ? sample_rate : 48000);
    lastInstanceUpdateClock_ = dsp_clock;

    for (auto& kv : globalParameters_) {
        kv.second.advance_time(delta_sec);
    }

    // Effect Automations
    for (const auto& set : modelSets_) {
        if (!set) continue;
        for (const auto& ea : set->effect_automations()) {
            float val = 0.0f;
            if (find_global_parameter_value(ea.param.name, val)) {
                const float effect_val = curve_eval(ea.curve, val);
                const auto bus_h = lookup_bus_cached(ea.bus_path);
                if (bus_h != 0) {
                    auto* bus = runtime.buses.get(bus_h);
                    if (bus) {
                        auto* eff = bus->effect_at(ea.effect_index);
                        if (eff) {
                            eff->set_parameter(ea.param_index, effect_val);
                        }
                    }
                }
            }
        }
    }

    for (std::size_t i = 0; i < activeInstances_.size(); ) {
        auto* inst = activeInstances_[i];
        if (inst) {
            inst->update(dsp_clock, sample_rate);
            if (!inst->is_active()) {
                activeInstances_.erase(activeInstances_.begin() + i);
                continue;
            }
        }
        ++i;
    }

    // Snapshots Processing
    struct ActiveSnapshot {
        const EventDescriptionModel* model = nullptr;
        float weight = 0.0f;
    };
    std::vector<ActiveSnapshot> active_snapshots;

    for (auto* inst : activeInstances_) {
        if (!inst || !inst->description()) continue;
        const auto* m = inst->description()->model();
        if (!m || m->kind != EventKind::Snapshot) continue;

        float f_env = 1.0f;
        const auto state = inst->playback_state();

        if (state == OAE_PLAYBACK_PLAYING) {
            if (m->fade_in_ms > 0) {
                const uint64_t elapsed_frames = (dsp_clock >= inst->start_dsp_clock()) ? (dsp_clock - inst->start_dsp_clock()) : 0;
                const uint64_t fade_frames = (static_cast<uint64_t>(m->fade_in_ms) * static_cast<uint64_t>(sample_rate > 0 ? sample_rate : 48000)) / 1000;
                if (fade_frames > 0) {
                    f_env = std::min(1.0f, static_cast<float>(elapsed_frames) / static_cast<float>(fade_frames));
                }
            }
        } else if (state == OAE_PLAYBACK_STOPPING) {
            if (m->stop_fadeout_ms > 0) {
                const uint64_t stopping_elapsed = (dsp_clock >= inst->stop_fadeout_start_clock()) ? (dsp_clock - inst->stop_fadeout_start_clock()) : 0;
                const uint64_t fadeout_frames = (static_cast<uint64_t>(m->stop_fadeout_ms) * static_cast<uint64_t>(sample_rate > 0 ? sample_rate : 48000)) / 1000;
                if (fadeout_frames > 0) {
                    f_env = std::max(0.0f, 1.0f - static_cast<float>(stopping_elapsed) / static_cast<float>(fadeout_frames));
                } else {
                    f_env = 0.0f;
                }
            } else {
                f_env = 0.0f;
            }
        } else {
            f_env = 0.0f;
        }

        float intensity = 1.0f;
        if (!m->intensity_param.empty()) {
            float param_val = 100.0f;
            if (inst->get_parameter(m->intensity_param, &param_val, nullptr) == OAE_OK) {
                intensity = std::clamp(param_val, 0.0f, 100.0f) / 100.0f;
            }
        }

        const float total_w = f_env * intensity;
        if (total_w > 0.0f) {
            active_snapshots.push_back({m, total_w});
        }
    }

    std::sort(active_snapshots.begin(), active_snapshots.end(), [](const ActiveSnapshot& a, const ActiveSnapshot& b) {
        return a.model->priority < b.model->priority;
    });

    std::unordered_map<std::string, float> busDeltasDb;
    std::unordered_map<std::string, bool> busHasOverride;
    std::unordered_map<std::string, float> busOverrideDelta;

    for (const auto& snap : active_snapshots) {
        for (const auto& target : snap.model->snapshot_targets) {
            if (target.param_id == -1) {
                const float weightedDelta = target.value * snap.weight;
                if (snap.model->snapshot_mode == SnapshotMode::Override) {
                    busHasOverride[target.bus_path] = true;
                    busOverrideDelta[target.bus_path] = weightedDelta;
                } else {
                    busDeltasDb[target.bus_path] += weightedDelta;
                }
            }
        }
    }

    std::unordered_set<std::string> busesToUpdate;
    for (const auto& kv : busDeltasDb) busesToUpdate.insert(kv.first);
    for (const auto& kv : busOverrideDelta) busesToUpdate.insert(kv.first);
    for (const auto& kv : lastSnapshotBusDb_) busesToUpdate.insert(kv.first);

    for (const auto& bpath : busesToUpdate) {
        float final_db = 0.0f;
        if (busHasOverride[bpath]) {
            final_db = busOverrideDelta[bpath] + busDeltasDb[bpath];
        } else {
            final_db = busDeltasDb[bpath];
        }

        const auto bus_h = lookup_bus_cached(bpath);
        if (bus_h != 0) {
            auto* b = runtime.buses.get(bus_h);
            if (b) {
                const float linear_gain = (final_db <= -80.0f) ? 0.0f : db_to_linear(final_db);
                b->set_snapshot_gain(linear_gain);
            }
        }
        if (final_db != 0.0f) {
            lastSnapshotBusDb_[bpath] = final_db;
        } else {
            lastSnapshotBusDb_.erase(bpath);
        }
    }
}

CallbackQueue& System::callback_queue() noexcept
{
    if (!callbackQueue_) {
        callbackQueue_ = std::make_unique<CallbackQueue>();
    }
    return *callbackQueue_;
}

void System::dispatch_callbacks() noexcept
{
    if (callbackQueue_) {
        in_callback_ = true;
        callbackQueue_->dispatch();
        in_callback_ = false;
    }
}

oae_result System::render(float* out, std::uint32_t frames) noexcept
{
    if (in_callback_) {
        return OAE_ERR_INVALID_STATE;
    }
    if (!initialized_) {
        return OAE_ERR_NOT_INITIALIZED;
    }
    if (!headless_) {
        return OAE_ERR_DEVICE;
    }
    if (frames == 0) {
        return OAE_OK;
    }
    const auto maxSamples = (std::numeric_limits<std::size_t>::max)() / sizeof(float);
    if (frames > maxSamples / ma_engine_get_channels(&engine_)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return mix(out, frames, false) == MA_SUCCESS ? OAE_OK : OAE_ERR_DEVICE;
}

ma_result System::mix(float* out, ma_uint32 frames, bool device) noexcept
{
    const auto channels = ma_engine_get_channels(&engine_);
    auto now = clock_.load(std::memory_order_relaxed);
    while (frames != 0) {
        const auto count = std::min<ma_uint32>(frames, 256);
        previousMixFrame_ = mixFrame_;
        mixFrame_ = now;
        if (ma_engine_get_time_in_pcm_frames(&engine_) != now) {
            ma_engine_set_time_in_pcm_frames(&engine_, now);
        }
        const auto result = ma_engine_read_pcm_frames(&engine_, out, count, nullptr);
        if (result != MA_SUCCESS) {
            if (device) {
                std::fill_n(out, static_cast<std::size_t>(frames) * channels, 0.0f);
                clock_.store(now + frames, std::memory_order_release);
            }
            return result;
        }
        now += count;
        clock_.store(now, std::memory_order_release);
        frames -= count;
        out += static_cast<std::size_t>(count) * channels;
    }
    return MA_SUCCESS;
}

void System::render_device(ma_device* device, void* output, const void*, ma_uint32 frames) noexcept
{
    auto* engine = static_cast<ma_engine*>(device->pUserData);
    auto& system = *static_cast<System*>(engine->pProcessUserData);
    static_cast<void>(system.mix(static_cast<float*>(output), frames, true));
}

}
