#pragma once

#include "../common/handle_pool.h"
#include "../core/bus.h"
#include "../core/channel.h"
#include "../core/effect.h"
#include "../core/bank.h"
#include "../core/event_instance.h"
#include "../core/vca.h"
#include "../core/system.h"

#include <cstddef>
#include <new>

namespace oae {

struct Runtime {
    ~Runtime();
    oae_result init_system(System& system, const oae_init_settings& settings);
    Bus* resolve_bus(const System& system, oae_bus handle) const noexcept;
    void destroy_bus_tree(oae_bus root) noexcept;
    oae_result try_make_physical(Channel& channel) noexcept;
    oae_result update_voices(System& system) noexcept;
    oae_result set_bus_paused(Bus& bus, bool paused) noexcept;
    std::uint64_t next_channel_order() noexcept { return channelOrder_++; }

    HandlePool<Bus, 4, 4096> buses;
    HandlePool<System, 1, 64> systems;
    HandlePool<Sound, 2, 4096> sounds;
    HandlePool<Effect, 5, 4096> effects;
    HandlePool<Bank, 6, 256> banks;
    static constexpr std::size_t channel_capacity = 4096;
    HandlePool<Channel, 3, channel_capacity> channels;
    HandlePool<EventDescription, 7, 4096> event_descriptions;
    HandlePool<EventInstance, 8, 4096> event_instances;
    HandlePool<Vca, 9, 256> vcas;

private:
    std::uint64_t channelOrder_ = 0;
};

extern Runtime runtime;

template <typename Operation>
oae_result api_call(Operation operation) noexcept
{
    try {
        return operation();
    } catch (const std::bad_alloc&) {
        return OAE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return OAE_ERR_INTERNAL;
    }
}

}
