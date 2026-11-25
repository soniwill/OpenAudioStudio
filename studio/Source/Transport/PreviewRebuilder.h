#pragma once

#include <juce_events/juce_events.h>
#include "../StudioEngine.h"
#include "../Model/AuthoringModel.h"
#include "../Project/ProjectDocument.h"
#include "../Project/AsyncBankPublisher.h"
#include "AuditionTransport.h"
#include <filesystem>
#include <functional>
#include <string>

namespace fs = std::filesystem;

namespace oae::studio {

/** Manages debounced (600 ms) automatic background compilation and in-flight hot reload of preview banks. */
class PreviewRebuilder : public juce::Timer, public AuthoringModel::Listener {
public:
    PreviewRebuilder(StudioEngine& engine,
                     AuthoringModel& model,
                     ProjectDocument& doc,
                     AuditionTransport& transport,
                     AsyncBankPublisher& publisher);
    ~PreviewRebuilder() override;

    void set_bankc_executable(const fs::path& bankc_exe);
    const fs::path& get_bankc_executable() const noexcept { return bankc_exe_; }

    void trigger_debounced_rebuild();
    bool rebuild_synchronously();
    void unload_preview_bank();

    /** Enables or disables debounced automatic preview bank rebuild timers. */
    void set_auto_rebuild_enabled(bool enabled) noexcept;

    void timerCallback() override;

    // AuthoringModel::Listener
    void on_project_changed() override;

    bool is_rebuilding() const noexcept { return is_rebuilding_; }
    uint64_t get_last_rebuilt_revision() const noexcept { return last_rebuilt_revision_; }
    oae_bank get_loaded_bank() const noexcept { return loaded_preview_bank_; }

    std::function<void(const std::string& status_text)> onStatusChanged;
    std::function<void(bool success, const std::string& diagnostics)> onRebuildFinished;

private:
    int next_preview_slot() const noexcept;
    fs::path preview_bank_path_for_slot(int slot) const;
    bool apply_compiled_bank(const fs::path& bank_path, int slot);

    StudioEngine& engine_;
    AuthoringModel& model_;
    ProjectDocument& doc_;
    AuditionTransport& transport_;
    AsyncBankPublisher& publisher_;
    fs::path bankc_exe_;

    uint64_t last_rebuilt_revision_{ 0 };
    uint64_t target_rebuilding_revision_{ 0 };
    bool is_rebuilding_{ false };
    oae_bank loaded_preview_bank_{ 0 };
    int active_preview_slot_{ -1 };
    fs::path active_preview_bank_path_;
    bool autoRebuildEnabled_{ true };
};

} // namespace oae::studio
