#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include "McpProtocol.h"
#include "../StudioEngine.h"
#include "../Project/ProjectDocument.h"
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"
#include "../Transport/PreviewRebuilder.h"
#include "../Project/AsyncBankPublisher.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace oae::studio {

class MainComponent;
class DiagnosticsLogPanel;
class TutorialHighlightOverlay;

/** Context dependency bundle for McpServer decoupling it from specific UI windows. */
struct McpContext {
    StudioEngine& engine;
    ProjectDocument& document;
    AuthoringModel& authoring_model;
    AuditionTransport& transport;
    juce::Component* root_component{ nullptr };
    PreviewRebuilder* preview_rebuilder{ nullptr };
    DiagnosticsLogPanel* diagnostics{ nullptr };
    AsyncBankPublisher* publisher{ nullptr };
    TutorialHighlightOverlay* highlight_overlay{ nullptr };
};

/** Local Model Context Protocol (MCP) JSON-RPC 2.0 Server for OpenAudio Studio (EPICO 6). */
class McpServer {
public:
    explicit McpServer(MainComponent& main_comp);
    explicit McpServer(McpContext ctx);
    ~McpServer();

    /** Processes a raw newline-delimited or framed JSON-RPC 2.0 message and returns the response JSON string. */
    std::string handle_message(const std::string& request_json);

    /** Processes a parsed JSON-RPC 2.0 request and returns the resulting JSON-RPC response. */
    nlohmann::json process_request(const nlohmann::json& req);

    /** Starts processing JSON-RPC 2.0 commands over stdio on a background worker thread. */
    bool start_stdio();

    /** Stops stdio worker thread. */
    void stop_stdio();

    /** Starts TCP socket listener on 127.0.0.1 at the specified port (default: 7451). */
    bool start_tcp(int port = 7451);

    /** Stops TCP socket listener and active client connections. */
    void stop_tcp();

    /** Stops all running transports. */
    void stop();

    bool is_running() const noexcept { return is_running_.load(); }
    int get_tcp_port() const noexcept { return tcp_port_; }

private:
    // JSON-RPC Handlers
    nlohmann::json handle_initialize(const nlohmann::json& id, const nlohmann::json& params);
    nlohmann::json handle_tools_list(const nlohmann::json& id);
    nlohmann::json handle_tools_call(const nlohmann::json& id, const nlohmann::json& params);
    nlohmann::json handle_resources_list(const nlohmann::json& id);
    nlohmann::json handle_resources_read(const nlohmann::json& id, const nlohmann::json& params);

    // MCP Tools (executed on JUCE Message Thread with RFC 6902 diffing)
    nlohmann::json tool_trigger_control(const nlohmann::json& args);
    nlohmann::json tool_select_item(const nlohmann::json& args);
    nlohmann::json tool_move_clip(const nlohmann::json& args);
    nlohmann::json tool_transport(const nlohmann::json& args);
    nlohmann::json tool_set_mixer_fader(const nlohmann::json& args);
    nlohmann::json tool_build_bank(const nlohmann::json& args);
    nlohmann::json tool_ui_inspect(const nlohmann::json& args);
    nlohmann::json tool_ui_key(const nlohmann::json& args);
    nlohmann::json tool_ui_type(const nlohmann::json& args);
    nlohmann::json tool_ui_mouse(const nlohmann::json& args);
    nlohmann::json tool_ui_set_control(const nlohmann::json& args);
    /** Captures a PNG bitmap snapshot of root_component or a target component_id on the JUCE Message Thread. */
    nlohmann::json tool_ui_screenshot(const nlohmann::json& args);
    /** Activates, updates, or clears the non-interactive spotlight overlay over a component_id on the Message Thread. */
    nlohmann::json tool_ui_highlight(const nlohmann::json& args);

    // MCP Resources
    nlohmann::json resource_project_state();
    nlohmann::json resource_audio_meters();
    nlohmann::json resource_diagnostics_log();

    // Safe thread dispatch to JUCE Message Thread
    template <typename F>
    auto run_on_message_thread(F&& func) -> decltype(func())
    {
        using ReturnType = decltype(func());
        if (juce::MessageManager::getInstance() != nullptr &&
            juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            return func();
        }

        struct Invoker {
            F fn;
            ReturnType result;
            std::exception_ptr error;
        };
        Invoker inv{ std::forward<F>(func), {}, nullptr };

        juce::MessageManager::getInstance()->callFunctionOnMessageThread(
            [](void* data) -> void* {
                auto* p = static_cast<Invoker*>(data);
                try {
                    p->result = p->fn();
                } catch (...) {
                    p->error = std::current_exception();
                }
                return nullptr;
            },
            &inv
        );

        if (inv.error) {
            std::rethrow_exception(inv.error);
        }
        return inv.result;
    }

    McpContext ctx_;
    std::atomic<bool> is_running_{ false };
    std::atomic<bool> stdio_running_{ false };
    std::atomic<bool> tcp_running_{ false };
    int tcp_port_{ 0 };

    std::thread stdio_thread_;
    std::thread tcp_thread_;
    std::unique_ptr<juce::StreamingSocket> tcp_listener_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(McpServer)
};

} // namespace oae::studio
