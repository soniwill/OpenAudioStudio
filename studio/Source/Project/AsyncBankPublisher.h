#pragma once

#include <nlohmann/json.hpp>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace oae::studio {

struct PublishResult {
    bool success{ false };
    int exit_code{ -1 };
    std::string diagnostics;
    fs::path bank_path;
    fs::path catalog_path;
};

class AsyncBankPublisher {
public:
    AsyncBankPublisher();
    ~AsyncBankPublisher();

    // Start background publishing job
    bool start_publish(const fs::path& bankc_exe,
                       const nlohmann::json& project_snapshot,
                       const fs::path& project_dir,
                       const fs::path& output_bank_path,
                       std::function<void(const PublishResult&)> on_complete = nullptr);

    // Status queries
    bool is_running() const { return running_.load(); }
    void wait_until_finished();

    // Latest result
    PublishResult get_last_result() const;
    std::string get_diagnostics() const;

private:
    void worker_thread(fs::path bankc_exe,
                       nlohmann::json snapshot,
                       fs::path project_dir,
                       fs::path output_bank_path,
                       std::function<void(const PublishResult&)> on_complete);

    std::atomic<bool> running_{ false };
    std::thread worker_;
    mutable std::mutex mutex_;
    PublishResult last_result_;
};

} // namespace oae::studio
