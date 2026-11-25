#include "bus.h"
#include "system.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace oae {
namespace {

oae_result node_error(ma_result result) noexcept
{
    if (result == MA_SUCCESS) {
        return OAE_OK;
    }
    return result == MA_OUT_OF_MEMORY ? OAE_ERR_OUT_OF_MEMORY : OAE_ERR_INTERNAL;
}

}

Bus::Bus(System& owner, Bus* parent, std::string path)
    : owner_(&owner), parent_(parent), path_(std::move(path)), depth_(parent ? parent->depth() + 1 : 0), strip_(owner)
{
    owner_->invalidate_bus_cache();
}

Bus::~Bus()
{
    owner_->invalidate_bus_cache();
    if (groupReady_) {
        ma_sound_group_uninit(&group_);
    }
}

oae_result Bus::init() noexcept
{
    auto* engine = owner_->engine();
    auto* graph = ma_engine_get_node_graph(engine);
    constexpr ma_uint32 groupFlags = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
    auto result = ma_sound_group_init(engine, groupFlags, nullptr, &group_);
    if (result != MA_SUCCESS) {
        return node_error(result);
    }
    groupReady_ = true;

    dsp_.meter.close(owner_->channels());
    const auto stripResult = strip_.init(&dsp_);
    if (stripResult != OAE_OK) {
        return stripResult;
    }

    ma_node* destination = parent_ ? static_cast<ma_node*>(parent_->group()) : ma_node_graph_get_endpoint(graph);
    result = ma_node_attach_output_bus(strip_.node(), 0, destination, 0);
    if (result == MA_SUCCESS) {
        result = ma_node_attach_output_bus(&group_, 0, strip_.node(), 0);
    }
    return node_error(result);
}

bool Bus::within(const Bus& ancestor) const noexcept
{
    for (const Bus* current = this; current; current = current->parent_) {
        if (current == &ancestor) {
            return true;
        }
    }
    return false;
}

bool Bus::can_reach(const Bus& target) const noexcept
{
    if (this == &target) {
        return true;
    }
    std::vector<const Bus*> stack{this};
    std::vector<const Bus*> visited;
    while (!stack.empty()) {
        const Bus* current = stack.back();
        stack.pop_back();
        if (current == &target) {
            return true;
        }
        if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
            continue;
        }
        visited.push_back(current);
        if (current->parent_) {
            stack.push_back(current->parent_);
        }
        for (std::size_t i = 0; i < BusDsp::send_limit; ++i) {
            const auto& slot = current->dsp_.send_slot(i);
            if (slot.active && slot.target) {
                stack.push_back(slot.target);
            }
        }
    }
    return false;
}

oae_result Bus::add_send(Bus& target, float level, int position) noexcept
{
    if (this == &target || target.can_reach(*this)) {
        return OAE_ERR_INVALID_PARAM;
    }
    if (dsp_.find_send(&target) >= 0) {
        return OAE_ERR_ALREADY_EXISTS;
    }
    const int slot = dsp_.allocate_send(target, level, position);
    if (slot < 0) {
        return OAE_ERR_OUT_OF_MEMORY;
    }
    const auto result = ma_node_attach_output_bus(
        strip_.node(), static_cast<ma_uint32>(1 + slot), target.group(), 0);
    if (result != MA_SUCCESS) {
        dsp_.remove_send(&target);
        return node_error(result);
    }
    return OAE_OK;
}

oae_result Bus::remove_send(const Bus& target) noexcept
{
    const int slot = dsp_.find_send(&target);
    if (slot < 0) {
        return OAE_ERR_NOT_FOUND;
    }
    dsp_.remove_send(&target);
    const auto result = ma_node_detach_output_bus(strip_.node(), static_cast<ma_uint32>(1 + slot));
    return node_error(result);
}

void Bus::detach_all_sends() noexcept
{
    for (std::size_t i = 0; i < BusDsp::send_limit; ++i) {
        if (dsp_.send_slot(i).active) {
            ma_node_detach_output_bus(strip_.node(), static_cast<ma_uint32>(1 + i));
        }
    }
    dsp_.clear_sends();
}

bool Bus::paused_in_tree() const noexcept
{
    for (const Bus* current = this; current; current = current->parent_) {
        if (current->paused()) {
            return true;
        }
    }
    return false;
}

double Bus::effective_gain() const noexcept
{
    double gain = 1.0;
    const auto now = owner_->clock();
    for (const Bus* current = this; current; current = current->parent_) {
        const auto level = static_cast<double>(current->volume()) 
                         * current->strip_.fade_gain(now)
                         * static_cast<double>(current->vca_gain())
                         * static_cast<double>(current->snapshot_gain());
        if (current->muted() || current->paused() || level == 0.0) {
            return 0.0;
        }
        gain *= level;
    }
    return gain;
}

oae_result Bus::set_paused(bool paused) noexcept
{
    const auto result = paused ? ma_sound_group_stop(&group_) : ma_sound_group_start(&group_);
    if (result == MA_SUCCESS) {
        paused_ = paused;
        dsp_.set_paused(paused);
    }
    return node_error(result);
}

}
