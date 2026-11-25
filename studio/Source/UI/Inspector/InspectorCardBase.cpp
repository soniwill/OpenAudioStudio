#include "InspectorCardBase.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

InspectorEditor::Row::Row(std::vector<LayoutItem> itemsToLayout, int verticalGap)
    : items(std::move(itemsToLayout)), gapBefore(verticalGap)
{
    for (const auto& item : items)
        addAndMakeVisible(*item.component);
}

int InspectorEditor::Row::heightForWidth(int width) const
{
    int inlineMinimumWidth = 0;
    int inlineHeight = 0;
    int stackedHeight = 0;
    for (const auto& item : items) {
        inlineMinimumWidth += item.width;
        inlineHeight = std::max(inlineHeight, item.height);
        stackedHeight += item.height;
    }
    inlineMinimumWidth += static_cast<int>(items.size() - 1) * 4;
    if (width < inlineMinimumWidth)
        stackedHeight += static_cast<int>(items.size() - 1) * 4;
    return gapBefore + (width < inlineMinimumWidth ? stackedHeight : inlineHeight);
}

void InspectorEditor::Row::resized()
{
    const int width = getWidth();
    int inlineMinimumWidth = 0;
    int rowHeight = 0;
    for (const auto& item : items) {
        inlineMinimumWidth += item.width;
        rowHeight = std::max(rowHeight, item.height);
    }
    inlineMinimumWidth += static_cast<int>(items.size() - 1) * 4;
    const bool stacked = width < inlineMinimumWidth;

    const auto contentArea = getLocalBounds().withTrimmedTop(gapBefore).toFloat();
    if (stacked) {
        juce::FlexBox column;
        column.flexDirection = juce::FlexBox::Direction::column;
        for (size_t index = 0; index < items.size(); ++index) {
            const auto& item = items[index];
            column.items.add(juce::FlexItem(*item.component)
                .withFlex(0.0f, 0.0f, static_cast<float>(item.height))
                .withMargin(juce::FlexItem::Margin(index == 0 ? 0.0f : 4.0f, 0.0f, 0.0f, 0.0f)));
        }
        column.performLayout(contentArea);
        return;
    }

    const auto rowArea = contentArea.withHeight(static_cast<float>(rowHeight));
    if (items.size() == 3) {
        juce::Grid grid;
        grid.alignItems = juce::Grid::AlignItems::center;
        grid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Px(rowHeight)));
        for (size_t index = 0; index < items.size(); ++index) {
            const auto& item = items[index];
            grid.templateColumns.add(juce::Grid::TrackInfo(
                juce::Grid::Fr(static_cast<unsigned long long>(item.width))));
            grid.items.add(juce::GridItem(*item.component).withArea(1, static_cast<int>(index + 1)));
        }
        grid.columnGap = juce::Grid::Px(4);
        grid.performLayout(rowArea.toNearestInt());
        return;
    }

    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.alignItems = juce::FlexBox::AlignItems::center;
    const float totalWidth = static_cast<float>(inlineMinimumWidth - static_cast<int>(items.size() - 1) * 4);
    for (size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        const auto grow = static_cast<float>(item.width) / totalWidth;
        row.items.add(juce::FlexItem(*item.component)
            .withFlex(grow, 1.0f, 0.0f)
            .withMargin(juce::FlexItem::Margin(0.0f,
                index + 1 < items.size() ? 4.0f : 0.0f, 0.0f, 0.0f))
            .withHeight(static_cast<float>(item.height)));
    }
    row.performLayout(rowArea);
}

void InspectorEditor::addLayoutItem(juce::Component* component, int group, int width, int height)
{
    layoutItems.push_back({ component, group, width, height });
}

void InspectorEditor::resized()
{
    createRowsIfNeeded();
    juce::FlexBox column;
    column.flexDirection = juce::FlexBox::Direction::column;
    for (const auto& row : rows) {
        column.items.add(juce::FlexItem(*row)
            .withFlex(0.0f, 0.0f, static_cast<float>(row->heightForWidth(getWidth()))));
    }
    column.performLayout(getLocalBounds().toFloat());
}

int InspectorEditor::contentHeightForWidth(int width)
{
    createRowsIfNeeded();
    int height = 8;
    for (const auto& row : rows)
        height += row->heightForWidth(width);
    return height;
}

void InspectorEditor::createRowsIfNeeded()
{
    if (!rows.empty() || layoutItems.empty())
        return;

    std::stable_sort(layoutItems.begin(), layoutItems.end(), [](const auto& left, const auto& right) {
        return left.group < right.group;
    });

    size_t first = 0;
    int previousBottom = 0;
    while (first < layoutItems.size()) {
        size_t last = first + 1;
        int rowHeight = layoutItems[first].height;
        while (last < layoutItems.size() && layoutItems[last].group == layoutItems[first].group) {
            rowHeight = std::max(rowHeight, layoutItems[last].height);
            ++last;
        }
        const int gap = std::max(0, layoutItems[first].group - previousBottom);
        std::vector<LayoutItem> rowItems(layoutItems.begin() + static_cast<std::ptrdiff_t>(first),
            layoutItems.begin() + static_cast<std::ptrdiff_t>(last));
        auto row = std::make_unique<Row>(std::move(rowItems), gap);
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
        previousBottom = layoutItems[first].group + rowHeight;
        first = last;
    }
}

void register_layout_item(juce::Component& parent, juce::Component* component,
                          int y, int width, int height)
{
    if (auto* editor = dynamic_cast<InspectorEditor*>(&parent))
        editor->addLayoutItem(component, y, width, height);
}

void resize_viewport_content(juce::Viewport& viewport, juce::Component& content)
{
    const int widthWithVerticalBar = std::max(0, viewport.getWidth() - viewport.getScrollBarThickness());
    auto contentHeight = [&content, &viewport](int width) {
        if (auto* editor = dynamic_cast<InspectorEditor*>(&content))
            return editor->contentHeightForWidth(width);
        return std::max(viewport.getHeight(), content.getHeight());
    };

    content.setSize(widthWithVerticalBar, contentHeight(widthWithVerticalBar));
    if (!viewport.getVerticalScrollBar().isVisible())
        content.setSize(viewport.getWidth(), contentHeight(viewport.getWidth()));
    content.resized();
}

std::pair<double, double> parameter_json_domain(const nlohmann::json& p)
{
    if (p.value("kind", "") == "labeled") {
        const size_t n = (p.contains("labels") && p["labels"].is_array()) ? p["labels"].size() : 0;
        return { 0.0, n > 0 ? static_cast<double>(n - 1) : 0.0 };
    }
    const double mn = p.value("min", 0.0);
    const double mx = p.value("max", 1.0);
    return { mn, std::max(mn, mx) };
}

const nlohmann::json* find_live_param(const nlohmann::json& root, bool is_local,
                                      const std::string& event_path, const std::string& name)
{
    if (is_local) {
        if (root.contains("events") && root["events"].is_array()) {
            for (const auto& ev : root["events"]) {
                if (ev.value("path", "") == event_path && ev.contains("parameters") && ev["parameters"].is_array()) {
                    for (const auto& p : ev["parameters"]) {
                        if (p.value("name", "") == name) return &p;
                    }
                    break;
                }
            }
        }
        return nullptr;
    }
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& p : root["global_parameters"]) {
            if (p.value("name", "") == name) return &p;
        }
    }
    return nullptr;
}

const nlohmann::json* resolve_event_param_any_scope(const nlohmann::json& root,
                                                    const nlohmann::json& ev,
                                                    const std::string& name)
{
    if (ev.contains("parameters") && ev["parameters"].is_array()) {
        for (const auto& p : ev["parameters"]) {
            if (p.value("name", "") == name) return &p;
        }
    }
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& p : root["global_parameters"]) {
            if (p.value("name", "") == name) return &p;
        }
    }
    return nullptr;
}

const nlohmann::json* find_live_instrument(const nlohmann::json& root,
                                           const std::string& event_path,
                                           size_t track_index,
                                           size_t inst_index)
{
    if (!root.contains("events") || !root["events"].is_array()) return nullptr;
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == event_path && ev.contains("tracks") && ev["tracks"].is_array()) {
            const auto& trks = ev["tracks"];
            if (track_index < trks.size() && trks[track_index].contains("instruments") && trks[track_index]["instruments"].is_array()) {
                const auto& insts = trks[track_index]["instruments"];
                if (inst_index < insts.size()) return &insts[inst_index];
            }
            break;
        }
    }
    return nullptr;
}

bool has_suffix(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_internal_user_prop(const std::string& key)
{
    if (key.rfind("track_", 0) != 0) return false;
    return has_suffix(key, "_mute") || has_suffix(key, "_solo");
}

std::string kind_badge_label(const std::string& kind)
{
    if (kind == "single") return "[SGL]";
    if (kind == "multi") return "[MLT]";
    if (kind == "scatterer") return "[SCT]";
    if (kind == "programmer") return "[PRG]";
    if (kind == "event") return "[EVT]";
    if (kind == "command") return "[CMD]";
    return "[SIL]";
}

std::vector<std::string> parse_comma_labels(const juce::String& text)
{
    std::vector<std::string> out;
    std::set<std::string> seen;
    juce::StringArray tokens;
    tokens.addTokens(text, ",", "");
    for (const auto& tok : tokens) {
        const auto s = tok.trim().toStdString();
        if (!s.empty() && seen.insert(s).second) {
            out.push_back(s);
        }
    }
    return out;
}

juce::Slider* create_gesture_slider(AuthoringModel& model, int& edit_depth, std::function<void()> on_edit_end,
                                    double min, double max, double val,
                                    const juce::String& suffix, std::function<void(double)> on_change)
{
    const double safeMax = (max > min) ? max : (min + 1.0);
    auto* s = new OaeValueBoxSlider();
    s->setRange(min, safeMax, (safeMax - min) > 100.0 ? 0.1 : 0.01);
    s->setValue(std::clamp(val, min, safeMax), juce::dontSendNotification);
    s->setTextValueSuffix(suffix);
    s->onDragStart = [&model, &edit_depth]() {
        ++edit_depth;
        model.begin_gesture();
    };
    s->onDragEnd = [&model, &edit_depth, on_edit_end]() {
        model.end_gesture();
        if (edit_depth > 0) --edit_depth;
        if (edit_depth == 0 && on_edit_end) on_edit_end();
    };
    s->onValueChange = [s, &edit_depth, on_edit_end, on_change]() {
        ++edit_depth;
        on_change(s->getValue());
        if (edit_depth > 0) --edit_depth;
        if (edit_depth == 0 && on_edit_end) on_edit_end();
    };
    return s;
}

void add_labeled_row(juce::Component& parent, std::vector<std::unique_ptr<juce::Component>>& owned,
                     int& y, const juce::String& text, juce::Component* comp, int compHeight)
{
    auto* label = new juce::Label("", text);
    label->setFont(juce::FontOptions(OaeTheme::Typography::compact));
    label->setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    register_layout_item(parent, label, y, 90, compHeight);
    parent.addAndMakeVisible(label);
    owned.emplace_back(label);

    register_layout_item(parent, comp, y, 160, compHeight);
    parent.addAndMakeVisible(comp);
    owned.emplace_back(comp);

    y += compHeight + 4;
}

void add_section_header(juce::Component& parent, std::vector<std::unique_ptr<juce::Component>>& owned,
                        int& y, const juce::String& title)
{
    y += 4;
    auto* label = new juce::Label("", title);
    label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label->setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    register_layout_item(parent, label, y, 260, 20);
    parent.addAndMakeVisible(label);
    owned.emplace_back(label);
    y += 24;
}

void append_parameter_editor_controls(juce::Component& view,
                                      std::vector<std::unique_ptr<juce::Component>>& comps,
                                      int& y,
                                      AuthoringModel& model,
                                      int& edit_depth,
                                      const std::function<void()>& on_edit_end,
                                      const nlohmann::json& param,
                                      bool is_local,
                                      const std::string& event_path,
                                      const std::string& id_prefix)
{
    const std::string name = param.value("name", "");
    const std::string kind = param.value("kind", "continuous");
    const juce::String indent = is_local ? "    " : "";

    auto update_live = [&model, is_local, event_path, name](const std::function<void(nlohmann::json&)>& mutate) {
        const auto* live = find_live_param(model.json(), is_local, event_path, name);
        if (!live) return;
        auto next = *live;
        mutate(next);
        if (is_local) {
            model.update_local_parameter(event_path, name, next);
        } else {
            model.update_global_parameter(name, next);
        }
    };

    if (is_local) {
        auto* nameEdit = new juce::TextEditor();
        nameEdit->setComponentID(id_prefix + ".name");
        nameEdit->setText(juce::String::fromUTF8(name.c_str()), false);
        register_layout_item(view, nameEdit, y, 222, 22);
        auto commit_rename = [&model, event_path, name, nameEdit]() {
            const auto nextName = nameEdit->getText().trim().toStdString();
            if (!nextName.empty() && nextName != name) {
                if (!model.rename_local_parameter(event_path, name, nextName)) {
                    nameEdit->setText(juce::String::fromUTF8(name.c_str()), false);
                }
            }
        };
        nameEdit->onReturnKey = commit_rename;
        nameEdit->onFocusLost = commit_rename;
        view.addAndMakeVisible(nameEdit);
        comps.emplace_back(nameEdit);

        auto* rmBtn = new juce::TextButton("X");
        rmBtn->setComponentID(id_prefix + ".remove");
        register_layout_item(view, rmBtn, y, 24, 22);
        rmBtn->onClick = [&model, event_path, name]() {
            model.remove_local_parameter(event_path, name);
        };
        view.addAndMakeVisible(rmBtn);
        comps.emplace_back(rmBtn);
        y += 26;
    }

    auto* kindCombo = new juce::ComboBox();
    kindCombo->setComponentID(id_prefix + ".kind");
    kindCombo->addItem("continuous", 1);
    kindCombo->addItem("discrete", 2);
    kindCombo->addItem("labeled", 3);
    kindCombo->setSelectedId(kind == "discrete" ? 2 : (kind == "labeled" ? 3 : 1), juce::dontSendNotification);
    kindCombo->onChange = [update_live, kindCombo]() {
        const auto chosen = kindCombo->getText().toStdString();
        update_live([chosen](nlohmann::json& next) {
            next["kind"] = chosen;
            if (chosen == "labeled" && (!next.contains("labels") || !next["labels"].is_array() || next["labels"].empty())) {
                next["labels"] = nlohmann::json::array({ "Option1", "Option2" });
            }
        });
    };
    add_labeled_row(view, comps, y, indent + "Kind", kindCombo);

    if (kind == "labeled") {
        juce::String joined;
        if (param.contains("labels") && param["labels"].is_array()) {
            for (size_t i = 0; i < param["labels"].size(); ++i) {
                if (i > 0) joined += ", ";
                joined += juce::String::fromUTF8(param["labels"][i].get<std::string>().c_str());
            }
        }
        auto* labelsEdit = new juce::TextEditor();
        labelsEdit->setComponentID(id_prefix + ".labels");
        labelsEdit->setText(joined, false);
        auto commit_labels = [update_live, labelsEdit, joined]() {
            const auto parsed = parse_comma_labels(labelsEdit->getText());
            if (parsed.empty() || parsed.size() > 64) {
                labelsEdit->setText(joined, false);
                return;
            }
            update_live([&parsed](nlohmann::json& next) {
                next["labels"] = parsed;
                const int curDef = static_cast<int>(std::round(next.value("default_value", 0.0)));
                next["default_value"] = std::clamp(curDef, 0, static_cast<int>(parsed.size() - 1));
            });
        };
        labelsEdit->onReturnKey = commit_labels;
        labelsEdit->onFocusLost = commit_labels;
        add_labeled_row(view, comps, y, indent + "Labels", labelsEdit);

        auto* defCombo = new juce::ComboBox();
        defCombo->setComponentID(id_prefix + ".default_label");
        const int labelCount = (param.contains("labels") && param["labels"].is_array())
            ? static_cast<int>(param["labels"].size()) : 0;
        for (int li = 0; li < labelCount; ++li) {
            const auto lbl = param["labels"][static_cast<size_t>(li)].get<std::string>();
            defCombo->addItem(juce::String(li) + ": " + juce::String::fromUTF8(lbl.c_str()), li + 1);
        }
        const int curDef = std::clamp(static_cast<int>(std::round(param.value("default_value", 0.0))),
                                      0, std::max(0, labelCount - 1));
        if (labelCount > 0) {
            defCombo->setSelectedId(curDef + 1, juce::dontSendNotification);
        }
        defCombo->onChange = [update_live, defCombo]() {
            if (defCombo->getSelectedId() > 0) {
                const int idx = defCombo->getSelectedId() - 1;
                update_live([idx](nlohmann::json& next) { next["default_value"] = idx; });
            }
        };
        add_labeled_row(view, comps, y, indent + "Default", defCombo);
    } else {
        const double step = (kind == "discrete") ? 1.0 : 0.01;
        const double mn = param.value("min", 0.0);
        const double mx = param.value("max", kind == "discrete" ? 10.0 : 100.0);
        const double def = param.value("default_value", kind == "discrete" ? mn : 50.0);

        auto* minSlider = create_gesture_slider(model, edit_depth, on_edit_end, -1000.0, 1000.0, mn, "",
            [update_live](double v) {
                update_live([v](nlohmann::json& next) {
                    next["min"] = v;
                    if (next.value("max", v) < v) next["max"] = v;
                });
            });
        minSlider->setComponentID(id_prefix + ".min");
        minSlider->setRange(-1000.0, 1000.0, step);
        minSlider->setValue(mn, juce::dontSendNotification);
        add_labeled_row(view, comps, y, indent + "Min", minSlider);

        auto* maxSlider = create_gesture_slider(model, edit_depth, on_edit_end, -1000.0, 1000.0, mx, "",
            [update_live](double v) {
                update_live([v](nlohmann::json& next) {
                    next["max"] = v;
                    if (next.value("min", v) > v) next["min"] = v;
                });
            });
        maxSlider->setComponentID(id_prefix + ".max");
        maxSlider->setRange(-1000.0, 1000.0, step);
        maxSlider->setValue(mx, juce::dontSendNotification);
        add_labeled_row(view, comps, y, indent + "Max", maxSlider);

        const double defMax = (mx > mn) ? mx : (mn + 1.0);
        auto* defSlider = create_gesture_slider(model, edit_depth, on_edit_end, mn, defMax, def, "",
            [update_live](double v) {
                update_live([v](nlohmann::json& next) { next["default_value"] = v; });
            });
        defSlider->setComponentID(id_prefix + ".default");
        defSlider->setRange(mn, defMax, step);
        defSlider->setValue(std::clamp(def, mn, defMax), juce::dontSendNotification);
        defSlider->setEnabled(mx > mn);
        add_labeled_row(view, comps, y, indent + "Default", defSlider);
    }

    auto* seekUpSlider = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 1000.0, param.value("seek_speed", 0.0), " /s",
        [update_live](double v) { update_live([v](nlohmann::json& next) { next["seek_speed"] = v; }); });
    seekUpSlider->setComponentID(id_prefix + ".seek_up");
    add_labeled_row(view, comps, y, indent + "Seek Up", seekUpSlider);

    auto* seekDownSlider = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 1000.0, param.value("seek_speed_down", 0.0), " /s",
        [update_live](double v) { update_live([v](nlohmann::json& next) { next["seek_speed_down"] = v; }); });
    seekDownSlider->setComponentID(id_prefix + ".seek_down");
    add_labeled_row(view, comps, y, indent + "Seek Down", seekDownSlider);

    auto* velSlider = create_gesture_slider(model, edit_depth, on_edit_end, -1000.0, 1000.0, param.value("velocity", 0.0), " /s",
        [update_live](double v) { update_live([v](nlohmann::json& next) { next["velocity"] = v; }); });
    velSlider->setComponentID(id_prefix + ".velocity");
    add_labeled_row(view, comps, y, indent + "Velocity", velSlider);

    auto* holdBtn = new juce::ToggleButton("Hold during playback");
    holdBtn->setComponentID(id_prefix + ".hold");
    holdBtn->setToggleState(param.value("hold_during_playback", false), juce::dontSendNotification);
    holdBtn->onClick = [update_live, holdBtn]() {
        const bool hold = holdBtn->getToggleState();
        update_live([hold](nlohmann::json& next) { next["hold_during_playback"] = hold; });
    };
    add_labeled_row(view, comps, y, indent + "Behavior", holdBtn);
}

void append_modulation_section(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                               int& y, AuthoringModel& model, int& edit_depth,
                               std::function<void()> on_edit_end, const ItemSelection& owner,
                               const nlohmann::json& owner_json, const juce::String& title_prefix,
                               const std::string& id_prefix)
{
    add_section_header(view, comps, y, title_prefix + "MODULATION");
    auto* addModBtn = new juce::TextButton("+ Add Mod");
    addModBtn->setComponentID(id_prefix + ".mod.add");
    register_layout_item(view, addModBtn, y, 110, 22);
    addModBtn->onClick = [&model, owner, addModBtn]() {
        juce::PopupMenu m;
        m.addItem(1, "Random (Volume)");
        m.addItem(2, "Random (Pitch)");
        m.addItem(3, "Random (Pan)");
        m.addItem(4, "AHDSR Envelope");
        juce::Component::SafePointer<juce::TextButton> btn(addModBtn);
        UiTestDialogSeam::show_popup_menu("inspector_add_mod", m,
            juce::PopupMenu::Options().withTargetComponent(addModBtn),
            [&model, owner, btn](int res) {
                if (!btn || res == 0) return;
                if (res == 1) model.add_modulator(owner, {{ "kind", "random" }, { "target", "volume" }, { "range", 6.0 }});
                else if (res == 2) model.add_modulator(owner, {{ "kind", "random" }, { "target", "pitch" }, { "range", 2.0 }});
                else if (res == 3) model.add_modulator(owner, {{ "kind", "random" }, { "target", "pan" }, { "range", 0.25 }});
                else if (res == 4) {
                    model.add_modulator(owner, {
                        { "kind", "ahdsr" }, { "attack_ms", 20.0 }, { "hold_ms", 0.0 },
                        { "decay_ms", 100.0 }, { "sustain", 1.0 }, { "release_ms", 200.0 },
                        { "release_on_key_off", true }
                    });
                }
            });
    };
    view.addAndMakeVisible(addModBtn);
    comps.emplace_back(addModBtn);
    y += 26;

    if (!owner_json.contains("modulators") || !owner_json["modulators"].is_array()) return;
    const auto& mods = owner_json["modulators"];
    for (size_t mi = 0; mi < mods.size(); ++mi) {
        const auto& mod = mods[mi];
        const auto kind = mod.value("kind", "");
        const std::string mp = id_prefix + ".mod." + std::to_string(mi);
        const int rowTop = y;
        auto* rmBtn = new juce::TextButton("X");
        rmBtn->setComponentID(mp + ".remove");
        rmBtn->onClick = [&model, owner, mi]() { model.remove_modulator(owner, mi); };
        view.addAndMakeVisible(rmBtn);
        comps.emplace_back(rmBtn);

        if (kind == "random") {
            const auto target = mod.value("target", "volume");
            const double max_r = (target == "volume") ? 80.0 : (target == "pitch" ? 48.0 : 1.0);
            const char* suffix = (target == "volume") ? " dB" : (target == "pitch" ? " st" : "");
            auto* rngS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, max_r, mod.value("range", 0.0), suffix,
                [&model, owner, mi, target](double v) {
                    model.update_modulator(owner, mi, {{ "kind", "random" }, { "target", target }, { "range", v }});
                });
            rngS->setComponentID(mp + ".range");
            auto* lbl = new juce::Label({}, "Rand " + juce::String(target));
            register_layout_item(view, lbl, y, 92, 20);
            register_layout_item(view, rngS, y, 132, 20);
            register_layout_item(view, rmBtn, rowTop, 24, 20);
            lbl->setFont(juce::FontOptions(11.0f));
            view.addAndMakeVisible(lbl);
            view.addAndMakeVisible(rngS);
            comps.emplace_back(lbl);
            comps.emplace_back(rngS);
            y += 24;
        } else if (kind == "ahdsr") {
            const double a = mod.value("attack_ms", 0.0);
            const double h = mod.value("hold_ms", 0.0);
            const double d = mod.value("decay_ms", 0.0);
            const double s = mod.value("sustain", 1.0);
            const double r = mod.value("release_ms", 0.0);
            const bool ko = mod.value("release_on_key_off", false);

            auto* lbl = new juce::Label({}, "AHDSR Envelope");
            register_layout_item(view, lbl, y, 200, 20);
            register_layout_item(view, rmBtn, rowTop, 24, 20);
            lbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
            view.addAndMakeVisible(lbl);
            comps.emplace_back(lbl);
            y += 22;

            auto* curve = new AhdsrCurveEditor(model, edit_depth, on_edit_end, a, h, d, s, r, ko,
                [&model, owner, mi, ko](double na, double nh, double nd, double ns, double nr) {
                    model.update_modulator(owner, mi, {
                        { "kind", "ahdsr" }, { "attack_ms", na }, { "hold_ms", nh },
                        { "decay_ms", nd }, { "sustain", ns }, { "release_ms", nr },
                        { "release_on_key_off", ko }
                    });
                });
            curve->setComponentID(mp + ".ahdsr");
            register_layout_item(view, curve, y, 250, 68);
            view.addAndMakeVisible(curve);
            comps.emplace_back(curve);
            y += 72;

            auto update_ahdsr = [&model, owner, mi](double na, double nh, double nd, double ns, double nr, bool nko) {
                model.update_modulator(owner, mi, {
                    { "kind", "ahdsr" }, { "attack_ms", na }, { "hold_ms", nh },
                    { "decay_ms", nd }, { "sustain", ns }, { "release_ms", nr },
                    { "release_on_key_off", nko }
                });
            };
            auto* aS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 5000.0, a, " ms",
                [=](double v) { update_ahdsr(v, h, d, s, r, ko); });
            aS->setComponentID(mp + ".attack");
            add_labeled_row(view, comps, y, "  Attack", aS);
            auto* hS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 5000.0, h, " ms",
                [=](double v) { update_ahdsr(a, v, d, s, r, ko); });
            hS->setComponentID(mp + ".hold");
            add_labeled_row(view, comps, y, "  Hold", hS);
            auto* dS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 5000.0, d, " ms",
                [=](double v) { update_ahdsr(a, h, v, s, r, ko); });
            dS->setComponentID(mp + ".decay");
            add_labeled_row(view, comps, y, "  Decay", dS);
            auto* sS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 1.0, s, "",
                [=](double v) { update_ahdsr(a, h, d, v, r, ko); });
            sS->setComponentID(mp + ".sustain");
            add_labeled_row(view, comps, y, "  Sustain", sS);
            auto* rS = create_gesture_slider(model, edit_depth, on_edit_end, 0.0, 5000.0, r, " ms",
                [=](double v) { update_ahdsr(a, h, d, s, v, ko); });
            rS->setComponentID(mp + ".release");
            add_labeled_row(view, comps, y, "  Release", rS);

            auto* koBtn = new juce::ToggleButton("Release on Key Off");
            koBtn->setComponentID(mp + ".key_off");
            koBtn->setToggleState(ko, juce::dontSendNotification);
            register_layout_item(view, koBtn, y, 200, 20);
            koBtn->onClick = [=]() { update_ahdsr(a, h, d, s, r, koBtn->getToggleState()); };
            view.addAndMakeVisible(koBtn);
            comps.emplace_back(koBtn);
            y += 24;
        }
    }
}

void append_param_automations_section(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                                      int& y, AuthoringModel& model, int& edit_depth,
                                      std::function<void()> on_edit_end, const std::string& event_path,
                                      int track_index, const nlohmann::json& owner_json,
                                      const std::string& id_prefix)
{
    add_section_header(view, comps, y, track_index < 0 ? "EVENT PARAM CURVES" : "TRACK PARAM CURVES");
    auto* addAutoBtn = new juce::TextButton("+ Add Curve");
    addAutoBtn->setComponentID(id_prefix + ".auto.add");
    register_layout_item(view, addAutoBtn, y, 120, 22);
    addAutoBtn->onClick = [&model, event_path, track_index, addAutoBtn]() {
        const auto locals = model.get_local_parameter_names(event_path);
        const auto globals = model.get_global_parameter_names();
        juce::PopupMenu m;
        struct Option { std::string scope, name, target; };
        std::vector<Option> opts;
        const char* targets[3] = { "volume", "pitch", "pan" };
        auto add_param_sub = [&](const std::string& scope, const std::string& pname) {
            juce::PopupMenu sub;
            for (const auto* t : targets) {
                opts.push_back({ scope, pname, t });
                sub.addItem(static_cast<int>(opts.size()), t);
            }
            m.addSubMenu(juce::String(scope == "global" ? "[G] " : "[L] ") + pname, sub);
        };
        for (const auto& lp : locals) add_param_sub("local", lp);
        for (const auto& gp : globals) add_param_sub("global", gp);
        juce::Component::SafePointer<juce::TextButton> btn(addAutoBtn);
        UiTestDialogSeam::show_popup_menu("inspector_add_auto", m,
            juce::PopupMenu::Options().withTargetComponent(addAutoBtn),
            [&model, event_path, track_index, opts, btn](int res) {
                if (!btn || res <= 0 || static_cast<size_t>(res) > opts.size()) return;
                const auto& o = opts[static_cast<size_t>(res - 1)];
                const auto dom = model.get_parameter_domain(event_path, o.scope, o.name);
                const nlohmann::json pts = nlohmann::json::array({
                    {{ "x", dom.first }, { "y", 0.0 }, { "shape", "linear" }},
                    {{ "x", dom.second }, { "y", 0.0 }, { "shape", "linear" }}
                });
                model.add_parameter_automation(event_path, track_index, o.target, o.scope, o.name, pts);
            });
    };
    view.addAndMakeVisible(addAutoBtn);
    comps.emplace_back(addAutoBtn);
    y += 26;

    if (!owner_json.contains("automations") || !owner_json["automations"].is_array()) return;
    const auto& autos = owner_json["automations"];
    for (size_t ai = 0; ai < autos.size(); ++ai) {
        const auto& au = autos[ai];
        if (au.value("is_timeline", false)) continue;
        const auto target = au.value("target", "volume");
        const auto p_obj = au.value("param", nlohmann::json::object());
        const auto scope = p_obj.value("scope", "local");
        const auto pname = p_obj.value("name", "");
        if (pname.empty()) continue;

        auto* headLbl = new juce::Label({}, "[" + juce::String(scope[0]) + "] " + juce::String(pname) + " -> " + juce::String(target));
        headLbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        register_layout_item(view, headLbl, y, 220, 20);
        view.addAndMakeVisible(headLbl);
        comps.emplace_back(headLbl);

        const std::string ap = id_prefix + ".auto." + std::to_string(ai);
        auto* rmBtn = new juce::TextButton("X");
        rmBtn->setComponentID(ap + ".remove");
        register_layout_item(view, rmBtn, y, 24, 20);
        rmBtn->onClick = [&model, event_path, track_index, ai]() {
            model.remove_automation(event_path, track_index, ai);
        };
        view.addAndMakeVisible(rmBtn);
        comps.emplace_back(rmBtn);
        y += 22;

        const auto dom = model.get_parameter_domain(event_path, scope, pname);
        const auto pts = (au.contains("curve") && au["curve"].contains("points"))
                             ? au["curve"]["points"]
                             : au.value("points", nlohmann::json::array());
        const double v_min = (target == "volume") ? -80.0 : (target == "pitch" ? -24.0 : -1.0);
        const double v_max = (target == "volume") ? 10.0 : (target == "pitch" ? 24.0 : 1.0);
        auto* curveEd = new AutomationCurveEditor(model, edit_depth, on_edit_end,
            dom.first, dom.second, v_min, v_max, pts, 0,
            [&model, event_path, track_index, ai](const nlohmann::json& new_pts) {
                model.update_automation_curve(event_path, track_index, ai, new_pts);
            },
            [](int) {});
        curveEd->setComponentID(ap + ".curve");
        register_layout_item(view, curveEd, y, 250, 74);
        view.addAndMakeVisible(curveEd);
        comps.emplace_back(curveEd);
        y += 78;
    }
}

void append_bus_effect_automations(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                                   int& y, AuthoringModel& model, int& edit_depth,
                                   std::function<void()> on_edit_end, const std::string& bus_path,
                                   const nlohmann::json& bus_json,
                                   const std::string& id_prefix)
{
    add_section_header(view, comps, y, "EFFECT PARAM CURVES");
    auto* addEaBtn = new juce::TextButton("+ Add Effect Curve");
    addEaBtn->setComponentID(id_prefix + ".fx_auto.add");
    register_layout_item(view, addEaBtn, y, 150, 22);
    addEaBtn->onClick = [&model, bus_path, bus_json, addEaBtn]() {
        const auto globals = model.get_global_parameter_names();
        if (globals.empty() || !bus_json.contains("effects") || !bus_json["effects"].is_array() || bus_json["effects"].empty()) return;
        juce::PopupMenu m;
        struct Opt { size_t fx_idx; std::string fx_type; std::string gparam; };
        std::vector<Opt> opts;
        const auto& fxs = bus_json["effects"];
        for (size_t fi = 0; fi < fxs.size(); ++fi) {
            const auto ftype = fxs[fi].value("type", "unknown");
            juce::PopupMenu sub;
            for (const auto& gp : globals) {
                opts.push_back({ fi, ftype, gp });
                sub.addItem(static_cast<int>(opts.size()), gp);
            }
            m.addSubMenu("Insert " + juce::String(fi) + ": " + ftype, sub);
        }
        juce::Component::SafePointer<juce::TextButton> btn(addEaBtn);
        UiTestDialogSeam::show_popup_menu("inspector_add_fx_auto", m,
            juce::PopupMenu::Options().withTargetComponent(addEaBtn),
            [&model, bus_path, opts, btn](int res) {
                if (!btn || res <= 0 || static_cast<size_t>(res) > opts.size()) return;
                const auto& o = opts[static_cast<size_t>(res - 1)];
                const auto dom = model.get_parameter_domain("", "global", o.gparam);
                const nlohmann::json pts = nlohmann::json::array({
                    {{ "x", dom.first }, { "y", 0.0 }, { "shape", "linear" }},
                    {{ "x", dom.second }, { "y", 1.0 }, { "shape", "linear" }}
                });
                model.add_effect_automation(bus_path, 0, o.fx_idx, o.fx_type, 0, o.gparam, pts);
            });
    };
    view.addAndMakeVisible(addEaBtn);
    comps.emplace_back(addEaBtn);
    y += 26;

    const auto& root = model.json();
    if (!root.contains("effect_automations") || !root["effect_automations"].is_array()) return;
    const auto& eas = root["effect_automations"];
    for (size_t i = 0; i < eas.size(); ++i) {
        const auto& ea = eas[i];
        if (ea.value("bus_path", "") != bus_path) continue;
        const auto gname = ea.value("param", nlohmann::json::object()).value("name", "");
        const auto dom = model.get_parameter_domain("", "global", gname);
        const auto pts = ea.contains("curve") && ea["curve"].contains("points") ? ea["curve"]["points"] : nlohmann::json::array();

        auto* lbl = new juce::Label({}, juce::String(gname) + " -> " + ea.value("effect_type", ""));
        register_layout_item(view, lbl, y, 224, 20);
        lbl->setFont(juce::FontOptions(11.0f));
        const std::string ep = id_prefix + ".fx_auto." + std::to_string(i);
        auto* rmBtn = new juce::TextButton("X");
        rmBtn->setComponentID(ep + ".remove");
        register_layout_item(view, rmBtn, y, 24, 20);
        rmBtn->onClick = [&model, i]() { model.remove_effect_automation(i); };
        view.addAndMakeVisible(lbl);
        view.addAndMakeVisible(rmBtn);
        comps.emplace_back(lbl);
        comps.emplace_back(rmBtn);
        y += 22;

        auto* curveEd = new AutomationCurveEditor(model, edit_depth, on_edit_end,
            dom.first, dom.second, -60.0, 20000.0, pts, 0,
            [&model, i](const nlohmann::json& new_pts) { model.update_effect_automation_curve(i, new_pts); },
            [](int) {});
        curveEd->setComponentID(ep + ".curve");
        register_layout_item(view, curveEd, y, 250, 74);
        view.addAndMakeVisible(curveEd);
        comps.emplace_back(curveEd);
        y += 78;
    }
}

TrackColorSwatchButton::TrackColorSwatchButton(size_t index, juce::Colour col, bool active)
    : juce::Button("track_color_" + juce::String(index)),
      colour_(col),
      active_(active)
{
    setComponentID("inspector_track_color_" + std::to_string(index));
    setTooltip(OaeTheme::TrackColors::labelForIndex(index));
}

void TrackColorSwatchButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    auto fill = colour_;
    if (shouldDrawButtonAsDown) fill = fill.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted) fill = fill.brighter(0.2f);
    g.setColour(fill);
    g.fillRoundedRectangle(b, 3.0f);
    if (active_) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
        g.drawRoundedRectangle(b.reduced(1.5f), 2.5f, 1.5f);
        g.setColour(juce::Colour(OaeTheme::Colors::indicator));
        g.drawRoundedRectangle(b, 3.0f, 1.5f);
    } else {
        g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
    }
}

TrackColorSwatchStrip::TrackColorSwatchStrip(AuthoringModel& model,
                                             const std::string& event_path,
                                             size_t track_index,
                                             const std::string& active_hex)
{
    const juce::Colour activeCol = OaeTheme::TrackColors::parseHexColor(active_hex, track_index);
    for (size_t ci = 0; ci < OaeTheme::TrackColors::paletteSize; ++ci) {
        const auto col = OaeTheme::TrackColors::colorForIndex(ci);
        const bool isActive = (col == activeCol);
        auto btn = std::make_unique<TrackColorSwatchButton>(ci, col, isActive);
        btn->onClick = [&model, event_path, track_index, ci]() {
            model.set_track_color(event_path, track_index, OaeTheme::TrackColors::hexForIndex(ci));
        };
        addAndMakeVisible(*btn);
        buttons_.push_back(std::move(btn));
    }
}

void TrackColorSwatchStrip::resized()
{
    const int count = static_cast<int>(buttons_.size());
    if (count == 0) return;
    const int gap = 2;
    const int w = getWidth();
    const int h = getHeight();
    const int cellW = std::max(8, (w - gap * (count - 1)) / count);
    for (int i = 0; i < count; ++i) {
        buttons_[static_cast<size_t>(i)]->setBounds(i * (cellW + gap), 0, cellW, h);
    }
}

InspectorCardBase::InspectorCardBase(const juce::String& cardId,
                                     std::string_view cardTypeId,
                                     const juce::String& title,
                                     const juce::String& badge,
                                     juce::Colour accentColour,
                                     bool expanded,
                                     AuthoringModel& model,
                                     int& edit_depth,
                                     std::function<void()> on_edit_end,
                                     std::function<void(bool)> onToggle)
    : cardTypeId_(cardTypeId)
    , accentColour_(accentColour)
    , expanded_(expanded)
    , model_(model)
    , edit_depth_(edit_depth)
    , on_edit_end_(std::move(on_edit_end))
    , onToggle_(std::move(onToggle))
    , header_(title, badge, accentColour, expanded, this, [this](bool nextExpanded) {
        set_card_expanded(nextExpanded);
        if (onToggle_) onToggle_(nextExpanded);
    })
{
    setComponentID(cardId);
    header_.set_card_id(cardId);
    addAndMakeVisible(header_);
    addChildComponent(body_);
    body_.setVisible(expanded_);
}

void InspectorCardBase::set_card_expanded(bool expanded)
{
    if (expanded_ != expanded) {
        expanded_ = expanded;
        header_.set_expanded(expanded);
        body_.setVisible(expanded);
        resized();
        repaint();
    }
}

int InspectorCardBase::height_for_width(int width)
{
    if (!expanded_) {
        return InspectorCardHeader::kHeaderHeight;
    }
    const int innerW = std::max(40, width - (kBodyPadX * 2));
    return InspectorCardHeader::kHeaderHeight + kBodyPadTop + body_.contentHeightForWidth(innerW) + kBodyPadBottom;
}

void InspectorCardBase::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 4.0f || bounds.getHeight() < 4.0f) return;

    g.setColour(juce::Colour(expanded_ ? OaeTheme::Colors::cardBodyBg : OaeTheme::Colors::cardHeaderBg));
    g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);

    g.setColour(juce::Colour(OaeTheme::Colors::cardBorder));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

void InspectorCardBase::resized()
{
    header_.setBounds(0, 0, getWidth(), InspectorCardHeader::kHeaderHeight);
    body_.setVisible(expanded_);
    if (expanded_) {
        auto area = getLocalBounds()
            .withTrimmedTop(InspectorCardHeader::kHeaderHeight + kBodyPadTop)
            .withTrimmedBottom(kBodyPadBottom)
            .reduced(kBodyPadX, 0);
        body_.setBounds(area);
    }
}

void CardStackEditor::addCard(std::unique_ptr<IInspectorCard> card)
{
    addAndMakeVisible(*card);
    cards_.push_back(std::move(card));
}

int CardStackEditor::contentHeightForWidth(int width)
{
    const int innerW = std::max(40, width - 12);
    int h = 10;
    for (size_t i = 0; i < cards_.size(); ++i) {
        if (i > 0) h += 10;
        h += cards_[i]->height_for_width(innerW);
    }
    return std::max(h + 10, 120);
}

void CardStackEditor::resized()
{
    const int innerW = std::max(40, getWidth() - 12);
    int curY = 10;
    for (size_t i = 0; i < cards_.size(); ++i) {
        if (i > 0) curY += 10;
        const int cardH = cards_[i]->height_for_width(innerW);
        cards_[i]->setBounds(6, curY, innerW, cardH);
        curY += cardH;
    }
}

} // namespace oae::studio
