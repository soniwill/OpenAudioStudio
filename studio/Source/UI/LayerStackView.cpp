#include "LayerStackView.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace oae::studio {

namespace {

fs::path resolve_sample_path(const AuthoringModel& model, const std::string& sample_id)
{
    if (sample_id.empty()) return {};
    const auto& root = model.document().get_root_json();
    if (!root.contains("samples") || !root["samples"].is_array()) return {};
    for (const auto& s : root["samples"]) {
        if (s.value("id", "") == sample_id) {
            const std::string rel = s.value("file", "");
            if (!rel.empty()) {
                return model.document().get_project_dir() / fs::u8path(rel);
            }
        }
    }
    return {};
}

const nlohmann::json* find_event_json(const nlohmann::json& root, const std::string& event_path)
{
    if (!root.contains("events") || !root["events"].is_array()) return nullptr;
    for (const auto& ev : root["events"]) {
        if (ev.is_object() && ev.value("path", "") == event_path) {
            return &ev;
        }
    }
    return nullptr;
}

bool is_supported_audio_extension(const juce::String& file_path)
{
    const auto ext = juce::File(file_path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

} // namespace

LayerStackView::MaskRangeCanvas::MaskRangeCanvas(LayerStackView& owner,
                                                 AuthoringModel& model,
                                                 const std::string& event_path,
                                                 size_t track_index,
                                                 std::function<void()> on_switch_to_timeline)
    : owner_(owner),
      model_(model),
      event_path_(event_path),
      track_index_(track_index),
      on_switch_to_timeline_(std::move(on_switch_to_timeline))
{
    setComponentID("layers.card." + juce::String(track_index_) + ".mask_canvas");

    editTimelineButton_.setComponentID("layers.card." + juce::String(track_index_) + ".edit_timeline");
    editTimelineButton_.setTooltip("Abrir edicao completa da curva na Timeline");
    editTimelineButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::accentAmber));
    editTimelineButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::bgDark));
    editTimelineButton_.onClick = [this]() {
        model_.select({ ItemType::Event, event_path_, static_cast<int>(track_index_), -1 });
        if (on_switch_to_timeline_) {
            on_switch_to_timeline_();
        }
    };
    addChildComponent(editTimelineButton_);
}

void LayerStackView::MaskRangeCanvas::update_state(const LayerMaskSpec& mask,
                                                   juce::Colour track_colour,
                                                   double live_param_norm,
                                                   bool has_live_param)
{
    mask_ = mask;
    track_colour_ = track_colour;
    live_param_norm_ = std::clamp(live_param_norm, 0.0, 1.0);
    has_live_param_ = has_live_param;

    const bool is_custom = (mask_.state == LayerMaskSpec::State::CustomCurve);
    editTimelineButton_.setVisible(is_custom);
    resized();
    repaint();
}

float LayerStackView::MaskRangeCanvas::norm_to_x(double norm) const noexcept
{
    const float usable = std::max(1.0f, static_cast<float>(getWidth() - 2 * kCanvasPadX));
    return static_cast<float>(kCanvasPadX) + static_cast<float>(std::clamp(norm, 0.0, 1.0)) * usable;
}

double LayerStackView::MaskRangeCanvas::x_to_norm(float x) const noexcept
{
    const float usable = std::max(1.0f, static_cast<float>(getWidth() - 2 * kCanvasPadX));
    return std::clamp(static_cast<double>(x - static_cast<float>(kCanvasPadX)) / static_cast<double>(usable), 0.0, 1.0);
}

void LayerStackView::MaskRangeCanvas::resized()
{
    if (editTimelineButton_.isVisible()) {
        auto r = getLocalBounds().reduced(kCanvasPadX, 4);
        editTimelineButton_.setBounds(r.removeFromRight(std::min(144, r.getWidth())));
    }
}

void LayerStackView::MaskRangeCanvas::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto content = bounds.reduced(static_cast<float>(kCanvasPadX), 2.0f);
    if (content.getWidth() <= 4.0f || content.getHeight() <= 4.0f) return;

    const auto barArea = juce::Rectangle<float>(
        content.getX(),
        content.getY() + 4.0f,
        content.getWidth(),
        std::min(10.0f, std::max(6.0f, content.getHeight() - 16.0f)));
    const auto legendArea = content.withTop(barArea.getBottom() + 3.0f);

    g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
    g.fillRoundedRectangle(barArea, OaeTheme::Edges::cornerRadius);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRoundedRectangle(barArea, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

    for (int step = 1; step <= 3; ++step) {
        const float gx = norm_to_x(static_cast<double>(step) * 0.25);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle).withAlpha(0.55f));
        g.drawVerticalLine(static_cast<int>(std::round(gx)), barArea.getY() + 1.0f, barArea.getBottom() - 1.0f);
    }

    if (mask_.state == LayerMaskSpec::State::CustomCurve) {
        g.setColour(juce::Colour(OaeTheme::Colors::tintAmber));
        g.fillRoundedRectangle(barArea.reduced(1.0f), OaeTheme::Edges::cornerRadius);

        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber));
        g.setFont(juce::FontOptions(OaeTheme::Typography::micro, juce::Font::bold));
        auto textArea = legendArea;
        if (editTimelineButton_.isVisible()) {
            textArea.removeFromRight(static_cast<float>(editTimelineButton_.getWidth() + 8));
        }
        g.drawText("Curva Personalizada", textArea, juce::Justification::centredLeft, true);
        return;
    }

    if (mask_.state == LayerMaskSpec::State::AlwaysActive) {
        g.setColour(OaeTheme::TrackColors::fadeTint(track_colour_));
        g.fillRoundedRectangle(barArea.reduced(1.0f), OaeTheme::Edges::cornerRadius);

        g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
        g.setFont(juce::FontOptions(OaeTheme::Typography::micro));
        g.drawText(juce::String::fromUTF8("Ativa: 0.00 \xe2\x80\x93 1.00 (Sempre Ativo)"), legendArea, juce::Justification::centredLeft, true);
        return;
    }

    const double fadeStart = std::max(0.0, mask_.min_norm - mask_.fade_norm);
    const double fadeEnd = std::min(1.0, mask_.max_norm + mask_.fade_norm);

    const float xFadeStart = norm_to_x(fadeStart);
    const float xMin = norm_to_x(mask_.min_norm);
    const float xMax = norm_to_x(mask_.max_norm);
    const float xFadeEnd = norm_to_x(fadeEnd);

    const auto innerBar = barArea.reduced(1.0f);
    if (xMin > xFadeStart) {
        g.setColour(OaeTheme::TrackColors::fadeTint(track_colour_));
        g.fillRoundedRectangle(xFadeStart, innerBar.getY(), xMin - xFadeStart, innerBar.getHeight(), 2.0f);
    }
    if (xFadeEnd > xMax) {
        g.setColour(OaeTheme::TrackColors::fadeTint(track_colour_));
        g.fillRoundedRectangle(xMax, innerBar.getY(), xFadeEnd - xMax, innerBar.getHeight(), 2.0f);
    }

    g.setColour(track_colour_);
    g.fillRoundedRectangle(xMin, innerBar.getY(), std::max(3.0f, xMax - xMin), innerBar.getHeight(), 2.0f);

    const float handleW = 4.0f;
    g.setColour(track_colour_.brighter(0.65f));
    g.fillRoundedRectangle(xMin - handleW * 0.5f, barArea.getY() - 2.0f, handleW, barArea.getHeight() + 4.0f, 1.5f);
    g.fillRoundedRectangle(xMax - handleW * 0.5f, barArea.getY() - 2.0f, handleW, barArea.getHeight() + 4.0f, 1.5f);

    g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
    g.setFont(juce::FontOptions(OaeTheme::Typography::micro));
    const auto activeLabel = juce::String::formatted("Ativa: %.2f ", mask_.min_norm)
                           + juce::String::fromUTF8("\xe2\x80\x93")
                           + juce::String::formatted(" %.2f", mask_.max_norm);
    g.drawText(activeLabel, legendArea, juce::Justification::centredLeft, true);

    if (has_live_param_) {
        const float cursorX = norm_to_x(live_param_norm_);
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.fillRect(cursorX - 1.0f, bounds.getY(), 2.0f, std::min(32.0f, bounds.getHeight()));
    }
}

void LayerStackView::MaskRangeCanvas::mouseMove(const juce::MouseEvent& event)
{
    if (mask_.state != LayerMaskSpec::State::MaskActive) {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    const float xMin = norm_to_x(mask_.min_norm);
    const float xMax = norm_to_x(mask_.max_norm);
    if (std::abs(event.position.x - xMin) <= 7.0f || std::abs(event.position.x - xMax) <= 7.0f) {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    } else if (event.position.x > xMin && event.position.x < xMax) {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    } else {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void LayerStackView::MaskRangeCanvas::mouseDown(const juce::MouseEvent& event)
{
    if (onSelectRequested) {
        onSelectRequested();
    }

    if (mask_.state != LayerMaskSpec::State::MaskActive || !event.mods.isLeftButtonDown()) {
        return;
    }

    const float xMin = norm_to_x(mask_.min_norm);
    const float xMax = norm_to_x(mask_.max_norm);
    drag_start_min_ = mask_.min_norm;
    drag_start_max_ = mask_.max_norm;
    drag_start_x_ = event.position.x;

    if (std::abs(event.position.x - xMin) <= 7.0f) {
        drag_mode_ = DragMode::LeftHandle;
    } else if (std::abs(event.position.x - xMax) <= 7.0f) {
        drag_mode_ = DragMode::RightHandle;
    } else if (event.position.x > xMin && event.position.x < xMax) {
        drag_mode_ = DragMode::MoveRange;
    } else {
        const double clickNorm = x_to_norm(event.position.x);
        if (std::abs(clickNorm - mask_.min_norm) <= std::abs(clickNorm - mask_.max_norm)) {
            drag_mode_ = DragMode::LeftHandle;
            drag_start_min_ = clickNorm;
        } else {
            drag_mode_ = DragMode::RightHandle;
            drag_start_max_ = clickNorm;
        }
    }

    owner_.set_interacting(true);
    model_.begin_gesture();
}

void LayerStackView::MaskRangeCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (drag_mode_ == DragMode::None || mask_.state != LayerMaskSpec::State::MaskActive) {
        return;
    }

    const float usable = std::max(1.0f, static_cast<float>(getWidth() - 2 * kCanvasPadX));
    const double deltaNorm = static_cast<double>(event.position.x - drag_start_x_) / static_cast<double>(usable);

    double newMin = mask_.min_norm;
    double newMax = mask_.max_norm;

    if (drag_mode_ == DragMode::LeftHandle) {
        newMin = std::clamp(x_to_norm(event.position.x), 0.0, mask_.max_norm);
    } else if (drag_mode_ == DragMode::RightHandle) {
        newMax = std::clamp(x_to_norm(event.position.x), mask_.min_norm, 1.0);
    } else if (drag_mode_ == DragMode::MoveRange) {
        const double span = drag_start_max_ - drag_start_min_;
        newMin = std::clamp(drag_start_min_ + deltaNorm, 0.0, std::max(0.0, 1.0 - span));
        newMax = std::clamp(newMin + span, newMin, 1.0);
    }

    mask_.min_norm = newMin;
    mask_.max_norm = newMax;
    model_.set_track_layer_mask(event_path_,
                                static_cast<int>(track_index_),
                                mask_.param_scope,
                                mask_.param_name,
                                newMin,
                                newMax,
                                mask_.fade_norm);
    if (onMaskEdited) {
        onMaskEdited();
    }
    repaint();
}

void LayerStackView::MaskRangeCanvas::mouseUp(const juce::MouseEvent&)
{
    if (drag_mode_ == DragMode::None) {
        return;
    }
    drag_mode_ = DragMode::None;
    model_.end_gesture();
    owner_.set_interacting(false);
    owner_.refresh();
}

std::vector<TestableTarget> LayerStackView::MaskRangeCanvas::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};
    if (mask_.state != LayerMaskSpec::State::MaskActive) {
        return {{ "canvas", "click", bounds, false }};
    }
    const int xMin = static_cast<int>(std::round(norm_to_x(mask_.min_norm)));
    const int xMax = static_cast<int>(std::round(norm_to_x(mask_.max_norm)));
    const int h = bounds.getHeight();
    std::vector<TestableTarget> out;
    const auto minRect = juce::Rectangle<int>(xMin - 4, 1, 9, std::max(1, h - 2)).getIntersection(bounds);
    const auto maxRect = juce::Rectangle<int>(xMax - 4, 1, 9, std::max(1, h - 2)).getIntersection(bounds);
    if (!minRect.isEmpty()) out.push_back({ "handle_min", "drag", minRect, isEnabled() });
    if (!maxRect.isEmpty()) out.push_back({ "handle_max", "drag", maxRect, isEnabled() });
    if (xMax - xMin >= 16) {
        const auto bodyRect = juce::Rectangle<int>(xMin + 6, 1, (xMax - xMin) - 12, std::max(1, h - 2)).getIntersection(bounds);
        if (!bodyRect.isEmpty()) out.push_back({ "range_body", "drag", bodyRect, isEnabled() });
    }
    return out;
}

LayerStackView::StemLayerCard::EyeToggleButton::EyeToggleButton(const juce::String& name)
    : juce::Button(name)
{
    setClickingTogglesState(false);
}

void LayerStackView::StemLayerCard::EyeToggleButton::paintButton(juce::Graphics& g,
                                                                 bool shouldDrawButtonAsHighlighted,
                                                                 bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool visible = getToggleState();

    juce::Colour bg = visible ? juce::Colour(OaeTheme::Colors::bgSurfaceRaised)
                              : juce::Colour(OaeTheme::Colors::accentAmber).withAlpha(0.22f);
    if (shouldDrawButtonAsDown) {
        bg = bg.darker(OaeTheme::States::pressedDarkening);
    } else if (shouldDrawButtonAsHighlighted) {
        bg = bg.brighter(OaeTheme::States::hoverBrightening);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);
    g.setColour(visible ? juce::Colour(OaeTheme::Colors::borderActive)
                        : juce::Colour(OaeTheme::Colors::accentAmber));
    g.drawRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float rx = 6.5f;
    const float ry = 4.0f;

    juce::Path eyePath;
    eyePath.startNewSubPath(cx - rx, cy);
    eyePath.quadraticTo(cx, cy - ry * 1.4f, cx + rx, cy);
    eyePath.quadraticTo(cx, cy + ry * 1.4f, cx - rx, cy);
    eyePath.closeSubPath();

    const juce::Colour iconCol = visible ? juce::Colour(OaeTheme::Colors::accentCyan)
                                         : juce::Colour(OaeTheme::Colors::textMuted);
    g.setColour(iconCol);
    g.strokePath(eyePath, juce::PathStrokeType(1.35f));

    if (visible) {
        g.fillEllipse(cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
    } else {
        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber));
        g.drawLine(cx - rx + 1.0f, cy + ry + 1.0f, cx + rx - 1.0f, cy - ry - 1.0f, 1.5f);
    }
}

LayerStackView::StemLayerCard::StemLayerCard(LayerStackView& owner,
                                             AuthoringModel& model,
                                             WaveformCache& waveform_cache,
                                             const std::string& event_path,
                                             size_t track_index,
                                             std::function<void()> on_switch_to_timeline)
    : owner_(owner),
      model_(model),
      waveform_cache_(waveform_cache),
      event_path_(event_path),
      track_index_(track_index),
      maskCanvas_(owner, model, event_path, track_index, std::move(on_switch_to_timeline))
{
    setComponentID("layers.card." + juce::String(track_index_));

    eyeButton_.setComponentID("layers.card." + juce::String(track_index_) + ".eye");
    eyeButton_.setTooltip("Visibilidade da camada (Mute / Unmute)");
    eyeButton_.onClick = [this]() {
        select_layer();
        const bool muted = model_.is_track_muted(event_path_, track_index_);
        model_.set_track_muted(event_path_, track_index_, !muted);
    };
    addAndMakeVisible(eyeButton_);

    soloButton_.setComponentID("layers.card." + juce::String(track_index_) + ".solo");
    soloButton_.setTooltip("Solo da camada");
    soloButton_.setClickingTogglesState(false);
    soloButton_.onClick = [this]() {
        select_layer();
        const bool solo = model_.is_track_solo(event_path_, track_index_);
        model_.set_track_solo(event_path_, track_index_, !solo);
    };
    addAndMakeVisible(soloButton_);

    titleLabel_.setFont(juce::FontOptions(OaeTheme::Typography::control, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    titleLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel_);

    addAndMakeVisible(syncBadge_);

    opacityLabel_.setFont(juce::FontOptions(OaeTheme::Typography::micro));
    opacityLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    opacityLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(opacityLabel_);

    opacitySlider_.setComponentID("layers.card." + juce::String(track_index_) + ".opacity");
    opacitySlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    opacitySlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    opacitySlider_.getProperties().set("oaeFlatBar", true);
    opacitySlider_.setRange(0.0, 100.0, 1.0);
    opacitySlider_.setDoubleClickReturnValue(true, 100.0, {});
    opacitySlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    opacitySlider_.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::trackFaderAccent));
    opacitySlider_.onDragStart = [this]() {
        select_layer();
        owner_.set_interacting(true);
        model_.begin_gesture();
    };
    opacitySlider_.onValueChange = [this]() {
        opacityLabel_.setText("Opacidade " + juce::String(static_cast<int>(std::round(opacitySlider_.getValue()))) + "%",
                              juce::dontSendNotification);
        if (updating_controls_) return;
        const double db = AuthoringModel::opacity_pct_to_db(opacitySlider_.getValue());
        model_.set_track_volume(event_path_, track_index_, db);
        refresh_live_state();
    };
    opacitySlider_.onDragEnd = [this]() {
        model_.end_gesture();
        owner_.set_interacting(false);
        owner_.refresh();
    };
    addAndMakeVisible(opacitySlider_);

    paramCombo_.setComponentID("layers.card." + juce::String(track_index_) + ".param");
    paramCombo_.setTooltip("Parametro associado a mascara da camada");
    paramCombo_.onChange = [this]() {
        if (updating_controls_) return;
        select_layer();
        const int selId = paramCombo_.getSelectedId();
        if (selId == 1) {
            model_.clear_track_layer_mask(event_path_, static_cast<int>(track_index_));
        } else if (selId >= 2) {
            const size_t idx = static_cast<size_t>(selId - 2);
            if (idx < param_choices_.size()) {
                const auto& choice = param_choices_[idx];
                const bool wasMask = (mask_.state == LayerMaskSpec::State::MaskActive);
                const double mn = wasMask ? mask_.min_norm : 0.2;
                const double mx = wasMask ? mask_.max_norm : 0.8;
                const double fd = wasMask ? mask_.fade_norm : 0.1;
                model_.set_track_layer_mask(event_path_, static_cast<int>(track_index_),
                                            choice.scope, choice.name, mn, mx, fd);
            }
        }
    };
    addAndMakeVisible(paramCombo_);

    fadeLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption));
    fadeLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    fadeLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(fadeLabel_);

    fadeSlider_.setComponentID("layers.card." + juce::String(track_index_) + ".fade");
    fadeSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fadeSlider_.getProperties().set("oaeFlatBar", true);
    fadeSlider_.setRange(0.0, 50.0, 1.0);
    fadeSlider_.setDoubleClickReturnValue(true, 10.0, {});
    fadeSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    fadeSlider_.setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    fadeSlider_.onDragStart = [this]() {
        select_layer();
        owner_.set_interacting(true);
        model_.begin_gesture();
    };
    fadeSlider_.onValueChange = [this]() {
        fadeLabel_.setText("Fade: " + juce::String(static_cast<int>(std::round(fadeSlider_.getValue()))) + "%",
                           juce::dontSendNotification);
        if (updating_controls_ || mask_.state != LayerMaskSpec::State::MaskActive) return;
        mask_.fade_norm = std::clamp(fadeSlider_.getValue() / 100.0, 0.0, 0.5);
        model_.set_track_layer_mask(event_path_,
                                    static_cast<int>(track_index_),
                                    mask_.param_scope,
                                    mask_.param_name,
                                    mask_.min_norm,
                                    mask_.max_norm,
                                    mask_.fade_norm);
        refresh_live_state();
    };
    fadeSlider_.onDragEnd = [this]() {
        model_.end_gesture();
        owner_.set_interacting(false);
        owner_.refresh();
    };
    addAndMakeVisible(fadeSlider_);

    addAndMakeVisible(statusBadge_);

    maskCanvas_.onMaskEdited = [this]() {
        refresh_live_state();
    };
    maskCanvas_.onSelectRequested = [this]() {
        select_layer();
    };
    addAndMakeVisible(maskCanvas_);

    refresh_from_model();
}

void LayerStackView::StemLayerCard::select_layer()
{
    model_.select({ ItemType::Event, event_path_, static_cast<int>(track_index_), primary_inst_index_ });
}

void LayerStackView::StemLayerCard::populate_parameter_combo()
{
    paramCombo_.clear(juce::dontSendNotification);
    param_choices_.clear();

    paramCombo_.addItem("Sempre Ativo", 1);

    int nextId = 2;
    for (const auto& name : model_.get_local_parameter_names(event_path_)) {
        param_choices_.push_back({ "local", name });
        paramCombo_.addItem(juce::String::fromUTF8(name.c_str()), nextId++);
    }
    for (const auto& name : model_.get_global_parameter_names()) {
        param_choices_.push_back({ "global", name });
        paramCombo_.addItem(juce::String("[Global] ") + juce::String::fromUTF8(name.c_str()), nextId++);
    }
}

std::shared_ptr<const WaveformEnvelope> LayerStackView::StemLayerCard::resolve_primary_waveform() const
{
    if (!primary_sample_id_.empty()) {
        const auto file = resolve_sample_path(model_, primary_sample_id_);
        if (!file.empty()) {
            if (auto env = waveform_cache_.get_or_load(file)) {
                cachedEnvelope_ = env;
                return env;
            }
        }
    } else if (primary_kind_ == "programmer") {
        auto env = waveform_cache_.get_or_build_synthetic(primary_target_path_.empty() ? "programmer" : primary_target_path_);
        if (env) {
            cachedEnvelope_ = env;
            return env;
        }
    }
    return cachedEnvelope_;
}

void LayerStackView::StemLayerCard::refresh_from_model()
{
    updating_controls_ = true;

    const auto& root = model_.document().get_root_json();
    const auto* ev = find_event_json(root, event_path_);

    double volumeDb = 0.0;
    primary_inst_index_ = -1;
    primary_loop_ = false;
    primary_offset_ms_ = 0.0;
    primary_length_ms_ = 1000.0;
    primary_start_offset_ms_ = 0.0;
    primary_pitch_semitones_ = 0.0;
    primary_fade_in_ms_ = 0.0;
    primary_fade_out_ms_ = 0.0;
    primary_kind_.clear();
    primary_sample_id_.clear();
    primary_target_path_.clear();

    if (ev && ev->contains("tracks") && (*ev)["tracks"].is_array() && track_index_ < (*ev)["tracks"].size()) {
        const auto& trk = (*ev)["tracks"][track_index_];
        volumeDb = trk.value("volume_db", 0.0);
        if (trk.contains("instruments") && trk["instruments"].is_array() && !trk["instruments"].empty()) {
            primary_inst_index_ = 0;
            const auto& inst = trk["instruments"][0];
            primary_kind_ = inst.value("kind", "single");
            primary_loop_ = inst.value("loop", false);
            primary_offset_ms_ = inst.value("offset_ms", 0.0);
            primary_length_ms_ = std::max(100.0, inst.value("length_ms", 1000.0));
            primary_start_offset_ms_ = inst.value("start_offset_ms", 0.0);
            primary_pitch_semitones_ = inst.value("pitch_semitones_base", inst.value("pitch", 0.0));
            primary_fade_in_ms_ = inst.value("fade_in_ms", 0.0);
            primary_fade_out_ms_ = inst.value("fade_out_ms", 0.0);
            primary_sample_id_ = inst.value("sample_id", "");
            primary_target_path_ = inst.value("target_event", inst.value("programmer_key", ""));
        }
    }

    const std::string trackName = model_.get_track_name(event_path_, track_index_);
    juce::String titleText = juce::String::fromUTF8(trackName.c_str());
    if (!primary_sample_id_.empty()) {
        titleText += " - " + juce::String::fromUTF8(primary_sample_id_.c_str());
    }
    titleLabel_.setText(titleText, juce::dontSendNotification);

    if (primary_inst_index_ >= 0) {
        syncBadge_.setButtonText(primary_loop_ ? "LOOP" : "1-SHOT");
        syncBadge_.setBadgeStyle(primary_loop_ ? OaeStatusBadge::Variant::Cyan : OaeStatusBadge::Variant::Neutral, false);
    } else {
        syncBadge_.setButtonText("VAZIA");
        syncBadge_.setBadgeStyle(OaeStatusBadge::Variant::Neutral, false);
    }

    const bool muted = model_.is_track_muted(event_path_, track_index_);
    eyeButton_.setToggleState(!muted, juce::dontSendNotification);

    const bool solo = model_.is_track_solo(event_path_, track_index_);
    soloButton_.setColour(juce::TextButton::buttonColourId,
                          solo ? juce::Colour(OaeTheme::Colors::accentCyan)
                               : juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    soloButton_.setColour(juce::TextButton::textColourOffId,
                          solo ? juce::Colour(OaeTheme::Colors::bgDark)
                               : juce::Colour(OaeTheme::Colors::textSecondary));

    const double opacityPct = AuthoringModel::db_to_opacity_pct(volumeDb);
    opacitySlider_.setValue(opacityPct, juce::dontSendNotification);
    opacityLabel_.setText("Opacidade " + juce::String(static_cast<int>(std::round(opacityPct))) + "%",
                          juce::dontSendNotification);

    mask_ = model_.get_track_layer_mask(event_path_, static_cast<int>(track_index_));
    populate_parameter_combo();

    if (mask_.state == LayerMaskSpec::State::CustomCurve) {
        paramCombo_.setSelectedId(0, juce::dontSendNotification);
        paramCombo_.setTextWhenNothingSelected("Curva Personalizada");
        paramCombo_.setEnabled(false);
        fadeSlider_.setEnabled(false);
        fadeSlider_.setValue(0.0, juce::dontSendNotification);
    } else if (mask_.state == LayerMaskSpec::State::MaskActive) {
        paramCombo_.setEnabled(true);
        int selectedId = 1;
        for (size_t i = 0; i < param_choices_.size(); ++i) {
            if (param_choices_[i].scope == mask_.param_scope && param_choices_[i].name == mask_.param_name) {
                selectedId = static_cast<int>(i + 2);
                break;
            }
        }
        paramCombo_.setSelectedId(selectedId, juce::dontSendNotification);
        fadeSlider_.setEnabled(true);
        fadeSlider_.setValue(mask_.fade_norm * 100.0, juce::dontSendNotification);
    } else {
        paramCombo_.setEnabled(true);
        paramCombo_.setSelectedId(1, juce::dontSendNotification);
        fadeSlider_.setEnabled(false);
        fadeSlider_.setValue(10.0, juce::dontSendNotification);
    }

    updating_controls_ = false;
    refresh_live_state();
}

void LayerStackView::StemLayerCard::refresh_live_state()
{
    const auto trackColour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);
    const bool muted = model_.is_track_muted(event_path_, track_index_);

    double liveNorm = 0.0;
    bool hasLive = false;
    if (mask_.state == LayerMaskSpec::State::MaskActive && !mask_.param_name.empty()) {
        liveNorm = owner_.get_normalized_parameter_value(mask_.param_scope, mask_.param_name);
        hasLive = true;
    }

    const double maskGain = AuthoringModel::evaluate_layer_mask_gain(mask_, liveNorm);
    const double opacityRatio = muted ? 0.0 : (opacitySlider_.getValue() / 100.0);
    const int effectivePct = static_cast<int>(std::round(maskGain * opacityRatio * 100.0));

    if (muted) {
        statusBadge_.setButtonText("MUTADO (0%)");
        statusBadge_.setBadgeStyle(OaeStatusBadge::Variant::Amber, false);
    } else if (mask_.state == LayerMaskSpec::State::CustomCurve) {
        statusBadge_.setButtonText("CURVA MANUAL");
        statusBadge_.setBadgeStyle(OaeStatusBadge::Variant::Amber, false);
    } else if (mask_.state == LayerMaskSpec::State::AlwaysActive) {
        statusBadge_.setButtonText("BASE ATIVA");
        statusBadge_.setBadgeStyle(OaeStatusBadge::Variant::Green, true);
    } else if (effectivePct > 0) {
        statusBadge_.setButtonText("TOCANDO (" + juce::String(effectivePct) + "%)");
        statusBadge_.setBadgeStyle(OaeStatusBadge::Variant::Green, true);
    } else {
        statusBadge_.setButtonText("AGUARDANDO");
        statusBadge_.setBadgeStyle(OaeStatusBadge::Variant::Neutral, false);
    }

    fadeLabel_.setText("Fade: " + juce::String(static_cast<int>(std::round(fadeSlider_.getValue()))) + "%",
                       juce::dontSendNotification);
    maskCanvas_.update_state(mask_, trackColour, liveNorm, hasLive);
    repaint();
}

void LayerStackView::StemLayerCard::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto trackColour = OaeTheme::TrackColors::parseHexColor(
        model_.get_track_color(event_path_, track_index_), track_index_);

    const auto& sel = model_.get_selection();
    const bool selected = (sel.type == ItemType::Event &&
                           sel.id_or_path == event_path_ &&
                           sel.track_index == static_cast<int>(track_index_));

    g.setColour(juce::Colour(OaeTheme::Colors::cardBodyBg));
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cardCornerRadius);

    g.setColour(selected ? juce::Colour(OaeTheme::Colors::accentCyan)
                         : juce::Colour(OaeTheme::Colors::cardBorder));
    g.drawRoundedRectangle(bounds.reduced(0.5f), OaeTheme::Edges::cardCornerRadius,
                           selected ? OaeTheme::Edges::selectedBorderStroke : OaeTheme::Edges::borderStroke);

    g.setColour(trackColour);
    g.fillRoundedRectangle(8.0f, 10.0f, 4.0f, 18.0f, 2.0f);

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawVerticalLine(kLeftColumnWidth, 6.0f, bounds.getHeight() - 6.0f);

    if (!waveformBounds_.isEmpty()) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
        g.fillRoundedRectangle(waveformBounds_, OaeTheme::Edges::cornerRadius);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawRoundedRectangle(waveformBounds_, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

        auto innerWave = waveformBounds_.reduced(2.0f, 2.0f);
        auto env = resolve_primary_waveform();
        if (env && !env->is_empty()) {
            env->render_clip(g,
                             innerWave,
                             primary_offset_ms_,
                             primary_length_ms_,
                             primary_start_offset_ms_,
                             primary_pitch_semitones_,
                             primary_loop_,
                             primary_fade_in_ms_,
                             primary_fade_out_ms_,
                             trackColour.brighter(0.55f).withAlpha(0.92f),
                             trackColour.brighter(0.15f).withAlpha(0.68f));
        } else {
            g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
            g.setFont(juce::FontOptions(OaeTheme::Typography::caption));
            g.drawText(primary_sample_id_.empty() ? "Sem sample" : "Carregando...",
                       innerWave, juce::Justification::centred, true);
        }
    }
}

void LayerStackView::StemLayerCard::resized()
{
    auto area = getLocalBounds();
    auto leftCol = area.removeFromLeft(kLeftColumnWidth);
    leftCol.removeFromLeft(16);
    leftCol = leftCol.reduced(4, 8);

    auto leftTop = leftCol.removeFromTop(22);
    eyeButton_.setBounds(leftTop.removeFromLeft(20).withSizeKeepingCentre(20, 20));
    leftTop.removeFromLeft(4);
    soloButton_.setBounds(leftTop.removeFromLeft(20).withSizeKeepingCentre(20, 20));
    leftTop.removeFromLeft(6);
    syncBadge_.setBounds(leftTop.removeFromRight(syncBadge_.preferredWidth(48)).withSizeKeepingCentre(syncBadge_.preferredWidth(48), 18));
    leftTop.removeFromRight(4);
    titleLabel_.setBounds(leftTop);

    leftCol.removeFromTop(8);
    auto leftBottom = leftCol;
    waveformBounds_ = leftBottom.removeFromLeft(182).toFloat();
    leftBottom.removeFromLeft(8);
    opacityLabel_.setBounds(leftBottom.removeFromTop(14));
    opacitySlider_.setBounds(leftBottom.withTrimmedTop(2));

    area.removeFromLeft(1);
    auto rightCol = area.reduced(kRightColPadX + 4, 8);
    auto rightTop = rightCol.removeFromTop(22);

    const int comboW = std::min(156, std::max(84, rightTop.getWidth() / 3));
    paramCombo_.setBounds(rightTop.removeFromLeft(comboW));
    rightTop.removeFromLeft(8);
    fadeLabel_.setBounds(rightTop.removeFromLeft(56));
    fadeSlider_.setBounds(rightTop.removeFromLeft(std::min(72, std::max(44, rightTop.getWidth() / 4))));
    rightTop.removeFromLeft(6);
    const int badgeW = std::min(116, rightTop.getWidth());
    statusBadge_.setBounds(rightTop.removeFromRight(badgeW).withSizeKeepingCentre(badgeW, 20));

    rightCol.removeFromTop(6);
    maskCanvas_.setBounds(rightCol);
}

void LayerStackView::StemLayerCard::mouseDown(const juce::MouseEvent&)
{
    select_layer();
}

LayerStackView::AdjustmentLayerCard::AdjustmentLayerCard(LayerStackView& owner,
                                                         AuthoringModel& model,
                                                         Info info)
    : owner_(owner), model_(model), info_(std::move(info))
{
    const juce::String cardId = (info_.target.type == ItemType::Bus)
        ? ("layers.adjustment.bus." + juce::String(info_.bus_index) + "." + juce::String(info_.effect_index))
        : ("layers.adjustment.track." + juce::String(info_.target.track_index) + "." + juce::String(info_.effect_index));
    setComponentID(cardId);

    bypassButton_.setComponentID(cardId + ".bypass");
    bypassButton_.setTooltip("Alternar bypass do efeito de ajuste");
    bypassButton_.onClick = [this]() {
        model_.select(info_.target);
        model_.modify_effect(info_.target, info_.effect_index, [](nlohmann::json& fx) {
            fx["bypass"] = !fx.value("bypass", false);
        });
    };
    addAndMakeVisible(bypassButton_);

    titleLabel_.setText(juce::String::fromUTF8(info_.title.c_str()), juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(OaeTheme::Typography::control, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    titleLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel_);

    statusLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
    statusLabel_.setJustificationType(juce::Justification::centredRight);
    statusLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(statusLabel_);

    refresh_live_state();
}

void LayerStackView::AdjustmentLayerCard::refresh_live_state()
{
    const auto* effects = model_.get_effects(info_.target);
    if (effects && info_.effect_index < effects->size()) {
        info_.bypassed = (*effects)[info_.effect_index].value("bypass", false);
    }

    bypassButton_.setColour(juce::TextButton::buttonColourId,
                            info_.bypassed ? juce::Colour(OaeTheme::Colors::bgSurfaceRaised)
                                           : juce::Colour(OaeTheme::Colors::accentPurple));
    bypassButton_.setColour(juce::TextButton::textColourOffId,
                            info_.bypassed ? juce::Colour(OaeTheme::Colors::textMuted)
                                           : juce::Colour(OaeTheme::Colors::textPrimary));

    has_live_param_ = !info_.param_name.empty();
    if (has_live_param_) {
        live_param_norm_ = owner_.get_normalized_parameter_value(info_.param_scope, info_.param_name);
    }

    const juce::String stateText = info_.bypassed
        ? "BYPASSED"
        : (has_live_param_ ? juce::String::fromUTF8(("ATIVO · $" + info_.param_name).c_str()) : "FX ATIVO");
    statusLabel_.setText(stateText, juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId,
                           info_.bypassed ? juce::Colour(OaeTheme::Colors::textMuted)
                                          : juce::Colour(OaeTheme::Colors::accentPurple).brighter(0.5f));
    repaint();
}

void LayerStackView::AdjustmentLayerCard::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(OaeTheme::Colors::cardHeaderBg));
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);

    g.setColour(juce::Colour(OaeTheme::Colors::accentPurple));
    g.fillRoundedRectangle(bounds.withWidth(6.0f), 2.0f);

    g.setColour(juce::Colour(OaeTheme::Colors::accentPurple).withAlpha(0.55f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawVerticalLine(kLeftColumnWidth, 4.0f, bounds.getHeight() - 4.0f);

    auto rightTrack = bounds;
    rightTrack.removeFromLeft(static_cast<float>(kLeftColumnWidth + 1 + kRightColPadX + kCanvasPadX));
    rightTrack.removeFromRight(static_cast<float>(kRightColPadX + kCanvasPadX));
    rightTrack = rightTrack.reduced(0.0f, 8.0f);
    if (rightTrack.getWidth() > 16.0f) {
        auto bar = rightTrack.withTrimmedTop(14.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
        g.fillRoundedRectangle(bar, OaeTheme::Edges::cornerRadius);
        g.setColour(juce::Colour(OaeTheme::Colors::accentPurple).withAlpha(info_.bypassed ? 0.12f : 0.34f));
        g.fillRoundedRectangle(bar.reduced(1.0f), OaeTheme::Edges::cornerRadius);

        g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
        g.setFont(juce::FontOptions(OaeTheme::Typography::caption));
        g.drawText(juce::String::fromUTF8(info_.summary.c_str()),
                   bar.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);

        if (has_live_param_) {
            const float cx = bar.getX() + static_cast<float>(std::clamp(live_param_norm_, 0.0, 1.0)) * bar.getWidth();
            g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
            g.drawLine(cx, bar.getY() - 2.0f, cx, bar.getBottom() + 2.0f, 1.8f);
        }
    }
}

void LayerStackView::AdjustmentLayerCard::resized()
{
    auto area = getLocalBounds();
    auto leftCol = area.removeFromLeft(kLeftColumnWidth);
    leftCol.removeFromLeft(10);
    leftCol = leftCol.reduced(4, 12);

    bypassButton_.setBounds(leftCol.removeFromLeft(34));
    leftCol.removeFromLeft(8);
    titleLabel_.setBounds(leftCol);

    area.removeFromLeft(1);
    auto rightTop = area.reduced(kRightColPadX + kCanvasPadX, 4).removeFromTop(16);
    statusLabel_.setBounds(rightTop);
}

void LayerStackView::AdjustmentLayerCard::mouseDown(const juce::MouseEvent&)
{
    model_.select(info_.target);
}

LayerStackView::LayerStackView(AuthoringModel& model, WaveformCache& waveform_cache, AuditionTransport* transport)
    : model_(model), waveform_cache_(waveform_cache), transport_(transport)
{
    setComponentID("layers_stack_view");
    model_.add_listener(this);
    waveform_cache_.addChangeListener(this);
    if (transport_) {
        transport_->add_listener(this);
    }

    viewport_.setViewedComponent(&contentContainer_, false);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(kScrollbarThickness);
    addAndMakeVisible(viewport_);
}

LayerStackView::~LayerStackView()
{
    if (transport_) {
        transport_->remove_listener(this);
    }
    waveform_cache_.removeChangeListener(this);
    model_.remove_listener(this);
}

void LayerStackView::set_event_path(const std::string& path)
{
    if (event_path_ == path) return;
    event_path_ = path;
    rebuild_cards();
}

void LayerStackView::set_transport(AuditionTransport* transport)
{
    if (transport_ == transport) return;
    if (transport_) {
        transport_->remove_listener(this);
    }
    transport_ = transport;
    if (transport_) {
        transport_->add_listener(this);
    }
    update_live_states();
}

void LayerStackView::refresh()
{
    rebuild_cards();
}

double LayerStackView::get_normalized_parameter_value(const std::string& scope, const std::string& name) const
{
    if (name.empty()) return 0.0;
    const auto [domMin, domMax] = model_.get_parameter_domain(event_path_, scope, name);
    const double span = (domMax > domMin) ? (domMax - domMin) : 1.0;

    double rawVal = domMin;
    if (transport_) {
        rawVal = (scope == "global")
            ? static_cast<double>(transport_->get_global_parameter(name))
            : static_cast<double>(transport_->get_local_parameter(name));
    } else {
        const auto& root = model_.document().get_root_json();
        if (scope == "global" && root.contains("global_parameters") && root["global_parameters"].is_array()) {
            for (const auto& p : root["global_parameters"]) {
                if (p.value("name", "") == name) {
                    rawVal = p.value("default_value", domMin);
                    break;
                }
            }
        } else if (const auto* ev = find_event_json(root, event_path_)) {
            if (ev->contains("parameters") && (*ev)["parameters"].is_array()) {
                for (const auto& p : (*ev)["parameters"]) {
                    if (p.value("name", "") == name) {
                        rawVal = p.value("default_value", domMin);
                        break;
                    }
                }
            }
        }
    }

    return std::clamp((rawVal - domMin) / span, 0.0, 1.0);
}

void LayerStackView::collect_adjustment_infos(const nlohmann::json& root,
                                              const nlohmann::json& event_json,
                                              std::vector<AdjustmentLayerCard::Info>& out) const
{
    const std::string busPath = event_json.value("bus_path", "bus:/");
    size_t busIndex = 0;
    if (root.contains("buses") && root["buses"].is_array()) {
        for (size_t bi = 0; bi < root["buses"].size(); ++bi) {
            if (root["buses"][bi].value("path", "") == busPath) {
                busIndex = bi;
                break;
            }
        }
    }

    if (root.contains("effect_automations") && root["effect_automations"].is_array()) {
        for (const auto& ea : root["effect_automations"]) {
            if (!ea.is_object()) continue;
            if (ea.value("bus_path", "") != busPath) continue;

            AdjustmentLayerCard::Info info;
            info.target = { ItemType::Bus, busPath };
            info.bus_index = busIndex;
            info.effect_index = static_cast<size_t>(ea.value("effect_index", 0));
            const std::string fxType = ea.value("effect_type", "effect");
            info.title = "Ajuste: " + fxType + " (" + busPath + ")";
            if (ea.contains("param") && ea["param"].is_object()) {
                info.param_scope = ea["param"].value("scope", "global");
                info.param_name = ea["param"].value("name", "");
            }
            info.summary = "Camada de Ajuste por parametro ($" + info.param_name + ")";
            out.push_back(std::move(info));
        }
    }

    if (event_json.contains("tracks") && event_json["tracks"].is_array()) {
        for (size_t ti = 0; ti < event_json["tracks"].size(); ++ti) {
            const auto& trk = event_json["tracks"][ti];
            if (!trk.contains("effects") || !trk["effects"].is_array() || trk["effects"].empty()) continue;

            for (size_t ei = 0; ei < trk["effects"].size(); ++ei) {
                const auto& fx = trk["effects"][ei];
                AdjustmentLayerCard::Info info;
                info.target = { ItemType::Event, event_path_, static_cast<int>(ti), -1 };
                info.effect_index = ei;
                info.bypassed = fx.value("bypass", false);
                const std::string fxType = fx.value("type", "fx");
                info.title = "Ajuste (" + model_.get_track_name(event_path_, ti) + "): " + fxType;
                info.summary = "Insert FX · " + fxType;
                const auto mask = model_.get_track_layer_mask(event_path_, static_cast<int>(ti));
                if (mask.state == LayerMaskSpec::State::MaskActive) {
                    info.param_scope = mask.param_scope;
                    info.param_name = mask.param_name;
                }
                out.push_back(std::move(info));
            }
        }
    }
}

void LayerStackView::rebuild_cards()
{
    adjustment_cards_.clear();
    stem_cards_.clear();
    contentContainer_.removeAllChildren();

    if (event_path_.empty()) {
        layout_cards();
        repaint();
        return;
    }

    const auto& root = model_.document().get_root_json();
    const auto* ev = find_event_json(root, event_path_);
    if (!ev) {
        layout_cards();
        repaint();
        return;
    }

    std::vector<AdjustmentLayerCard::Info> adjInfos;
    collect_adjustment_infos(root, *ev, adjInfos);
    for (auto& info : adjInfos) {
        auto card = std::make_unique<AdjustmentLayerCard>(*this, model_, std::move(info));
        contentContainer_.addAndMakeVisible(*card);
        adjustment_cards_.push_back(std::move(card));
    }

    if (ev->contains("tracks") && (*ev)["tracks"].is_array()) {
        const size_t trackCount = (*ev)["tracks"].size();
        for (size_t i = 0; i < trackCount; ++i) {
            auto card = std::make_unique<StemLayerCard>(
                *this, model_, waveform_cache_, event_path_, i, onSwitchToTimelineRequested);
            contentContainer_.addAndMakeVisible(*card);
            stem_cards_.push_back(std::move(card));
        }
    }

    layout_cards();
    repaint();
}

void LayerStackView::layout_cards()
{
    const int viewW = std::max(420, viewport_.getWidth() - kScrollbarThickness);
    int y = kCardGap;

    for (auto& card : adjustment_cards_) {
        card->setBounds(kCardGap, y, viewW - 2 * kCardGap, kAdjustmentCardHeight);
        y += kAdjustmentCardHeight + kCardGap;
    }

    for (auto& card : stem_cards_) {
        card->setBounds(kCardGap, y, viewW - 2 * kCardGap, kStemCardHeight);
        y += kStemCardHeight + kCardGap;
    }

    contentContainer_.setSize(viewW, std::max(y, std::max(100, viewport_.getHeight())));
}

void LayerStackView::update_live_states()
{
    for (auto& adj : adjustment_cards_) {
        adj->refresh_live_state();
    }
    for (auto& stem : stem_cards_) {
        stem->refresh_live_state();
    }
}

void LayerStackView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));

    auto headerArea = bounds.removeFromTop(kColumnHeaderHeight);
    g.setColour(juce::Colour(OaeTheme::Colors::cardHeaderBg));
    g.fillRect(headerArea);

    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(headerArea.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

    const int dividerX = kCardGap + kLeftColumnWidth;
    g.drawVerticalLine(dividerX, static_cast<float>(headerArea.getY()), static_cast<float>(headerArea.getBottom()));

    g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
    g.setFont(juce::FontOptions(OaeTheme::Typography::micro, juce::Font::bold));
    g.drawText("CAMADA (STEM & MIX)",
               headerArea.withWidth(dividerX).reduced(12, 0),
               juce::Justification::centredLeft, true);

    auto rightHeader = headerArea.withTrimmedLeft(dividerX + 1).withTrimmedRight(kScrollbarThickness + kCardGap);
    const float barLeft = static_cast<float>(rightHeader.getX() + kRightColPadX + kCanvasPadX);
    const float barRight = static_cast<float>(rightHeader.getRight() - kRightColPadX - kCanvasPadX);
    if (barRight > barLeft + 40.0f) {
        g.drawText(juce::String::fromUTF8("M\xc3\x81SCARA DE ATIVA\xc3\x87\xc3\x83O (0.0 \xe2\x80\x94 1.0)"),
                   juce::Rectangle<float>(barLeft, static_cast<float>(rightHeader.getY()), 220.0f, static_cast<float>(rightHeader.getHeight())),
                   juce::Justification::centredLeft, true);

        for (const auto& card : stem_cards_) {
            const auto& m = card->get_mask_spec();
            if (m.state == LayerMaskSpec::State::MaskActive && !m.param_name.empty()) {
                const double norm = get_normalized_parameter_value(m.param_scope, m.param_name);
                const auto [domMin, domMax] = model_.get_parameter_domain(event_path_, m.param_scope, m.param_name);
                const double liveVal = domMin + norm * (domMax - domMin);
                g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
                g.drawText(juce::String::formatted("Cursor ao vivo: $%s = %.2f", m.param_name.c_str(), liveVal),
                           juce::Rectangle<float>(barLeft + 180.0f, static_cast<float>(rightHeader.getY()),
                                                  std::max(0.0f, barRight - (barLeft + 180.0f)),
                                                  static_cast<float>(rightHeader.getHeight())),
                           juce::Justification::centredRight, true);
                break;
            }
        }
    }

    if (stem_cards_.empty() && adjustment_cards_.empty()) {
        g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
        g.setFont(juce::FontOptions(OaeTheme::Typography::body));
        g.drawText("Nenhuma camada de stem neste evento. Clique em \"+ Stem Layer\" ou arraste arquivos de audio (.wav, .ogg, .mp3, .flac).",
                   bounds.reduced(24), juce::Justification::centred, true);
    }
}

void LayerStackView::paintOverChildren(juce::Graphics& g)
{
    if (is_file_drag_over_) {
        auto area = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(0.14f));
        g.fillRoundedRectangle(area, OaeTheme::Edges::cornerRadius);
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.drawRoundedRectangle(area, OaeTheme::Edges::cornerRadius, 2.0f);
    }
}

void LayerStackView::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kColumnHeaderHeight);
    viewport_.setBounds(area);
    layout_cards();
}

bool LayerStackView::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (event_path_.empty()) return false;
    for (const auto& f : files) {
        if (is_supported_audio_extension(f)) return true;
    }
    return false;
}

void LayerStackView::fileDragEnter(const juce::StringArray&, int, int)
{
    is_file_drag_over_ = true;
    repaint();
}

void LayerStackView::fileDragMove(const juce::StringArray&, int, int)
{
}

void LayerStackView::fileDragExit(const juce::StringArray&)
{
    is_file_drag_over_ = false;
    repaint();
}

void LayerStackView::filesDropped(const juce::StringArray& files, int, int)
{
    is_file_drag_over_ = false;
    if (event_path_.empty()) {
        repaint();
        return;
    }

    model_.begin_gesture();
    bool anyImported = false;
    for (const auto& file : files) {
        if (!is_supported_audio_extension(file)) continue;
        if (model_.import_audio_stem_layer(event_path_, fs::u8path(file.toStdString()))) {
            anyImported = true;
        }
    }
    if (anyImported) {
        model_.end_gesture();
    } else {
        model_.cancel_gesture();
    }
    repaint();
}

void LayerStackView::on_project_changed()
{
    if (is_interacting_) {
        update_live_states();
        return;
    }
    rebuild_cards();
}

void LayerStackView::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && !selection.id_or_path.empty() && selection.id_or_path != event_path_) {
        event_path_ = selection.id_or_path;
        rebuild_cards();
        return;
    }
    repaint();
    for (auto& card : stem_cards_) {
        card->repaint();
    }
}

void LayerStackView::on_parameter_changed(const std::string&, float, bool)
{
    update_live_states();
}

void LayerStackView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveform_cache_) {
        for (auto& card : stem_cards_) {
            card->repaint();
        }
    }
}

} // namespace oae::studio
