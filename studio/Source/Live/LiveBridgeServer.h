#pragma once

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace oae::studio {

/** Asynchronous TCP bridge server (default port 7450) for Live-Linking between OpenAudio Studio and game runtimes. */
class LiveBridgeServer {
public:
    static constexpr int kDefaultPort = 7450;
    static constexpr int kProtocolVersion = 1;

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void on_live_client_connected(const std::string& client_name, uint32_t client_crc32, bool crc_match) {
            (void)client_name; (void)client_crc32; (void)crc_match;
        }
        virtual void on_live_client_disconnected() {}
        virtual void on_live_parameter_received(const std::string& name, float value, bool is_global, const std::string& event_path) {
            (void)name; (void)value; (void)is_global; (void)event_path;
        }
        virtual void on_live_pong_received(int64_t rtt_ms) { (void)rtt_ms; }
    };

    LiveBridgeServer();
    ~LiveBridgeServer();

    LiveBridgeServer(const LiveBridgeServer&) = delete;
    LiveBridgeServer& operator=(const LiveBridgeServer&) = delete;

    bool start(int port = kDefaultPort);
    void stop();

    bool is_running() const noexcept { return running_.load(); }
    int get_port() const noexcept { return port_.load(); }

    void set_studio_bank_crc32(uint32_t crc32) noexcept { studio_bank_crc32_.store(crc32); }
    uint32_t get_studio_bank_crc32() const noexcept { return studio_bank_crc32_.load(); }

    bool is_client_connected() const noexcept { return client_connected_.load(); }
    bool is_crc_matched() const noexcept { return crc_matched_.load(); }
    uint32_t get_client_bank_crc32() const noexcept { return client_bank_crc32_.load(); }
    int64_t get_last_rtt_ms() const noexcept { return last_rtt_ms_.load(); }
    std::string get_client_name() const;

    void add_listener(Listener* l);
    void remove_listener(Listener* l);

    /** Drains incoming messages queued by the network thread and dispatches Listener callbacks on the caller thread. */
    void poll_incoming();

    bool send_bus_volume(const std::string& path, float volume_db);
    bool send_bus_pan(const std::string& path, float pan);
    bool send_bus_mute(const std::string& path, bool mute);
    bool send_vca_volume(const std::string& path, float volume_db);
    bool send_parameter(const std::string& name, float value, bool is_global, const std::string& event_path = {});
    bool send_curve_update(const std::string& event_path, int track_index, const std::string& target, const nlohmann::json& points);

    bool send_event_play(const std::string& event_path, double position_ms = 0.0, bool loop = false);
    bool send_event_stop(const std::string& event_path, bool immediate = false);
    bool send_event_key_off(const std::string& event_path);
    bool send_snapshot_toggle(const std::string& snapshot_path, bool active);
    bool send_ping();

private:
    struct IncomingEvent {
        enum class Kind {
            Connected,
            Disconnected,
            ParamSet,
            Pong
        };
        Kind kind{ Kind::Disconnected };
        std::string text;
        std::string event_path;
        float value{ 0.0f };
        bool flag{ false };
        uint32_t crc32{ 0 };
        int64_t rtt_ms{ 0 };
    };

    void accept_loop();
    void handle_client(std::unique_ptr<juce::StreamingSocket> client);
    bool send_json(const nlohmann::json& msg);
    void enqueue_event(IncomingEvent ev);
    void close_active_client();

    std::atomic<bool> running_{ false };
    std::atomic<int> port_{ kDefaultPort };
    std::atomic<uint32_t> studio_bank_crc32_{ 0 };

    std::atomic<bool> client_connected_{ false };
    std::atomic<bool> crc_matched_{ true };
    std::atomic<uint32_t> client_bank_crc32_{ 0 };
    std::atomic<int64_t> last_rtt_ms_{ 0 };

    mutable std::mutex client_name_mutex_;
    std::string client_name_;

    std::unique_ptr<juce::StreamingSocket> listener_socket_;
    std::thread accept_thread_;

    mutable std::mutex client_socket_mutex_;
    std::unique_ptr<juce::StreamingSocket> active_client_;

    mutable std::mutex queue_mutex_;
    std::deque<IncomingEvent> incoming_queue_;

    std::mutex listeners_mutex_;
    std::vector<Listener*> listeners_;
};

} // namespace oae::studio
