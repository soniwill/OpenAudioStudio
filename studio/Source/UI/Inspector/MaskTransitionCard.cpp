#include "MaskTransitionCard.h"

#include <algorithm>
#include <cmath>

namespace oae::studio {

MaskTransitionCard::MaskTransitionCard(const std::string& path,
                                       size_t track_index,
                                       bool expanded,
                                       AuthoringModel& model,
                                       int& edit_depth,
                                       std::function<void()> on_edit_end,
                                       std::function<void(bool)> onToggle,
                                       std::function<void()> onSwitchToTimeline)
    : InspectorCardBase("inspector_card_mask",
                        "mask",
                        juce::String::fromUTF8("M\xc3\x81SCARA & TRANSI\xc3\x87\xc3\x83O"),
                        "MASK",
                        juce::Colour(OaeTheme::Colors::accentCyan),
                        expanded,
                        model,
                        edit_depth,
                        std::move(on_edit_end),
                        std::move(onToggle))
    , path_(path)
    , track_index_(track_index)
    , on_switch_to_timeline_(std::move(onSwitchToTimeline))
{
    build_ui();
}

void MaskTransitionCard::refresh()
{
    build_ui();
    resized();
    repaint();
}

void MaskTransitionCard::reset_to_default()
{
    model_.begin_gesture();
    model_.clear_track_layer_mask(path_, static_cast<int>(track_index_));
    model_.end_gesture();
    if (on_edit_end_) on_edit_end_();
}

nlohmann::json MaskTransitionCard::serialize_values() const
{
    const auto spec = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));
    return {
        { "state", static_cast<int>(spec.state) },
        { "param_scope", spec.param_scope },
        { "param_name", spec.param_name },
        { "min_norm", spec.min_norm },
        { "max_norm", spec.max_norm },
        { "fade_norm", spec.fade_norm }
    };
}

bool MaskTransitionCard::deserialize_values(const nlohmann::json& payload)
{
    if (!payload.is_object()) return false;
    const int st = payload.value("state", 0);
    model_.begin_gesture();
    if (st == static_cast<int>(LayerMaskSpec::State::MaskActive)) {
        model_.set_track_layer_mask(
            path_,
            static_cast<int>(track_index_),
            payload.value("param_scope", "local"),
            payload.value("param_name", ""),
            payload.value("min_norm", 0.0),
            payload.value("max_norm", 1.0),
            payload.value("fade_norm", 0.1));
    } else if (st == static_cast<int>(LayerMaskSpec::State::AlwaysActive)) {
        model_.clear_track_layer_mask(path_, static_cast<int>(track_index_));
    }
    model_.end_gesture();
    if (on_edit_end_) on_edit_end_();
    return true;
}

void MaskTransitionCard::build_ui()
{
    comps_.clear();
    y_ = 4;

    const auto mask = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));

    if (mask.state == LayerMaskSpec::State::CustomCurve) {
        header_.set_badge("CURVE");
        auto* statusBadge = new OaeStatusBadge("CURVA MANUAL", OaeStatusBadge::Variant::Amber, false);
        add_labeled_row("Estado", statusBadge, 20);
    } else if (mask.state == LayerMaskSpec::State::MaskActive) {
        header_.set_badge("MASK");
        auto* statusBadge = new OaeStatusBadge(
            juce::String::fromUTF8("M\xc3\x81SCARA ATIVA"), OaeStatusBadge::Variant::Cyan, true);
        add_labeled_row("Estado", statusBadge, 20);
    } else {
        header_.set_badge("BASE");
        auto* statusBadge = new OaeStatusBadge("SEMPRE ATIVO", OaeStatusBadge::Variant::Green, true);
        add_labeled_row("Estado", statusBadge, 20);
    }

    struct Choice { std::string scope; std::string name; };
    std::vector<Choice> choices;
    auto* paramCombo = new juce::ComboBox("mask_param");
    paramCombo->setComponentID("inspector_mask_param");
    paramCombo->addItem("Sempre Ativo", 1);
    int nextId = 2;
    int selectedId = 1;

    for (const auto& name : model_.get_local_parameter_names(path_)) {
        choices.push_back({ "local", name });
        paramCombo->addItem(juce::String::fromUTF8(name.c_str()), nextId);
        if (mask.state == LayerMaskSpec::State::MaskActive && mask.param_scope == "local" && mask.param_name == name) {
            selectedId = nextId;
        }
        ++nextId;
    }
    for (const auto& name : model_.get_global_parameter_names()) {
        choices.push_back({ "global", name });
        paramCombo->addItem(juce::String("[Global] ") + juce::String::fromUTF8(name.c_str()), nextId);
        if (mask.state == LayerMaskSpec::State::MaskActive && mask.param_scope == "global" && mask.param_name == name) {
            selectedId = nextId;
        }
        ++nextId;
    }

    if (mask.state == LayerMaskSpec::State::CustomCurve) {
        paramCombo->setSelectedId(0, juce::dontSendNotification);
        paramCombo->setTextWhenNothingSelected("Curva Personalizada");
        paramCombo->setEnabled(false);
    } else {
        paramCombo->setSelectedId(selectedId, juce::dontSendNotification);
    }

    paramCombo->onChange = [this, paramCombo, choices]() {
        const int id = paramCombo->getSelectedId();
        if (id == 1) {
            model_.begin_gesture();
            model_.clear_track_layer_mask(path_, static_cast<int>(track_index_));
            model_.end_gesture();
            if (on_edit_end_) on_edit_end_();
        } else if (id >= 2 && static_cast<size_t>(id - 2) < choices.size()) {
            const auto& c = choices[static_cast<size_t>(id - 2)];
            const auto cur = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));
            const double mn = (cur.state == LayerMaskSpec::State::MaskActive) ? cur.min_norm : 0.25;
            const double mx = (cur.state == LayerMaskSpec::State::MaskActive) ? cur.max_norm : 0.75;
            const double fd = (cur.state == LayerMaskSpec::State::MaskActive) ? cur.fade_norm : 0.10;
            model_.begin_gesture();
            model_.set_track_layer_mask(path_, static_cast<int>(track_index_), c.scope, c.name, mn, mx, fd);
            model_.end_gesture();
            if (on_edit_end_) on_edit_end_();
        }
    };
    add_labeled_row("Parametro", paramCombo);

    if (mask.state == LayerMaskSpec::State::MaskActive) {
        auto* minSlider = create_slider(0.0, 1.0, mask.min_norm, "", [this](double val) {
            auto cur = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));
            if (cur.state != LayerMaskSpec::State::MaskActive) return;
            cur.min_norm = std::min(val, cur.max_norm);
            model_.set_track_layer_mask(path_, static_cast<int>(track_index_),
                                        cur.param_scope, cur.param_name,
                                        cur.min_norm, cur.max_norm, cur.fade_norm);
        });
        minSlider->setComponentID("inspector_mask_min");
        add_labeled_row("Min (norm)", minSlider);

        auto* maxSlider = create_slider(0.0, 1.0, mask.max_norm, "", [this](double val) {
            auto cur = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));
            if (cur.state != LayerMaskSpec::State::MaskActive) return;
            cur.max_norm = std::max(val, cur.min_norm);
            model_.set_track_layer_mask(path_, static_cast<int>(track_index_),
                                        cur.param_scope, cur.param_name,
                                        cur.min_norm, cur.max_norm, cur.fade_norm);
        });
        maxSlider->setComponentID("inspector_mask_max");
        add_labeled_row("Max (norm)", maxSlider);

        auto* fadeSlider = create_slider(0.0, 50.0, mask.fade_norm * 100.0, "%", [this](double val) {
            auto cur = model_.get_track_layer_mask(path_, static_cast<int>(track_index_));
            if (cur.state != LayerMaskSpec::State::MaskActive) return;
            cur.fade_norm = std::clamp(val / 100.0, 0.0, 0.5);
            model_.set_track_layer_mask(path_, static_cast<int>(track_index_),
                                        cur.param_scope, cur.param_name,
                                        cur.min_norm, cur.max_norm, cur.fade_norm);
        });
        fadeSlider->setComponentID("inspector_mask_fade");
        add_labeled_row("Fade (%)", fadeSlider);
    }

    auto* editBtn = new OaeToolbarButton("Editar Curva na Timeline...", OaeToolbarButton::Variant::AccentCyan);
    editBtn->setComponentID("inspector_mask_edit_timeline");
    editBtn->onClick = [this]() {
        if (on_switch_to_timeline_) {
            on_switch_to_timeline_();
        }
    };
    register_layout_item(body_, editBtn, y_, 250, 24);
    body_.addAndMakeVisible(editBtn);
    comps_.emplace_back(editBtn);
    y_ += 28;
}

} // namespace oae::studio
