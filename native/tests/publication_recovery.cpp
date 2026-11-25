#include <doctest.h>
#include "../tools/bankc/publication.h"
#include "../src/studio/bank_writer.h"
#include "../src/studio/bank_format.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thread>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using oae::bankc::PublicationSession;
using json = nlohmann::json;

struct Pair {
    std::vector<std::uint8_t> bank;
    std::string catalog;

    explicit Pair(const std::string& event)
    {
        oae::ModelSetBuilder builder;
        builder.add_event(oae::EventBuilder(event));
        oae::BankWriter writer;
        writer.set_model_set(builder.build());
        bank = writer.write();
        catalog = json{{"version", 1}, {"bank_size", bank.size()},
            {"bank_crc32", oae::calc_crc32(bank.data(), bank.size())}, {"events", json::array()}}.dump(2);
    }
};

struct TempDirectory {
    fs::path path;
    TempDirectory()
    {
        path = fs::temp_directory_path() / ("oae_publication_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        REQUIRE(fs::create_directory(path));
    }
    ~TempDirectory()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string contents(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    return std::string(std::istreambuf_iterator<char>(input), {});
}

void overwrite(const fs::path& path, const std::string& value)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output << value;
    output.close();
    REQUIRE(output.good());
}

fs::path catalog_path(fs::path bank)
{
    bank += ".catalog.json";
    return bank;
}

void check_pair(const fs::path& bank, const Pair& expected)
{
    CHECK(contents(bank) == std::string(expected.bank.begin(), expected.bank.end()));
    CHECK(contents(catalog_path(bank)) == expected.catalog);
}

void publish(const fs::path& bank, const Pair& pair)
{
    std::ostringstream log;
    PublicationSession session(bank, log);
    const auto opened = session.open();
    INFO(log.str());
    REQUIRE(opened == 0);
    const auto result = session.publish(pair.bank, pair.catalog);
    INFO(log.str());
    REQUIRE(result == 0);
}

fs::path journal_for(const fs::path& directory)
{
    for (const auto& entry : fs::recursive_directory_iterator(directory / ".bankc-stage")) {
        if (entry.path().filename() == "journal.json") return entry.path();
    }
    return {};
}

struct Process {
#ifdef _WIN32
    PROCESS_INFORMATION info{};
    Process(const fs::path& bank, const std::string& mode, const std::string& phase)
    {
        std::wstring module(32768, L'\0');
        const auto count = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        REQUIRE(count > 0);
        module.resize(count);
        const auto helper = fs::path(module).parent_path() / "oae_publication_probe.exe";
        REQUIRE(fs::exists(helper));
        std::wstring command = L"\"" + helper.wstring() + L"\" " + fs::u8path(mode).wstring() + L" \"" +
            bank.wstring() + L"\" " + fs::u8path(phase).wstring();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        REQUIRE(CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &startup, &info) != FALSE);
    }
    int wait()
    {
        REQUIRE(WaitForSingleObject(info.hProcess, 10000) == WAIT_OBJECT_0);
        DWORD code = 0;
        REQUIRE(GetExitCodeProcess(info.hProcess, &code) != FALSE);
        return static_cast<int>(code);
    }
    void terminate() { REQUIRE(TerminateProcess(info.hProcess, 72) != FALSE); }
    ~Process()
    {
        if (info.hProcess) {
            if (WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT) TerminateProcess(info.hProcess, 73);
            CloseHandle(info.hProcess);
        }
        if (info.hThread) CloseHandle(info.hThread);
    }
#else
    pid_t pid = -1;
    Process(const fs::path& bank, const std::string& mode, const std::string& phase)
    {
        const auto helper = fs::canonical("/proc/self/exe").parent_path() / "oae_publication_probe";
        REQUIRE(fs::exists(helper));
        pid = fork();
        REQUIRE(pid >= 0);
        if (pid == 0) {
            execl(helper.c_str(), helper.c_str(), mode.c_str(), bank.c_str(), phase.c_str(), nullptr);
            std::_Exit(99);
        }
    }
    int wait()
    {
        int status = 0;
        REQUIRE(waitpid(pid, &status, 0) == pid);
        pid = -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : 72;
    }
    void terminate() { REQUIRE(kill(pid, SIGKILL) == 0); }
    ~Process() { if (pid > 0) { kill(pid, SIGKILL); int status = 0; waitpid(pid, &status, 0); } }
#endif
};

}

TEST_CASE("T13: Publication lock excludes processes and path aliases but permits distinct destinations")
{
    TempDirectory temp;
    const auto bank = temp.path / "master.bank";
    Process owner(bank, "hold", "none");
    for (int retry = 0; retry < 200 && !fs::exists(temp.path / "ready.flag"); ++retry)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(fs::exists(temp.path / "ready.flag"));
    std::ostringstream log;
    {
        PublicationSession busy(bank, log);
        CHECK(busy.open() == 2);
        PublicationSession alias(temp.path / "." / "master.bank", log);
        CHECK(alias.open() == 2);
#ifdef _WIN32
        PublicationSession different_case(temp.path / "MASTER.bank", log);
        CHECK(different_case.open() == 2);
#endif
        PublicationSession independent(temp.path / "music.bank", log);
        REQUIRE(independent.open() == 0);
        Pair pair("event:/independent");
        REQUIRE(independent.publish(pair.bank, pair.catalog) == 0);
    }
    owner.terminate();
    CHECK(owner.wait() == 72);
    PublicationSession next(bank, log);
    CHECK(next.open() == 0);
}

TEST_CASE("T13: Process interruption at publication boundaries recovers deterministically")
{
    const Pair old_pair("event:/publication_old");
    const Pair new_pair("event:/publication_new");
    const std::vector<std::string> phases{
        "before_journal_PREPARED", "after_journal_PREPARED", "before_catalog_replace", "after_catalog_replace",
        "before_journal_CATALOG_PUBLISHED", "after_journal_CATALOG_PUBLISHED", "before_bank_replace",
        "after_bank_replace", "before_journal_COMMITTED", "after_journal_COMMITTED", "before_cleanup", "after_cleanup"
    };
    for (const bool existing : {false, true}) {
        for (std::size_t i = 0; i < phases.size(); ++i) {
            CAPTURE(existing);
            CAPTURE(phases[i]);
            TempDirectory temp;
            const auto bank = temp.path / "master.bank";
            if (existing) publish(bank, old_pair);
            Process crashed(bank, "publish", phases[i]);
            REQUIRE(crashed.wait() == 71);
            std::ostringstream log;
            PublicationSession recovery(bank, log);
            const auto result = recovery.open();
            INFO(log.str());
            REQUIRE(result == 0);
            if (i >= 7) check_pair(bank, new_pair);
            else if (existing) check_pair(bank, old_pair);
            else {
                CHECK(!fs::exists(bank));
                CHECK(!fs::exists(catalog_path(bank)));
            }
            CHECK(!fs::exists(temp.path / ".bankc-stage"));
        }
    }
}

TEST_CASE("T13: Failed publication rolls back and rollback failure retains recoverable evidence")
{
    TempDirectory temp;
    const auto bank = temp.path / "master.bank";
    const Pair old_pair("event:/old");
    const Pair new_pair("event:/new");
    publish(bank, old_pair);
    std::ostringstream log;
    SUBCASE("Bank rename failure restores previous pair") {
        PublicationSession session(bank, log, [](const std::string& phase, const fs::path&) {
            if (phase == "before_bank_replace") throw std::runtime_error("Injected bank rename error");
        });
        REQUIRE(session.open() == 0);
        CHECK(session.publish(new_pair.bank, new_pair.catalog) == 1);
        check_pair(bank, old_pair);
        CHECK(!fs::exists(temp.path / ".bankc-stage"));
    }
    SUBCASE("First publication removes newly published catalog on error") {
        const auto first_bank = temp.path / "first.bank";
        PublicationSession session(first_bank, log, [](const std::string& phase, const fs::path&) {
            if (phase == "before_bank_replace") throw std::runtime_error("Injected bank rename error");
        });
        REQUIRE(session.open() == 0);
        CHECK(session.publish(new_pair.bank, new_pair.catalog) == 1);
        CHECK(!fs::exists(first_bank));
        CHECK(!fs::exists(catalog_path(first_bank)));
    }
    SUBCASE("Rollback failure preserves backups until successful recovery") {
        {
            PublicationSession session(bank, log, [](const std::string& phase, const fs::path&) {
                if (phase == "before_bank_replace" || phase == "before_rollback") throw std::runtime_error("Injected filesystem error");
            });
            REQUIRE(session.open() == 0);
            CHECK(session.publish(new_pair.bank, new_pair.catalog) == 3);
        }
        const auto journal = journal_for(temp.path);
        REQUIRE(!journal.empty());
        CHECK(fs::exists(journal.parent_path() / "catalog.old"));
        CHECK(fs::exists(journal.parent_path() / "catalog.new"));
        CHECK(json::parse(contents(journal))["state"] == "RECOVERY_REQUIRED");
        PublicationSession recovery(bank, log);
        REQUIRE(recovery.open() == 0);
        check_pair(bank, old_pair);
    }
    SUBCASE("Catalog rename failure leaves previous pair unchanged") {
        PublicationSession session(bank, log, [](const std::string& phase, const fs::path&) {
            if (phase == "before_catalog_replace") throw std::runtime_error("Injected catalog rename error");
        });
        REQUIRE(session.open() == 0);
        CHECK(session.publish(new_pair.bank, new_pair.catalog) == 1);
        check_pair(bank, old_pair);
    }
}

TEST_CASE("T13: Recovery refuses unknown artifacts, missing backup and inconsistent baseline")
{
    TempDirectory temp;
    const auto bank = temp.path / "master.bank";
    Pair old_pair("event:/old");
    publish(bank, old_pair);
    Process crashed(bank, "publish", "after_catalog_replace");
    REQUIRE(crashed.wait() == 71);
    const auto journal = journal_for(temp.path);
    REQUIRE(!journal.empty());
    SUBCASE("Unknown bank") { overwrite(bank, "unknown bank"); }
    SUBCASE("Unknown catalog") { overwrite(catalog_path(bank), "unknown catalog"); }
    SUBCASE("Missing backup") { REQUIRE(fs::remove(journal.parent_path() / "catalog.old")); }
    SUBCASE("Invalid journal") { overwrite(journal, "{}"); }
    SUBCASE("Multiple transactions") { REQUIRE(fs::create_directory(journal.parent_path().parent_path() / "unknown")); }
    const auto before_bank = contents(bank);
    const auto before_catalog = contents(catalog_path(bank));
    std::ostringstream log;
    PublicationSession recovery(bank, log);
    CHECK(recovery.open() == 3);
    CHECK(contents(bank) == before_bank);
    CHECK(contents(catalog_path(bank)) == before_catalog);
    CHECK(fs::exists(journal));

    const auto unmanaged = temp.path / "unmanaged.bank";
    overwrite(unmanaged, "orphan bank");
    PublicationSession unmatched(unmanaged, log);
    CHECK(unmatched.open() == 3);
    CHECK(contents(unmanaged) == "orphan bank");
}

TEST_CASE("T13: Recovery completes catalog for committed bank and handles identical bank republication")
{
    TempDirectory temp;
    const auto bank = temp.path / "master.bank";
    const Pair old_pair("event:/old");
    const Pair new_pair("event:/publication_new");
    publish(bank, old_pair);
    SUBCASE("Committed bank with absent catalog is repaired") {
        Process crashed(bank, "publish", "after_bank_replace");
        REQUIRE(crashed.wait() == 71);
        REQUIRE(fs::remove(catalog_path(bank)));
        std::ostringstream log;
        PublicationSession recovery(bank, log);
        REQUIRE(recovery.open() == 0);
        check_pair(bank, new_pair);
    }
    SUBCASE("Identical bank prefers new known catalog") {
        auto changed = old_pair;
        auto catalog = json::parse(changed.catalog);
        catalog["events"] = json::array({{{"path", "event:/metadata"}}});
        changed.catalog = catalog.dump(2);
        {
            std::ostringstream log;
            PublicationSession interrupted(bank, log, [](const std::string& phase, const fs::path&) {
                if (phase == "before_catalog_replace" || phase == "before_rollback") throw std::runtime_error("Injected interruption");
            });
            REQUIRE(interrupted.open() == 0);
            REQUIRE(interrupted.publish(changed.bank, changed.catalog) == 3);
        }
        std::ostringstream log;
        PublicationSession recovery(bank, log);
        REQUIRE(recovery.open() == 0);
        check_pair(bank, changed);
    }
    SUBCASE("Cleanup failure leaves committed pair and next recovery cleans") {
        {
            std::ostringstream log;
            PublicationSession session(bank, log, [](const std::string& phase, const fs::path&) {
                if (phase == "before_cleanup") throw std::runtime_error("Injected cleanup error");
            });
            REQUIRE(session.open() == 0);
            REQUIRE(session.publish(new_pair.bank, new_pair.catalog) == 0);
            check_pair(bank, new_pair);
        }
        std::ostringstream log;
        PublicationSession recovery(bank, log);
        REQUIRE(recovery.open() == 0);
        check_pair(bank, new_pair);
        CHECK(!fs::exists(temp.path / ".bankc-stage"));
    }
}
