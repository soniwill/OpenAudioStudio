#include "BusCard.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

BusCard::BusCard(const std::string& path,
                 bool expanded,
                 AuthoringModel& model,
                 int& edit_depth,
                 std::function<void()> on_edit_end,
                 std::function<void(bool)> onToggle)
    : InspectorCardBase("inspector_card_bus",
                        "bus",
                        "BUS: " + juce::String(path),
                        "Bus",
                        juce::Colour(OaeTheme::Colors::accentAmber),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
{
    build_ui();
}

void BusCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void BusCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto& root = model_.json();
    const nlohmann::json* bus_ptr = nullptr;
    if (path_ == "bus:/") {
        if (root.contains("mixer") && root["mixer"].contains("master")) bus_ptr = &root["mixer"]["master"];
    } else if (root.contains("buses") && root["buses"].is_array()) {
        for (const auto& b : root["buses"]) {
            if (b.value("path", "") == path_) { bus_ptr = &b; break; }
        }
    }
    if (!bus_ptr) return;
    const auto& bus = *bus_ptr;

    const auto path = path_;
    auto* volSlider = create_slider(0.0, 2.0, bus.value("volume", 1.0), "",
        [this, path](double v) {
            model_.modify([path, v](nlohmann::json& r) {
                if (path == "bus:/") {
                    r["mixer"]["master"]["volume"] = v;
                } else {
                    for (auto& item : r["buses"]) if (item.value("path", "") == path) item["volume"] = v;
                }
            });
        });
    volSlider->setComponentID("inspector_volume");
    volSlider->setDoubleClickReturnValue(true, 1.0, {});
    add_labeled_row("Volume", volSlider);

    if (path_ != "bus:/") {
        auto* parentCombo = new juce::ComboBox();
        parentCombo->setComponentID("inspector.bus.parent");
        int idx = 1;
        int selIdx = 1;
        for (const auto& b : model_.get_bus_paths()) {
            if (b != path_) {
                parentCombo->addItem(b, idx);
                if (b == bus.value("parent", "bus:/")) selIdx = idx;
                ++idx;
            }
        }
        parentCombo->setSelectedId(selIdx, juce::dontSendNotification);
        parentCombo->onChange = [this, path, parentCombo]() {
            const auto p = parentCombo->getText().toStdString();
            model_.modify([path, p](nlohmann::json& r) {
                for (auto& item : r["buses"]) if (item.value("path", "") == path) item["parent"] = p;
            });
        };
        add_labeled_row("Parent", parentCombo);
    }

    // DSP Effects Rack
    add_section_header("DSP EFFECTS RACK");
    auto* addFxBtn = new juce::TextButton("+ Add Effect");
    addFxBtn->setComponentID("inspector.bus.add_fx");
    register_layout_item(body_, addFxBtn, y_, 160, 22);
    addFxBtn->onClick = [this, path]() {
        juce::PopupMenu m;
        m.addItem(1, "Limiter");
        m.addItem(2, "Compressor");
        m.addItem(3, "EQ3");
        m.addItem(4, "Lowpass");
        m.addItem(5, "Highpass");
        m.addItem(6, "Reverb");
        m.addItem(7, "Delay");
        const int r = UiTestDialogSeam::show_modal_menu("bus_add_fx", m);
        if (r == 1) model_.add_effect_to_bus(path, "limiter");
        else if (r == 2) model_.add_effect_to_bus(path, "compressor");
        else if (r == 3) model_.add_effect_to_bus(path, "eq3");
        else if (r == 4) model_.add_effect_to_bus(path, "lowpass");
        else if (r == 5) model_.add_effect_to_bus(path, "highpass");
        else if (r == 6) model_.add_effect_to_bus(path, "reverb");
        else if (r == 7) model_.add_effect_to_bus(path, "delay");
    };
    body_.addAndMakeVisible(addFxBtn);
    comps_.emplace_back(addFxBtn);
    y_ += 28;

    if (bus.contains("effects") && bus["effects"].is_array()) {
        const auto& fxs = bus["effects"];
        for (size_t fi = 0; fi < fxs.size(); ++fi) {
            const auto& fx = fxs[fi];
            const auto type = fx.value("type", "unknown");
            add_section_header("  Insert " + juce::String(fi) + ": " + type);

            auto* bypassToggle = new juce::ToggleButton("Bypass");
            bypassToggle->setComponentID("inspector.bus.fx." + juce::String(fi) + ".bypass");
            bypassToggle->setToggleState(fx.value("bypass", false), juce::dontSendNotification);
            bypassToggle->onClick = [this, path, fi, bypassToggle]() {
                const bool bp = bypassToggle->getToggleState();
                model_.modify([path, fi, bp](nlohmann::json& r) {
                    for (auto& item : r["buses"]) {
                        if (item.value("path", "") == path && fi < item["effects"].size()) {
                            item["effects"][fi]["bypass"] = bp;
                        }
                    }
                });
            };
            register_layout_item(body_, bypassToggle, y_, 160, 20);
            body_.addAndMakeVisible(bypassToggle);
            comps_.emplace_back(bypassToggle);
            y_ += 24;

            auto* wetSlider = create_slider(0.0, 1.0, fx.value("wet", 1.0), "",
                [this, path, fi](double v) {
                    model_.modify([path, fi, v](nlohmann::json& r) {
                        for (auto& item : r["buses"]) {
                            if (item.value("path", "") == path && fi < item["effects"].size()) {
                                item["effects"][fi]["wet"] = v;
                            }
                        }
                    });
                });
            wetSlider->setComponentID("inspector.bus.fx." + juce::String(fi) + ".wet");
            wetSlider->setDoubleClickReturnValue(true, 1.0, {});
            add_labeled_row("  Wet", wetSlider);
        }
    }

    append_bus_effect_automations(body_, comps_, y_, model_, edit_depth_, on_edit_end_, path, bus, "inspector.bus");
}

void BusCard::reset_to_default()
{
    const auto path = path_;
    model_.modify([path](nlohmann::json& r) {
        if (path == "bus:/") {
            r["mixer"]["master"]["volume"] = 1.0;
        } else if (r.contains("buses") && r["buses"].is_array()) {
            for (auto& item : r["buses"]) {
                if (item.value("path", "") == path) {
                    item["volume"] = 1.0;
                    break;
                }
            }
        }
    });
}

nlohmann::json BusCard::serialize_values() const
{
    const auto& root = model_.json();
    nlohmann::json out = nlohmann::json::object();
    if (path_ == "bus:/") {
        if (root.contains("mixer") && root["mixer"].contains("master")) {
            out["volume"] = root["mixer"]["master"].value("volume", 1.0);
        }
    } else if (root.contains("buses") && root["buses"].is_array()) {
        for (const auto& b : root["buses"]) {
            if (b.value("path", "") == path_) {
                out["volume"] = b.value("volume", 1.0);
                out["parent"] = b.value("parent", "bus:/");
                break;
            }
        }
    }
    return out;
}

bool BusCard::deserialize_values(const nlohmann::json& data)
{
    if (!data.is_object()) return false;
    const auto path = path_;
    model_.modify([path, data](nlohmann::json& r) {
        if (path == "bus:/") {
            if (data.contains("volume")) r["mixer"]["master"]["volume"] = data["volume"];
        } else if (r.contains("buses") && r["buses"].is_array()) {
            for (auto& item : r["buses"]) {
                if (item.value("path", "") == path) {
                    if (data.contains("volume")) item["volume"] = data["volume"];
                    if (data.contains("parent")) item["parent"] = data["parent"];
                    break;
                }
            }
        }
    });
    return true;
}

} // namespace oae::studio
