#pragma once

#include <oae_audio.h>
#include "../StudioEngine.h"
#include "../Model/AuthoringModel.h"
#include "../Live/LiveBridgeServer.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace oae::studio {

enum class TransportState {
    Stopped,
    Playing,
    Paused
};

/** Captures the active playback state to enable seamless in-flight preview bank swapping. */
struct AuditionSnapshotState {
    bool was_playing{ false };
    std::string event_path;
    double position_ms{ 0.0 };
    std::unordered_map<std::string, float> local_params;
    std::unordered_map<std::string, float> global_params;
    std::vector<std::string> active_snapshots;
};

/** Controls the real-time playback lifecycle of authoring events, snapshots, and parameter modulation. */
class AuditionTransport : public LiveBridgeServer::Listener, public AuthoringModel::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void on_transport_state_changed(TransportState new_state) { (void)new_state; }
        virtual void on_parameter_changed(const std::string& name, float val, bool is_global) {
            (void)name; (void)val; (void)is_global;
        }
    };

    AuditionTransport(StudioEngine& engine, AuthoringModel& model);
    ~AuditionTransport();

    void add_listener(Listener* l);
    void remove_listener(Listener* l);

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return event_path_; }

    bool play();
    bool pause();
    bool resume();
    bool stop(bool immediate = false);
    bool seek(double time_ms);
    bool key_off();

    void set_loop(bool loop) noexcept { loop_enabled_ = loop; }
    bool is_loop_enabled() const noexcept { return loop_enabled_; }

    TransportState get_state() const noexcept { return state_; }
    oae_event_instance get_active_instance() const noexcept { return active_instance_; }
    double get_current_position_ms();

    void set_local_parameter(const std::string& name, float value);
    void set_global_parameter(const std::string& name, float value);
    float get_local_parameter(const std::string& name) const;
    float get_global_parameter(const std::string& name) const;

    bool toggle_snapshot(const std::string& snapshot_path, bool active);
    bool is_snapshot_active(const std::string& snapshot_path) const;
    void stop_all_snapshots();

    void capture_state(AuditionSnapshotState& out_state) const;
    void restore_state(const AuditionSnapshotState& state);

    void update_tick();

    void set_live_bridge(LiveBridgeServer* bridge);
    LiveBridgeServer* get_live_bridge() const noexcept { return live_bridge_; }
    bool is_remote_audition_active() const noexcept;

    // LiveBridgeServer::Listener
    void on_live_client_disconnected() override;
    void on_live_parameter_received(const std::string& name, float value, bool is_global, const std::string& event_path) override;

    // AuthoringModel::Listener
    void on_project_changed() override;

    /** Applies current AuthoringModel track Volume, Pan, Mute, and Solo states to the active instance track buses. */
    void sync_active_track_mix();

private:
    void notify_state_changed();

    StudioEngine& engine_;
    AuthoringModel& model_;

    std::string event_path_;
    oae_event_instance active_instance_{ 0 };
    TransportState state_{ TransportState::Stopped };
    double stopped_position_ms_{ 0.0 };
    double paused_position_ms_{ 0.0 };
    bool loop_enabled_{ false };

    std::unordered_map<std::string, float> local_params_;
    std::unordered_map<std::string, float> global_params_;
    std::unordered_map<std::string, oae_event_instance> active_snapshots_;

    std::vector<Listener*> listeners_;
    LiveBridgeServer* live_bridge_{ nullptr };
    bool remote_playing_{ false };
    bool suppress_live_echo_{ false };
};

} // namespace oae::studio
