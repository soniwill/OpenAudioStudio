#include <nlohmann/json.hpp>
#include "publication.h"
#include "../../src/studio/bank_writer.h"
#include "../../src/studio/bank_format.h"
#include "../../src/core/studio_model.h"

#define MINIAUDIO_IMPLEMENTATION_NONE
#include <miniaudio.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

void print_usage(const char* exe_name) {
    std::cout << "OpenAudioEngine Bank Compiler (oae_bankc) v1.0\n"
              << "Usage: " << exe_name << " <project.json> -o <out.bank> [options]\n\n"
              << "       " << exe_name << " --recover <out.bank>\n\n"
              << "Options:\n"
              << "  --verbose            Display compiled event paths\n"
              << "  -o, --output <path>   Output .bank file path (required)\n"
              << "  -v, --version         Display compiler version\n"
              << "  -h, --help            Display this help message\n";
}

bool read_file_bytes(const fs::path& path, std::vector<uint8_t>& out_bytes) {
    std::ifstream is(path, std::ios::binary | std::ios::ate);
    if (!is.is_open()) {
        return false;
    }
    const auto size = is.tellg();
    if (size <= 0) {
        return false;
    }
    out_bytes.resize(static_cast<size_t>(size));
    is.seekg(0, std::ios::beg);
    is.read(reinterpret_cast<char*>(out_bytes.data()), size);
    return is.gcount() == static_cast<std::streamsize>(size);
}

oae::Guid make_deterministic_guid(const std::string& path) {
    uint64_t hi = 0xCBF29CE484222325ULL;
    uint64_t lo = 0x100000001B3ULL;
    for (char c : path) {
        hi ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hi *= 0x100000001B3ULL;
        lo ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        lo = (lo << 13) | (lo >> (64 - 13));
    }
    if (hi == 0 && lo == 0) {
        lo = 1;
    }
    return { hi, lo };
}

} // namespace

#include "project_parser.h"

static int run(const std::vector<std::string>& args) {
    const auto argc = args.size();
    if (argc > 1 && args[1] == "--recover") {
        if (argc != 3) {
            std::cerr << "Usage: oae_bankc --recover <out.bank>\n";
            return 1;
        }
        oae::bankc::PublicationSession recovery(fs::u8path(args[2]), std::cerr);
        return recovery.open();
    }
    if (argc < 2) {
        print_usage(args[0].c_str());
        return 1;
    }

    std::string project_path_str;
    std::string output_path_str;
    bool verbose = false;

    for (std::size_t i = 1; i < argc; ++i) {
        std::string arg = args[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(args[0].c_str());
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "oae_bankc version 1.0.0 (ABI v1)\n";
            return 0;
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_path_str = args[++i];
            } else {
                std::cerr << "Error: Missing argument for output file.\n";
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            if (project_path_str.empty()) {
                project_path_str = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'.\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown flag '" << arg << "'.\n";
            return 1;
        }
    }

    if (project_path_str.empty()) {
        std::cerr << "Error: No project JSON file specified.\n";
        return 1;
    }
    if (output_path_str.empty()) {
        std::cerr << "Error: No output .bank path specified (use -o <out.bank>).\n";
        return 1;
    }

    const fs::path final_bank = fs::absolute(fs::u8path(output_path_str));
    oae::bankc::PublicationSession publication(final_bank, std::cerr);
    if (const int status = publication.open(); status != 0) return status;

    const fs::path project_path = fs::absolute(fs::u8path(project_path_str));
    if (!fs::exists(project_path)) {
        std::cerr << "Error: Project file not found: " << project_path_str << "\n";
        return 1;
    }

    const fs::path project_dir = project_path.parent_path();

    std::ifstream project_file(project_path);
    if (!project_file.is_open()) {
        std::cerr << "Error: Could not open project file: " << project_path_str << "\n";
        return 1;
    }

    json root;
    try {
        std::vector<std::set<std::string>> object_keys;
        root = json::parse(project_file, [&](int, json::parse_event_t event, json& parsed) {
            if (event == json::parse_event_t::object_start) object_keys.emplace_back();
            else if (event == json::parse_event_t::object_end) object_keys.pop_back();
            else if (event == json::parse_event_t::key && !object_keys.back().insert(parsed.get<std::string>()).second)
                bankc::fail("/" + bankc::pointer_key(parsed.get<std::string>()), "Duplicate JSON key.");
            return true;
        });
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse project JSON: " << e.what() << "\n";
        return 1;
    }

    oae::BankWriter writer;
    std::unordered_map<std::string, oae::Guid> event_path_to_guid;
    std::vector<uint8_t> bank_bytes;
    try {
        bankc::ProjectParser(root, project_dir).compile(writer, event_path_to_guid);
        bank_bytes = writer.write();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    const uint32_t bank_crc = oae::calc_crc32(bank_bytes.data(), bank_bytes.size());

    // 9. Catalog JSON
    json catalog;
    catalog["version"] = 1;
    catalog["bank_size"] = bank_bytes.size();
    catalog["bank_crc32"] = bank_crc;
    catalog["events"] = json::array();

    std::vector<std::string> event_paths;
    event_paths.reserve(event_path_to_guid.size());
    for (const auto& pair : event_path_to_guid) event_paths.push_back(pair.first);
    std::sort(event_paths.begin(), event_paths.end());
    for (const auto& path : event_paths) {
        json ev_entry;
        ev_entry["path"] = path;
        ev_entry["kind"] = path.rfind("snapshot:/", 0) == 0 ? "snapshot" : "event";
        catalog["events"].push_back(ev_entry);
    }

    const std::string catalog_str = catalog.dump(2);

    if (const int status = publication.publish(bank_bytes, catalog_str); status != 0) return status;
    if (verbose) {
        for (const auto& path : event_paths) std::cout << path << "\n";
    }
    std::cout << "Successfully compiled bank: " << final_bank.u8string()
              << " (" << bank_bytes.size() << " bytes, CRC32: 0x" << std::hex << bank_crc << std::dec << ")\n";
    return 0;
}

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) {
#if defined(_WIN32)
            args.push_back(fs::path(argv[i]).u8string());
#else
            args.emplace_back(argv[i]);
#endif
        }
        return run(args);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}