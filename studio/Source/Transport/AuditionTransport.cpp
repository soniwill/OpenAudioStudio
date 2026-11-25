#include "AuditionTransport.h"
#include <algorithm>
#include <limits>

namespace oae::studio {

AuditionTransport::AuditionTransport(StudioEngine& engine, AuthoringModel& model)
    : engine_(engine), model_(model)
{
    model_.add_listener(this);
}

AuditionTransport::~AuditionTransport()
{
    model_.remove_listener(this);
    if (live_bridge_) {
        live_bridge_->remove_listener(this);
        live_bridge_ = nullptr;
    }
    stop(true);
    stop_all_snapshots();
}

void AuditionTransport::set_live_bridge(LiveBridgeServer* bridge)
{
    if (live_bridge_ == bridge) return;
    if (live_bridge_) {
        live_bridge_->remove_listener(this);
    }
    live_bridge_ = bridge;
    if (live_bridge_) {
        live_bridge_->add_listener(this);
    }
}

bool AuditionTransport::is_remote_audition_active() const noexcept
{
    return live_bridge_ != nullptr && live_bridge_->is_client_connected();
}

void AuditionTransport::on_live_client_disconnected()
{
    if (remote_playing_) {
        remote_playing_ = false;
        if (active_instance_ == 0 && state_ != TransportState::Stopped) {
            stopped_position_ms_ = 0.0;
            state_ = TransportState::Stopped;
            notify_state_changed();
        }
    }
}

void AuditionTransport::on_live_parameter_received(const std::string& name, float value, bool is_global, const std::string& event_path)
{
    suppress_live_echo_ = true;
    if (is_global) {
        set_global_parameter(name, value);
    } else if (event_path.empty() || event_path == event_path_) {
        set_local_parameter(name, value);
    }
    suppress_live_echo_ = false;
}

void AuditionTransport::add_listener(Listener* l)
{
    if (std::find(listeners_.begin(), listeners_.end(), l) == listeners_.end()) {
        listeners_.push_back(l);
    }
}

void AuditionTransport::remove_listener(Listener* l)
{
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
}

void AuditionTransport::notify_state_changed()
{
    for (auto* l : listeners_) {
        l->on_transport_state_changed(state_);
    }
}

void AuditionTransport::set_event_path(const std::string& path)
{
    if (event_path_ != path) {
        if (state_ == TransportState::Playing || state_ == TransportState::Paused) {
            stop(true);
        }
        event_path_ = path;
        stopped_position_ms_ = 0.0;
        local_params_.clear();
    }
}

bool AuditionTransport::play()
{
    if (event_path_.empty()) return false;

    if (is_remote_audition_active()) {
        const double start_ms = (state_ == TransportState::Paused) ? paused_position_ms_ : 0.0;
        if (live_bridge_->send_event_play(event_path_, start_ms, loop_enabled_)) {
            for (const auto& [name, val] : local_params_) {
                live_bridge_->send_parameter(name, val, false, event_path_);
            }
            remote_playing_ = true;
            state_ = TransportState::Playing;
            notify_state_changed();
            return true;
        }
        remote_playing_ = false;
    }

    if (state_ == TransportState::Paused) {
        return resume();
    }

    if (active_instance_ != 0) {
        engine_.stop_instance(active_instance_, false);
        engine_.release_instance(active_instance_);
        active_instance_ = 0;
    }

    auto res = engine_.create_event_instance(event_path_, &active_instance_);
    if (res != OAE_OK || active_instance_ == 0) {
        return false;
    }

    // Apply stored parameters
    for (const auto& [name, val] : local_params_) {
        engine_.set_instance_parameter(active_instance_, name, val);
    }
    for (const auto& [name, val] : global_params_) {
        engine_.set_global_parameter(name, val);
    }

    if (state_ == TransportState::Stopped && stopped_position_ms_ > 0.0) {
        res = engine_.set_instance_timeline_position(active_instance_, static_cast<int32_t>(stopped_position_ms_));
    }
    if (res == OAE_OK) {
        res = oae_event_instance_start(active_instance_);
    }
    if (res != OAE_OK) {
        engine_.release_instance(active_instance_);
        active_instance_ = 0;
        return false;
    }

    state_ = TransportState::Playing;
    sync_active_track_mix();
    notify_state_changed();
    return true;
}

bool AuditionTransport::pause()
{
    if (state_ != TransportState::Playing || active_instance_ == 0) {
        return false;
    }

    paused_position_ms_ = get_current_position_ms();
    engine_.stop_instance(active_instance_, false);
    engine_.release_instance(active_instance_);
    active_instance_ = 0;

    state_ = TransportState::Paused;
    notify_state_changed();
    return true;
}

bool AuditionTransport::resume()
{
    if (state_ != TransportState::Paused || event_path_.empty()) {
        return false;
    }

    auto res = engine_.create_event_instance(event_path_, &active_instance_);
    if (res != OAE_OK || active_instance_ == 0) {
        return false;
    }

    for (const auto& [name, val] : local_params_) {
        engine_.set_instance_parameter(active_instance_, name, val);
    }
    for (const auto& [name, val] : global_params_) {
        engine_.set_global_parameter(name, val);
    }

    res = engine_.set_instance_timeline_position(active_instance_, static_cast<int32_t>(paused_position_ms_));
    if (res == OAE_OK) {
        res = oae_event_instance_start(active_instance_);
    }
    if (res != OAE_OK) {
        engine_.release_instance(active_instance_);
        active_instance_ = 0;
        return false;
    }

    state_ = TransportState::Playing;
    sync_active_track_mix();
    notify_state_changed();
    return true;
}

bool AuditionTransport::stop(bool immediate)
{
    if (remote_playing_) {
        if (live_bridge_ && live_bridge_->is_client_connected() && !event_path_.empty()) {
            live_bridge_->send_event_stop(event_path_, immediate);
        }
        remote_playing_ = false;
    }

    if (active_instance_ != 0) {
        engine_.stop_instance(active_instance_, !immediate);
        engine_.release_instance(active_instance_);
        active_instance_ = 0;
    }

    stopped_position_ms_ = 0.0;
    paused_position_ms_ = 0.0;
    state_ = TransportState::Stopped;
    notify_state_changed();
    return true;
}

bool AuditionTransport::seek(double time_ms)
{
    const double valid_ms = std::min(std::max(0.0, time_ms), static_cast<double>(std::numeric_limits<int32_t>::max()));
    if (state_ == TransportState::Playing && active_instance_ != 0) {
        engine_.set_instance_timeline_position(active_instance_, static_cast<int32_t>(valid_ms));
        return true;
    }
    if (state_ == TransportState::Paused) {
        paused_position_ms_ = valid_ms;
        return true;
    }
    if (state_ == TransportState::Stopped && !event_path_.empty() && !is_remote_audition_active()) {
        stopped_position_ms_ = valid_ms;
        return true;
    }
    return false;
}

bool AuditionTransport::key_off()
{
    if (remote_playing_ && live_bridge_ && live_bridge_->is_client_connected() && !event_path_.empty()) {
        return live_bridge_->send_event_key_off(event_path_);
    }
    if (active_instance_ != 0) {
        return engine_.key_off(active_instance_) == OAE_OK;
    }
    return false;
}

double AuditionTransport::get_current_position_ms()
{
    if (state_ == TransportState::Playing && active_instance_ != 0) {
        int32_t pos = 0;
        if (engine_.get_instance_timeline_position(active_instance_, &pos) == OAE_OK) {
            return static_cast<double>(pos);
        }
    }
    if (state_ == TransportState::Paused) {
        return paused_position_ms_;
    }
    if (state_ == TransportState::Stopped && !is_remote_audition_active()) {
        return stopped_position_ms_;
    }
    return 0.0;
}

void AuditionTransport::set_local_parameter(const std::string& name, float value)
{
    local_params_[name] = value;
    if (active_instance_ != 0) {
        engine_.set_instance_parameter(active_instance_, name, value);
    }
    if (!suppress_live_echo_ && live_bridge_ && live_bridge_->is_client_connected()) {
        live_bridge_->send_parameter(name, value, false, event_path_);
    }
    for (auto* l : listeners_) {
        l->on_parameter_changed(name, value, false);
    }
}

void AuditionTransport::set_global_parameter(const std::string& name, float value)
{
    global_params_[name] = value;
    engine_.set_global_parameter(name, value);
    if (!suppress_live_echo_ && live_bridge_ && live_bridge_->is_client_connected()) {
        live_bridge_->send_parameter(name, value, true);
    }
    for (auto* l : listeners_) {
        l->on_parameter_changed(name, value, true);
    }
}

float AuditionTransport::get_local_parameter(const std::string& name) const
{
    auto it = local_params_.find(name);
    return (it != local_params_.end()) ? it->second : 0.0f;
}

float AuditionTransport::get_global_parameter(const std::string& name) const
{
    auto it = global_params_.find(name);
    return (it != global_params_.end()) ? it->second : 0.0f;
}

bool AuditionTransport::toggle_snapshot(const std::string& snapshot_path, bool active)
{
    if (live_bridge_ && live_bridge_->is_client_connected()) {
        live_bridge_->send_snapshot_toggle(snapshot_path, active);
    }

    auto it = active_snapshots_.find(snapshot_path);
    if (active) {
        if (it != active_snapshots_.end()) return true; // Already active
        oae_event_instance snap_inst = 0;
        if (engine_.create_event_instance(snapshot_path, &snap_inst) == OAE_OK && snap_inst != 0) {
            oae_event_instance_start(snap_inst);
            active_snapshots_[snapshot_path] = snap_inst;
            return true;
        }
        return false;
    } else {
        if (it != active_snapshots_.end()) {
            engine_.stop_instance(it->second, true);
            engine_.release_instance(it->second);
            active_snapshots_.erase(it);
        }
        return true;
    }
}

bool AuditionTransport::is_snapshot_active(const std::string& snapshot_path) const
{
    auto it = active_snapshots_.find(snapshot_path);
    if (it == active_snapshots_.end() || it->second == 0) return false;
    int playback_state = 0;
    if (engine_.get_instance_playback_state(it->second, &playback_state) == OAE_OK) {
        return (playback_state == OAE_PLAYBACK_PLAYING || playback_state == OAE_PLAYBACK_STARTING);
    }
    return false;
}

void AuditionTransport::stop_all_snapshots()
{
    for (auto& [path, inst] : active_snapshots_) {
        if (inst != 0) {
            engine_.stop_instance(inst, false);
            engine_.release_instance(inst);
        }
    }
    active_snapshots_.clear();
}

void AuditionTransport::capture_state(AuditionSnapshotState& out_state) const
{
    out_state.was_playing = (state_ == TransportState::Playing);
    out_state.event_path = event_path_;
    out_state.position_ms = const_cast<AuditionTransport*>(this)->get_current_position_ms();
    out_state.local_params = local_params_;
    out_state.global_params = global_params_;

    out_state.active_snapshots.clear();
    for (const auto& [path, inst] : active_snapshots_) {
        (void)inst;
        out_state.active_snapshots.push_back(path);
    }
}

void AuditionTransport::restore_state(const AuditionSnapshotState& state)
{
    set_event_path(state.event_path);
    local_params_ = state.local_params;
    global_params_ = state.global_params;

    // Reactivate snapshots
    for (const auto& snap_path : state.active_snapshots) {
        toggle_snapshot(snap_path, true);
    }

    if (state.was_playing && !event_path_.empty()) {
        play();
        seek(state.position_ms);
    }
}

void AuditionTransport::on_project_changed()
{
    sync_active_track_mix();
}

void AuditionTransport::sync_active_track_mix()
{
    if (state_ != TransportState::Playing || active_instance_ == 0 || event_path_.empty()) {
        return;
    }

    const size_t n = model_.get_track_count(event_path_);
    if (n == 0) return;

    std::vector<bool> muted(n, false);
    std::vector<bool> solo(n, false);
    std::vector<int> output_track(n, -1);
    bool any_solo = false;

    for (size_t t = 0; t < n; ++t) {
        muted[t] = model_.is_track_muted(event_path_, t);
        solo[t] = model_.is_track_solo(event_path_, t);
        output_track[t] = model_.get_track_output_track(event_path_, t);
        if (solo[t]) {
            any_solo = true;
        }
    }

    auto reaches = [&](size_t from, size_t target) {
        int cur = output_track[from];
        for (size_t step = 0; step < n && cur >= 0 && static_cast<size_t>(cur) < n; ++step) {
            if (static_cast<size_t>(cur) == target) {
                return true;
            }
            cur = output_track[static_cast<size_t>(cur)];
        }
        return false;
    };

    for (size_t t = 0; t < n; ++t) {
        bool effective_mute = muted[t];
        if (any_solo) {
            bool keep_audible = solo[t];
            if (!keep_audible) {
                for (size_t s = 0; s < n; ++s) {
                    if (!solo[s]) continue;
                    if (reaches(s, t) || reaches(t, s)) {
                        keep_audible = true;
                        break;
                    }
                }
            }
            effective_mute = !keep_audible || muted[t];
        }

        oae_bus trk_bus = 0;
        if (engine_.get_instance_track_bus(active_instance_, static_cast<uint32_t>(t), &trk_bus) == OAE_OK && trk_bus != 0) {
            engine_.set_bus_volume_db(trk_bus, static_cast<float>(model_.get_track_volume(event_path_, t)));
            engine_.set_bus_pan(trk_bus, static_cast<float>(model_.get_track_pan(event_path_, t)));
            engine_.set_bus_mute(trk_bus, effective_mute);
        }
    }
}

void AuditionTransport::update_tick()
{
    if (state_ == TransportState::Playing && active_instance_ != 0) {
        int playback_state = 0;
        if (engine_.get_instance_playback_state(active_instance_, &playback_state) == OAE_OK) {
            if (playback_state == OAE_PLAYBACK_STOPPED) {
                if (loop_enabled_) {
                    engine_.set_instance_timeline_position(active_instance_, 0);
                    if (oae_event_instance_start(active_instance_) == OAE_OK) {
                        sync_active_track_mix();
                    }
                } else {
                    stop(true);
                }
            }
        }
    }
}

} // namespace oae::studio
