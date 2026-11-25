#pragma once

#include "bank_format.h"
#include "../core/studio_model.h"

#include <string>
#include <vector>

namespace oae {

struct SampleEntry {
    std::vector<uint8_t> data;
    uint64_t length_frames = 0;
    uint32_t sample_rate = 48000;
    bool stream = false;
    uint64_t file_offset = 0;
    uint64_t file_length = 0;
};

struct BusEntry {
    std::string name;
    int32_t parent_index = -1;
    float volume = 1.0f;
    struct EffectDesc {
        int32_t position = 0;
        int32_t type = 0;
        bool bypass = false;
        int32_t sidechain_bus = -1;
        std::vector<float> params;
        float wet = 1.0f;
        float dry = 0.0f;
    };
    std::vector<EffectDesc> effects;
};

class BankWriter {
public:
    BankWriter() = default;

    void set_model_set(std::shared_ptr<const ModelSet> set) { model_set_ = std::move(set); }
    void add_sample(SampleEntry sample) { samples_.push_back(std::move(sample)); }
    void add_bus(BusEntry bus) { buses_.push_back(std::move(bus)); }

    std::vector<uint8_t> write();

private:
    uint32_t intern_string(const std::string& str);

    std::shared_ptr<const ModelSet> model_set_;
    std::vector<SampleEntry> samples_;
    std::vector<BusEntry> buses_;
    std::vector<std::string> string_table_;
    std::unordered_map<std::string, uint32_t> string_to_index_;
};

}
