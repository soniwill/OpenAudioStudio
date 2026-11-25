#include <oae_audio.h>

#include "arguments.h"
#include "runtime.h"

#include <cmath>
#include <memory>

extern "C" {

oae_result OAE_CALL oae_studio_get_event(oae_system system, const char* path, oae_event_description* out_description)
{
    return oae::api_call([&] {
        if (!path || !out_description) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }

        std::shared_ptr<const oae::ModelSet> model_set;
        const auto* model = sys->find_event_model(path, model_set);
        if (!model) {
            return OAE_ERR_NOT_FOUND;
        }

        auto desc = std::make_unique<oae::EventDescription>(system, std::move(model_set), model);
        const auto handle = oae::runtime.event_descriptions.insert(std::move(desc));
        if (handle == 0) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        auto* registered = oae::runtime.event_descriptions.get(handle);
        if (registered) {
            registered->set_handle(handle);
        }
        *out_description = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_studio_play_one_shot(oae_system system, const char* path)
{
    return oae::api_call([&] {
        if (!path) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }

        std::shared_ptr<const oae::ModelSet> model_set;
        const auto* model = sys->find_event_model(path, model_set);
        if (!model) {
            return OAE_ERR_NOT_FOUND;
        }

        auto desc = std::make_unique<oae::EventDescription>(system, std::move(model_set), model);
        const auto handle = oae::runtime.event_descriptions.insert(std::move(desc));
        if (handle == 0) {
            return OAE_ERR_OUT_OF_MEMORY;
        }
        auto* registered_desc = oae::runtime.event_descriptions.get(handle);
        registered_desc->set_handle(handle);

        oae::EventInstance* inst = nullptr;
        const auto create_res = registered_desc->create_instance(&inst);
        if (create_res != OAE_OK) {
            oae::runtime.event_descriptions.erase(handle);
            return create_res;
        }

        const auto inst_handle = oae::runtime.event_instances.insert(std::unique_ptr<oae::EventInstance>(inst));
        if (inst_handle == 0) {
            delete inst;
            oae::runtime.event_descriptions.erase(handle);
            return OAE_ERR_OUT_OF_MEMORY;
        }

        auto* registered_inst = oae::runtime.event_instances.get(inst_handle);
        registered_inst->set_handle(inst_handle);
        const auto start_res = registered_inst->start();
        sys->add_active_instance(registered_inst);
        return start_res;
    });
}

oae_result OAE_CALL oae_event_description_create_instance(oae_event_description description, oae_event_instance* out_instance)
{
    return oae::api_call([&] {
        if (!out_instance) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc) {
            return OAE_ERR_INVALID_HANDLE;
        }

        oae::EventInstance* inst = nullptr;
        const auto res = desc->create_instance(&inst);
        if (res != OAE_OK) {
            return res;
        }

        const auto handle = oae::runtime.event_instances.insert(std::unique_ptr<oae::EventInstance>(inst));
        if (handle == 0) {
            delete inst;
            return OAE_ERR_OUT_OF_MEMORY;
        }
        auto* registered = oae::runtime.event_instances.get(handle);
        if (registered) {
            registered->set_handle(handle);
        }
        *out_instance = handle;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_description_get_length_ms(oae_event_description description, uint32_t* out_length_ms)
{
    return oae::api_call([&] {
        if (!out_length_ms) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc || !desc->model()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_length_ms = desc->model()->total_length_ms();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_description_is_oneshot(oae_event_description description, int* out_is_oneshot)
{
    return oae::api_call([&] {
        if (!out_is_oneshot) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc || !desc->model()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_is_oneshot = desc->model()->is_oneshot() ? 1 : 0;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_description_get_instance_count(oae_event_description description, int* out_count)
{
    return oae::api_call([&] {
        if (!out_count) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_count = desc->active_instance_count();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_start(oae_event_instance instance)
{
    return oae::api_call([&] {
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const auto res = inst->start();
        if (inst->description()) {
            auto* sys = oae::runtime.systems.get(inst->description()->system());
            if (sys) {
                sys->add_active_instance(inst);
            }
        }
        return res;
    });
}

oae_result OAE_CALL oae_event_instance_stop(oae_event_instance instance, int mode)
{
    return oae::api_call([&] {
        if (mode != OAE_STOP_ALLOW_FADEOUT && mode != OAE_STOP_IMMEDIATE) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return inst->stop(mode);
    });
}

oae_result OAE_CALL oae_event_instance_release(oae_event_instance instance)
{
    return oae::api_call([&] {
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->stop(OAE_STOP_IMMEDIATE);
        if (inst->description()) {
            auto* sys = oae::runtime.systems.get(inst->description()->system());
            if (sys) {
                sys->remove_active_instance(inst);
            }
        }
        return oae::runtime.event_instances.erase(instance) ? OAE_OK : OAE_ERR_INTERNAL;
    });
}

oae_result OAE_CALL oae_event_instance_get_playback_state(oae_event_instance instance, int* out_state)
{
    return oae::api_call([&] {
        if (!out_state) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_state = static_cast<int>(inst->playback_state());
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_description(oae_event_instance instance, oae_event_description* out_description)
{
    return oae::api_call([&] {
        if (!out_description) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst || !inst->description()) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_description = inst->description()->handle();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_volume(oae_event_instance instance, float volume)
{
    return oae::api_call([&] {
        if (!std::isfinite(volume) || volume < 0.0f) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_volume(volume);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_volume(oae_event_instance instance, float* out_volume)
{
    return oae::api_call([&] {
        if (!out_volume) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_volume = inst->volume();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_pitch(oae_event_instance instance, float pitch)
{
    return oae::api_call([&] {
        if (!std::isfinite(pitch) || pitch <= 0.0f) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_pitch(pitch);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_pitch(oae_event_instance instance, float* out_pitch)
{
    return oae::api_call([&] {
        if (!out_pitch) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_pitch = inst->pitch();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_pan(oae_event_instance instance, float pan)
{
    return oae::api_call([&] {
        if (!std::isfinite(pan) || pan < -1.0f || pan > 1.0f) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_pan(pan);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_pan(oae_event_instance instance, float* out_pan)
{
    return oae::api_call([&] {
        if (!out_pan) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_pan = inst->pan();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_callback(oae_event_instance instance, oae_event_callback callback, uint32_t mask, void* user_data)
{
    return oae::api_call([&] {
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_callback(callback, mask, user_data);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_key_off(oae_event_instance instance)
{
    return oae::api_call([&] {
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->key_off();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_timeline_position_ms(oae_event_instance instance, int32_t* out_position_ms)
{
    return oae::api_call([&] {
        if (!out_position_ms) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_position_ms = static_cast<int32_t>(std::llround(inst->timeline_position_ms()));
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_timeline_position_ms(oae_event_instance instance, int32_t position_ms)
{
    return oae::api_call([&] {
        if (position_ms < 0) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_timeline_position_ms(static_cast<double>(position_ms));
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_parameter_by_name(oae_event_instance instance, const char* name, float value, int ignore_seek_speed)
{
    return oae::api_call([&] {
        if (!name || !std::isfinite(value)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return inst->set_parameter(name, value, ignore_seek_speed != 0);
    });
}

oae_result OAE_CALL oae_event_instance_set_parameter_by_name_with_label(oae_event_instance instance, const char* name, const char* label, int ignore_seek_speed)
{
    return oae::api_call([&] {
        if (!name || !label) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return inst->set_parameter_with_label(name, label, ignore_seek_speed != 0);
    });
}

oae_result OAE_CALL oae_event_instance_get_parameter_by_name(oae_event_instance instance, const char* name, float* out_value, float* out_final_value)
{
    return oae::api_call([&] {
        if (!name || (!out_value && !out_final_value)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return inst->get_parameter(name, out_value, out_final_value);
    });
}

oae_result OAE_CALL oae_studio_set_global_parameter_by_name(oae_system system, const char* name, float value, int ignore_seek_speed)
{
    return oae::api_call([&] {
        if (!name || !std::isfinite(value)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return sys->set_global_parameter(name, value, ignore_seek_speed != 0);
    });
}

oae_result OAE_CALL oae_studio_set_global_parameter_by_name_with_label(oae_system system, const char* name, const char* label, int ignore_seek_speed)
{
    return oae::api_call([&] {
        if (!name || !label) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return sys->set_global_parameter_with_label(name, label, ignore_seek_speed != 0);
    });
}

oae_result OAE_CALL oae_studio_get_global_parameter_by_name(oae_system system, const char* name, float* out_value, float* out_final_value)
{
    return oae::api_call([&] {
        if (!name || (!out_value && !out_final_value)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return sys->get_global_parameter(name, out_value, out_final_value);
    });
}

oae_result OAE_CALL oae_event_description_load_sample_data(oae_event_description description)
{
    return oae::api_call([&] {
        auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto* sys = oae::runtime.systems.get(desc->system());
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (sys->in_callback()) {
            return OAE_ERR_INVALID_STATE;
        }
        return desc->load_sample_data();
    });
}

oae_result OAE_CALL oae_event_description_unload_sample_data(oae_event_description description)
{
    return oae::api_call([&] {
        auto* desc = oae::runtime.event_descriptions.get(description);
        if (!desc) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto* sys = oae::runtime.systems.get(desc->system());
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (sys->in_callback()) {
            return OAE_ERR_INVALID_STATE;
        }
        return desc->unload_sample_data();
    });
}

oae_result OAE_CALL oae_event_instance_get_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus)
{
    return oae::api_call([&] {
        if (!out_bus) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        const oae_bus bus = inst->track_bus(track_index);
        if (bus == 0) {
            return OAE_ERR_NOT_FOUND;
        }
        *out_bus = bus;
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_3d_attributes(oae_event_instance instance, const oae_3d_attributes* attributes)
{
    return oae::api_call([&] {
        if (!attributes || !oae::valid_3d_attributes(*attributes)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_3d_attributes(*attributes);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_3d_attributes(oae_event_instance instance, oae_3d_attributes* out_attributes)
{
    return oae::api_call([&] {
        if (!out_attributes) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_attributes = inst->get_3d_attributes();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_set_spatializer_settings(oae_event_instance instance, const oae_spatializer_settings* settings)
{
    return oae::api_call([&] {
        if (!settings || !oae::valid_spatializer_settings(*settings)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        inst->set_spatializer_settings(*settings);
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_event_instance_get_spatializer_settings(oae_event_instance instance, oae_spatializer_settings* out_settings)
{
    return oae::api_call([&] {
        if (!out_settings) {
            return OAE_ERR_INVALID_PARAM;
        }
        const auto* inst = oae::runtime.event_instances.get(instance);
        if (!inst) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_settings = inst->get_spatializer_settings();
        return OAE_OK;
    });
}

}
