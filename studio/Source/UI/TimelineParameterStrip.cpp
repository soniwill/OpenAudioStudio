#include "TimelineParameterStrip.h"
#include <cctype>
#include <cmath>
#include <set>
#include <unordered_set>

namespace oae::studio {

namespace {

std::string sanitize_strip_id(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isalnum(ch) || ch == '_') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "unnamed" : out;
}

} // namespace

TimelineParameterStrip::TimelineParameterStrip(AuthoringModel& model, AuditionTransport* transport)
    : model_(model), transport_(transport)
{
    setComponentID("timeline_params_strip");
    model_.add_listener(this);
    if (transport_) {
        transport_->add_listener(this);
    }

    toggleButton.setComponentID("btn_timeline_params_toggle");
    toggleButton.setTooltip("Toggle Contextual Parameters Strip");
    toggleButton.onClick = [this] {
        set_expanded(!expanded_);
    };
    addAndMakeVisible(toggleButton);

    viewport_.setViewedComponent(&contentContainer_, false);
    viewport_.setScrollBarsShown(false, true);
    viewport_.setScrollBarThickness(OaeTheme::Spacing::unit);
    addAndMakeVisible(viewport_);
}

TimelineParameterStrip::~TimelineParameterStrip()
{
    if (transport_) {
        transport_->remove_listener(this);
    }
    model_.remove_listener(this);
}

void TimelineParameterStrip::set_transport(AuditionTransport* transport)
{
    if (transport_ == transport) return;
    if (transport_) {
        transport_->remove_listener(this);
    }
    transport_ = transport;
    if (transport_) {
        transport_->add_listener(this);
    }
    refresh();
}

void TimelineParameterStrip::set_event_path(const std::string& path)
{
    if (eventPath_ == path) return;
    eventPath_ = path;
    refresh();
}

int TimelineParameterStrip::preferred_height() const noexcept
{
    if (items_.empty()) return 0;
    return expanded_ ? 42 : 24;
}

void TimelineParameterStrip::set_expanded(bool expanded)
{
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    viewport_.setVisible(expanded_);
    toggleButton.setButtonText(juce::String(expanded_ ? "[v] LIVE PARAMS (" : "[>] LIVE PARAMS (") + juce::String(items_.size()) + ")");
    resized();
    if (onHeightChanged) {
        onHeightChanged();
    }
}

void TimelineParameterStrip::PillContainer::paint(juce::Graphics& g)
{
    for (const auto& item : items_) {
        if (item.pillBounds.isEmpty()) continue;
        const auto box = item.pillBounds.toFloat();
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
        g.fillRoundedRectangle(box, OaeTheme::Edges::cardCornerRadius);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawRoundedRectangle(box.reduced(0.5f), OaeTheme::Edges::cardCornerRadius, OaeTheme::Edges::borderStroke);
    }
}

OaeSlider* TimelineParameterStrip::find_slider_by_name(const std::string& name, bool is_global) const
{
    for (const auto& item : items_) {
        if (item.name == name && item.is_global == is_global) {
            return item.slider.get();
        }
    }
    return nullptr;
}

void TimelineParameterStrip::on_parameter_changed(const std::string& name, float val, bool is_global)
{
    for (auto& item : items_) {
        if (item.name == name && item.is_global == is_global) {
            if (item.slider) {
                item.slider->setValue(val, juce::dontSendNotification);
            }
            if (item.boolBadge && item.labels.size() == 2) {
                const int idx = std::clamp(static_cast<int>(std::round(val)), 0, 1);
                item.boolBadge->setButtonText(juce::String::fromUTF8(item.labels[static_cast<size_t>(idx)].c_str()));
                item.boolBadge->setBadgeStyle(idx == 1 ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral, true);
            }
        }
    }
}

void TimelineParameterStrip::on_project_changed()
{
    refresh();
}

void TimelineParameterStrip::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && !selection.id_or_path.empty()) {
        set_event_path(selection.id_or_path);
    }
}

void TimelineParameterStrip::refresh()
{
    items_.clear();
    contentContainer_.removeAllChildren();

    if (eventPath_.empty()) {
        toggleButton.setButtonText("[v] LIVE PARAMS (0)");
        viewport_.setVisible(false);
        if (onHeightChanged) onHeightChanged();
        return;
    }

    const auto& root = model_.document().get_root_json();
    if (!root.contains("events") || !root["events"].is_array()) {
        toggleButton.setButtonText("[v] LIVE PARAMS (0)");
        viewport_.setVisible(false);
        if (onHeightChanged) onHeightChanged();
        return;
    }

    const nlohmann::json* eventJson = nullptr;
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == eventPath_) {
            eventJson = &ev;
            break;
        }
    }

    if (!eventJson) {
        toggleButton.setButtonText("[v] LIVE PARAMS (0)");
        viewport_.setVisible(false);
        if (onHeightChanged) onHeightChanged();
        return;
    }

    // 1. Local parameters
    std::unordered_set<std::string> localParamNames;
    if (eventJson->contains("parameters") && (*eventJson)["parameters"].is_array()) {
        for (const auto& p : (*eventJson)["parameters"]) {
            const std::string name = p.value("name", "");
            if (name.empty()) continue;
            localParamNames.insert(name);

            Item item;
            item.name = name;
            item.is_global = false;
            item.kind = p.value("kind", "continuous");
            if (p.contains("labels") && p["labels"].is_array()) {
                for (const auto& l : p["labels"]) {
                    if (l.is_string()) item.labels.push_back(l.get<std::string>());
                }
            }

            const bool labeled = item.kind == "labeled";
            const double minVal = labeled ? 0.0 : p.value("min", 0.0);
            const double maxVal = labeled && !item.labels.empty() ? static_cast<double>(item.labels.size() - 1) : p.value("max", 1.0);
            const double defaultVal = p.value("default_value", minVal);
            const double step = item.kind == "continuous" ? 0.0 : 1.0;

            item.label = std::make_unique<juce::Label>(name, juce::String::fromUTF8(name.c_str()));
            item.label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
            item.label->setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));

            item.slider = std::make_unique<OaeSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            const std::string safeParam = sanitize_strip_id(name);
            item.slider->setComponentID(juce::String("timeline_param.local.") + safeParam + ".slider");
            item.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, labeled ? 65 : 45, 18);
            item.slider->setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::accentCyan));
            item.slider->setRange(minVal, maxVal, step);
            item.slider->setDoubleClickReturnValue(true, defaultVal, {});

            if (labeled && !item.labels.empty()) {
                const auto labelsCopy = item.labels;
                item.slider->textFromValueFunction = [labelsCopy](double val) -> juce::String {
                    const int idx = static_cast<int>(std::round(val));
                    if (idx >= 0 && idx < static_cast<int>(labelsCopy.size())) {
                        return juce::String::fromUTF8(labelsCopy[static_cast<size_t>(idx)].c_str());
                    }
                    return juce::String(idx);
                };
                item.slider->valueFromTextFunction = [labelsCopy](const juce::String& text) -> double {
                    for (size_t i = 0; i < labelsCopy.size(); ++i) {
                        if (text.equalsIgnoreCase(juce::String::fromUTF8(labelsCopy[i].c_str()))) {
                            return static_cast<double>(i);
                        }
                    }
                    return text.getDoubleValue();
                };
            }

            float currentVal = static_cast<float>(defaultVal);
            if (transport_) {
                currentVal = transport_->get_local_parameter(name);
            }
            item.slider->setValue(currentVal, juce::dontSendNotification);
            item.slider->updateText();

            item.slider->onValueChange = [this, name, s = item.slider.get()] {
                if (transport_) {
                    transport_->set_local_parameter(name, static_cast<float>(s->getValue()));
                }
            };

            contentContainer_.addAndMakeVisible(*item.label);
            if (labeled && item.labels.size() == 2) {
                const int idx = std::clamp(static_cast<int>(std::round(currentVal)), 0, 1);
                item.boolBadge = std::make_unique<OaeStatusBadge>(
                    juce::String::fromUTF8(item.labels[static_cast<size_t>(idx)].c_str()),
                    idx == 1 ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral,
                    true);
                item.boolBadge->setComponentID(juce::String("timeline_param.local.") + safeParam + ".badge");
                item.boolBadge->setInteractive(true);
                item.boolBadge->onClick = [this, name, s = item.slider.get(), b = item.boolBadge.get(), labels = item.labels]() {
                    const int nextIdx = (static_cast<int>(std::round(s->getValue())) == 0) ? 1 : 0;
                    s->setValue(static_cast<double>(nextIdx), juce::sendNotificationSync);
                    b->setButtonText(juce::String::fromUTF8(labels[static_cast<size_t>(nextIdx)].c_str()));
                    b->setBadgeStyle(nextIdx == 1 ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral, true);
                };
                contentContainer_.addChildComponent(*item.slider);
                contentContainer_.addAndMakeVisible(*item.boolBadge);
            } else {
                contentContainer_.addAndMakeVisible(*item.slider);
            }
            items_.push_back(std::move(item));
        }
    }

    // 2. Referenced global parameters
    std::set<std::string> referencedGlobals;
    const auto scan_condition = [&](const nlohmann::json& cond) {
        if (cond.contains("terms") && cond["terms"].is_array()) {
            for (const auto& term : cond["terms"]) {
                if (term.contains("param") && term["param"].value("scope", "") == "global") {
                    const std::string gpName = term["param"].value("name", "");
                    if (!gpName.empty()) referencedGlobals.insert(gpName);
                }
            }
        }
    };
    const auto scan_automations = [&](const nlohmann::json& autos) {
        if (autos.is_array()) {
            for (const auto& a : autos) {
                if (a.contains("param") && a["param"].value("scope", "") == "global") {
                    const std::string gpName = a["param"].value("name", "");
                    if (!gpName.empty()) referencedGlobals.insert(gpName);
                }
            }
        }
    };

    if (eventJson->contains("automations")) scan_automations((*eventJson)["automations"]);
    if (eventJson->contains("tracks") && (*eventJson)["tracks"].is_array()) {
        for (const auto& trk : (*eventJson)["tracks"]) {
            if (trk.contains("automations")) scan_automations(trk["automations"]);
            if (trk.contains("instruments") && trk["instruments"].is_array()) {
                for (const auto& inst : trk["instruments"]) {
                    if (inst.contains("condition")) scan_condition(inst["condition"]);
                    const std::string sheetParam = inst.value("sheet_parameter", "");
                    if (!sheetParam.empty()) referencedGlobals.insert(sheetParam);
                }
            }
        }
    }
    if (eventJson->contains("logic_items") && (*eventJson)["logic_items"].is_array()) {
        for (const auto& li : (*eventJson)["logic_items"]) {
            if (li.contains("condition")) scan_condition(li["condition"]);
        }
    }

    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& gp : root["global_parameters"]) {
            const std::string name = gp.value("name", "");
            if (name.empty() || localParamNames.count(name) > 0) continue;
            if (referencedGlobals.count(name) == 0) continue;

            Item item;
            item.name = name;
            item.is_global = true;
            item.kind = gp.value("kind", "continuous");
            if (gp.contains("labels") && gp["labels"].is_array()) {
                for (const auto& l : gp["labels"]) {
                    if (l.is_string()) item.labels.push_back(l.get<std::string>());
                }
            }

            const bool labeled = item.kind == "labeled";
            const double minVal = labeled ? 0.0 : gp.value("min", 0.0);
            const double maxVal = labeled && !item.labels.empty() ? static_cast<double>(item.labels.size() - 1) : gp.value("max", 1.0);
            const double defaultVal = gp.value("default_value", minVal);
            const double step = item.kind == "continuous" ? 0.0 : 1.0;

            item.label = std::make_unique<juce::Label>(name, juce::String::fromUTF8(name.c_str()) + " (G)");
            item.label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
            item.label->setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentAmber));

            item.slider = std::make_unique<OaeSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            const std::string safeParam = sanitize_strip_id(name);
            item.slider->setComponentID(juce::String("timeline_param.global.") + safeParam + ".slider");
            item.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, labeled ? 65 : 45, 18);
            item.slider->setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::accentAmber));
            item.slider->setRange(minVal, maxVal, step);
            item.slider->setDoubleClickReturnValue(true, defaultVal, {});

            if (labeled && !item.labels.empty()) {
                const auto labelsCopy = item.labels;
                item.slider->textFromValueFunction = [labelsCopy](double val) -> juce::String {
                    const int idx = static_cast<int>(std::round(val));
                    if (idx >= 0 && idx < static_cast<int>(labelsCopy.size())) {
                        return juce::String::fromUTF8(labelsCopy[static_cast<size_t>(idx)].c_str());
                    }
                    return juce::String(idx);
                };
                item.slider->valueFromTextFunction = [labelsCopy](const juce::String& text) -> double {
                    for (size_t i = 0; i < labelsCopy.size(); ++i) {
                        if (text.equalsIgnoreCase(juce::String::fromUTF8(labelsCopy[i].c_str()))) {
                            return static_cast<double>(i);
                        }
                    }
                    return text.getDoubleValue();
                };
            }

            float currentVal = static_cast<float>(defaultVal);
            if (transport_) {
                currentVal = transport_->get_global_parameter(name);
            }
            item.slider->setValue(currentVal, juce::dontSendNotification);
            item.slider->updateText();

            item.slider->onValueChange = [this, name, s = item.slider.get()] {
                if (transport_) {
                    transport_->set_global_parameter(name, static_cast<float>(s->getValue()));
                }
            };

            contentContainer_.addAndMakeVisible(*item.label);
            if (labeled && item.labels.size() == 2) {
                const int idx = std::clamp(static_cast<int>(std::round(currentVal)), 0, 1);
                item.boolBadge = std::make_unique<OaeStatusBadge>(
                    juce::String::fromUTF8(item.labels[static_cast<size_t>(idx)].c_str()),
                    idx == 1 ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral,
                    true);
                item.boolBadge->setComponentID(juce::String("timeline_param.global.") + safeParam + ".badge");
                item.boolBadge->setInteractive(true);
                item.boolBadge->onClick = [this, name, s = item.slider.get(), b = item.boolBadge.get(), labels = item.labels]() {
                    const int nextIdx = (static_cast<int>(std::round(s->getValue())) == 0) ? 1 : 0;
                    s->setValue(static_cast<double>(nextIdx), juce::sendNotificationSync);
                    b->setButtonText(juce::String::fromUTF8(labels[static_cast<size_t>(nextIdx)].c_str()));
                    b->setBadgeStyle(nextIdx == 1 ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral, true);
                };
                contentContainer_.addChildComponent(*item.slider);
                contentContainer_.addAndMakeVisible(*item.boolBadge);
            } else {
                contentContainer_.addAndMakeVisible(*item.slider);
            }
            items_.push_back(std::move(item));
        }
    }

    toggleButton.setButtonText(juce::String(expanded_ ? "[v] LIVE PARAMS (" : "[>] LIVE PARAMS (") + juce::String(items_.size()) + ")");
    viewport_.setVisible(expanded_ && !items_.empty());
    resized();
    if (onHeightChanged) onHeightChanged();
}

void TimelineParameterStrip::paint(juce::Graphics& g)
{
    if (items_.empty()) return;
    g.fillAll(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TimelineParameterStrip::resized()
{
    auto area = getLocalBounds();
    if (items_.empty()) return;

    toggleButton.setBounds(area.removeFromLeft(132).reduced(4, 6));

    if (!expanded_) {
        viewport_.setVisible(false);
        return;
    }
    viewport_.setVisible(true);
    viewport_.setBounds(area);

    constexpr int pillHeight = 28;
    const int pillY = std::max(0, (area.getHeight() - pillHeight) / 2);
    int curX = 4;
    for (auto& item : items_) {
        const bool isBoolPill = (item.boolBadge != nullptr);
        const int pillW = isBoolPill ? 156 : 204;
        item.pillBounds = juce::Rectangle<int>(curX, pillY, pillW, pillHeight);
        auto inner = item.pillBounds.reduced(8, 3);
        item.label->setBounds(inner.removeFromLeft(70));
        inner.removeFromLeft(4);
        if (isBoolPill) {
            item.boolBadge->setBounds(inner.reduced(0, 1));
            item.slider->setBounds({});
        } else {
            item.slider->setBounds(inner);
        }
        curX += pillW + 6;
    }
    contentContainer_.setSize(std::max(area.getWidth(), curX + 4), area.getHeight());
}

} // namespace oae::studio
