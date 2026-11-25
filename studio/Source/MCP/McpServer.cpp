#include "McpServer.h"
#include "../MainComponent.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace oae::studio {

namespace {

juce::Component* find_component_recursive(juce::Component* parent, const juce::String& targetID)
{
    if (!parent) return nullptr;
    if (parent->getComponentID() == targetID) return parent;
    const int count = parent->getNumChildComponents();
    for (int i = 0; i < count; ++i) {
        auto* child = parent->getChildComponent(i);
        if (auto* found = find_component_recursive(child, targetID)) {
            return found;
        }
    }
    return nullptr;
}

std::string classify_component_type(juce::Component* comp)
{
    if (!comp) return "null";
    if (dynamic_cast<juce::Button*>(comp)) return "Button";
    if (dynamic_cast<juce::Slider*>(comp)) return "Slider";
    if (dynamic_cast<juce::TextEditor*>(comp)) return "TextEditor";
    if (dynamic_cast<juce::ComboBox*>(comp)) return "ComboBox";
    if (dynamic_cast<juce::Label*>(comp)) return "Label";
    if (dynamic_cast<juce::TreeView*>(comp)) return "TreeView";
    if (dynamic_cast<juce::ListBox*>(comp)) return "ListBox";
    return "Component";
}

nlohmann::json describe_component(juce::Component* comp, juce::Component* root)
{
    if (!comp) return nullptr;
    const auto bounds = root ? root->getLocalArea(comp, comp->getLocalBounds()) : comp->getBounds();
    nlohmann::json info = {
        { "component_id", comp->getComponentID().toStdString() },
        { "name", comp->getName().toStdString() },
        { "type", classify_component_type(comp) },
        { "visible", comp->isVisible() },
        { "enabled", comp->isEnabled() },
        { "focused", comp->hasKeyboardFocus(true) },
        { "wants_focus", comp->getWantsKeyboardFocus() },
        { "bounds", {
            { "x", bounds.getX() },
            { "y", bounds.getY() },
            { "w", bounds.getWidth() },
            { "h", bounds.getHeight() }
        }}
    };

    if (auto* btn = dynamic_cast<juce::Button*>(comp)) {
        info["text"] = btn->getButtonText().toStdString();
        info["toggle_state"] = btn->getToggleState();
    } else if (auto* te = dynamic_cast<juce::TextEditor*>(comp)) {
        info["text"] = te->getText().toStdString();
        info["read_only"] = te->isReadOnly();
    } else if (auto* slider = dynamic_cast<juce::Slider*>(comp)) {
        info["value"] = slider->getValue();
        info["min"] = slider->getMinimum();
        info["max"] = slider->getMaximum();
    } else if (auto* cb = dynamic_cast<juce::ComboBox*>(comp)) {
        info["text"] = cb->getText().toStdString();
        info["selected_id"] = cb->getSelectedId();
        nlohmann::json items = nlohmann::json::array();
        for (int i = 0; i < cb->getNumItems(); ++i) {
            items.push_back({
                { "id", cb->getItemId(i) },
                { "text", cb->getItemText(i).toStdString() }
            });
        }
        info["items"] = std::move(items);
    } else if (auto* lbl = dynamic_cast<juce::Label*>(comp)) {
        info["text"] = lbl->getText().toStdString();
        info["editable"] = lbl->isEditable();
    }
    return info;
}

void collect_components(juce::Component* comp,
                        juce::Component* root,
                        const std::string& type_filter,
                        bool visible_only,
                        bool include_unnamed,
                        int depth,
                        int max_depth,
                        nlohmann::json& out_list)
{
    if (!comp || depth > max_depth) return;
    if (visible_only && !comp->isVisible()) return;

    const std::string ctype = classify_component_type(comp);
    const bool is_interactive = (ctype != "Component" && ctype != "Label") ||
                                (ctype == "Label" && dynamic_cast<juce::Label*>(comp)->isEditable());
    const bool has_identity = comp->getComponentID().isNotEmpty() || comp->getName().isNotEmpty();
    const bool type_matches = (type_filter.empty() || type_filter == "all" || type_filter == ctype);

    if (type_matches && (include_unnamed || is_interactive || has_identity)) {
        nlohmann::json item = describe_component(comp, root);
        item["depth"] = depth;
        out_list.push_back(std::move(item));
    }

    for (int i = 0; i < comp->getNumChildComponents(); ++i) {
        collect_components(comp->getChildComponent(i), root, type_filter, visible_only, include_unnamed, depth + 1, max_depth, out_list);
    }
}

juce::KeyPress parse_key_string(const std::string& key_str)
{
    juce::String s = juce::String::fromUTF8(key_str.c_str()).trim();
    juce::String lower = s.toLowerCase();

    int mods = 0;
    while (true) {
        if (lower.startsWith("ctrl+")) {
            mods |= juce::ModifierKeys::ctrlModifier;
            s = s.substring(5).trim();
            lower = lower.substring(5).trim();
        } else if (lower.startsWith("shift+")) {
            mods |= juce::ModifierKeys::shiftModifier;
            s = s.substring(6).trim();
            lower = lower.substring(6).trim();
        } else if (lower.startsWith("alt+")) {
            mods |= juce::ModifierKeys::altModifier;
            s = s.substring(4).trim();
            lower = lower.substring(4).trim();
        } else {
            break;
        }
    }

    const juce::ModifierKeys mod_keys(mods);
    if (lower == "tab") return juce::KeyPress(juce::KeyPress::tabKey, mod_keys, 0);
    if (lower == "enter" || lower == "return") return juce::KeyPress(juce::KeyPress::returnKey, mod_keys, '\r');
    if (lower == "escape" || lower == "esc") return juce::KeyPress(juce::KeyPress::escapeKey, mod_keys, 0);
    if (lower == "space") return juce::KeyPress(juce::KeyPress::spaceKey, mod_keys, ' ');
    if (lower == "delete" || lower == "del") return juce::KeyPress(juce::KeyPress::deleteKey, mod_keys, 0);
    if (lower == "backspace") return juce::KeyPress(juce::KeyPress::backspaceKey, mod_keys, 0);
    if (lower == "up") return juce::KeyPress(juce::KeyPress::upKey, mod_keys, 0);
    if (lower == "down") return juce::KeyPress(juce::KeyPress::downKey, mod_keys, 0);
    if (lower == "left") return juce::KeyPress(juce::KeyPress::leftKey, mod_keys, 0);
    if (lower == "right") return juce::KeyPress(juce::KeyPress::rightKey, mod_keys, 0);
    if (lower == "home") return juce::KeyPress(juce::KeyPress::homeKey, mod_keys, 0);
    if (lower == "end") return juce::KeyPress(juce::KeyPress::endKey, mod_keys, 0);
    if (lower == "pageup") return juce::KeyPress(juce::KeyPress::pageUpKey, mod_keys, 0);
    if (lower == "pagedown") return juce::KeyPress(juce::KeyPress::pageDownKey, mod_keys, 0);
    if (lower == "f1") return juce::KeyPress(juce::KeyPress::F1Key, mod_keys, 0);
    if (lower == "f2") return juce::KeyPress(juce::KeyPress::F2Key, mod_keys, 0);
    if (lower == "f3") return juce::KeyPress(juce::KeyPress::F3Key, mod_keys, 0);
    if (lower == "f4") return juce::KeyPress(juce::KeyPress::F4Key, mod_keys, 0);
    if (lower == "f5") return juce::KeyPress(juce::KeyPress::F5Key, mod_keys, 0);

    if (s.length() == 1) {
        const juce::juce_wchar ch = s[0];
        const int code = static_cast<int>(juce::CharacterFunctions::toUpperCase(ch));
        const juce::juce_wchar text_char = (mods & (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier)) ? 0 : ch;
        return juce::KeyPress(code, mod_keys, text_char);
    }

    return juce::KeyPress::createFromDescription(juce::String::fromUTF8(key_str.c_str()));
}

bool dispatch_key_to_hierarchy(juce::Component* root, juce::Component* target, const juce::KeyPress& kp)
{
    if (!target) return false;
    juce::Component::SafePointer<juce::Component> safe_target(target);
    juce::Component* current = target;
    while (current) {
        juce::Component::SafePointer<juce::Component> safe_parent(current->getParentComponent());
        if (current->keyPressed(kp)) {
            return true;
        }
        if (current == root) break;
        current = safe_parent.getComponent();
    }

    if (safe_target && kp.getKeyCode() == juce::KeyPress::tabKey) {
        const bool forward = !kp.getModifiers().isShiftDown();
        if (root) {
            if (auto traverser = root->createFocusTraverser()) {
                juce::Component* next = nullptr;
                if (safe_target.getComponent() == root) {
                    next = traverser->getDefaultComponent(root);
                } else {
                    next = forward ? traverser->getNextComponent(safe_target.getComponent())
                                   : traverser->getPreviousComponent(safe_target.getComponent());
                    if (!next) {
                        next = traverser->getDefaultComponent(root);
                    }
                }
                if (next) {
                    next->grabKeyboardFocus();
                    return true;
                }
            }
        }
        safe_target->moveKeyboardFocusToSibling(forward);
        return true;
    }
    return false;
}

juce::TextEditor* find_child_text_editor(juce::Component* parent)
{
    if (!parent) return nullptr;
    if (auto* te = dynamic_cast<juce::TextEditor*>(parent)) return te;
    for (int i = 0; i < parent->getNumChildComponents(); ++i) {
        if (auto* found = find_child_text_editor(parent->getChildComponent(i))) {
            return found;
        }
    }
    return nullptr;
}

juce::Point<float> resolve_target_point(juce::Component* root, const nlohmann::json& args, bool is_dest = false)
{
    const std::string id_key = is_dest ? "to_component_id" : "component_id";
    const std::string x_key = is_dest ? "to_x" : "x";
    const std::string y_key = is_dest ? "to_y" : "y";

    const std::string cid = args.value(id_key, "");
    if (!cid.empty() && root) {
        if (auto* comp = find_component_recursive(root, juce::String(cid))) {
            const auto area = root->getLocalArea(comp, comp->getLocalBounds());
            const float ox = is_dest ? 0.0f : args.value("offset_x", 0.0f);
            const float oy = is_dest ? 0.0f : args.value("offset_y", 0.0f);
            return { static_cast<float>(area.getCentreX()) + ox, static_cast<float>(area.getCentreY()) + oy };
        }
    }
    return { args.value(x_key, 0.0f), args.value(y_key, 0.0f) };
}

juce::MouseEvent make_mouse_event(juce::Component* target,
                                  juce::Point<float> local_pos,
                                  juce::Point<float> local_down_pos,
                                  juce::ModifierKeys mods,
                                  int clicks,
                                  bool dragged)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(src,
                            local_pos,
                            mods,
                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            target,
                            target,
                            now,
                            local_down_pos,
                            now,
                            clicks,
                            dragged);
}

} // namespace

McpServer::McpServer(MainComponent& main_comp)
    : ctx_{
        main_comp.engine(),
        main_comp.document(),
        main_comp.authoring_model(),
        main_comp.transport(),
        &main_comp,
        &main_comp.preview_rebuilder(),
        &main_comp.diagnostics(),
        &main_comp.publisher(),
        &main_comp.highlight_overlay()
    }
{
}

McpServer::McpServer(McpContext ctx)
    : ctx_(ctx)
{
}

McpServer::~McpServer()
{
    stop();
}

std::string McpServer::handle_message(const std::string& request_json)
{
    if (request_json.empty()) {
        return {};
    }

    nlohmann::json req = nlohmann::json::parse(request_json, nullptr, false);
    if (req.is_discarded()) {
        return mcp::make_error_response(nullptr, mcp::kParseError, "Parse error: invalid JSON").dump();
    }

    nlohmann::json response = process_request(req);
    if (response.is_null()) {
        return {};
    }
    return response.dump();
}

nlohmann::json McpServer::process_request(const nlohmann::json& req)
{
    if (!req.is_object()) {
        return mcp::make_error_response(nullptr, mcp::kInvalidRequest, "Invalid Request: root must be JSON object");
    }

    const nlohmann::json id = req.value("id", nlohmann::json(nullptr));
    const std::string method = req.value("method", "");
    const nlohmann::json params = req.value("params", nlohmann::json::object());

    if (method == "initialize") {
        return handle_initialize(id, params);
    }
    if (method == "notifications/initialized" || method == "initialized") {
        if (!id.is_null()) {
            return mcp::make_success_response(id, nlohmann::json::object());
        }
        return nullptr;
    }
    if (method == "tools/list") {
        return handle_tools_list(id);
    }
    if (method == "tools/call") {
        return handle_tools_call(id, params);
    }
    if (method == "resources/list") {
        return handle_resources_list(id);
    }
    if (method == "resources/read") {
        return handle_resources_read(id, params);
    }
    if (method == "ping") {
        return mcp::make_success_response(id, nlohmann::json::object());
    }

    return mcp::make_error_response(id, mcp::kMethodNotFound, "Method not found: " + method);
}

nlohmann::json McpServer::handle_initialize(const nlohmann::json& id, const nlohmann::json& /*params*/)
{
    nlohmann::json res;
    res["protocolVersion"] = "2024-11-05";
    res["capabilities"] = {
        { "tools", nlohmann::json::object() },
        { "resources", nlohmann::json::object() }
    };
    res["serverInfo"] = {
        { "name", "openaudio-studio-mcp" },
        { "version", "1.0.0" }
    };
    return mcp::make_success_response(id, res);
}

nlohmann::json McpServer::handle_tools_list(const nlohmann::json& id)
{
    nlohmann::json res;
    res["tools"] = mcp::get_tools_catalog();
    return mcp::make_success_response(id, res);
}

nlohmann::json McpServer::handle_tools_call(const nlohmann::json& id, const nlohmann::json& params)
{
    if (!params.is_object() || !params.contains("name") || !params["name"].is_string()) {
        return mcp::make_error_response(id, mcp::kInvalidParams, "Missing or invalid tool 'name'");
    }

    const std::string tool_name = params["name"].get<std::string>();
    const nlohmann::json args = params.value("arguments", nlohmann::json::object());

    // Execute tool on JUCE message thread with RFC 6902 JSON patch diffing
    nlohmann::json tool_result = run_on_message_thread([this, &tool_name, &args]() -> nlohmann::json {
        const nlohmann::json before_state = ctx_.authoring_model.json();

        nlohmann::json tr;
        if (tool_name == "studio_trigger_control") {
            tr = tool_trigger_control(args);
        } else if (tool_name == "studio_select_item") {
            tr = tool_select_item(args);
        } else if (tool_name == "studio_move_clip") {
            tr = tool_move_clip(args);
        } else if (tool_name == "studio_transport") {
            tr = tool_transport(args);
        } else if (tool_name == "studio_set_mixer_fader") {
            tr = tool_set_mixer_fader(args);
        } else if (tool_name == "studio_build_bank") {
            tr = tool_build_bank(args);
        } else if (tool_name == "studio_ui_inspect") {
            tr = tool_ui_inspect(args);
        } else if (tool_name == "studio_ui_key") {
            tr = tool_ui_key(args);
        } else if (tool_name == "studio_ui_type") {
            tr = tool_ui_type(args);
        } else if (tool_name == "studio_ui_mouse") {
            tr = tool_ui_mouse(args);
        } else if (tool_name == "studio_ui_set_control") {
            tr = tool_ui_set_control(args);
        } else if (tool_name == "studio_ui_screenshot") {
            tr = tool_ui_screenshot(args);
        } else if (tool_name == "studio_ui_highlight") {
            tr = tool_ui_highlight(args);
        } else {
            tr["content"] = nlohmann::json::array({
                { { "type", "text" }, { "text", "Unknown tool: " + tool_name } }
            });
            tr["isError"] = true;
            tr["diff"] = nlohmann::json::array();
            return tr;
        }

        const nlohmann::json after_state = ctx_.authoring_model.json();
        tr["diff"] = nlohmann::json::diff(before_state, after_state);
        return tr;
    });

    return mcp::make_success_response(id, tool_result);
}

nlohmann::json McpServer::tool_trigger_control(const nlohmann::json& args)
{
    const std::string cid = args.value("component_id", "");
    if (cid.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'component_id'" } } } },
            { "isError", true }
        };
    }

    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    juce::Component* target = find_component_recursive(ctx_.root_component, juce::String(cid));

    if (!target) {
        return {
            { "content", { { { "type", "text" }, { "text", "Component not found: " + cid } } } },
            { "isError", true }
        };
    }

    if (auto* btn = dynamic_cast<juce::Button*>(target)) {
        if (btn->getClickingTogglesState()) {
            btn->setToggleState(!btn->getToggleState(), juce::sendNotificationSync);
        }
        if (btn->onClick) {
            btn->onClick();
        } else {
            btn->triggerClick();
        }
        return {
            { "content", { { { "type", "text" }, { "text", "Button triggered: " + cid } } } },
            { "isError", false }
        };
    }

    if (auto* slider = dynamic_cast<juce::Slider*>(target)) {
        (void)slider;
        return {
            { "content", { { { "type", "text" }, { "text", "Slider control located: " + cid } } } },
            { "isError", false }
        };
    }

    return {
        { "content", { { { "type", "text" }, { "text", "Control triggered: " + cid } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_select_item(const nlohmann::json& args)
{
    const std::string type_str = args.value("type", "");
    const std::string path = args.value("path", "");
    if (type_str.empty() || path.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'type' or 'path'" } } } },
            { "isError", true }
        };
    }

    ItemType it = ItemType::Event;
    if (type_str == "event") it = ItemType::Event;
    else if (type_str == "bus") it = ItemType::Bus;
    else if (type_str == "parameter") it = ItemType::GlobalParameter;
    else if (type_str == "snapshot") it = ItemType::Snapshot;
    else if (type_str == "vca") it = ItemType::Vca;
    else {
        return {
            { "content", { { { "type", "text" }, { "text", "Unknown item type: " + type_str } } } },
            { "isError", true }
        };
    }

    ctx_.authoring_model.select(ItemSelection{ it, path });
    return {
        { "content", { { { "type", "text" }, { "text", "Selected " + type_str + ": " + path } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_move_clip(const nlohmann::json& args)
{
    const std::string event_path = args.value("event_path", "");
    const int track_idx = args.value("track_index", 0);
    const int inst_idx = args.value("instrument_index", 0);
    const double new_offset = args.value("new_offset_ms", 0.0);

    if (event_path.empty() || track_idx < 0 || inst_idx < 0) {
        return {
            { "content", { { { "type", "text" }, { "text", "Invalid arguments for studio_move_clip" } } } },
            { "isError", true }
        };
    }

    const bool ok = ctx_.authoring_model.move_instrument(
        event_path,
        static_cast<size_t>(track_idx),
        static_cast<size_t>(inst_idx),
        static_cast<size_t>(track_idx),
        new_offset
    );

    if (!ok) {
        return {
            { "content", { { { "type", "text" }, { "text", "Failed to move clip: item not found" } } } },
            { "isError", true }
        };
    }

    return {
        { "content", { { { "type", "text" }, { "text", "Moved clip in " + event_path + " to " + std::to_string(new_offset) + " ms" } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_transport(const nlohmann::json& args)
{
    const std::string action = args.value("action", "");
    bool ok = false;
    if (action == "play") {
        ok = ctx_.transport.play();
    } else if (action == "pause") {
        ok = ctx_.transport.pause();
    } else if (action == "resume") {
        ok = ctx_.transport.resume();
    } else if (action == "stop") {
        ok = ctx_.transport.stop();
    } else if (action == "seek") {
        const double pos_ms = args.value("position_ms", 0.0);
        ok = ctx_.transport.seek(pos_ms);
    } else {
        return {
            { "content", { { { "type", "text" }, { "text", "Unknown transport action: " + action } } } },
            { "isError", true }
        };
    }

    return {
        { "content", { { { "type", "text" }, { "text", "Transport action '" + action + "' executed (" + (ok ? "success" : "idle") + ")" } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_set_mixer_fader(const nlohmann::json& args)
{
    const std::string bus_path = args.value("bus_path", "");
    const double vol_db = args.value("volume_db", 0.0);

    if (bus_path.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing 'bus_path'" } } } },
            { "isError", true }
        };
    }

    const double linear_vol = (vol_db <= -80.0) ? 0.0 : std::pow(10.0, vol_db / 20.0);

    // 1. Mutate AuthoringModel project JSON
    ctx_.authoring_model.modify([bus_path, linear_vol](nlohmann::json& r) {
        if (bus_path == "bus:/" || bus_path == "Master" || bus_path == "master") {
            if (!r.contains("mixer")) r["mixer"] = nlohmann::json::object();
            if (!r["mixer"].contains("master")) r["mixer"]["master"] = nlohmann::json::object();
            r["mixer"]["master"]["volume"] = linear_vol;
        } else {
            if (r.contains("buses") && r["buses"].is_array()) {
                for (auto& item : r["buses"]) {
                    if (item.value("path", "") == bus_path) {
                        item["volume"] = linear_vol;
                        break;
                    }
                }
            }
        }
    });

    // 2. Update real-time engine bus DSP
    oae_bus b = 0;
    if (ctx_.engine.get_bus(bus_path, &b) == OAE_OK) {
        ctx_.engine.set_bus_volume_db(b, static_cast<float>(vol_db));
    }

    return {
        { "content", { { { "type", "text" }, { "text", "Mixer fader for " + bus_path + " set to " + std::to_string(vol_db) + " dB" } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_build_bank(const nlohmann::json& args)
{
    fs::path bankc_exe;
    if (ctx_.preview_rebuilder) {
        bankc_exe = ctx_.preview_rebuilder->get_bankc_executable();
    }
    if (bankc_exe.empty()) {
        const auto app_file = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const auto app_dir = fs::u8path(app_file.getParentDirectory().getFullPathName().toStdString());
        const std::vector<fs::path> candidates = {
            app_dir / "oae_bankc.exe",
            app_dir / "oae_bankc",
            app_dir / "Debug" / "oae_bankc.exe",
            app_dir / "Release" / "oae_bankc.exe",
            app_dir / ".." / "Debug" / "oae_bankc.exe",
            app_dir / ".." / "Release" / "oae_bankc.exe"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) { bankc_exe = fs::canonical(c); break; }
        }
        if (ctx_.preview_rebuilder && !bankc_exe.empty()) {
            ctx_.preview_rebuilder->set_bankc_executable(bankc_exe);
        }
    }

    const std::string dest_path_str = args.value("destination_path", "");
    if (!dest_path_str.empty() && ctx_.publisher) {
        if (!bankc_exe.empty()) {
            const fs::path dest_bank = fs::u8path(dest_path_str);
            ctx_.publisher->start_publish(bankc_exe, ctx_.authoring_model.create_immutable_snapshot(), ctx_.document.get_project_dir(), dest_bank);
            ctx_.publisher->wait_until_finished();
            auto res = ctx_.publisher->get_last_result();
            return {
                { "content", { { { "type", "text" }, { "text", res.success ? "Bank published to " + dest_path_str : "Publish failed: " + res.diagnostics } } } },
                { "isError", !res.success }
            };
        }
    }

    if (ctx_.preview_rebuilder) {
        const bool ok = ctx_.preview_rebuilder->rebuild_synchronously();
        std::string diag;
        if (ctx_.publisher) {
            diag = ctx_.publisher->get_diagnostics();
        }
        return {
            { "content", { { { "type", "text" }, { "text", ok ? "Bank built and hot-swapped successfully" : ("Bank build failed: " + diag) } } } },
            { "isError", !ok }
        };
    }

    return {
        { "content", { { { "type", "text" }, { "text", "Build triggered successfully" } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_inspect(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    const std::string start_id = args.value("component_id", "");
    const std::string type_filter = args.value("type", "all");
    const bool visible_only = args.value("visible_only", true);
    const int max_depth = std::clamp(args.value("max_depth", 12), 1, 32);
    const bool include_unnamed = args.value("include_unnamed", false);

    juce::Component* start = ctx_.root_component;
    if (!start_id.empty()) {
        start = find_component_recursive(ctx_.root_component, juce::String(start_id));
        if (!start) {
            return {
                { "content", { { { "type", "text" }, { "text", "Component not found: " + start_id } } } },
                { "isError", true }
            };
        }
    }

    nlohmann::json components = nlohmann::json::array();
    collect_components(start, ctx_.root_component, type_filter, visible_only, include_unnamed, 0, max_depth, components);

    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    nlohmann::json payload = {
        { "root_size", { { "width", ctx_.root_component->getWidth() }, { "height", ctx_.root_component->getHeight() } } },
        { "focused_component", focused ? describe_component(focused, ctx_.root_component) : nlohmann::json(nullptr) },
        { "count", components.size() },
        { "components", std::move(components) }
    };

    return {
        { "content", { { { "type", "text" }, { "text", payload.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_key(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    const std::string key_str = args.value("key", "");
    if (key_str.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'key'" } } } },
            { "isError", true }
        };
    }

    const std::string cid = args.value("component_id", "");
    if (!cid.empty()) {
        auto* comp = find_component_recursive(ctx_.root_component, juce::String(cid));
        if (!comp) {
            return {
                { "content", { { { "type", "text" }, { "text", "Component not found: " + cid } } } },
                { "isError", true }
            };
        }
        comp->grabKeyboardFocus();
    }

    const juce::KeyPress kp = parse_key_string(key_str);
    if (!kp.isValid()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Invalid key specification: " + key_str } } } },
            { "isError", true }
        };
    }

    const int repeat = std::clamp(args.value("repeat", 1), 1, 50);
    bool any_consumed = false;
    for (int i = 0; i < repeat; ++i) {
        juce::Component* target = juce::Component::getCurrentlyFocusedComponent();
        if (!target) {
            target = ctx_.root_component;
        }
        if (dispatch_key_to_hierarchy(ctx_.root_component, target, kp)) {
            any_consumed = true;
        }
    }

    auto* focused_after = juce::Component::getCurrentlyFocusedComponent();
    nlohmann::json result = {
        { "key", key_str },
        { "consumed", any_consumed },
        { "focused_after", focused_after ? describe_component(focused_after, ctx_.root_component) : nlohmann::json(nullptr) }
    };

    return {
        { "content", { { { "type", "text" }, { "text", result.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_type(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    const std::string text_utf8 = args.value("text", "");
    const std::string cid = args.value("component_id", "");
    const bool clear_first = args.value("clear_first", false);
    const bool press_enter = args.value("press_enter", false);
    const bool press_tab = args.value("press_tab", false);

    if (!cid.empty()) {
        auto* comp = find_component_recursive(ctx_.root_component, juce::String(cid));
        if (!comp) {
            return {
                { "content", { { { "type", "text" }, { "text", "Component not found: " + cid } } } },
                { "isError", true }
            };
        }
        if (auto* lbl = dynamic_cast<juce::Label*>(comp)) {
            if (lbl->isEditable()) {
                lbl->showEditor();
                if (auto* ed = lbl->getCurrentTextEditor()) {
                    ed->grabKeyboardFocus();
                }
            }
        } else if (auto* slider = dynamic_cast<juce::Slider*>(comp)) {
            slider->showTextBox();
            if (auto* ed = find_child_text_editor(slider)) {
                ed->grabKeyboardFocus();
            }
        } else if (auto* ed = find_child_text_editor(comp)) {
            ed->grabKeyboardFocus();
        } else {
            comp->grabKeyboardFocus();
        }
    }

    juce::Component::SafePointer<juce::Component> safe_target(juce::Component::getCurrentlyFocusedComponent());
    if (!safe_target) {
        return {
            { "content", { { { "type", "text" }, { "text", "No component has keyboard focus for typing" } } } },
            { "isError", true }
        };
    }

    if (clear_first) {
        if (auto* te = dynamic_cast<juce::TextEditor*>(safe_target.getComponent())) {
            te->selectAll();
        } else {
            dispatch_key_to_hierarchy(ctx_.root_component, safe_target.getComponent(), parse_key_string("ctrl+a"));
        }
        if (safe_target) {
            dispatch_key_to_hierarchy(ctx_.root_component, safe_target.getComponent(), parse_key_string("backspace"));
        }
    }

    const juce::String jtext = juce::String::fromUTF8(text_utf8.c_str());
    int typed_count = 0;
    for (int i = 0; i < jtext.length() && safe_target; ++i) {
        const juce::juce_wchar ch = jtext[i];
        const juce::KeyPress kp(static_cast<int>(ch), juce::ModifierKeys(), ch);
        dispatch_key_to_hierarchy(ctx_.root_component, safe_target.getComponent(), kp);
        ++typed_count;
    }

    std::string text_after;
    if (auto* te = dynamic_cast<juce::TextEditor*>(safe_target.getComponent())) {
        text_after = te->getText().toStdString();
    }

    if (press_enter && safe_target) {
        dispatch_key_to_hierarchy(ctx_.root_component, safe_target.getComponent(), parse_key_string("enter"));
    }
    if (press_tab && safe_target) {
        dispatch_key_to_hierarchy(ctx_.root_component, safe_target.getComponent(), parse_key_string("tab"));
    }

    auto* focused_after = juce::Component::getCurrentlyFocusedComponent();
    nlohmann::json result = {
        { "typed_length", typed_count },
        { "text_after", text_after },
        { "focused_after", focused_after ? describe_component(focused_after, ctx_.root_component) : nlohmann::json(nullptr) }
    };

    return {
        { "content", { { { "type", "text" }, { "text", result.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_mouse(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    const std::string action = args.value("action", "");
    if (action.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'action'" } } } },
            { "isError", true }
        };
    }

    int mod_flags = 0;
    if (args.value("ctrl", false)) mod_flags |= juce::ModifierKeys::ctrlModifier;
    if (args.value("shift", false)) mod_flags |= juce::ModifierKeys::shiftModifier;
    if (args.value("alt", false)) mod_flags |= juce::ModifierKeys::altModifier;
    const bool is_right = (action == "right_click");
    if (is_right) {
        mod_flags |= juce::ModifierKeys::rightButtonModifier;
    } else if (action != "move" && action != "wheel") {
        mod_flags |= juce::ModifierKeys::leftButtonModifier;
    }
    const juce::ModifierKeys mods(mod_flags);

    const juce::Point<float> start_pt = resolve_target_point(ctx_.root_component, args, false);
    juce::Component* hit = ctx_.root_component->getComponentAt(start_pt.roundToInt());
    if (!hit) {
        return {
            { "content", { { { "type", "text" }, { "text", "No component at target coordinates" } } } },
            { "isError", true }
        };
    }

    juce::Component::SafePointer<juce::Component> safe_target(hit);
    const auto to_local = [this](juce::Component* comp, juce::Point<float> root_pt) -> juce::Point<float> {
        return comp->getLocalPoint(ctx_.root_component, root_pt);
    };
    const juce::Point<float> local_start = to_local(hit, start_pt);

    bool destroyed_mid_gesture = false;
    if (action == "move") {
        safe_target->mouseMove(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 0, false));
    } else if (action == "down") {
        if (safe_target->getWantsKeyboardFocus()) safe_target->grabKeyboardFocus();
        if (safe_target) safe_target->mouseDown(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
    } else if (action == "up") {
        safe_target->mouseUp(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
    } else if (action == "click" || action == "right_click") {
        if (safe_target->getWantsKeyboardFocus()) safe_target->grabKeyboardFocus();
        if (safe_target) safe_target->mouseDown(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
        if (safe_target) {
            safe_target->mouseUp(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
        } else {
            destroyed_mid_gesture = true;
        }
    } else if (action == "double_click") {
        if (safe_target->getWantsKeyboardFocus()) safe_target->grabKeyboardFocus();
        if (safe_target) safe_target->mouseDown(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
        if (safe_target) safe_target->mouseUp(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
        if (safe_target) safe_target->mouseDown(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 2, false));
        if (safe_target) safe_target->mouseDoubleClick(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 2, false));
        if (safe_target) {
            safe_target->mouseUp(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 2, false));
        } else {
            destroyed_mid_gesture = true;
        }
    } else if (action == "drag") {
        const juce::Point<float> end_pt = resolve_target_point(ctx_.root_component, args, true);
        const int steps = std::clamp(args.value("steps", 8), 1, 50);
        if (safe_target->getWantsKeyboardFocus()) safe_target->grabKeyboardFocus();
        if (safe_target) safe_target->mouseDown(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 1, false));
        for (int s = 1; s <= steps && safe_target; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            const juce::Point<float> cur_root = start_pt + (end_pt - start_pt) * t;
            const juce::Point<float> cur_local = to_local(safe_target.getComponent(), cur_root);
            safe_target->mouseDrag(make_mouse_event(safe_target.getComponent(), cur_local, local_start, mods, 1, true));
        }
        if (safe_target) {
            const juce::Point<float> end_local = to_local(safe_target.getComponent(), end_pt);
            safe_target->mouseUp(make_mouse_event(safe_target.getComponent(), end_local, local_start, mods, 1, true));
        } else {
            destroyed_mid_gesture = true;
        }
    } else if (action == "wheel") {
        const float dy = args.value("delta_y", 0.0f);
        const juce::MouseWheelDetails wheel{ 0.0f, dy, false, false };
        safe_target->mouseWheelMove(make_mouse_event(safe_target.getComponent(), local_start, local_start, mods, 0, false), wheel);
    } else {
        return {
            { "content", { { { "type", "text" }, { "text", "Unknown mouse action: " + action } } } },
            { "isError", true }
        };
    }

    nlohmann::json result = {
        { "action", action },
        { "root_point", { { "x", start_pt.x }, { "y", start_pt.y } } },
        { "target_destroyed_during_gesture", destroyed_mid_gesture },
        { "target_after", safe_target ? describe_component(safe_target.getComponent(), ctx_.root_component) : nlohmann::json(nullptr) }
    };

    return {
        { "content", { { { "type", "text" }, { "text", result.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_set_control(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    juce::Component* target = nullptr;
    const std::string cid = args.value("component_id", "");
    if (!cid.empty()) {
        target = find_component_recursive(ctx_.root_component, juce::String(cid));
    } else if (args.contains("x") && args.contains("y")) {
        target = ctx_.root_component->getComponentAt(static_cast<int>(args.value("x", 0.0f)),
                                                     static_cast<int>(args.value("y", 0.0f)));
    }

    if (!target) {
        return {
            { "content", { { { "type", "text" }, { "text", "Target control not found" } } } },
            { "isError", true }
        };
    }

    juce::Component::SafePointer<juce::Component> safe_target(target);
    if (auto* slider = dynamic_cast<juce::Slider*>(target)) {
        if (!args.contains("value")) {
            return {
                { "content", { { { "type", "text" }, { "text", "Missing 'value' for Slider" } } } },
                { "isError", true }
            };
        }
        slider->setValue(args["value"].get<double>(), juce::sendNotificationSync);
    } else if (auto* cb = dynamic_cast<juce::ComboBox*>(target)) {
        if (args.contains("value")) {
            cb->setSelectedId(args["value"].get<int>(), juce::sendNotificationSync);
        } else if (args.contains("index")) {
            cb->setSelectedItemIndex(args["index"].get<int>(), juce::sendNotificationSync);
        } else if (args.contains("text")) {
            const std::string want = args["text"].get<std::string>();
            int found_id = 0;
            for (int i = 0; i < cb->getNumItems(); ++i) {
                if (cb->getItemText(i).toStdString() == want) {
                    found_id = cb->getItemId(i);
                    break;
                }
            }
            if (found_id == 0) {
                return {
                    { "content", { { { "type", "text" }, { "text", "ComboBox item not found: " + want } } } },
                    { "isError", true }
                };
            }
            cb->setSelectedId(found_id, juce::sendNotificationSync);
        } else {
            return {
                { "content", { { { "type", "text" }, { "text", "Provide 'value', 'index', or 'text' for ComboBox" } } } },
                { "isError", true }
            };
        }
    } else if (auto* btn = dynamic_cast<juce::Button*>(target)) {
        if (args.contains("toggle")) {
            btn->setToggleState(args["toggle"].get<bool>(), juce::sendNotificationSync);
        } else {
            btn->triggerClick();
        }
    } else if (auto* te = dynamic_cast<juce::TextEditor*>(target)) {
        if (!args.contains("text")) {
            return {
                { "content", { { { "type", "text" }, { "text", "Missing 'text' for TextEditor" } } } },
                { "isError", true }
            };
        }
        te->setText(juce::String::fromUTF8(args["text"].get<std::string>().c_str()), true);
    } else {
        return {
            { "content", { { { "type", "text" }, { "text", "Unsupported control type: " + classify_component_type(target) } } } },
            { "isError", true }
        };
    }

    nlohmann::json result = {
        { "control_after", safe_target ? describe_component(safe_target.getComponent(), ctx_.root_component) : nlohmann::json(nullptr) }
    };
    return {
        { "content", { { { "type", "text" }, { "text", result.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_screenshot(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    const std::string cid = (args.contains("component_id") && args["component_id"].is_string())
        ? args["component_id"].get<std::string>()
        : std::string{};

    juce::Component* target = ctx_.root_component;
    if (!cid.empty()) {
        target = find_component_recursive(ctx_.root_component, juce::String::fromUTF8(cid.c_str()));
        if (!target) {
            return {
                { "content", { { { "type", "text" }, { "text", "Component not found: " + cid } } } },
                { "isError", true }
            };
        }
    }

    const auto local_bounds = target->getLocalBounds();
    if (local_bounds.getWidth() <= 0 || local_bounds.getHeight() <= 0) {
        return {
            { "content", { { { "type", "text" }, { "text", "Component has non-renderable bounds (width <= 0 or height <= 0): " + (cid.empty() ? std::string("root") : cid) } } } },
            { "isError", true }
        };
    }

    float scale = 1.0f;
    if (args.contains("scale") && args["scale"].is_number()) {
        const float raw_scale = args["scale"].get<float>();
        if (std::isfinite(raw_scale)) {
            scale = std::clamp(raw_scale, 0.25f, 2.0f);
        }
    }

    juce::Image snapshot = target->createComponentSnapshot(local_bounds, true, scale);
    if (!snapshot.isValid()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Failed to render component snapshot" } } } },
            { "isError", true }
        };
    }

    juce::MemoryOutputStream png_mem;
    juce::PNGImageFormat png_format;
    if (!png_format.writeImageToStream(snapshot, png_mem) || png_mem.getDataSize() == 0) {
        return {
            { "content", { { { "type", "text" }, { "text", "Failed to encode snapshot as PNG" } } } },
            { "isError", true }
        };
    }

    const std::string output_path_raw = (args.contains("output_path") && args["output_path"].is_string())
        ? args["output_path"].get<std::string>()
        : std::string{};
    const bool has_output_path = !output_path_raw.empty();
    const bool include_base64 = (args.contains("include_base64") && args["include_base64"].is_boolean())
        ? args["include_base64"].get<bool>()
        : !has_output_path;

    fs::path out_path;
    if (has_output_path) {
        std::error_code ec;
        out_path = fs::absolute(fs::u8path(output_path_raw), ec);
        if (ec) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid output_path: " + output_path_raw + " (" + ec.message() + ")" } } } },
                { "isError", true }
            };
        }
        if (out_path.has_parent_path()) {
            fs::create_directories(out_path.parent_path(), ec);
            if (ec) {
                return {
                    { "content", { { { "type", "text" }, { "text", "Failed to create output directory: " + out_path.parent_path().u8string() + " (" + ec.message() + ")" } } } },
                    { "isError", true }
                };
            }
        }
        std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Failed to open output file for writing: " + out_path.u8string() } } } },
                { "isError", true }
            };
        }
        ofs.write(static_cast<const char*>(png_mem.getData()), static_cast<std::streamsize>(png_mem.getDataSize()));
        ofs.flush();
        if (!ofs.good()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Failed to write PNG bytes to file: " + out_path.u8string() } } } },
                { "isError", true }
            };
        }
    }

    nlohmann::json meta = {
        { "component_id", cid.empty() ? "root" : cid },
        { "width", snapshot.getWidth() },
        { "height", snapshot.getHeight() },
        { "scale", scale },
        { "byte_size", png_mem.getDataSize() }
    };
    if (has_output_path) {
        meta["output_path"] = out_path.u8string();
    }

    nlohmann::json content = nlohmann::json::array();
    content.push_back({
        { "type", "text" },
        { "text", meta.dump() }
    });
    if (include_base64) {
        const juce::String b64 = juce::Base64::toBase64(png_mem.getData(), png_mem.getDataSize());
        content.push_back({
            { "type", "image" },
            { "mimeType", "image/png" },
            { "data", b64.toStdString() }
        });
    }

    return {
        { "content", std::move(content) },
        { "isError", false }
    };
}

nlohmann::json McpServer::tool_ui_highlight(const nlohmann::json& args)
{
    if (!ctx_.root_component) {
        return {
            { "content", { { { "type", "text" }, { "text", "No root component available" } } } },
            { "isError", true }
        };
    }

    if (!ctx_.highlight_overlay) {
        return {
            { "content", { { { "type", "text" }, { "text", "No tutorial highlight overlay available" } } } },
            { "isError", true }
        };
    }

    bool clear = false;
    if (args.contains("clear")) {
        if (!args["clear"].is_boolean()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid 'clear' parameter: expected boolean" } } } },
                { "isError", true }
            };
        }
        clear = args["clear"].get<bool>();
    }

    if (clear) {
        ctx_.highlight_overlay->clear();
        const nlohmann::json payload = {
            { "active", false }
        };
        return {
            { "content", { { { "type", "text" }, { "text", payload.dump() } } } },
            { "isError", false }
        };
    }

    if (!args.contains("component_id") || !args["component_id"].is_string()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'component_id'" } } } },
            { "isError", true }
        };
    }

    const std::string cid = args["component_id"].get<std::string>();
    if (cid.empty()) {
        return {
            { "content", { { { "type", "text" }, { "text", "Missing required 'component_id'" } } } },
            { "isError", true }
        };
    }

    std::string label_str;
    if (args.contains("label")) {
        if (!args["label"].is_string()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid 'label' parameter: expected string" } } } },
                { "isError", true }
            };
        }
        label_str = args["label"].get<std::string>();
    }

    double dim_opacity = 0.45;
    if (args.contains("dim_opacity")) {
        if (!args["dim_opacity"].is_number()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid 'dim_opacity' parameter: expected number" } } } },
                { "isError", true }
            };
        }
        const double raw_dim = args["dim_opacity"].get<double>();
        if (!std::isfinite(raw_dim)) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid 'dim_opacity' parameter: non-finite number" } } } },
                { "isError", true }
            };
        }
        dim_opacity = std::clamp(raw_dim, 0.0, 0.85);
    }

    std::string color_str = "cyan";
    if (args.contains("color")) {
        if (!args["color"].is_string()) {
            return {
                { "content", { { { "type", "text" }, { "text", "Invalid 'color' parameter: expected string" } } } },
                { "isError", true }
            };
        }
        color_str = args["color"].get<std::string>();
    }

    HighlightColor color_enum = HighlightColor::Cyan;
    if (color_str == "cyan") {
        color_enum = HighlightColor::Cyan;
    } else if (color_str == "amber") {
        color_enum = HighlightColor::Amber;
    } else if (color_str == "green") {
        color_enum = HighlightColor::Green;
    } else {
        return {
            { "content", { { { "type", "text" }, { "text", "Invalid highlight color: " + color_str + " (expected cyan, amber, or green)" } } } },
            { "isError", true }
        };
    }

    juce::Component* target = find_component_recursive(ctx_.root_component, juce::String::fromUTF8(cid.c_str()));
    if (!target) {
        return {
            { "content", { { { "type", "text" }, { "text", "Component not found: " + cid } } } },
            { "isError", true }
        };
    }

    if (!TutorialHighlightOverlay::is_visible_under_root(target, ctx_.root_component)) {
        return {
            { "content", { { { "type", "text" }, { "text", "Component is not visible: " + cid } } } },
            { "isError", true }
        };
    }

    const auto bounds = ctx_.root_component->getLocalArea(target, target->getLocalBounds());
    if (target->getWidth() <= 0 || target->getHeight() <= 0 || bounds.getWidth() <= 0 || bounds.getHeight() <= 0) {
        return {
            { "content", { { { "type", "text" }, { "text", "Component has non-renderable bounds (width <= 0 or height <= 0): " + cid } } } },
            { "isError", true }
        };
    }

    ctx_.highlight_overlay->set_highlight(
        target,
        juce::String::fromUTF8(label_str.c_str()),
        static_cast<float>(dim_opacity),
        color_enum);

    const nlohmann::json payload = {
        { "active", true },
        { "component_id", cid },
        { "bounds", {
            { "x", bounds.getX() },
            { "y", bounds.getY() },
            { "w", bounds.getWidth() },
            { "h", bounds.getHeight() }
        }},
        { "label", ctx_.highlight_overlay->label().toStdString() },
        { "dim_opacity", dim_opacity },
        { "color", color_str }
    };

    return {
        { "content", { { { "type", "text" }, { "text", payload.dump() } } } },
        { "isError", false }
    };
}

nlohmann::json McpServer::handle_resources_list(const nlohmann::json& id)
{
    nlohmann::json res;
    res["resources"] = mcp::get_resources_catalog();
    return mcp::make_success_response(id, res);
}

nlohmann::json McpServer::handle_resources_read(const nlohmann::json& id, const nlohmann::json& params)
{
    const std::string uri = params.value("uri", "");
    if (uri.empty()) {
        return mcp::make_error_response(id, mcp::kInvalidParams, "Missing 'uri' in resources/read");
    }

    nlohmann::json content_obj;
    if (uri == "oae://project/state") {
        content_obj = run_on_message_thread([this]() { return resource_project_state(); });
    } else if (uri == "oae://audio/meters") {
        content_obj = run_on_message_thread([this]() { return resource_audio_meters(); });
    } else if (uri == "oae://diagnostics/log") {
        content_obj = run_on_message_thread([this]() { return resource_diagnostics_log(); });
    } else {
        return mcp::make_error_response(id, mcp::kInvalidParams, "Resource not found: " + uri);
    }

    nlohmann::json res;
    res["contents"] = nlohmann::json::array({ std::move(content_obj) });
    return mcp::make_success_response(id, res);
}

nlohmann::json McpServer::resource_project_state()
{
    nlohmann::json item;
    item["uri"] = "oae://project/state";
    item["mimeType"] = "application/json";
    item["text"] = ctx_.authoring_model.json().dump(2);
    return item;
}

nlohmann::json McpServer::resource_audio_meters()
{
    nlohmann::json meters;
    oae_bus mb = 0;
    float m_peak = -80.0f;
    float m_rms = -80.0f;
    if (ctx_.engine.get_bus("bus:/", &mb) == OAE_OK) {
        oae_meter m{};
        if (ctx_.engine.get_bus_meter(mb, &m) == OAE_OK) {
            m_peak = (m.peak[0] <= 0.0001f) ? -80.0f : 20.0f * std::log10(m.peak[0]);
            m_rms = (m.rms[0] <= 0.0001f) ? -80.0f : 20.0f * std::log10(m.rms[0]);
        }
    }
    meters["master"] = {
        { "peak_db", m_peak },
        { "rms_db", m_rms }
    };
    meters["buses"] = nlohmann::json::array();
    for (const auto& bp : ctx_.authoring_model.get_bus_paths()) {
        oae_bus b = 0;
        float b_peak = -80.0f;
        float b_rms = -80.0f;
        if (ctx_.engine.get_bus(bp, &b) == OAE_OK) {
            oae_meter m{};
            if (ctx_.engine.get_bus_meter(b, &m) == OAE_OK) {
                b_peak = (m.peak[0] <= 0.0001f) ? -80.0f : 20.0f * std::log10(m.peak[0]);
                b_rms = (m.rms[0] <= 0.0001f) ? -80.0f : 20.0f * std::log10(m.rms[0]);
            }
        }
        meters["buses"].push_back({
            { "path", bp },
            { "peak_db", b_peak },
            { "rms_db", b_rms }
        });
    }

    nlohmann::json item;
    item["uri"] = "oae://audio/meters";
    item["mimeType"] = "application/json";
    item["text"] = meters.dump(2);
    return item;
}

nlohmann::json McpServer::resource_diagnostics_log()
{
    std::string text;
    if (ctx_.diagnostics) {
        text = ctx_.diagnostics->get_log_text().toStdString();
    }
    nlohmann::json item;
    item["uri"] = "oae://diagnostics/log";
    item["mimeType"] = "text/plain";
    item["text"] = std::move(text);
    return item;
}

bool McpServer::start_stdio()
{
    stop_stdio();
    stdio_running_ = true;
    is_running_ = true;
    stdio_thread_ = std::thread([this]() {
        std::string line;
        while (stdio_running_.load() && std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::string response = handle_message(line);
            if (!response.empty()) {
                std::cout << response << "\n" << std::flush;
            }
        }
        stdio_running_ = false;
    });
    return true;
}

void McpServer::stop_stdio()
{
    stdio_running_ = false;
    if (stdio_thread_.joinable()) {
        stdio_thread_.detach();
    }
}

bool McpServer::start_tcp(int port)
{
    stop_tcp();
    tcp_listener_ = std::make_unique<juce::StreamingSocket>();
    if (!tcp_listener_->createListener(port, "127.0.0.1")) {
        tcp_listener_.reset();
        return false;
    }
    tcp_port_ = port;
    tcp_running_ = true;
    is_running_ = true;

    tcp_thread_ = std::thread([this]() {
        while (tcp_running_.load()) {
            auto* client = tcp_listener_->waitForNextConnection();
            if (!client) {
                if (!tcp_running_.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            std::thread([this, client]() {
                std::unique_ptr<juce::StreamingSocket> sock(client);
                std::string buffer;
                char temp[1024];
                while (tcp_running_.load() && sock->isConnected()) {
                    const int ready = sock->waitUntilReady(true, 100);
                    if (ready > 0) {
                        const int bytes = sock->read(temp, sizeof(temp) - 1, false);
                        if (bytes <= 0) break;
                        temp[bytes] = '\0';
                        buffer.append(temp, static_cast<size_t>(bytes));
                        size_t newline_pos = 0;
                        while ((newline_pos = buffer.find('\n')) != std::string::npos) {
                            std::string line = buffer.substr(0, newline_pos);
                            buffer.erase(0, newline_pos + 1);
                            if (!line.empty() && line.back() == '\r') line.pop_back();
                            if (line.empty()) continue;
                            std::string response = handle_message(line);
                            if (!response.empty()) {
                                response += "\n";
                                sock->write(response.data(), static_cast<int>(response.size()));
                            }
                        }
                    } else if (ready < 0) {
                        break;
                    }
                }
            }).detach();
        }
    });

    return true;
}

void McpServer::stop_tcp()
{
    tcp_running_ = false;
    if (tcp_listener_) {
        tcp_listener_->close();
    }
    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }
    tcp_listener_.reset();
    tcp_port_ = 0;
}

void McpServer::stop()
{
    is_running_ = false;
    stop_tcp();
    stop_stdio();
}

} // namespace oae::studio
