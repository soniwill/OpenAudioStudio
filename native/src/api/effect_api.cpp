#include "runtime.h"

#include <cmath>

namespace {

template <typename Operation>
oae_result with_effect(oae_effect handle, Operation operation) noexcept
{
    return oae::api_call([&] {
        auto* effect = oae::runtime.effects.get(handle);
        return effect ? operation(*effect) : OAE_ERR_INVALID_HANDLE;
    });
}

bool unit(float value) noexcept { return value >= 0.0f && value <= 1.0f; }

}

extern "C" {

oae_result OAE_CALL oae_bus_add_effect(oae_bus bus, int type, int32_t index, oae_effect* out_effect)
{
    return oae_bus_add_effect_ex(bus, type, OAE_EFFECT_POST_FADER, index, out_effect);
}

oae_result OAE_CALL oae_bus_add_effect_ex(oae_bus bus, int type, int position, int32_t index, oae_effect* out_effect)
{
    return oae::api_call([&] {
        if (!out_effect || !oae::effect_parameter(type, 0) ||
            (position != OAE_EFFECT_PRE_FADER && position != OAE_EFFECT_POST_FADER) ||
            index < OAE_EFFECT_INDEX_LAST || index >= static_cast<int32_t>(oae::BusDsp::chain_limit)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* target = oae::runtime.buses.get(bus);
        if (!target) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto& dsp = target->dsp();
        if (index != OAE_EFFECT_INDEX_LAST && static_cast<std::size_t>(index) > dsp.size(position)) {
            return OAE_ERR_INVALID_PARAM;
        }
        if (!dsp.can_insert(position, index)) {
            return OAE_ERR_INVALID_STATE;
        }
        auto effect = std::make_unique<oae::Effect>(*target, type, target->owner()->sample_rate(), target->owner()->channels());
        auto* instance = effect.get();
        const auto handle = oae::runtime.effects.insert(std::move(effect));
        if (!handle) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        dsp.insert(*instance, position, index);
        *out_effect = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_set_param(oae_effect effect, int32_t param_id, float value)
{
    if (param_id < 0 || param_id >= 5 || !std::isfinite(value)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](oae::Effect& instance) {
        return instance.set_parameter(param_id, value) ? OAE_OK : OAE_ERR_INVALID_PARAM;
    });
}

oae_result OAE_CALL oae_effect_get_param(oae_effect effect, int32_t param_id, float* out_value)
{
    if (!out_value || param_id < 0 || param_id >= 5) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](const oae::Effect& instance) {
        if (!oae::effect_parameter(instance.type(), param_id)) {
            return OAE_ERR_INVALID_PARAM;
        }
        *out_value = instance.parameter(param_id);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_set_bypass(oae_effect effect, int bypass)
{
    if (bypass != 0 && bypass != 1) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](oae::Effect& instance) {
        instance.set_bypass(bypass != 0);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_get_bypass(oae_effect effect, int* out_bypass)
{
    if (!out_bypass) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](const oae::Effect& instance) {
        *out_bypass = instance.bypass() ? 1 : 0;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_set_wet_dry(oae_effect effect, float wet, float dry)
{
    if (!unit(wet) || !unit(dry)) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](oae::Effect& instance) {
        instance.set_mix(wet, dry);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_get_wet_dry(oae_effect effect, float* out_wet, float* out_dry)
{
    if (!out_wet || !out_dry) {
        return OAE_ERR_INVALID_PARAM;
    }
    return with_effect(effect, [&](const oae::Effect& instance) {
        *out_wet = instance.wet();
        *out_dry = instance.dry();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_set_sidechain(oae_effect effect, oae_bus source_bus)
{
    return with_effect(effect, [&](oae::Effect& instance) {
        const auto* source = oae::runtime.buses.get(source_bus);
        if (!source || source->owner() != instance.bus().owner()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (instance.type() != OAE_EFFECT_COMPRESSOR || source == &instance.bus()) {
            return OAE_ERR_INVALID_PARAM;
        }
        instance.set_sidechain(source->dsp().tap);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_effect_release(oae_effect effect)
{
    return with_effect(effect, [&](oae::Effect& instance) {
        instance.bus().dsp().remove(instance);
        return oae::runtime.effects.erase(effect) ? OAE_OK : OAE_ERR_INTERNAL;
    });
}

oae_result OAE_CALL oae_bus_get_meter(oae_bus bus, oae_meter* out_meter)
{
    return oae::api_call([&] {
        if (!out_meter) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* target = oae::runtime.buses.get(bus);
        if (!target) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_meter = target->dsp().meter.snapshot();
        return OAE_OK;
    });
}

}
