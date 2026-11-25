#include "runtime.h"

#include <algorithm>
#include <array>

namespace oae {

Runtime runtime;

Runtime::~Runtime()
{
    event_instances.erase_if([](const EventInstance&) { return true; });
    event_descriptions.erase_if([](const EventDescription&) { return true; });
    channels.erase_if([](const Channel&) { return true; });
    buses.for_each([](Bus& bus) { bus.dsp().clear(); });
    effects.erase_if([](const Effect&) { return true; });
    for (unsigned depth = Bus::max_depth + 1; depth > 0; --depth) {
        buses.erase_if([&](const Bus& bus) { return bus.depth() == depth - 1; });
    }
}

oae_result Runtime::init_system(System& system, const oae_init_settings& settings)
{
    if (system.update() == OAE_OK) {
        return OAE_ERR_ALREADY_INITIALIZED;
    }
    auto master = std::make_unique<Bus>(system, nullptr, "bus:/");
    const auto result = system.init(settings);
    if (result != OAE_OK) {
        return result;
    }
    const auto busResult = master->init();
    if (busResult != OAE_OK) {
        master.reset();
        system.shutdown();
        return busResult;
    }
    const auto handle = buses.insert(std::move(master));
    if (handle == 0) {
        system.shutdown();
        return OAE_ERR_OUT_OF_MEMORY;
    }
    system.set_master_bus(handle);
    return OAE_OK;
}

Bus* Runtime::resolve_bus(const System& system, oae_bus handle) const noexcept
{
    auto* bus = buses.get(handle != 0 ? handle : system.master_bus());
    return bus && bus->owner() == &system ? bus : nullptr;
}

oae_result Runtime::try_make_physical(Channel& channel) noexcept
{
    if (channel.physical() || !channel.playing()) {
        return OAE_OK;
    }
    const auto* owner = channel.owner();
    std::uint32_t assigned = 0;
    channels.for_each([&](const Channel& current) {
        if (current.owner() == owner && current.physical()) {
            ++assigned;
        }
    });
    if (assigned >= owner->max_voices()) {
        return OAE_OK;
    }
    return channel.set_physical(true, owner->clock());
}

oae_result Runtime::update_voices(System& system) noexcept
{
    struct Voice {
        Channel* channel;
        double gain;
    };
    std::array<Voice, channel_capacity> candidates;
    std::size_t count = 0;
    const auto now = system.clock();
    channels.erase_if([&](const Channel& channel) {
        return channel.owner() == &system && channel.finished_at(now);
    });

    auto status = OAE_OK;
    channels.for_each([&](Channel& channel) {
        if (channel.owner() != &system) {
            return;
        }
        if (channel.playing()) {
            candidates[count++] = {&channel, channel.audibility()};
        } else if (channel.physical() && channel.set_physical(false, now) != OAE_OK) {
            status = OAE_ERR_INTERNAL;
        }
    });
    if (status != OAE_OK) {
        return status;
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count),
        [](const Voice& a, const Voice& b) {
            if (a.channel->priority() != b.channel->priority()) {
                return a.channel->priority() < b.channel->priority();
            }
            if (a.gain != b.gain) {
                return a.gain > b.gain;
            }
            return a.channel->order() < b.channel->order();
        });
    const auto limit = std::min<std::size_t>(count, system.max_voices());
    for (auto i = limit; i < count; ++i) {
        if (candidates[i].channel->set_physical(false, now) != OAE_OK) {
            return OAE_ERR_INTERNAL;
        }
    }
    for (std::size_t i = 0; i < limit; ++i) {
        if (candidates[i].channel->set_physical(true, now) != OAE_OK) {
            return OAE_ERR_INTERNAL;
        }
    }
    buses.for_each([&](Bus& bus) {
        if (bus.owner() == &system) {
            bus.dsp().meter.close(system.channels());
        }
    });
    return OAE_OK;
}

oae_result Runtime::set_bus_paused(Bus& bus, bool paused) noexcept
{
    const auto now = bus.owner()->clock();
    channels.for_each([&](Channel& channel) {
        if (channel.bus().within(bus)) {
            channel.sync_virtual(now);
        }
    });
    return bus.set_paused(paused);
}

void Runtime::destroy_bus_tree(oae_bus rootHandle) noexcept
{
    const auto* root = buses.get(rootHandle);
    if (!root) {
        return;
    }
    channels.erase_if([&](const Channel& channel) { return channel.bus().within(*root); });
    buses.for_each([&](Bus& bus) {
        if (bus.within(*root)) {
            bus.detach_all_sends();
            bus.dsp().clear();
        } else {
            for (std::size_t i = 0; i < BusDsp::send_limit; ++i) {
                const auto slot = bus.dsp().send_slot(i);
                if (slot.active && slot.target && slot.target->within(*root)) {
                    bus.remove_send(*slot.target);
                }
            }
        }
    });
    effects.erase_if([&](const Effect& effect) { return effect.bus().within(*root); });
    for (unsigned depth = Bus::max_depth; depth > root->depth(); --depth) {
        buses.erase_if([&](const Bus& bus) { return bus.depth() == depth && bus.within(*root); });
    }
    buses.erase(rootHandle);
}

}
