#include "runtime.h"
#include "arguments.h"
#include "../studio/bank_reader.h"
#include "../core/bank.h"
#include "../core/sound.h"

#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>

namespace {

oae_result load_bank_parsed_internal(oae::System& sys, oae::BankParseResult& parsed, const std::filesystem::path& bank_path, oae_bank* out_bank) noexcept
{
    std::vector<oae::BankSampleEntry> sample_entries;
    std::vector<oae_sound> created_sounds;
    for (auto& sample : parsed.samples) {
        oae::BankSampleEntry entry;
        entry.data = std::move(sample.data);
        entry.length_frames = sample.length_frames;
        entry.sample_rate = sample.sample_rate;
        entry.stream = sample.stream && !bank_path.empty();
        entry.stream_path = bank_path;
        entry.stream_offset = sample.file_offset;
        entry.stream_length = sample.file_length;
        entry.sound = 0;
        entry.pin_count = 1;

        if (entry.stream && entry.stream_length > 0) {
            auto sdata = std::make_shared<oae::SoundData>();
            if (oae::load_bank_slice(entry.stream_path, entry.stream_offset, entry.stream_length, *sdata) == OAE_OK) {
                entry.sound = oae::runtime.sounds.insert(std::make_unique<oae::Sound>(oae::Sound{&sys, std::move(sdata)}));
            }
        } else if (!entry.data.empty()) {
            oae_sound snd = 0;
            oae_sound_create_from_memory(sys.handle(), entry.data.data(), entry.data.size(), OAE_SOUND_DEFAULT, &snd);
            entry.sound = snd;
        }
        created_sounds.push_back(entry.sound);
        sample_entries.push_back(std::move(entry));
    }

    // Bind created sounds to instruments in the model set
    auto model_set = parsed.model_set;
    if (model_set && !created_sounds.empty()) {
        oae::ModelSetBuilder b;
        for (auto ev : model_set->events()) {
            for (auto& trk : ev.tracks) {
                for (auto& inst : trk.instruments) {
                    if (inst.sound_handle == 0 && inst.sample_index >= 0 && static_cast<size_t>(inst.sample_index) < created_sounds.size()) {
                        inst.sound_handle = created_sounds[static_cast<size_t>(inst.sample_index)];
                    }
                    for (auto& pe : inst.playlist) {
                        if (pe.sound_handle == 0 && pe.sample_index >= 0 && static_cast<size_t>(pe.sample_index) < created_sounds.size()) {
                            pe.sound_handle = created_sounds[static_cast<size_t>(pe.sample_index)];
                        }
                    }
                }
            }
            b.add_event(std::move(ev));
        }
        for (const auto& gp : model_set->global_parameters()) {
            b.add_global_parameter(gp);
        }
        for (const auto& ea : model_set->effect_automations()) {
            b.add_effect_automation(ea);
        }
        for (const auto& v : model_set->vcas()) {
            b.add_vca(v);
        }
        for (const auto& at : model_set->audio_table()) {
            oae::AudioTableEntry entry = at;
            if (entry.sound_handle == 0 && entry.sample_index >= 0 && static_cast<size_t>(entry.sample_index) < created_sounds.size()) {
                entry.sound_handle = created_sounds[static_cast<size_t>(entry.sample_index)];
            }
            b.add_audio_table_entry(std::move(entry));
        }
        model_set = b.build();
    }

    auto bank_obj = std::make_unique<oae::Bank>(sys, std::move(model_set), std::move(sample_entries), false);
    const auto mix_res = bank_obj->attach_mixer(parsed.buses);
    if (mix_res != OAE_OK) {
        return mix_res;
    }
    bank_obj->set_loading_state(OAE_BANK_LOADED);

    const auto handle = oae::runtime.banks.insert(std::move(bank_obj));
    if (handle == 0) {
        return OAE_ERR_OUT_OF_MEMORY;
    }

    *out_bank = handle;
    return OAE_OK;
}

void run_async_parsed_load(oae::Bank* bank_ptr, oae_system sys_handle, oae::BankParseResult parsed, const std::filesystem::path& bank_path)
{
    std::vector<oae::BankSampleEntry> sample_entries;
    for (auto& sample : parsed.samples) {
        oae::BankSampleEntry entry;
        entry.data = std::move(sample.data);
        entry.length_frames = sample.length_frames;
        entry.sample_rate = sample.sample_rate;
        entry.stream = sample.stream && !bank_path.empty();
        entry.stream_path = bank_path;
        entry.stream_offset = sample.file_offset;
        entry.stream_length = sample.file_length;
        entry.sound = 0;
        entry.pin_count = 0;
        sample_entries.push_back(std::move(entry));
    }

    bank_ptr->set_samples(std::move(sample_entries));
    bank_ptr->set_model_set(parsed.model_set);

    auto* sys = oae::runtime.systems.get(sys_handle);
    if (sys) {
        const auto mix_res = bank_ptr->attach_mixer(parsed.buses);
        if (mix_res != OAE_OK) {
            bank_ptr->set_loading_state(OAE_BANK_FAILED);
            return;
        }
        if (parsed.model_set) {
            sys->register_model_set(parsed.model_set);
        }
    }

    bank_ptr->set_loading_state(OAE_BANK_LOADED);
}

void run_async_memory_load(oae::Bank* bank_ptr, oae_system sys_handle, std::vector<uint8_t> buffer)
{
    oae::BankParseResult parsed;
    const auto parse_res = oae::BankReader::parse(buffer.data(), buffer.size(), parsed);
    if (parse_res != OAE_OK) {
        bank_ptr->set_loading_state(OAE_BANK_FAILED);
        return;
    }
    run_async_parsed_load(bank_ptr, sys_handle, std::move(parsed), {});
}

}

extern "C" {

oae_result OAE_CALL oae_studio_load_bank_memory_ex(oae_system system, const void* bytes, size_t size, uint32_t flags, oae_bank* out_bank)
{
    return oae::api_call([&] {
        if (!out_bank || !bytes || size == 0) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }

        if (flags & OAE_LOAD_BANK_NONBLOCKING) {
            auto bank_obj = std::make_unique<oae::Bank>(*sys, nullptr, std::vector<oae::BankSampleEntry>{}, true);
            bank_obj->set_loading_state(OAE_BANK_LOADING);
            auto* bank_ptr = bank_obj.get();

            const auto handle = oae::runtime.banks.insert(std::move(bank_obj));
            if (handle == 0) {
                return OAE_ERR_OUT_OF_MEMORY;
            }
            *out_bank = handle;

            std::vector<uint8_t> buffer(static_cast<const uint8_t*>(bytes), static_cast<const uint8_t*>(bytes) + size);
            std::thread worker(run_async_memory_load, bank_ptr, system, std::move(buffer));
            bank_ptr->set_async_worker(std::move(worker));

            return OAE_OK;
        }

        oae::BankParseResult parsed;
        const auto parse_res = oae::BankReader::parse(static_cast<const uint8_t*>(bytes), size, parsed);
        if (parse_res != OAE_OK) {
            return parse_res;
        }
        return load_bank_parsed_internal(*sys, parsed, {}, out_bank);
    });
}

oae_result OAE_CALL oae_studio_load_bank_memory(oae_system system, const void* bytes, size_t size, oae_bank* out_bank)
{
    return oae_studio_load_bank_memory_ex(system, bytes, size, 0, out_bank);
}

oae_result OAE_CALL oae_studio_load_bank_file_ex(oae_system system, const char* path, uint32_t flags, oae_bank* out_bank)
{
    return oae::api_call([&] {
        if (!out_bank || !oae::valid_utf8_argument(path) || !*path) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }

        if (flags & OAE_LOAD_BANK_NONBLOCKING) {
            auto bank_obj = std::make_unique<oae::Bank>(*sys, nullptr, std::vector<oae::BankSampleEntry>{}, true);
            bank_obj->set_loading_state(OAE_BANK_LOADING);
            auto* bank_ptr = bank_obj.get();

            const auto handle = oae::runtime.banks.insert(std::move(bank_obj));
            if (handle == 0) {
                return OAE_ERR_OUT_OF_MEMORY;
            }
            *out_bank = handle;

            std::filesystem::path file_path = std::filesystem::u8path(path);
            std::thread worker([bank_ptr, system, file_path]() {
                oae::BankParseResult parsed;
                if (oae::BankReader::parse_file(file_path, parsed) != OAE_OK) {
                    bank_ptr->set_loading_state(OAE_BANK_FAILED);
                    return;
                }
                run_async_parsed_load(bank_ptr, system, std::move(parsed), file_path);
            });
            bank_ptr->set_async_worker(std::move(worker));

            return OAE_OK;
        }

        const auto file_path = std::filesystem::u8path(path);
        oae::BankParseResult parsed;
        const auto parse_res = oae::BankReader::parse_file(file_path, parsed);
        if (parse_res != OAE_OK) {
            return parse_res;
        }
        return load_bank_parsed_internal(*sys, parsed, file_path, out_bank);
    });
}

oae_result OAE_CALL oae_studio_load_bank_file(oae_system system, const char* path, oae_bank* out_bank)
{
    return oae_studio_load_bank_file_ex(system, path, 0, out_bank);
}

oae_result OAE_CALL oae_studio_get_bank_loading_state(oae_system system, oae_bank bank, int* out_state)
{
    return oae::api_call([&] {
        if (!out_state) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        auto* b = oae::runtime.banks.get(bank);
        if (!b || b->owner() != sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_state = b->loading_state();
        return OAE_OK;
    });
}

oae_result OAE_CALL oae_studio_unload_bank(oae_system system, oae_bank bank)
{
    return oae::api_call([&] {
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        if (sys->in_callback()) {
            return OAE_ERR_INVALID_STATE;
        }
        auto* b = oae::runtime.banks.get(bank);
        if (!b || b->owner() != sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        b->unload();
        oae::runtime.banks.erase(bank);
        return OAE_OK;
    });
}

}
