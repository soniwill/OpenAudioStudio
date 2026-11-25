#include "EffectChainDeck.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace oae::studio {
namespace {

struct ParamSpec {
    const char* name;
    double min_val;
    double max_val;
    double step;
    const char* suffix;
};

constexpr const char* kEffectTypes[] = {
    "eq3", "compressor", "limiter", "lowpass", "highpass", "reverb", "delay"
};

std::vector<ParamSpec> specs_for_effect(const std::string& type)
{
    if (type == "eq3") {
        return {
            { "Low Gain", -24.0, 12.0, 0.1, " dB" },
            { "Mid Gain", -24.0, 12.0, 0.1, " dB" },
            { "High Gain", -24.0, 12.0, 0.1, " dB" },
            { "Low Xover", 40.0, 1000.0, 1.0, " Hz" },
            { "High Xover", 800.0, 16000.0, 10.0, " Hz" }
        };
    }
    if (type == "compressor") {
        return {
            { "Threshold", -60.0, 0.0, 0.1, " dB" },
            { "Ratio", 1.0, 20.0, 0.1, ":1" },
            { "Attack", 0.1, 200.0, 0.5, " ms" },
            { "Release", 5.0, 1000.0, 1.0, " ms" },
            { "Makeup", 0.0, 24.0, 0.1, " dB" }
        };
    }
    if (type == "limiter") {
        return {
            { "Ceiling", -12.0, 0.0, 0.1, " dB" },
            { "Release", 1.0, 500.0, 1.0, " ms" },
            { "Lookahead", 0.0, 5.0, 0.1, " ms" }
        };
    }
    if (type == "lowpass" || type == "highpass") {
        return {
            { "Cutoff", 20.0, 20000.0, 10.0, " Hz" },
            { "Q", 0.1, 10.0, 0.01, "" }
        };
    }
    if (type == "reverb") {
        return {
            { "Room Size", 0.0, 1.0, 0.01, "" },
            { "Damping", 0.0, 1.0, 0.01, "" },
            { "Width", 0.0, 1.0, 0.01, "" }
        };
    }
    if (type == "delay") {
        return {
            { "Time", 1.0, 2000.0, 1.0, " ms" },
            { "Feedback", 0.0, 0.95, 0.01, "" }
        };
    }
    return {};
}

float freq_from_norm(float t)
{
    return 20.0f * std::pow(1000.0f, std::clamp(t, 0.0f, 1.0f));
}

float norm_from_freq(float hz)
{
    return std::log(std::clamp(hz, 20.0f, 20000.0f) / 20.0f) / std::log(1000.0f);
}

} // namespace

EffectChainDeck::EqPlot::EqPlot(EffectChainDeck& owner) : owner_(owner)
{
    setComponentID("effect_eq_plot");
}

void EffectChainDeck::EqPlot::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    const float centerY = area.getCentreY();
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle).withAlpha(OaeTheme::States::curveGridOpacity));
    g.drawHorizontalLine(static_cast<int>(centerY), area.getX(), area.getRight());
    for (float hz : { 100.0f, 1000.0f, 10000.0f }) {
        const float x = area.getX() + norm_from_freq(hz) * area.getWidth();
        g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
    }

    const auto* fx = owner_.selected_effect();
    if (fx == nullptr) {
        g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("No effect selected", area, juce::Justification::centred);
        return;
    }

    const std::string type = fx->value("type", "");
    const auto params = fx->value("params", nlohmann::json::array());
    const bool bypassed = fx->value("bypass", false);
    const float maxDb = 24.0f;
    const float scaleY = (area.getHeight() * 0.42f) / maxDb;

    juce::Path curve;
    constexpr int numPoints = 96;
    for (int i = 0; i <= numPoints; ++i) {
        const float t = static_cast<float>(i) / numPoints;
        const float hz = freq_from_norm(t);
        float db = 0.0f;

        if (type == "eq3" && params.size() >= 5) {
            const float lowG = params[0].get<float>();
            const float midG = params[1].get<float>();
            const float highG = params[2].get<float>();
            const float lowX = std::max(20.0f, params[3].get<float>());
            const float highX = std::max(lowX + 10.0f, params[4].get<float>());
            const float wLow = 1.0f / (1.0f + std::pow(hz / lowX, 4.0f));
            const float wHigh = 1.0f / (1.0f + std::pow(highX / hz, 4.0f));
            const float wMid = std::max(0.0f, 1.0f - wLow - wHigh);
            db = lowG * wLow + midG * wMid + highG * wHigh;
        } else if (type == "lowpass" && params.size() >= 2) {
            const float cutoff = std::max(20.0f, params[0].get<float>());
            const float q = std::max(0.1f, params[1].get<float>());
            const float r = hz / cutoff;
            const float denom = std::sqrt(std::pow(1.0f - r * r, 2.0f) + std::pow(r / q, 2.0f));
            db = 20.0f * std::log10(std::max(1e-4f, 1.0f / std::max(1e-4f, denom)));
        } else if (type == "highpass" && params.size() >= 2) {
            const float cutoff = std::max(20.0f, params[0].get<float>());
            const float q = std::max(0.1f, params[1].get<float>());
            const float r = hz / cutoff;
            const float denom = std::sqrt(std::pow(1.0f - r * r, 2.0f) + std::pow(r / q, 2.0f));
            db = 20.0f * std::log10(std::max(1e-4f, (r * r) / std::max(1e-4f, denom)));
        }

        db = std::clamp(db, -maxDb, maxDb);
        const float x = area.getX() + t * area.getWidth();
        const float y = centerY - db * scaleY;
        if (i == 0) curve.startNewSubPath(x, y);
        else curve.lineTo(x, y);
    }

    const auto accent = bypassed ? juce::Colour(OaeTheme::Colors::textSecondary) : juce::Colour(OaeTheme::Colors::accentCyan);
    juce::Path ribbon = curve;
    ribbon.lineTo(area.getRight(), area.getBottom());
    ribbon.lineTo(area.getX(), area.getBottom());
    ribbon.closeSubPath();
    juce::ColourGradient grad(accent.withAlpha(OaeTheme::States::curveRibbonTopOpacity), area.getX(), area.getY(),
                              accent.withAlpha(OaeTheme::States::curveRibbonBottomOpacity), area.getX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(ribbon);

    g.setColour(accent);
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void EffectChainDeck::EqPlot::mouseDown(const juce::MouseEvent& event)
{
    const auto* fx = owner_.selected_effect();
    if (fx == nullptr) return;
    const std::string type = fx->value("type", "");
    if (type != "eq3" && type != "lowpass" && type != "highpass") return;

    const float t = std::clamp(event.position.x / std::max(1.0f, static_cast<float>(getWidth())), 0.0f, 1.0f);
    if (type == "eq3") {
        drag_index_ = (t < 0.33f) ? 0 : (t < 0.67f ? 1 : 2);
    } else {
        drag_index_ = 0;
    }
    owns_gesture_ = true;
    owner_.model_.begin_gesture();
    mouseDrag(event);
}

void EffectChainDeck::EqPlot::mouseDrag(const juce::MouseEvent& event)
{
    if (drag_index_ < 0) return;
    const auto* fx = owner_.selected_effect();
    if (fx == nullptr) return;
    const std::string type = fx->value("type", "");

    if (type == "eq3") {
        const float centerY = static_cast<float>(getHeight()) * 0.5f;
        const float scaleY = (static_cast<float>(getHeight()) * 0.42f) / 24.0f;
        const double db = std::clamp(static_cast<double>((centerY - event.position.y) / std::max(1.0f, scaleY)), -24.0, 12.0);
        owner_.set_parameter(static_cast<size_t>(drag_index_), db);
    } else if (type == "lowpass" || type == "highpass") {
        const float t = std::clamp(event.position.x / std::max(1.0f, static_cast<float>(getWidth())), 0.0f, 1.0f);
        owner_.set_parameter(0, static_cast<double>(freq_from_norm(t)));
    }
}

void EffectChainDeck::EqPlot::mouseUp(const juce::MouseEvent&)
{
    if (owns_gesture_) {
        owns_gesture_ = false;
        owner_.model_.end_gesture();
    }
    drag_index_ = -1;
}

std::vector<TestableTarget> EffectChainDeck::EqPlot::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};
    const auto* fx = owner_.selected_effect();
    if (fx == nullptr) {
        return {{ "plot", "click", bounds, false }};
    }
    const std::string type = fx->value("type", "");
    if (type == "eq3") {
        const int w = bounds.getWidth();
        const int h = bounds.getHeight();
        const int w1 = std::max(1, w / 3);
        const int w2 = std::max(1, (2 * w) / 3 - w1);
        const int w3 = std::max(1, w - w1 - w2);
        return {
            { "band_low", "drag", { 0, 0, w1, h }, isEnabled() },
            { "band_mid", "drag", { w1, 0, w2, h }, isEnabled() },
            { "band_high", "drag", { w1 + w2, 0, w3, h }, isEnabled() }
        };
    }
    if (type == "lowpass" || type == "highpass") {
        return {{ "cutoff", "drag", bounds, isEnabled() }};
    }
    return {{ "plot", "click", bounds, false }};
}

EffectChainDeck::EffectChainDeck(StudioEngine&, AuthoringModel& model)
    : model_(model)
{
    model_.add_listener(this);

    viewport_.setViewedComponent(&controls_, false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &heading_, &status_, &busSelector_, &addType_, &addPosition_, &addButton_,
             &effectSelector_, &removeButton_, &bypassButton_, &wetLabel_, &dryLabel_,
             &wetSlider_, &drySlider_, &eqPlot_ }) {
        controls_.addAndMakeVisible(*c);
    }
    for (size_t i = 0; i < paramLabels_.size(); ++i) {
        controls_.addAndMakeVisible(paramLabels_[i]);
        controls_.addAndMakeVisible(paramSliders_[i]);
        paramLabels_[i].setFont(juce::FontOptions(10.0f));
        paramLabels_[i].setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
        bind_slider(paramSliders_[i], "effect_param_" + juce::String(static_cast<int>(i)));
        paramSliders_[i].onValueChange = [this, i] {
            if (!refreshing_) set_parameter(i, paramSliders_[i].getValue());
        };
    }

    heading_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    heading_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    status_.setFont(juce::FontOptions(11.0f));
    status_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    busSelector_.setComponentID("effect_bus_selector");
    busSelector_.onChange = [this] {
        if (refreshing_) return;
        const int idx = busSelector_.getSelectedItemIndex();
        if (idx >= 0 && static_cast<size_t>(idx) < buses_.size()) {
            set_target_bus(buses_[static_cast<size_t>(idx)]);
        }
    };

    addType_.setComponentID("effect_add_type");
    addType_.addItemList({ "EQ3", "Compressor", "Limiter", "Lowpass", "Highpass", "Reverb", "Delay" }, 1);
    addType_.setSelectedItemIndex(0, juce::dontSendNotification);

    addPosition_.setComponentID("effect_add_position");
    addPosition_.addItemList({ "Pre-fader", "Post-fader" }, 1);
    addPosition_.setSelectedItemIndex(0, juce::dontSendNotification);

    addButton_.setComponentID("effect_add");
    addButton_.onClick = [this] {
        const int typeIdx = addType_.getSelectedItemIndex();
        const int posIdx = addPosition_.getSelectedItemIndex();
        if (typeIdx < 0 || posIdx < 0) return;
        const auto* existing = model_.get_effects(target_);
        const int nextIndex = existing ? static_cast<int>(existing->size()) : 0;
        if (model_.add_effect(target_, kEffectTypes[typeIdx], posIdx)) {
            selected_index_ = nextIndex;
            refresh();
        }
    };

    effectSelector_.setComponentID("effect_selector");
    effectSelector_.onChange = [this] {
        if (refreshing_) return;
        selected_index_ = std::max(0, effectSelector_.getSelectedItemIndex());
        refresh_parameters();
        eqPlot_.repaint();
    };

    removeButton_.setComponentID("effect_remove");
    removeButton_.onClick = [this] {
        if (model_.remove_effect(target_, static_cast<size_t>(selected_index_))) {
            if (selected_index_ > 0) --selected_index_;
            refresh();
        }
    };

    bypassButton_.setComponentID("effect_bypass");
    bypassButton_.onClick = [this] {
        if (refreshing_) return;
        const bool bypass = bypassButton_.getToggleState();
        model_.modify_effect(target_, static_cast<size_t>(selected_index_), [bypass](auto& fx) {
            fx["bypass"] = bypass;
        });
    };

    wetLabel_.setFont(juce::FontOptions(10.0f));
    dryLabel_.setFont(juce::FontOptions(10.0f));
    wetLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    dryLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    bind_slider(wetSlider_, "effect_wet");
    wetSlider_.setRange(0.0, 1.0, 0.01);
    wetSlider_.setDoubleClickReturnValue(true, 1.0, {});
    wetSlider_.onValueChange = [this] {
        if (refreshing_) return;
        const double val = wetSlider_.getValue();
        model_.modify_effect(target_, static_cast<size_t>(selected_index_), [val](auto& fx) {
            fx["wet"] = val;
        });
    };

    bind_slider(drySlider_, "effect_dry");
    drySlider_.setRange(0.0, 1.0, 0.01);
    drySlider_.setDoubleClickReturnValue(true, 0.0, {});
    drySlider_.onValueChange = [this] {
        if (refreshing_) return;
        const double val = drySlider_.getValue();
        model_.modify_effect(target_, static_cast<size_t>(selected_index_), [val](auto& fx) {
            fx["dry"] = val;
        });
    };

    refresh();
}

EffectChainDeck::~EffectChainDeck()
{
    if (owns_slider_gesture_) {
        model_.end_gesture();
        owns_slider_gesture_ = false;
    }
    model_.remove_listener(this);
}

void EffectChainDeck::bind_slider(juce::Slider& slider, const juce::String& id)
{
    slider.setComponentID(id);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 20);
    slider.onDragStart = [this] {
        if (!owns_slider_gesture_) {
            owns_slider_gesture_ = true;
            model_.begin_gesture();
        }
    };
    slider.onDragEnd = [this] {
        if (owns_slider_gesture_) {
            owns_slider_gesture_ = false;
            model_.end_gesture();
        }
    };
}

void EffectChainDeck::set_target_bus(const std::string& bus_path)
{
    target_bus_ = bus_path.empty() ? "bus:/" : bus_path;
    target_ = { ItemType::Bus, target_bus_ };
    track_selected_ = false;
    selected_index_ = 0;
    refresh();
}

void EffectChainDeck::on_project_changed()
{
    refresh();
}

void EffectChainDeck::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Bus && !selection.id_or_path.empty()) {
        set_target_bus(selection.id_or_path);
    } else if (selection.type == ItemType::Event && selection.track_index >= 0 && !selection.id_or_path.empty()) {
        target_ = { ItemType::Event, selection.id_or_path, selection.track_index };
        track_selected_ = true;
        selected_index_ = 0;
        refresh();
    }
}

const nlohmann::json* EffectChainDeck::selected_effect() const
{
    const auto* effects = model_.get_effects(target_);
    if (effects == nullptr || selected_index_ < 0 || static_cast<size_t>(selected_index_) >= effects->size()) {
        return nullptr;
    }
    return &(*effects)[static_cast<size_t>(selected_index_)];
}

void EffectChainDeck::set_parameter(size_t index, double value)
{
    model_.modify_effect(target_, static_cast<size_t>(selected_index_), [index, value](auto& fx) {
        const auto specs = specs_for_effect(fx.value("type", ""));
        if (index >= specs.size()) return;
        double clamped = std::clamp(value, specs[index].min_val, specs[index].max_val);
        if (!fx.contains("params") || !fx["params"].is_array()) {
            fx["params"] = nlohmann::json::array();
        }
        while (fx["params"].size() <= index) fx["params"].push_back(0.0);
        if (fx.value("type", "") == "eq3" && fx["params"].size() >= 5) {
            if (index == 3) {
                const double highXover = fx["params"][4].get<double>();
                clamped = std::min(clamped, std::max(specs[3].min_val, highXover - 1.0));
            } else if (index == 4) {
                const double lowXover = fx["params"][3].get<double>();
                clamped = std::max(clamped, std::min(specs[4].max_val, lowXover + 1.0));
            }
        }
        fx["params"][index] = clamped;
    });
}

void EffectChainDeck::refresh()
{
    const juce::ScopedValueSetter<bool> guard(refreshing_, true);

    buses_ = model_.get_bus_paths();
    busSelector_.clear(juce::dontSendNotification);
    int busIdx = -1;
    for (size_t i = 0; i < buses_.size(); ++i) {
        busSelector_.addItem(juce::String::fromUTF8(buses_[i].c_str()), static_cast<int>(i) + 1);
        if (!track_selected_ && buses_[i] == target_.id_or_path) busIdx = static_cast<int>(i);
    }
    if (!track_selected_ && busIdx >= 0) {
        busSelector_.setSelectedItemIndex(busIdx, juce::dontSendNotification);
    }
    if (busSelector_.isVisible() != !track_selected_) {
        busSelector_.setVisible(!track_selected_);
        if (getWidth() > 0 && getHeight() > 0) {
            resized();
        }
    }

    if (track_selected_) {
        heading_.setText("Track effects", juce::dontSendNotification);
        status_.setText(juce::String::fromUTF8(target_.id_or_path.c_str()) +
                        "  [Track " + juce::String(target_.track_index + 1) + "]",
                        juce::dontSendNotification);
    } else {
        heading_.setText("Bus effects", juce::dontSendNotification);
        status_.setText(juce::String::fromUTF8(target_.id_or_path.c_str()), juce::dontSendNotification);
    }

    const auto* effects = model_.get_effects(target_);
    effectSelector_.clear(juce::dontSendNotification);
    if (effects != nullptr) {
        for (size_t i = 0; i < effects->size(); ++i) {
            const auto& fx = (*effects)[i];
            const std::string type = fx.value("type", "effect");
            const std::string pos = fx.value("position", 0) == 0 ? "Pre" : "Post";
            effectSelector_.addItem(
                juce::String(static_cast<int>(i) + 1) + ". " +
                juce::String::fromUTF8(type.c_str()) + " (" + pos.c_str() + ")",
                static_cast<int>(i) + 1);
        }
        if (effects->empty()) {
            selected_index_ = 0;
        } else {
            selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(effects->size()) - 1);
            effectSelector_.setSelectedItemIndex(selected_index_, juce::dontSendNotification);
        }
    } else {
        selected_index_ = 0;
    }

    const bool validTarget = (target_.type == ItemType::Bus) ||
                             (target_.type == ItemType::Event && target_.track_index >= 0);
    addButton_.setEnabled(validTarget);
    addType_.setEnabled(validTarget);
    addPosition_.setEnabled(validTarget);

    refresh_parameters();
    eqPlot_.repaint();
}

void EffectChainDeck::refresh_parameters()
{
    const juce::ScopedValueSetter<bool> guard(refreshing_, true);
    const auto* fx = selected_effect();
    const bool hasFx = fx != nullptr;

    effectSelector_.setEnabled(effectSelector_.getNumItems() > 0);
    removeButton_.setEnabled(hasFx);
    bypassButton_.setEnabled(hasFx);
    wetSlider_.setEnabled(hasFx);
    drySlider_.setEnabled(hasFx);

    if (!hasFx) {
        bypassButton_.setToggleState(false, juce::dontSendNotification);
        wetSlider_.setValue(1.0, juce::dontSendNotification);
        drySlider_.setValue(0.0, juce::dontSendNotification);
        for (size_t i = 0; i < paramLabels_.size(); ++i) {
            paramLabels_[i].setVisible(false);
            paramSliders_[i].setVisible(false);
        }
        return;
    }

    bypassButton_.setToggleState(fx->value("bypass", false), juce::dontSendNotification);
    wetSlider_.setValue(fx->value("wet", 1.0), juce::dontSendNotification);
    drySlider_.setValue(fx->value("dry", 0.0), juce::dontSendNotification);

    const std::string type = fx->value("type", "");
    const auto specs = specs_for_effect(type);
    const auto defaults = AuthoringModel::default_effect_parameters(type);
    const auto params = fx->value("params", nlohmann::json::array());

    for (size_t i = 0; i < paramLabels_.size(); ++i) {
        const bool visible = i < specs.size();
        paramLabels_[i].setVisible(visible);
        paramSliders_[i].setVisible(visible);
        if (visible) {
            paramLabels_[i].setText(specs[i].name, juce::dontSendNotification);
            paramSliders_[i].setRange(specs[i].min_val, specs[i].max_val, 0.0);
            paramSliders_[i].setNumDecimalPlacesToDisplay(specs[i].step < 0.1 ? 3 : 1);
            paramSliders_[i].setDoubleClickReturnValue(i < defaults.size(),
                i < defaults.size() ? defaults[i].get<double>() : 0.0, {});
            paramSliders_[i].setTextValueSuffix(specs[i].suffix);
            const double val = (i < params.size() && params[i].is_number())
                                   ? params[i].get<double>()
                                   : specs[i].min_val;
            paramSliders_[i].setValue(std::clamp(val, specs[i].min_val, specs[i].max_val),
                                      juce::dontSendNotification);
        }
    }
}

void EffectChainDeck::resized()
{
    viewport_.setBounds(getLocalBounds());
    const int contentWidth = std::max(560, viewport_.getMaximumVisibleWidth());
    const int contentHeight = 250;
    controls_.setBounds(0, 0, contentWidth, contentHeight);

    const auto area = controls_.getLocalBounds().reduced(OaeTheme::Spacing::small).toFloat();

    juce::FlexBox topBarBox;
    topBarBox.flexDirection = juce::FlexBox::Direction::row;
    topBarBox.alignItems = juce::FlexBox::AlignItems::center;
    topBarBox.items.add(juce::FlexItem(heading_).withFlex(0.0f, 1.0f, 110.0f).withMaxWidth(110.0f).withHeight(22.0f));
    if (busSelector_.isVisible()) {
        topBarBox.items.add(juce::FlexItem(busSelector_).withFlex(0.0f, 1.0f, 170.0f).withMaxWidth(170.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 4.0f }));
    }
    topBarBox.items.add(juce::FlexItem(status_).withFlex(1.0f).withHeight(22.0f));

    juce::FlexBox addRowBox;
    addRowBox.flexDirection = juce::FlexBox::Direction::row;
    addRowBox.alignItems = juce::FlexBox::AlignItems::center;
    addRowBox.items.add(juce::FlexItem(addType_).withFlex(0.0f, 1.0f, 120.0f).withMaxWidth(120.0f).withHeight(24.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    addRowBox.items.add(juce::FlexItem(addPosition_).withFlex(0.0f, 1.0f, 105.0f).withMaxWidth(105.0f).withHeight(24.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    addRowBox.items.add(juce::FlexItem(addButton_).withFlex(0.0f, 1.0f, 100.0f).withMaxWidth(100.0f).withHeight(24.0f).withMargin({ 0.0f, 10.0f, 0.0f, 0.0f }));
    addRowBox.items.add(juce::FlexItem(effectSelector_).withFlex(1.0f, 1.0f, 160.0f).withMaxWidth(220.0f).withHeight(24.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    addRowBox.items.add(juce::FlexItem(removeButton_).withFlex(0.0f, 1.0f, 75.0f).withMaxWidth(75.0f).withHeight(24.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    addRowBox.items.add(juce::FlexItem(bypassButton_).withFlex(0.0f, 1.0f, 75.0f).withMaxWidth(75.0f).withHeight(24.0f));

    juce::FlexBox mixRowBox;
    mixRowBox.flexDirection = juce::FlexBox::Direction::row;
    mixRowBox.alignItems = juce::FlexBox::AlignItems::center;
    mixRowBox.items.add(juce::FlexItem(wetLabel_).withFlex(0.0f, 0.5f, 36.0f).withMaxWidth(36.0f).withHeight(22.0f));
    mixRowBox.items.add(juce::FlexItem(wetSlider_).withFlex(1.0f, 1.0f, 140.0f).withMaxWidth(180.0f).withHeight(22.0f).withMargin({ 0.0f, 8.0f, 0.0f, 2.0f }));
    mixRowBox.items.add(juce::FlexItem(dryLabel_).withFlex(0.0f, 0.5f, 36.0f).withMaxWidth(36.0f).withHeight(22.0f));
    mixRowBox.items.add(juce::FlexItem(drySlider_).withFlex(1.0f, 1.0f, 140.0f).withMaxWidth(180.0f).withHeight(22.0f).withMargin({ 0.0f, 0.0f, 0.0f, 2.0f }));

    juce::FlexBox rightColBox;
    rightColBox.flexDirection = juce::FlexBox::Direction::column;
    rightColBox.items.add(juce::FlexItem(mixRowBox).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 0.0f, 0.0f, 2.0f, 0.0f }));
    rightColBox.items.add(juce::FlexItem().withFlex(1.0f));

    juce::FlexBox bodyBox;
    bodyBox.flexDirection = juce::FlexBox::Direction::row;
    bodyBox.items.add(juce::FlexItem(eqPlot_).withFlex(0.35f, 1.0f, 260.0f).withMaxWidth(280.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
    bodyBox.items.add(juce::FlexItem(rightColBox).withFlex(1.0f));

    juce::FlexBox rootBox;
    rootBox.flexDirection = juce::FlexBox::Direction::column;
    rootBox.items.add(juce::FlexItem(topBarBox).withFlex(0.0f, 0.0f, 26.0f));
    rootBox.items.add(juce::FlexItem(addRowBox).withFlex(0.0f, 0.0f, 28.0f).withMargin({ 0.0f, 0.0f, 4.0f, 0.0f }));
    rootBox.items.add(juce::FlexItem(bodyBox).withFlex(1.0f));
    rootBox.performLayout(area);

    juce::Grid paramGrid;
    paramGrid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Px(95.0f)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
    for (size_t i = 0; i < paramLabels_.size(); ++i) {
        paramGrid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Px(24.0f)));
        paramGrid.items.add(juce::GridItem(paramLabels_[i]).withArea(static_cast<int>(i + 1), 1));
        paramGrid.items.add(juce::GridItem(paramSliders_[i]).withArea(static_cast<int>(i + 1), 2));
    }
    paramGrid.performLayout(rightColBox.items[1].currentBounds.toNearestInt());
}

} // namespace oae::studio
