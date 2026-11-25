#include "LiveBridgeServer.h"

#include <algorithm>
#include <chrono>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

namespace oae::studio {

namespace {

void enable_tcp_nodelay(juce::StreamingSocket& sock)
{
    const int raw = sock.getRawSocketHandle();
    if (raw < 0) return;
    const int flag = 1;
#if defined(_WIN32)
    setsockopt(static_cast<SOCKET>(raw), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));
#else
    setsockopt(raw, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif
}

int64_t now_epoch_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

LiveBridgeServer::LiveBridgeServer() = default;

LiveBridgeServer::~LiveBridgeServer()
{
    stop();
}

bool LiveBridgeServer::start(int port)
{
    if (running_.load()) {
        if (port_.load() == port) return true;
        stop();
    }

    auto sock = std::make_unique<juce::StreamingSocket>();
    if (!sock->createListener(port, "127.0.0.1")) {
        return false;
    }

    port_.store(port);
    listener_socket_ = std::move(sock);
    running_.store(true);
    accept_thread_ = std::thread([this]() { accept_loop(); });
    return true;
}

void LiveBridgeServer::stop()
{
    if (!running_.exchange(false)) {
        return;
    }

    if (listener_socket_) {
        listener_socket_->close();
    }
    close_active_client();

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    listener_socket_.reset();
    client_connected_.store(false);
}

std::string LiveBridgeServer::get_client_name() const
{
    std::lock_guard<std::mutex> lock(client_name_mutex_);
    return client_name_;
}

void LiveBridgeServer::add_listener(Listener* l)
{
    if (!l) return;
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    if (std::find(listeners_.begin(), listeners_.end(), l) == listeners_.end()) {
        listeners_.push_back(l);
    }
}

void LiveBridgeServer::remove_listener(Listener* l)
{
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
}

void LiveBridgeServer::enqueue_event(IncomingEvent ev)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    incoming_queue_.push_back(std::move(ev));
}

void LiveBridgeServer::poll_incoming()
{
    std::deque<IncomingEvent> local;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        local.swap(incoming_queue_);
    }

    if (local.empty()) return;

    std::vector<Listener*> snapshot;
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        snapshot = listeners_;
    }

    for (const auto& ev : local) {
        for (auto* l : snapshot) {
            if (!l) continue;
            switch (ev.kind) {
            case IncomingEvent::Kind::Connected:
                l->on_live_client_connected(ev.text, ev.crc32, ev.flag);
                break;
            case IncomingEvent::Kind::Disconnected:
                l->on_live_client_disconnected();
                break;
            case IncomingEvent::Kind::ParamSet:
                l->on_live_parameter_received(ev.text, ev.value, ev.flag, ev.event_path);
                break;
            case IncomingEvent::Kind::Pong:
                l->on_live_pong_received(ev.rtt_ms);
                break;
            }
        }
    }
}

void LiveBridgeServer::close_active_client()
{
    std::lock_guard<std::mutex> lock(client_socket_mutex_);
    if (active_client_) {
        active_client_->close();
    }
}

void LiveBridgeServer::accept_loop()
{
    while (running_.load()) {
        if (!listener_socket_) break;
        auto* raw_client = listener_socket_->waitForNextConnection();
        if (!raw_client) {
            if (!running_.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        std::unique_ptr<juce::StreamingSocket> client(raw_client);
        handle_client(std::move(client));
    }
}

void LiveBridgeServer::handle_client(std::unique_ptr<juce::StreamingSocket> client)
{
    enable_tcp_nodelay(*client);

    {
        std::lock_guard<std::mutex> lock(client_socket_mutex_);
        active_client_ = std::move(client);
    }

    std::string buffer;
    char chunk[2048];
    bool handshake_done = false;

    while (running_.load()) {
        juce::StreamingSocket* sock_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lock(client_socket_mutex_);
            sock_ptr = active_client_.get();
        }
        if (!sock_ptr || !sock_ptr->isConnected()) {
            break;
        }

        const int ready = sock_ptr->waitUntilReady(true, 50);
        if (ready < 0) {
            break;
        }
        if (ready == 0) {
            continue;
        }

        const int bytes = sock_ptr->read(chunk, sizeof(chunk), false);
        if (bytes <= 0) {
            break;
        }

        buffer.append(chunk, static_cast<size_t>(bytes));

        size_t pos = 0;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) continue;

            nlohmann::json msg;
            try {
                msg = nlohmann::json::parse(line);
            } catch (...) {
                continue;
            }

            const std::string type = msg.value("type", std::string{});
            if (type == "hello") {
                const int version = msg.value("version", 0);
                const std::string cname = msg.value("client", std::string{ "Unity" });
                const uint32_t client_crc = msg.value("bank_crc32", 0u);
                const uint32_t studio_crc = studio_bank_crc32_.load();

                if (version != kProtocolVersion) {
                    send_json({
                        {"type", "hello_ack"},
                        {"version", kProtocolVersion},
                        {"studio_version", "1.0.0"},
                        {"studio_bank_crc32", studio_crc},
                        {"crc_match", false},
                        {"status", "version_mismatch"}
                    });
                    close_active_client();
                    break;
                }

                const bool crc_ok = (studio_crc == 0 || client_crc == 0 || studio_crc == client_crc);
                crc_matched_.store(crc_ok);
                client_bank_crc32_.store(client_crc);
                {
                    std::lock_guard<std::mutex> lock(client_name_mutex_);
                    client_name_ = cname;
                }
                client_connected_.store(true);
                handshake_done = true;

                send_json({
                    {"type", "hello_ack"},
                    {"version", kProtocolVersion},
                    {"studio_version", "1.0.0"},
                    {"studio_bank_crc32", studio_crc},
                    {"crc_match", crc_ok},
                    {"status", "connected"}
                });

                IncomingEvent ev;
                ev.kind = IncomingEvent::Kind::Connected;
                ev.text = cname;
                ev.crc32 = client_crc;
                ev.flag = crc_ok;
                enqueue_event(std::move(ev));
            } else if (type == "param_set") {
                IncomingEvent ev;
                ev.kind = IncomingEvent::Kind::ParamSet;
                ev.text = msg.value("name", std::string{});
                ev.value = msg.value("value", 0.0f);
                ev.flag = msg.value("is_global", true);
                ev.event_path = msg.value("event_path", std::string{});
                if (!ev.text.empty()) {
                    enqueue_event(std::move(ev));
                }
            } else if (type == "pong") {
                const int64_t ts = msg.value("ts_ms", int64_t{ 0 });
                const int64_t rtt = std::max<int64_t>(0, now_epoch_ms() - ts);
                last_rtt_ms_.store(rtt);
                IncomingEvent ev;
                ev.kind = IncomingEvent::Kind::Pong;
                ev.rtt_ms = rtt;
                enqueue_event(std::move(ev));
            }
        }
    }

    const bool was_connected = client_connected_.exchange(false);
    {
        std::lock_guard<std::mutex> lock(client_socket_mutex_);
        if (active_client_) {
            active_client_->close();
            active_client_.reset();
        }
    }
    {
        std::lock_guard<std::mutex> lock(client_name_mutex_);
        client_name_.clear();
    }
    client_bank_crc32_.store(0);
    crc_matched_.store(true);

    if (was_connected || handshake_done) {
        IncomingEvent ev;
        ev.kind = IncomingEvent::Kind::Disconnected;
        enqueue_event(std::move(ev));
    }
}

bool LiveBridgeServer::send_json(const nlohmann::json& msg)
{
    std::string payload = msg.dump() + "\n";
    std::lock_guard<std::mutex> lock(client_socket_mutex_);
    if (!active_client_ || !active_client_->isConnected()) {
        return false;
    }

    const int total = static_cast<int>(payload.size());
    int sent = 0;
    while (sent < total) {
        const int n = active_client_->write(payload.data() + sent, total - sent);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

bool LiveBridgeServer::send_bus_volume(const std::string& path, float volume_db)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "bus_volume"},
        {"path", path},
        {"volume_db", volume_db}
    });
}

bool LiveBridgeServer::send_bus_pan(const std::string& path, float pan)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "bus_pan"},
        {"path", path},
        {"pan", pan}
    });
}

bool LiveBridgeServer::send_bus_mute(const std::string& path, bool mute)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "bus_mute"},
        {"path", path},
        {"mute", mute}
    });
}

bool LiveBridgeServer::send_vca_volume(const std::string& path, float volume_db)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "vca_volume"},
        {"path", path},
        {"volume_db", volume_db}
    });
}

bool LiveBridgeServer::send_parameter(const std::string& name, float value, bool is_global, const std::string& event_path)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "param_set"},
        {"name", name},
        {"value", value},
        {"is_global", is_global},
        {"event_path", event_path}
    });
}

bool LiveBridgeServer::send_curve_update(const std::string& event_path, int track_index, const std::string& target, const nlohmann::json& points)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "curve_update"},
        {"event_path", event_path},
        {"track_index", track_index},
        {"target", target},
        {"points", points}
    });
}

bool LiveBridgeServer::send_event_play(const std::string& event_path, double position_ms, bool loop)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "event_play"},
        {"event_path", event_path},
        {"position_ms", position_ms},
        {"loop", loop}
    });
}

bool LiveBridgeServer::send_event_stop(const std::string& event_path, bool immediate)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "event_stop"},
        {"event_path", event_path},
        {"immediate", immediate}
    });
}

bool LiveBridgeServer::send_event_key_off(const std::string& event_path)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "event_key_off"},
        {"event_path", event_path}
    });
}

bool LiveBridgeServer::send_snapshot_toggle(const std::string& snapshot_path, bool active)
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "snapshot_toggle"},
        {"snapshot_path", snapshot_path},
        {"active", active}
    });
}

bool LiveBridgeServer::send_ping()
{
    if (!client_connected_.load()) return false;
    return send_json({
        {"type", "ping"},
        {"ts_ms", now_epoch_ms()}
    });
}

} // namespace oae::studio
