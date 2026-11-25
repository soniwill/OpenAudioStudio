#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace oae::studio::mcp {

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;

inline nlohmann::json make_success_response(const nlohmann::json& id, const nlohmann::json& result)
{
    nlohmann::json res;
    res["jsonrpc"] = "2.0";
    res["id"] = id;
    res["result"] = result;
    return res;
}

inline nlohmann::json make_error_response(const nlohmann::json& id, int code, const std::string& message, const nlohmann::json& data = nullptr)
{
    nlohmann::json res;
    res["jsonrpc"] = "2.0";
    res["id"] = id;
    nlohmann::json err;
    err["code"] = code;
    err["message"] = message;
    if (!data.is_null()) {
        err["data"] = data;
    }
    res["error"] = std::move(err);
    return res;
}

inline nlohmann::json get_tools_catalog()
{
    return nlohmann::json::array({
        {
            { "name", "studio_trigger_control" },
            { "description", "Trigger or click a UI control in OpenAudio Studio by its semantic ComponentID" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "component_id", {
                        { "type", "string" },
                        { "description", "Semantic component ID (e.g. btn_play, btn_pause, btn_stop, btn_build_bank)" }
                    }}
                }},
                { "required", nlohmann::json::array({ "component_id" }) }
            }}
        },
        {
            { "name", "studio_select_item" },
            { "description", "Select an authoring item in OpenAudio Studio (event, bus, parameter, snapshot)" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "type", {
                        { "type", "string" },
                        { "enum", nlohmann::json::array({ "event", "bus", "parameter", "snapshot" }) },
                        { "description", "Type of the item to select" }
                    }},
                    { "path", {
                        { "type", "string" },
                        { "description", "Path or identifier of the item" }
                    }}
                }},
                { "required", nlohmann::json::array({ "type", "path" }) }
            }}
        },
        {
            { "name", "studio_move_clip" },
            { "description", "Move an event track timeline instrument clip to a new offset in milliseconds" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "event_path", { { "type", "string" }, { "description", "Event path" } } },
                    { "track_index", { { "type", "integer" }, { "description", "Track index" } } },
                    { "instrument_index", { { "type", "integer" }, { "description", "Instrument clip index" } } },
                    { "new_offset_ms", { { "type", "number" }, { "description", "New timeline start offset in milliseconds" } } }
                }},
                { "required", nlohmann::json::array({ "event_path", "track_index", "instrument_index", "new_offset_ms" }) }
            }}
        },
        {
            { "name", "studio_transport" },
            { "description", "Control the real-time audition transport playback state (play, pause, resume, stop, seek)" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "action", {
                        { "type", "string" },
                        { "enum", nlohmann::json::array({ "play", "pause", "resume", "stop", "seek" }) },
                        { "description", "Audition transport playback action" }
                    }},
                    { "position_ms", {
                        { "type", "number" },
                        { "description", "Target playback position in milliseconds (used with seek)" }
                    }}
                }},
                { "required", nlohmann::json::array({ "action" }) }
            }}
        },
        {
            { "name", "studio_set_mixer_fader" },
            { "description", "Update the volume fader in dB for Master or a child mixer bus/VCA" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "bus_path", {
                        { "type", "string" },
                        { "description", "Bus path (e.g. 'bus:/' for Master, 'bus:/sfx', 'bus:/music')" }
                    }},
                    { "volume_db", {
                        { "type", "number" },
                        { "description", "Target fader volume in decibels (-80.0 to 10.0)" }
                    }}
                }},
                { "required", nlohmann::json::array({ "bus_path", "volume_db" }) }
            }}
        },
        {
            { "name", "studio_build_bank" },
            { "description", "Rebuild the audition sound bank or publish to destination" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "destination_path", {
                        { "type", "string" },
                        { "description", "Optional output sound bank path" }
                    }}
                }}
            }}
        },
        {
            { "name", "studio_ui_inspect" },
            { "description", "Inspect visible UI components, bounds, values, and current keyboard focus in OpenAudio Studio" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "component_id", { { "type", "string" }, { "description", "Optional root component ID to inspect subtree" } } },
                    { "type", { { "type", "string" }, { "description", "Optional component type filter (Button, Slider, TextEditor, ComboBox, Label, TreeView, ListBox, all)" } } },
                    { "visible_only", { { "type", "boolean" }, { "description", "Only return visible components (default: true)" } } },
                    { "max_depth", { { "type", "integer" }, { "description", "Maximum traversal depth (default: 12)" } } },
                    { "include_unnamed", { { "type", "boolean" }, { "description", "Include unnamed generic components (default: false)" } } }
                }}
            }}
        },
        {
            { "name", "studio_ui_key" },
            { "description", "Simulate a real keyboard key press (e.g. tab, shift+tab, enter, escape, delete, f2, ctrl+z, arrows) on the focused or specified UI control" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "key", { { "type", "string" }, { "description", "Key name or shortcut (e.g. tab, shift+tab, enter, escape, space, delete, backspace, up, down, left, right, f2, ctrl+z, ctrl+y, ctrl+d)" } } },
                    { "component_id", { { "type", "string" }, { "description", "Optional component ID to focus before sending the key" } } },
                    { "repeat", { { "type", "integer" }, { "description", "Number of times to press the key (1..50, default: 1)" } } }
                }},
                { "required", nlohmann::json::array({ "key" }) }
            }}
        },
        {
            { "name", "studio_ui_type" },
            { "description", "Simulate a user typing text character-by-character into a focused or specified TextEditor, Slider, or editable Label, optionally pressing Enter or Tab" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "text", { { "type", "string" }, { "description", "Text to type character-by-character" } } },
                    { "component_id", { { "type", "string" }, { "description", "Optional component ID to focus/open editor before typing" } } },
                    { "clear_first", { { "type", "boolean" }, { "description", "Select all and clear existing text before typing (default: false)" } } },
                    { "press_enter", { { "type", "boolean" }, { "description", "Press Enter/Return after typing (default: false)" } } },
                    { "press_tab", { { "type", "boolean" }, { "description", "Press Tab after typing to commit and move focus (default: false)" } } }
                }},
                { "required", nlohmann::json::array({ "text" }) }
            }}
        },
        {
            { "name", "studio_ui_mouse" },
            { "description", "Simulate real mouse events (click, double_click, right_click, down, move, up, drag, wheel) at coordinates or on a component" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "action", {
                        { "type", "string" },
                        { "enum", nlohmann::json::array({ "click", "double_click", "right_click", "down", "move", "up", "drag", "wheel" }) },
                        { "description", "Mouse gesture to simulate" }
                    }},
                    { "component_id", { { "type", "string" }, { "description", "Optional target component ID" } } },
                    { "x", { { "type", "number" }, { "description", "Root-relative X coordinate" } } },
                    { "y", { { "type", "number" }, { "description", "Root-relative Y coordinate" } } },
                    { "offset_x", { { "type", "number" }, { "description", "X offset from component center" } } },
                    { "offset_y", { { "type", "number" }, { "description", "Y offset from component center" } } },
                    { "to_component_id", { { "type", "string" }, { "description", "Destination component ID for drag" } } },
                    { "to_x", { { "type", "number" }, { "description", "Destination root-relative X for drag" } } },
                    { "to_y", { { "type", "number" }, { "description", "Destination root-relative Y for drag" } } },
                    { "steps", { { "type", "integer" }, { "description", "Intermediate drag steps (1..50, default: 8)" } } },
                    { "delta_y", { { "type", "number" }, { "description", "Scroll wheel delta Y" } } },
                    { "ctrl", { { "type", "boolean" }, { "description", "Hold Ctrl modifier" } } },
                    { "shift", { { "type", "boolean" }, { "description", "Hold Shift modifier" } } },
                    { "alt", { { "type", "boolean" }, { "description", "Hold Alt modifier" } } }
                }},
                { "required", nlohmann::json::array({ "action" }) }
            }}
        },
        {
            { "name", "studio_ui_set_control" },
            { "description", "Set value or selection on a UI control (Slider value, ComboBox item by id/index/text, or ToggleButton state) through its UI notifications" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "component_id", { { "type", "string" }, { "description", "Target component ID" } } },
                    { "x", { { "type", "number" }, { "description", "Optional root-relative X coordinate if component has no component_id" } } },
                    { "y", { { "type", "number" }, { "description", "Optional root-relative Y coordinate if component has no component_id" } } },
                    { "value", { { "type", "number" }, { "description", "Numeric value for Slider or selected ID for ComboBox" } } },
                    { "index", { { "type", "integer" }, { "description", "Zero-based item index for ComboBox" } } },
                    { "text", { { "type", "string" }, { "description", "Item text for ComboBox or text for TextEditor" } } },
                    { "toggle", { { "type", "boolean" }, { "description", "Boolean state for ToggleButton" } } }
                }}
            }}
        },
        {
            { "name", "studio_ui_screenshot" },
            { "description", "Render a PNG bitmap snapshot of the full OpenAudio Studio window or a specific UI component on the JUCE Message Thread" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "component_id", { { "type", "string" }, { "description", "Optional semantic component ID to capture (empty or omitted captures the root window)" } } },
                    { "output_path", { { "type", "string" }, { "description", "Optional file path on disk where the PNG image will be saved" } } },
                    { "scale", { { "type", "number" }, { "description", "Optional scale factor clamped to [0.25, 2.0] (default: 1.0)" } } },
                    { "include_base64", { { "type", "boolean" }, { "description", "Include base64-encoded PNG image block in MCP content (default: true when output_path is omitted, false when output_path is provided)" } } }
                }}
            }}
        },
        {
            { "name", "studio_ui_highlight" },
            { "description", "Activate, update, or clear a non-interactive visual spotlight highlight over a semantic UI component on the JUCE Message Thread" },
            { "inputSchema", {
                { "type", "object" },
                { "properties", {
                    { "component_id", { { "type", "string" }, { "description", "Target semantic component ID to highlight (required unless clear is true)" } } },
                    { "label", { { "type", "string" }, { "description", "Optional short caption pill displayed adjacent to the highlighted control" } } },
                    { "dim_opacity", { { "type", "number" }, { "minimum", 0.0 }, { "maximum", 0.85 }, { "description", "Peripheral dim scrim opacity clamped to [0.0, 0.85] (default: 0.45)" } } },
                    { "color", { { "type", "string" }, { "enum", nlohmann::json::array({ "cyan", "amber", "green" }) }, { "description", "Accent border color preset from OaeTheme (default: cyan)" } } },
                    { "clear", { { "type", "boolean" }, { "description", "When true, disables and hides any active highlight overlay (default: false)" } } }
                }}
            }}
        }
    });
}

inline nlohmann::json get_resources_catalog()
{
    return nlohmann::json::array({
        {
            { "uri", "oae://project/state" },
            { "name", "Project Authoring State" },
            { "description", "Complete current JSON authoring project document state" },
            { "mimeType", "application/json" }
        },
        {
            { "uri", "oae://audio/meters" },
            { "name", "Acoustic Telemetry Meters" },
            { "description", "Real-time acoustic Peak dB and RMS dB telemetry for Master and all active mixer buses" },
            { "mimeType", "application/json" }
        },
        {
            { "uri", "oae://diagnostics/log" },
            { "name", "Diagnostics & Build Log" },
            { "description", "Live text console output and build diagnostics messages" },
            { "mimeType", "text/plain" }
        }
    });
}

} // namespace oae::studio::mcp
