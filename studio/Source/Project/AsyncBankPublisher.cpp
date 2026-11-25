#include "AsyncBankPublisher.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <atomic>
#include <chrono>
#include <algorithm>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace oae::studio {

namespace {

bool write_unique_snapshot(const fs::path& directory, const std::string& content, fs::path& snapshot_path)
{
    static std::atomic<std::uint64_t> sequence{0};
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        const auto filename = ".snapshot-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                              "-" + std::to_string(sequence.fetch_add(1)) + ".json";
        const auto candidate = directory / filename;
#ifdef _WIN32
        HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS || GetLastError() == ERROR_ALREADY_EXISTS) continue;
            return false;
        }
        snapshot_path = candidate;
        bool okay = true;
        std::size_t offset = 0;
        while (offset < content.size()) {
            const auto count = static_cast<DWORD>((std::min)(content.size() - offset, static_cast<std::size_t>(MAXDWORD)));
            DWORD written = 0;
            if (!WriteFile(file, content.data() + offset, count, &written, nullptr) || written != count) {
                okay = false;
                break;
            }
            offset += written;
        }
        if (!FlushFileBuffers(file)) okay = false;
        if (!CloseHandle(file)) okay = false;
#else
        const int file = ::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (file < 0) {
            if (errno == EEXIST) continue;
            return false;
        }
        snapshot_path = candidate;
        bool okay = true;
        std::size_t offset = 0;
        while (offset < content.size()) {
            const auto written = ::write(file, content.data() + offset, content.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { okay = false; break; }
            offset += static_cast<std::size_t>(written);
        }
        if (::close(file) != 0) okay = false;
#endif
        if (!okay) {
            std::error_code ec;
            fs::remove(snapshot_path, ec);
        }
        return okay;
    }
    return false;
}

#ifdef _WIN32
bool run_command_capture_output(const std::wstring& cmdline, int& out_exit_code, std::string& out_output)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;

    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return false;
    }

    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdline_buf(cmdline.begin(), cmdline.end());
    cmdline_buf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdline_buf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(write_pipe);

    if (!success) {
        CloseHandle(read_pipe);
        return false;
    }

    std::string captured;
    char buffer[1024];
    DWORD bytes_read = 0;
    while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        captured.append(buffer, bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_exit_code = static_cast<int>(exit_code);
    out_output = std::move(captured);

    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}
#endif

} // namespace

AsyncBankPublisher::AsyncBankPublisher() = default;

AsyncBankPublisher::~AsyncBankPublisher()
{
    wait_until_finished();
}

void AsyncBankPublisher::wait_until_finished()
{
    if (worker_.joinable()) {
        worker_.join();
    }
}

PublishResult AsyncBankPublisher::get_last_result() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_result_;
}

std::string AsyncBankPublisher::get_diagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_result_.diagnostics;
}

bool AsyncBankPublisher::start_publish(const fs::path& bankc_exe,
                                       const nlohmann::json& project_snapshot,
                                       const fs::path& project_dir,
                                       const fs::path& output_bank_path,
                                       std::function<void(const PublishResult&)> on_complete)
{
    if (running_.load()) {
        return false;
    }

    wait_until_finished();
    running_.store(true);

    worker_ = std::thread(&AsyncBankPublisher::worker_thread,
                          this,
                          bankc_exe,
                          project_snapshot,
                          project_dir,
                          output_bank_path,
                          on_complete);
    return true;
}

void AsyncBankPublisher::worker_thread(fs::path bankc_exe,
                                       nlohmann::json snapshot,
                                       fs::path project_dir,
                                       fs::path output_bank_path,
                                       std::function<void(const PublishResult&)> on_complete)
{
    PublishResult result;
    result.bank_path = output_bank_path;
    result.catalog_path = output_bank_path;
    result.catalog_path += ".catalog.json";

    std::ostringstream diag;
    diag << "[AsyncBankPublisher] Starting publication...\n";

    std::error_code ec;
    if (output_bank_path.has_parent_path()) {
        fs::create_directories(output_bank_path.parent_path(), ec);
    }

    fs::path snapshot_path;
    {
        bool written = false;
        try { written = write_unique_snapshot(project_dir, snapshot.dump(2), snapshot_path); }
        catch (const std::exception& ex) { diag << "[Error] Snapshot serialization failed: " << ex.what() << '\n'; }
        if (!written) {
            diag << "[Error] Failed to write exclusive snapshot JSON in: " << project_dir.u8string() << "\n";
            result.success = false;
            result.exit_code = -1;
            result.diagnostics = diag.str();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_result_ = result;
            }
            running_.store(false);
            if (on_complete) on_complete(result);
            return;
        }
    }

    std::wstring cmd = L"\"" + bankc_exe.wstring() + L"\" \"" + snapshot_path.wstring() + L"\" -o \"" + output_bank_path.wstring() + L"\"";

    int exit_code = -1;
    std::string process_output;
    bool proc_ok = false;

#ifdef _WIN32
    proc_ok = run_command_capture_output(cmd, exit_code, process_output);
#endif

    diag << process_output;

    if (!proc_ok || exit_code != 0) {
        diag << "[AsyncBankPublisher] Bank compilation failed with code " << exit_code << "\n";
        result.success = false;
        result.exit_code = exit_code;
    } else {
        diag << "[AsyncBankPublisher] Publication succeeded: " << output_bank_path.string() << "\n";
        result.success = true;
        result.exit_code = 0;
    }

    fs::remove(snapshot_path, ec);
    result.diagnostics = diag.str();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = result;
    }

    running_.store(false);

    if (on_complete) {
        on_complete(result);
    }
}

} // namespace oae::studio
