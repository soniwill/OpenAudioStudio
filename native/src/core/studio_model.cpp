#include "studio_model.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace oae {

namespace {

std::uint64_t fnv1a_64(const std::string& s, std::uint64_t seed) noexcept
{
    std::uint64_t h = seed;
    for (const unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 0x100000001b3ull;
    }
    return h;
}

}

Guid guid_from_string(const std::string& s) noexcept
{
    Guid g;
    g.hi = fnv1a_64(s, 0xcbf29ce484222325ull);
    g.lo = fnv1a_64(s, 0x84222325cbf29ce4ull ^ 0x9E3779B97F4A7C15ull);
    if (g.hi == 0 && g.lo == 0) {
        g.lo = 1;
    }
    return g;
}

std::uint32_t EventDescriptionModel::total_length_ms() const noexcept
{
    double max_end = 0.0;
    for (const auto& track : tracks) {
        for (const auto& inst : track.instruments) {
            const double end = inst.offset_ms + inst.length_ms + inst.delay_ms_random_range;
            if (end > max_end) {
                max_end = end;
            }
        }
    }
    return static_cast<std::uint32_t>(std::ceil(max_end));
}

bool EventDescriptionModel::is_oneshot() const noexcept
{
    for (const auto& item : logic_items) {
        if (item.kind == LogicItemKind::LoopRegion && (item.repeat_count == -1 || item.until_key_off)) {
            return false;
        }
        if (item.kind == LogicItemKind::SustainPoint) {
            return false;
        }
    }
    for (const auto& track : tracks) {
        for (const auto& inst : track.instruments) {
            if (inst.loop) {
                return false;
            }
        }
    }
    return true;
}

const EventDescriptionModel* ModelSet::find_event_by_path(const std::string& path) const noexcept
{
    const auto it = path_to_index_.find(path);
    if (it != path_to_index_.end() && it->second < events_.size()) {
        return &events_[it->second];
    }
    return nullptr;
}

const EventDescriptionModel* ModelSet::find_event_by_guid(const Guid& guid) const noexcept
{
    const auto it = guid_hi_to_index_.find(guid.hi);
    if (it != guid_hi_to_index_.end() && it->second < events_.size()) {
        const auto& ev = events_[it->second];
        if (ev.id == guid) {
            return &ev;
        }
    }
    for (const auto& ev : events_) {
        if (ev.id == guid) {
            return &ev;
        }
    }
    return nullptr;
}

const ParameterDefinition* ModelSet::find_global_parameter(const std::string& name) const noexcept
{
    const auto it = global_param_to_index_.find(name);
    if (it != global_param_to_index_.end() && it->second < global_parameters_.size()) {
        return &global_parameters_[it->second];
    }
    return nullptr;
}

const VcaModel* ModelSet::find_vca(const std::string& path) const noexcept
{
    const auto it = vca_to_index_.find(path);
    if (it != vca_to_index_.end() && it->second < vcas_.size()) {
        return &vcas_[it->second];
    }
    return nullptr;
}

ModelSetBuilder& ModelSetBuilder::add_event(EventDescriptionModel event)
{
    events_.push_back(std::move(event));
    return *this;
}

std::shared_ptr<const ModelSet> ModelSetBuilder::build()
{
    auto set = std::make_shared<ModelSet>();
    set->events_ = std::move(events_);
    set->global_parameters_ = std::move(global_parameters_);
    set->effect_automations_ = std::move(effect_automations_);
    set->vcas_ = std::move(vcas_);
    set->audio_table_ = std::move(audio_table_);

    for (std::size_t vi = 0; vi < set->vcas_.size(); ++vi) {
        auto& v = set->vcas_[vi];
        if (v.id.is_null()) {
            v.id = guid_from_string(v.path);
        }
        set->vca_to_index_[v.path] = vi;
    }

    for (std::size_t gi = 0; gi < set->global_parameters_.size(); ++gi) {
        auto& gp = set->global_parameters_[gi];
        if (gp.id.is_null()) {
            gp.id = guid_from_string("global_param#" + gp.name);
        }
        set->global_param_to_index_[gp.name] = gi;
    }

    for (std::size_t ei = 0; ei < set->events_.size(); ++ei) {
        auto& ev = set->events_[ei];
        if (ev.id.is_null()) {
            ev.id = guid_from_string(ev.path);
        }
        for (std::size_t ti = 0; ti < ev.tracks.size(); ++ti) {
            auto& track = ev.tracks[ti];
            if (track.id.is_null()) {
                track.id = guid_from_string(ev.path + "#track" + std::to_string(ti));
            }
            for (std::size_t ii = 0; ii < track.instruments.size(); ++ii) {
                auto& inst = track.instruments[ii];
                if (inst.id.is_null()) {
                    inst.id = guid_from_string(ev.path + "#track" + std::to_string(ti) + "#inst" + std::to_string(ii));
                }
            }
        }
        for (std::size_t li = 0; li < ev.logic_items.size(); ++li) {
            auto& item = ev.logic_items[li];
            if (item.id.is_null()) {
                item.id = guid_from_string(ev.path + "#logic" + std::to_string(li));
            }
        }
        set->path_to_index_[ev.path] = ei;
        set->guid_hi_to_index_[ev.id.hi] = ei;
    }

    return set;
}

std::size_t PlaylistState::pick(const std::vector<PlaylistEntry>& entries, PlaylistMode mode)
{
    if (entries.empty()) {
        return 0;
    }
    if (entries.size() == 1) {
        last_picked_ = 0;
        return 0;
    }

    switch (mode) {
    case PlaylistMode::Sequential: {
        const std::size_t idx = seq_index_ % entries.size();
        seq_index_ = (idx + 1) % entries.size();
        last_picked_ = idx;
        return idx;
    }
    case PlaylistMode::Shuffle: {
        if (shuffle_deck_.size() != entries.size() || shuffle_pos_ >= shuffle_deck_.size()) {
            shuffle_deck_.resize(entries.size());
            std::iota(shuffle_deck_.begin(), shuffle_deck_.end(), std::size_t{0});
            for (std::size_t i = shuffle_deck_.size(); i > 1; --i) {
                const auto j = rng_.next_bounded(static_cast<std::uint32_t>(i));
                std::swap(shuffle_deck_[i - 1], shuffle_deck_[j]);
            }
            shuffle_pos_ = 0;
        }
        const std::size_t idx = shuffle_deck_[shuffle_pos_++];
        last_picked_ = idx;
        return idx;
    }
    case PlaylistMode::RandomNoRepeat: {
        float total_weight = 0.0f;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i != last_picked_) {
                total_weight += std::max(0.0f, entries[i].weight);
            }
        }
        if (total_weight <= 0.0f) {
            std::vector<std::size_t> candidates;
            for (std::size_t i = 0; i < entries.size(); ++i) {
                if (i != last_picked_) candidates.push_back(i);
            }
            const auto pick_idx = rng_.next_bounded(static_cast<std::uint32_t>(candidates.size()));
            last_picked_ = candidates[pick_idx];
            return last_picked_;
        }
        const float r = rng_.next_float() * total_weight;
        float current = 0.0f;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i == last_picked_) continue;
            current += std::max(0.0f, entries[i].weight);
            if (r <= current) {
                last_picked_ = i;
                return i;
            }
        }
        for (std::size_t i = entries.size(); i > 0; --i) {
            if (i - 1 != last_picked_) {
                last_picked_ = i - 1;
                return last_picked_;
            }
        }
        return 0;
    }
    case PlaylistMode::Random:
    default: {
        float total_weight = 0.0f;
        for (const auto& entry : entries) {
            total_weight += std::max(0.0f, entry.weight);
        }
        if (total_weight <= 0.0f) {
            const auto pick_idx = rng_.next_bounded(static_cast<std::uint32_t>(entries.size()));
            last_picked_ = pick_idx;
            return pick_idx;
        }
        const float r = rng_.next_float() * total_weight;
        float current = 0.0f;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            current += std::max(0.0f, entries[i].weight);
            if (r <= current) {
                last_picked_ = i;
                return i;
            }
        }
        last_picked_ = entries.size() - 1;
        return last_picked_;
    }
    }
}

}
