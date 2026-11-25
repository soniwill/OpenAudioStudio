#include "bank.h"
#include "sound.h"
#include "system.h"
#include "../api/runtime.h"
#include "../studio/bank_reader.h"

#include <algorithm>
#include <cmath>

namespace oae {

/** Reference-counted mixer bus installed from a bank definition. */
struct BankBus {
    System* system = nullptr;
    oae_bus handle = 0;
    std::string path;
    std::string parent_path;
    std::shared_ptr<BankBus> parent;
    float volume = 1;
    float original_volume = 1;
    std::vector<std::pair<BusEntry::EffectDesc, std::string>> effects;
    std::vector<oae_effect> owned_effects;

    ~BankBus()
    {
        if (!handle) return;
        if (path == "bus:/") {
            for (const auto effect : owned_effects) oae_effect_release(effect);
            if (auto* bus = runtime.buses.get(handle)) bus->set_volume(original_volume);
        } else {
            runtime.destroy_bus_tree(handle);
        }
        const auto it = system->bankMixer_.find(path);
        if (it != system->bankMixer_.end() && it->second.expired()) system->bankMixer_.erase(it);
    }
};

bool System::owns_bank_bus(const std::string& path) const noexcept
{
    const auto it = bankMixer_.find(path);
    return it != bankMixer_.end() && !it->second.expired();
}

oae_result Bank::attach_mixer(const std::vector<BusEntry>& buses)
{
    if (buses.empty()) return OAE_OK;
    const auto initialized = owner_->update();
    if (initialized != OAE_OK) return initialized;

    std::vector<std::shared_ptr<BankBus>> installed;
    std::vector<std::shared_ptr<BankBus>> created;
    installed.reserve(buses.size());
    created.reserve(buses.size());
    for (std::size_t i = 0; i < buses.size(); ++i) {
        const auto& authored = buses[i];
        std::string norm_name = authored.name;
        if (norm_name == "master") norm_name = "bus:/";
        else if (norm_name.rfind("bus:/", 0) != 0) norm_name = "bus:/" + norm_name;

        const bool master = (norm_name == "bus:/");
        if (authored.parent_index < -1 ||
            (master && authored.parent_index != -1) ||
            (authored.parent_index >= 0 && static_cast<std::size_t>(authored.parent_index) >= i) ||
            !std::isfinite(authored.volume) || authored.volume < 0) return OAE_ERR_FORMAT;

        // Content banks with only a default empty root bus definition perform no mixer mutation
        if (master && buses.size() == 1 && authored.effects.empty() && authored.volume == 1.0f) {
            return OAE_OK;
        }

        auto entry = std::make_shared<BankBus>();
        entry->system = owner_;
        entry->path = norm_name;
        entry->volume = authored.volume;
        if (!master) {
            entry->parent_path = "bus:/";
            if (authored.parent_index >= 0) {
                entry->parent = installed[static_cast<std::size_t>(authored.parent_index)];
                entry->parent_path = entry->parent->path;
            }
        }
        for (const auto& effect : authored.effects) {
            if (effect.sidechain_bus < -1 || (effect.sidechain_bus >= 0 &&
                static_cast<std::size_t>(effect.sidechain_bus) >= buses.size())) return OAE_ERR_FORMAT;
            std::string source;
            if (effect.sidechain_bus >= 0) {
                source = buses[static_cast<std::size_t>(effect.sidechain_bus)].name;
                if (source == "master") source = "bus:/";
                else if (source.rfind("bus:/", 0) != 0) source = "bus:/" + source;
            }
            if (source == entry->path) return OAE_ERR_FORMAT;
            entry->effects.emplace_back(effect, source);
        }
        std::stable_sort(entry->effects.begin(), entry->effects.end(), [](const auto& a, const auto& b) {
            return a.first.position < b.first.position;
        });

        const auto previous = owner_->bankMixer_.find(entry->path);
        if (previous != owner_->bankMixer_.end()) {
            if (auto shared = previous->second.lock()) {
                if (shared->parent_path != entry->parent_path || shared->volume != entry->volume ||
                    shared->effects.size() != entry->effects.size() ||
                    !std::equal(shared->effects.begin(), shared->effects.end(), entry->effects.begin(),
                        [](const auto& a, const auto& b) {
                            const auto& x = a.first;
                            const auto& y = b.first;
                            return a.second == b.second && x.position == y.position && x.type == y.type &&
                                x.bypass == y.bypass && x.params == y.params && x.wet == y.wet && x.dry == y.dry;
                        })) return OAE_ERR_ALREADY_EXISTS;
                installed.push_back(std::move(shared));
                continue;
            }
        }
        if (master) {
            auto* bus = runtime.buses.get(owner_->master_bus());
            if (!bus) return OAE_ERR_NOT_INITIALIZED;
            if (!entry->effects.empty() && (bus->dsp().size(OAE_EFFECT_PRE_FADER) ||
                bus->dsp().size(OAE_EFFECT_POST_FADER))) return OAE_ERR_ALREADY_EXISTS;
            entry->original_volume = bus->volume();
            entry->handle = owner_->master_bus();
        } else {
            if (owner_->lookup_bus_cached(entry->path)) return OAE_ERR_ALREADY_EXISTS;
            auto* parent = runtime.buses.get(entry->parent ? entry->parent->handle : owner_->master_bus());
            if (!parent || parent->depth() >= Bus::max_depth) return OAE_ERR_FORMAT;
            auto bus = std::make_unique<Bus>(*owner_, parent, entry->path);
            const auto result = bus->init();
            if (result != OAE_OK) return result;
            entry->handle = runtime.buses.insert(std::move(bus));
            if (!entry->handle) return OAE_ERR_OUT_OF_MEMORY;
        }
        runtime.buses.get(entry->handle)->set_volume(entry->volume);
        owner_->bankMixer_[entry->path] = entry;
        installed.push_back(entry);
        created.push_back(std::move(entry));
    }

    for (const auto& entry : created) {
        entry->owned_effects.reserve(entry->effects.size());
        for (const auto& [effect, source] : entry->effects) {
            oae_effect handle = 0;
            auto result = oae_bus_add_effect_ex(entry->handle, effect.type, effect.position,
                OAE_EFFECT_INDEX_LAST, &handle);
            if (result != OAE_OK) return result;
            entry->owned_effects.push_back(handle);
            bool high_first = false;
            if (effect.type == OAE_EFFECT_EQ3 && effect.params.size() > OAE_EQ3_HIGH_XOVER_HZ) {
                float initial_high = 0.0f;
                result = oae_effect_get_param(handle, OAE_EQ3_HIGH_XOVER_HZ, &initial_high);
                if (result != OAE_OK) return result;
                high_first = effect.params[OAE_EQ3_LOW_XOVER_HZ] >= initial_high;
                if (high_first) {
                    result = oae_effect_set_param(handle, OAE_EQ3_HIGH_XOVER_HZ, effect.params[OAE_EQ3_HIGH_XOVER_HZ]);
                    if (result != OAE_OK) return result;
                }
            }
            for (std::size_t j = 0; j < effect.params.size(); ++j) {
                if (high_first && j == OAE_EQ3_HIGH_XOVER_HZ) continue;
                result = oae_effect_set_param(handle, static_cast<int32_t>(j), effect.params[j]);
                if (result != OAE_OK) return result;
            }
            result = oae_effect_set_bypass(handle, effect.bypass);
            if (result != OAE_OK) return result;
            result = oae_effect_set_wet_dry(handle, effect.wet, effect.dry);
            if (result != OAE_OK) return result;
            if (!source.empty()) {
                const auto source_handle = owner_->lookup_bus_cached(source);
                result = oae_effect_set_sidechain(handle, source_handle);
                if (result != OAE_OK) return result;
            }
        }
    }
    mixer_ = std::move(installed);
    return OAE_OK;
}

void Bank::prepare_async(BankParseResult parsed)
{
    prepared_ = std::make_unique<BankParseResult>(std::move(parsed));
    prepared_ready_.store(true, std::memory_order_release);
}

void Bank::complete_async()
{
    if (owner_->in_callback() || !prepared_ready_.exchange(false, std::memory_order_acq_rel)) return;
    auto parsed = std::move(prepared_);
    try {
        const auto result = attach_mixer(parsed->buses);
        if (result != OAE_OK) {
            set_loading_state(OAE_BANK_FAILED);
            return;
        }
        samples_.reserve(parsed->samples.size());
        for (auto& sample : parsed->samples) {
            samples_.push_back({std::move(sample.data), sample.length_frames, sample.sample_rate, 0, 0});
        }
        set_model_set(std::move(parsed->model_set));
        if (model_set_) owner_->register_model_set(model_set_);
        set_loading_state(OAE_BANK_LOADED);
    } catch (...) {
        if (model_set_) owner_->unregister_model_set(model_set_);
        model_set_.reset();
        mixer_.clear();
        set_loading_state(OAE_BANK_FAILED);
    }
}

Bank::Bank(System& owner, std::shared_ptr<const ModelSet> model_set, std::vector<BankSampleEntry> samples, bool nonblocking)
    : owner_(&owner), model_set_(std::move(model_set)), samples_(std::move(samples)), nonblocking_(nonblocking)
{
    if (owner_) {
        owner_->register_bank(this);
        if (model_set_) {
            owner_->register_model_set(model_set_);
        }
    }
}

Bank::~Bank()
{
    unload();
}

void Bank::set_model_set(std::shared_ptr<const ModelSet> set) noexcept
{
    model_set_ = std::move(set);
}

void Bank::set_samples(std::vector<BankSampleEntry> samples)
{
    std::lock_guard<std::mutex> lock(mutex_);
    samples_ = std::move(samples);
}

void Bank::set_async_worker(std::thread t)
{
    async_worker_ = std::move(t);
}

void Bank::wait_async_load()
{
    if (async_worker_.joinable()) {
        async_worker_.join();
    }
}

void Bank::unload() noexcept
{
    if (unloaded_) return;
    unloaded_ = true;

    wait_async_load();

    if (owner_) {
        owner_->unregister_bank(this);
        if (model_set_) {
            owner_->unregister_model_set(model_set_);
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : samples_) {
        if (s.sound != 0) {
            oae_sound_release(s.sound);
            s.sound = 0;
        }
    }
    samples_.clear();
    mixer_.clear();
}

bool Bank::is_sample_pinned(int32_t sample_idx) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sample_idx < 0 || static_cast<size_t>(sample_idx) >= samples_.size()) {
        return false;
    }
    return samples_[static_cast<size_t>(sample_idx)].pin_count > 0;
}

oae_sound Bank::get_sample_sound(int32_t sample_idx) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sample_idx < 0 || static_cast<size_t>(sample_idx) >= samples_.size()) {
        return 0;
    }
    return samples_[static_cast<size_t>(sample_idx)].sound;
}

oae_result Bank::pin_sample(int32_t sample_idx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sample_idx < 0 || static_cast<size_t>(sample_idx) >= samples_.size()) {
        return OAE_ERR_INVALID_PARAM;
    }
    auto& entry = samples_[static_cast<size_t>(sample_idx)];
    entry.pin_count++;
    if (entry.pin_count == 1 && entry.sound == 0) {
        if (!owner_) return OAE_ERR_INVALID_STATE;
        if (entry.stream && entry.stream_length > 0) {
            auto data = std::make_shared<SoundData>();
            const auto res = load_bank_slice(entry.stream_path, entry.stream_offset, entry.stream_length, *data);
            if (res != OAE_OK) {
                entry.pin_count--;
                return res;
            }
            const auto snd = runtime.sounds.insert(std::make_unique<Sound>(Sound{owner_, std::move(data)}));
            if (snd == 0) {
                entry.pin_count--;
                return OAE_ERR_OUT_OF_MEMORY;
            }
            entry.sound = snd;
        } else if (!entry.data.empty()) {
            oae_sound snd = 0;
            const auto res = oae_sound_create_from_memory(owner_->handle(), entry.data.data(), entry.data.size(), OAE_SOUND_DEFAULT, &snd);
            if (res != OAE_OK) {
                entry.pin_count--;
                return res;
            }
            entry.sound = snd;
        }
    }
    return OAE_OK;
}

oae_result Bank::unpin_sample(int32_t sample_idx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sample_idx < 0 || static_cast<size_t>(sample_idx) >= samples_.size()) {
        return OAE_ERR_INVALID_PARAM;
    }
    auto& entry = samples_[static_cast<size_t>(sample_idx)];
    if (entry.pin_count > 0) {
        entry.pin_count--;
        if (entry.pin_count == 0 && entry.sound != 0) {
            oae_sound_release(entry.sound);
            entry.sound = 0;
        }
    }
    return OAE_OK;
}

}
