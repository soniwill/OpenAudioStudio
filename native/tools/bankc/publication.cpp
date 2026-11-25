#include "publication.h"
#include "../../src/studio/bank_reader.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace oae::bankc {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;

std::uint32_t rotate_right(std::uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32 - bits));
}

std::string sha256(const Bytes& input)
{
    static constexpr std::uint32_t constants[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    std::array<std::uint32_t, 8> hash{
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    Bytes data = input;
    const auto bit_length = static_cast<std::uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back(static_cast<std::uint8_t>(bit_length >> (i * 8)));
    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            for (std::size_t j = 0; j < 4; ++j) words[i] = (words[i] << 8) | data[offset + i * 4 + j];
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto x = words[i - 15];
            const auto y = words[i - 2];
            words[i] = words[i - 16] + (rotate_right(x,7) ^ rotate_right(x,18) ^ (x >> 3)) +
                       words[i - 7] + (rotate_right(y,17) ^ rotate_right(y,19) ^ (y >> 10));
        }
        auto state = hash;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto s1 = rotate_right(state[4],6) ^ rotate_right(state[4],11) ^ rotate_right(state[4],25);
            const auto choose = (state[4] & state[5]) ^ (~state[4] & state[6]);
            const auto t1 = state[7] + s1 + choose + constants[i] + words[i];
            const auto s0 = rotate_right(state[0],2) ^ rotate_right(state[0],13) ^ rotate_right(state[0],22);
            const auto majority = (state[0] & state[1]) ^ (state[0] & state[2]) ^ (state[1] & state[2]);
            const auto t2 = s0 + majority;
            for (std::size_t j = 7; j > 0; --j) state[j] = state[j - 1];
            state[4] += t1;
            state[0] = t1 + t2;
        }
        for (std::size_t i = 0; i < 8; ++i) hash[i] += state[i];
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto word : hash) out << std::setw(8) << word;
    return out.str();
}

Bytes bytes(const std::string& value)
{
    return Bytes(value.begin(), value.end());
}

void require_plain_file(const fs::path& path)
{
    const auto status = fs::symlink_status(path);
    if (!fs::is_regular_file(status)) throw std::runtime_error("Expected a regular file: " + path.u8string());
    if (fs::hard_link_count(path) != 1) throw std::runtime_error("Hard-linked destination/artifact is unsupported: " + path.u8string());
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw std::runtime_error("Reparse artifact is unsupported: " + path.u8string());
#endif
}

Bytes read_bytes(const fs::path& path)
{
    require_plain_file(path);
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (!input || size < 0 || static_cast<std::uint64_t>(size) > static_cast<std::uint64_t>((std::numeric_limits<std::streamsize>::max)()))
        throw std::runtime_error("Cannot read file: " + path.u8string());
    Bytes result(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!result.empty()) input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    if (!input) throw std::runtime_error("Incomplete read: " + path.u8string());
    return result;
}

void write_bytes(const fs::path& path, const Bytes& value)
{
    if (fs::symlink_status(path).type() != fs::file_type::not_found) require_plain_file(path);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Cannot write file: " + path.u8string());
    if (!value.empty()) output.write(reinterpret_cast<const char*>(value.data()), static_cast<std::streamsize>(value.size()));
    output.flush();
    output.close();
    if (!output) throw std::runtime_error("Incomplete write: " + path.u8string());
}

bool exists_any(const fs::path& path)
{
    const auto status = fs::symlink_status(path);
    return status.type() != fs::file_type::not_found;
}

json fingerprint(const Bytes& value)
{
    return json{{"size", value.size()}, {"sha256", sha256(value)}};
}

json fingerprint_file(const fs::path& path)
{
    return exists_any(path) ? fingerprint(read_bytes(path)) : json(nullptr);
}

bool valid_fingerprint(const json& value, bool nullable)
{
    if (value.is_null()) return nullable;
    if (!value.is_object() || value.size() != 2 || !value.contains("size") || !value["size"].is_number_unsigned() ||
        !value.contains("sha256") || !value["sha256"].is_string()) return false;
    const auto digest = value["sha256"].get<std::string>();
    return digest.size() == 64 && std::all_of(digest.begin(), digest.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

void replace_file(const fs::path& from, const fs::path& to)
{
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Atomic replacement failed");
#else
    fs::rename(from, to);
#endif
}

bool valid_pair(const Bytes& bank, const Bytes& catalog)
{
    const auto parsed = json::parse(catalog.begin(), catalog.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object() || !parsed.contains("version") || parsed["version"] != 1 ||
        !parsed.contains("bank_size") || !parsed["bank_size"].is_number_unsigned() ||
        !parsed.contains("bank_crc32") || !parsed["bank_crc32"].is_number_unsigned() ||
        parsed["bank_size"].get<std::uint64_t>() != bank.size() ||
        parsed["bank_crc32"].get<std::uint64_t>() != calc_crc32(bank.data(), bank.size())) return false;
    BankParseResult result;
    return BankReader::parse(bank.data(), bank.size(), result) == OAE_OK;
}

std::string unique_id()
{
    static std::atomic<std::uint64_t> sequence{0};
    std::random_device random;
    std::ostringstream id;
    id << std::hex << std::chrono::steady_clock::now().time_since_epoch().count()
       << '-' << random() << '-' << sequence.fetch_add(1);
    return id.str();
}

fs::path normalized_destination(const fs::path& requested)
{
    auto absolute = fs::absolute(requested).lexically_normal();
    if (absolute.filename().empty() || absolute.filename() == "." || absolute.filename() == "..")
        throw std::runtime_error("Invalid bank destination");
    fs::create_directories(absolute.parent_path());
    auto parent = fs::canonical(absolute.parent_path());
#ifdef _WIN32
    HANDLE directory = CreateFileW(parent.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (directory == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot resolve destination directory");
    std::wstring resolved(32768, L'\0');
    const auto length = GetFinalPathNameByHandleW(directory, resolved.data(), static_cast<DWORD>(resolved.size()), FILE_NAME_NORMALIZED);
    CloseHandle(directory);
    if (length == 0 || length >= resolved.size()) throw std::runtime_error("Cannot resolve destination directory identity");
    resolved.resize(length);
    parent = fs::path(resolved);
    const auto filename = absolute.filename().wstring();
    if (filename.find(L':') != std::wstring::npos || filename.back() == L'.' || filename.back() == L' ')
        throw std::runtime_error("Ambiguous Windows destination name");
#endif
    auto result = parent / absolute.filename();
    if (exists_any(result)) {
        require_plain_file(result);
#ifdef _WIN32
        std::wstring long_name(32768, L'\0');
        const auto long_length = GetLongPathNameW(result.c_str(), long_name.data(), static_cast<DWORD>(long_name.size()));
        if (long_length == 0 || long_length >= long_name.size()) throw std::runtime_error("Cannot resolve destination alias");
        long_name.resize(long_length);
        result = fs::path(long_name);
#endif
    }
    return result;
}

std::string identity_for(const fs::path& path)
{
#ifdef _WIN32
    auto value = path.wstring();
    std::wstring lower(value.size(), L'\0');
    if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, value.data(), static_cast<int>(value.size()),
                      lower.data(), static_cast<int>(lower.size()), nullptr, nullptr, 0) == 0)
        throw std::runtime_error("Cannot normalize destination identity");
    return fs::path(lower).u8string();
#else
    return path.u8string();
#endif
}

void require_directory(const fs::path& path)
{
    if (!fs::is_directory(fs::symlink_status(path))) throw std::runtime_error("Unsafe staging directory: " + path.u8string());
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw std::runtime_error("Reparse staging directory is unsupported: " + path.u8string());
#endif
}

}

struct PublicationSession::Impl {
    fs::path requested;
    fs::path bank_path;
    fs::path catalog_path;
    fs::path stage_root;
    fs::path destination_stage;
    fs::path transaction;
    std::string identity;
    std::string key;
    std::ostream& diagnostics;
    PhaseHook hook;
    json journal;
    bool opened = false;
    bool prepared = false;
#ifdef _WIN32
    HANDLE lock = INVALID_HANDLE_VALUE;
#else
    int lock = -1;
#endif

    Impl(fs::path path, std::ostream& output, PhaseHook callback)
        : requested(std::move(path)), diagnostics(output), hook(std::move(callback)) {}

    ~Impl()
    {
#ifdef _WIN32
        if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
#else
        if (lock >= 0) close(lock);
#endif
    }

    void phase(const std::string& name)
    {
        if (hook) hook(name, transaction);
    }

    int error(int code, const std::string& message)
    {
        diagnostics << (code == 2 ? "DESTINATION_BUSY: " : code == 3 ? "RECOVERY_REQUIRED: " : "Error: ") << message;
        if (!transaction.empty()) diagnostics << " [transaction: " << transaction.u8string() << ']';
        diagnostics << '\n';
        return code;
    }

    int acquire()
    {
        const auto path = bank_path.parent_path() / (".bankc-lock-" + key);
        if (exists_any(path) && !fs::is_regular_file(fs::symlink_status(path)))
            return error(1, "Destination lock is not a regular file");
#ifdef _WIN32
        lock = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock == INVALID_HANDLE_VALUE) {
            const auto code = GetLastError();
            return error(code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION ? 2 : 1,
                         "Cannot acquire destination lock (" + std::to_string(code) + ")");
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(lock, &info) || info.nNumberOfLinks != 1 ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            return error(1, "Destination lock has an unsupported alias");
#else
        lock = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (lock < 0) return error(1, "Cannot open destination lock");
        if (flock(lock, LOCK_EX | LOCK_NB) != 0) return error(errno == EWOULDBLOCK ? 2 : 1, "Cannot acquire destination lock");
#endif
        return 0;
    }

    void save_journal(const std::string& state)
    {
        journal["state"] = state;
        write_bytes(transaction / "journal.tmp", bytes(journal.dump(2)));
        phase("before_journal_" + state);
        replace_file(transaction / "journal.tmp", transaction / "journal.json");
        if (state == "PREPARED") prepared = true;
        phase("after_journal_" + state);
    }

    void validate_journal()
    {
        if (!journal.is_object() || journal.value("version", 0) != 1 ||
            journal.value("identity", "") != identity || journal.value("transaction", "") != transaction.filename().u8string())
            throw std::runtime_error("Invalid journal identity/version");
        const auto state = journal.value("state", "");
        if (state != "PREPARED" && state != "CATALOG_PUBLISHED" && state != "COMMITTED" && state != "RECOVERY_REQUIRED")
            throw std::runtime_error("Invalid journal state");
        for (const auto* field : {"old_bank", "old_catalog", "new_bank", "new_catalog"}) {
            if (!journal.contains(field) || !valid_fingerprint(journal[field], field[0] == 'o'))
                throw std::runtime_error("Invalid journal fingerprint");
        }
        if (journal["old_bank"].is_null() != journal["old_catalog"].is_null())
            throw std::runtime_error("Inconsistent previous pair in journal");
    }

    void cleanup()
    {
        phase("before_cleanup");
        static const std::array<const char*, 7> allowed{
            "journal.json", "journal.tmp", "bank.new", "catalog.new", "catalog.old", "catalog.publish", "catalog.restore"
        };
        for (const auto& entry : fs::directory_iterator(transaction)) {
            const auto name = entry.path().filename().u8string();
            if (std::none_of(allowed.begin(), allowed.end(), [&](const char* item) { return name == item; }))
                throw std::runtime_error("Unknown staging entry preserved: " + name);
            require_plain_file(entry.path());
        }
        for (const auto* name : allowed) {
            if (std::string(name) != "journal.json") fs::remove(transaction / name);
        }
        fs::remove(transaction / "journal.json");
        fs::remove(transaction);
        std::error_code ec;
        fs::remove(destination_stage, ec);
        fs::remove(stage_root, ec);
        phase("after_cleanup");
        transaction.clear();
        prepared = false;
    }

    void restore_catalog(const Bytes& content)
    {
        write_bytes(transaction / "catalog.restore", content);
        replace_file(transaction / "catalog.restore", catalog_path);
    }

    int recover()
    {
        if (!exists_any(stage_root)) return 0;
        require_directory(stage_root);
        if (!exists_any(destination_stage)) return 0;
        require_directory(destination_stage);
        std::vector<fs::path> transactions;
        for (const auto& entry : fs::directory_iterator(destination_stage)) transactions.push_back(entry.path());
        if (transactions.empty()) return 0;
        if (transactions.size() != 1) return error(3, "Multiple or unknown transactions for destination");
        transaction = transactions.front();
        require_directory(transaction);
        const auto journal_path = exists_any(transaction / "journal.json") ? transaction / "journal.json" : transaction / "journal.tmp";
        if (!exists_any(journal_path)) return error(3, "Incomplete journal; staging preserved for inspection");
        const auto journal_bytes = read_bytes(journal_path);
        journal = json::parse(journal_bytes.begin(), journal_bytes.end());
        validate_journal();
        prepared = true;

        const auto current_bank = fingerprint_file(bank_path);
        const auto current_catalog = fingerprint_file(catalog_path);
        if (current_bank != journal["new_bank"] && current_bank != journal["old_bank"])
            return error(3, "Unknown bank version; refusing to overwrite it");
        if (!current_catalog.is_null() && current_catalog != journal["new_catalog"] && current_catalog != journal["old_catalog"])
            return error(3, "Unknown catalog version; refusing to overwrite it");

        if (current_bank == journal["new_bank"]) {
            if (current_catalog != journal["new_catalog"]) {
                const auto new_catalog = read_bytes(transaction / "catalog.new");
                if (fingerprint(new_catalog) != journal["new_catalog"]) return error(3, "New catalog backup does not match journal");
                restore_catalog(new_catalog);
            }
            if (!valid_pair(read_bytes(bank_path), read_bytes(catalog_path))) return error(3, "Recovered new pair is invalid");
            diagnostics << "Recovery completed new publication.\n";
        } else {
            if (journal["old_catalog"].is_null()) {
                if (!current_catalog.is_null()) fs::remove(catalog_path);
            } else if (current_catalog != journal["old_catalog"]) {
                if (current_catalog.is_null()) return error(3, "Previous catalog unexpectedly absent");
                const auto previous = read_bytes(transaction / "catalog.old");
                if (fingerprint(previous) != journal["old_catalog"]) return error(3, "Previous catalog backup does not match journal");
                restore_catalog(previous);
            }
            if (!journal["old_bank"].is_null() && !valid_pair(read_bytes(bank_path), read_bytes(catalog_path)))
                return error(3, "Recovered previous pair is invalid");
            diagnostics << "Recovery restored previous publication.\n";
        }
        try { cleanup(); }
        catch (const std::exception& ex) { diagnostics << "Warning: Recovery completed; staging cleanup pending: " << ex.what() << '\n'; }
        return 0;
    }

    int baseline()
    {
        const bool has_bank = exists_any(bank_path);
        const bool has_catalog = exists_any(catalog_path);
        if (has_bank) require_plain_file(bank_path);
        if (has_catalog) require_plain_file(catalog_path);
        if (has_bank != has_catalog) return error(3, "Existing bank/catalog pair is incomplete without a recoverable journal");
        if (has_bank && !valid_pair(read_bytes(bank_path), read_bytes(catalog_path)))
            return error(3, "Existing bank/catalog pair is inconsistent without a recoverable journal");
        return 0;
    }

    int open()
    {
        if (opened) return error(1, "Publication session already opened");
        try {
            bank_path = normalized_destination(requested);
            catalog_path = bank_path;
            catalog_path += ".catalog.json";
            identity = identity_for(bank_path);
            key = sha256(bytes(identity));
            stage_root = bank_path.parent_path() / ".bankc-stage";
            destination_stage = stage_root / key;
            const int lock_result = acquire();
            if (lock_result != 0) return lock_result;
            int result = 0;
            try { result = recover(); }
            catch (const std::exception& ex) { return error(3, ex.what()); }
            if (result != 0) return result;
            if (!transaction.empty()) return error(3, "Recovery cleanup is pending; refusing a new transaction");
            result = baseline();
            if (result != 0) return result;
            opened = true;
            return 0;
        } catch (const std::exception& ex) { return error(1, ex.what()); }
    }

    int publish(const Bytes& bank, const std::string& catalog)
    {
        if (!opened || !transaction.empty()) return error(1, "Publication session is not ready");
        const auto new_catalog = bytes(catalog);
        if (!valid_pair(bank, new_catalog)) return error(1, "Generated bank/catalog pair failed validation");
        try {
            const auto initial_result = baseline();
            if (initial_result != 0) return initial_result;
            fs::create_directory(stage_root);
            require_directory(stage_root);
            fs::create_directory(destination_stage);
            require_directory(destination_stage);
            do { transaction = destination_stage / unique_id(); } while (!fs::create_directory(transaction));
            journal = json{{"version", 1}, {"identity", identity}, {"transaction", transaction.filename().u8string()},
                           {"old_bank", fingerprint_file(bank_path)}, {"old_catalog", fingerprint_file(catalog_path)},
                           {"new_bank", fingerprint(bank)}, {"new_catalog", fingerprint(new_catalog)},
                           {"bank_crc32", calc_crc32(bank.data(), bank.size())}};
            write_bytes(transaction / "bank.new", bank);
            write_bytes(transaction / "catalog.new", new_catalog);
            write_bytes(transaction / "catalog.publish", new_catalog);
            if (!journal["old_catalog"].is_null()) write_bytes(transaction / "catalog.old", read_bytes(catalog_path));
            save_journal("PREPARED");
            phase("before_catalog_replace");
            replace_file(transaction / "catalog.publish", catalog_path);
            phase("after_catalog_replace");
            save_journal("CATALOG_PUBLISHED");
            phase("before_bank_replace");
            replace_file(transaction / "bank.new", bank_path);
            phase("after_bank_replace");
            if (fingerprint_file(bank_path) != journal["new_bank"] || fingerprint_file(catalog_path) != journal["new_catalog"])
                return error(3, "Published files changed unexpectedly");
            save_journal("COMMITTED");
            try { cleanup(); }
            catch (const std::exception& ex) { diagnostics << "Warning: Publication committed; staging cleanup pending: " << ex.what() << '\n'; }
            diagnostics << "Successfully published bank and catalog: " << bank_path.u8string() << '\n';
            return 0;
        } catch (const std::exception& ex) {
            diagnostics << "Error: Publication failed: " << ex.what() << '\n';
            if (!prepared) {
                if (!transaction.empty()) {
                    try { cleanup(); }
                    catch (const std::exception& cleanup_error) { diagnostics << "Warning: Staging preserved: " << cleanup_error.what() << '\n'; }
                }
                return 1;
            }
            try {
                phase("before_rollback");
                const auto current_bank = fingerprint_file(bank_path);
                const auto current_catalog = fingerprint_file(catalog_path);
                if (current_bank == journal["new_bank"] && current_catalog == journal["new_catalog"]) {
                    save_journal("COMMITTED");
                    diagnostics << "Publication committed despite metadata update error.\n";
                    try { cleanup(); } catch (const std::exception& cleanup_error) { diagnostics << "Warning: " << cleanup_error.what() << '\n'; }
                    return 0;
                }
                if (current_bank != journal["old_bank"] ||
                    (current_catalog != journal["old_catalog"] && current_catalog != journal["new_catalog"]))
                    throw std::runtime_error("Artifacts no longer match known pre-publication versions");
                if (current_catalog != journal["old_catalog"]) {
                    if (journal["old_catalog"].is_null()) fs::remove(catalog_path);
                    else {
                        const auto previous = read_bytes(transaction / "catalog.old");
                        if (fingerprint(previous) != journal["old_catalog"]) throw std::runtime_error("Previous catalog backup is invalid");
                        restore_catalog(previous);
                    }
                }
                phase("after_rollback");
                cleanup();
                return 1;
            } catch (const std::exception& rollback_error) {
                try { save_journal("RECOVERY_REQUIRED"); } catch (...) {}
                return error(3, std::string("Rollback failed after publication error: ") + rollback_error.what());
            }
        }
    }
};

PublicationSession::PublicationSession(fs::path destination, std::ostream& diagnostics, PhaseHook hook)
    : impl_(std::make_unique<Impl>(std::move(destination), diagnostics, std::move(hook))) {}

PublicationSession::~PublicationSession() = default;

int PublicationSession::open() { return impl_->open(); }

int PublicationSession::publish(const std::vector<std::uint8_t>& bank, const std::string& catalog)
{
    return impl_->publish(bank, catalog);
}

}
