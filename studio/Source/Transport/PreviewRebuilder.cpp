#include "PreviewRebuilder.h"

namespace oae::studio {

namespace {

void cleanup_preview_files(const fs::path& bank_path)
{
    if (bank_path.empty()) return;
    std::error_code ec;
    const bool removed = fs::remove(bank_path, ec);
    if (removed || (!ec && !fs::exists(bank_path, ec))) {
        fs::path catalog_path = bank_path;
        catalog_path += ".catalog.json";
        fs::remove(catalog_path, ec);
    }
}

} // namespace

PreviewRebuilder::PreviewRebuilder(StudioEngine& engine,
                                   AuthoringModel& model,
                                   ProjectDocument& doc,
                                   AuditionTransport& transport,
                                   AsyncBankPublisher& publisher)
    : engine_(engine), model_(model), doc_(doc), transport_(transport), publisher_(publisher)
{
    model_.add_listener(this);
}

PreviewRebuilder::~PreviewRebuilder()
{
    stopTimer();
    model_.remove_listener(this);
    unload_preview_bank();
}

void PreviewRebuilder::set_bankc_executable(const fs::path& bankc_exe)
{
    bankc_exe_ = bankc_exe;
}

int PreviewRebuilder::next_preview_slot() const noexcept
{
    return (active_preview_slot_ == 0) ? 1 : 0;
}

fs::path PreviewRebuilder::preview_bank_path_for_slot(int slot) const
{
    return doc_.get_project_dir() / "build" / (slot == 0 ? "preview_a.bank" : "preview_b.bank");
}

void PreviewRebuilder::set_auto_rebuild_enabled(bool enabled) noexcept
{
    autoRebuildEnabled_ = enabled;
    if (!autoRebuildEnabled_) {
        stopTimer();
    }
}

void PreviewRebuilder::trigger_debounced_rebuild()
{
    if (!autoRebuildEnabled_) {
        return;
    }
    startTimer(600);
    if (onStatusChanged) {
        onStatusChanged("Editing... (Rebuild pending)");
    }
}

void PreviewRebuilder::unload_preview_bank()
{
    stopTimer();
    if (loaded_preview_bank_ != 0) {
        if (transport_.get_state() == TransportState::Playing) {
            transport_.stop(true);
        }
        transport_.stop_all_snapshots();
        engine_.unload_bank(loaded_preview_bank_);
        loaded_preview_bank_ = 0;
        engine_.update();
    }
    if (!active_preview_bank_path_.empty()) {
        cleanup_preview_files(active_preview_bank_path_);
        active_preview_bank_path_.clear();
    }
    active_preview_slot_ = -1;
}

void PreviewRebuilder::on_project_changed()
{
    if (!doc_.is_loaded()) {
        unload_preview_bank();
        return;
    }
    if (loaded_preview_bank_ != 0 && model_.get_content_revision() == last_rebuilt_revision_) {
        return;
    }
    if (!bankc_exe_.empty()) {
        trigger_debounced_rebuild();
    }
}

void PreviewRebuilder::timerCallback()
{
    stopTimer();
    if (is_rebuilding_) return;
    if (!doc_.is_loaded() || bankc_exe_.empty()) return;
    if (model_.is_in_gesture()) {
        startTimer(600);
        return;
    }
    if (loaded_preview_bank_ != 0 && model_.get_content_revision() == last_rebuilt_revision_) {
        return;
    }

    const auto snapshot = model_.create_immutable_snapshot();
    target_rebuilding_revision_ = model_.get_content_revision();
    is_rebuilding_ = true;

    if (onStatusChanged) {
        onStatusChanged("Building preview bank...");
    }

    const int next_slot = next_preview_slot();
    const fs::path preview_bank = preview_bank_path_for_slot(next_slot);
    const bool started = publisher_.start_publish(bankc_exe_, snapshot, doc_.get_project_dir(), preview_bank,
        [this, preview_bank, next_slot](const PublishResult& r) {
            auto finish_action = [this, preview_bank, next_slot, r]() {
                is_rebuilding_ = false;

                if (!doc_.is_loaded()) {
                    cleanup_preview_files(preview_bank);
                    return;
                }

                const bool applied = r.success && apply_compiled_bank(preview_bank, next_slot);
                if (applied) {
                    last_rebuilt_revision_ = target_rebuilding_revision_;

                    if (onStatusChanged) {
                        onStatusChanged("Preview In Sync (Rev " + std::to_string(last_rebuilt_revision_) + ")");
                    }

                    if (model_.get_content_revision() > target_rebuilding_revision_) {
                        startTimer(600);
                    }
                } else {
                    if (onStatusChanged) {
                        onStatusChanged("Preview Build Error");
                    }
                }

                if (onRebuildFinished) {
                    onRebuildFinished(applied, r.diagnostics);
                }
            };

            if (juce::MessageManager::getInstanceWithoutCreating() != nullptr) {
                juce::MessageManager::callAsync(finish_action);
            } else {
                finish_action();
            }
        });
    if (!started) {
        is_rebuilding_ = false;
        startTimer(600);
    }
}

bool PreviewRebuilder::apply_compiled_bank(const fs::path& bank_path, int slot)
{
    AuditionSnapshotState snap;
    transport_.capture_state(snap);

    if (snap.was_playing) {
        transport_.stop(true);
    }
    transport_.stop_all_snapshots();

    const fs::path previous_bank_path = active_preview_bank_path_;
    if (loaded_preview_bank_ != 0) {
        engine_.unload_bank(loaded_preview_bank_);
        loaded_preview_bank_ = 0;
    }
    engine_.update();

    const auto load_res = engine_.load_bank_file(bank_path.string().c_str(), &loaded_preview_bank_);
    if (load_res == OAE_OK && loaded_preview_bank_ != 0) {
        active_preview_slot_ = slot;
        active_preview_bank_path_ = bank_path;
        transport_.restore_state(snap);
        if (!previous_bank_path.empty() && previous_bank_path != bank_path) {
            cleanup_preview_files(previous_bank_path);
        }
        return true;
    }

    active_preview_slot_ = -1;
    active_preview_bank_path_.clear();
    if (!previous_bank_path.empty() && previous_bank_path != bank_path) {
        cleanup_preview_files(previous_bank_path);
    }
    return false;
}

bool PreviewRebuilder::rebuild_synchronously()
{
    if (!doc_.is_loaded() || bankc_exe_.empty()) return false;
    stopTimer();

    const auto snapshot = model_.create_immutable_snapshot();
    target_rebuilding_revision_ = model_.get_content_revision();
    const int next_slot = next_preview_slot();
    const fs::path preview_bank = preview_bank_path_for_slot(next_slot);

    if (!publisher_.start_publish(bankc_exe_, snapshot, doc_.get_project_dir(), preview_bank)) {
        return false;
    }
    publisher_.wait_until_finished();
    const auto r = publisher_.get_last_result();

    if (r.success && apply_compiled_bank(preview_bank, next_slot)) {
        last_rebuilt_revision_ = target_rebuilding_revision_;
        if (onStatusChanged) {
            onStatusChanged("Preview In Sync (Rev " + std::to_string(last_rebuilt_revision_) + ")");
        }
        if (onRebuildFinished) {
            onRebuildFinished(true, r.diagnostics);
        }
        return true;
    }
    if (onStatusChanged) {
        onStatusChanged("Preview Build Error");
    }
    if (onRebuildFinished) {
        onRebuildFinished(false, r.diagnostics);
    }
    return false;
}

} // namespace oae::studio
