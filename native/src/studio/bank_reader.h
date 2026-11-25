#pragma once

#include "bank_format.h"
#include "bank_writer.h"
#include "../core/studio_model.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace oae {

struct BankParseResult {
    std::shared_ptr<const ModelSet> model_set;
    std::vector<SampleEntry> samples;
    std::vector<BusEntry> buses;
};

class BankReader {
public:
    static oae_result parse(const uint8_t* data, size_t size, BankParseResult& out_result) noexcept;
    static oae_result parse_file(const std::filesystem::path& path, BankParseResult& out_result) noexcept;
};

}
