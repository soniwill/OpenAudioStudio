#include "Spatializer3DDeck.h"
#include "Theme.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace oae::studio {

Spatializer3DDeck::Spatializer3DDeck(AuditionTransport& transport)
    : transport_(transport)
{
    transport_.add_listener(this);
    addAndMakeVisible(headerLabel_);
    headerLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    headerLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    addAndMakeVisible(coordsLabel_);
    coordsLabel_.setFont(juce::FontOptions(11.0f));

    auto setup_distance = [this](juce::Slider& slider, juce::Label& label,
                                 const char* id, double low, double high, double initial) {
        addAndMakeVisible(label);
        label.setFont(juce::FontOptions(10.0f));
        label.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
        addAndMakeVisible(slider);
        slider.setComponentID(id);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
        slider.setRange(low, high, 0.1);
        slider.setValue(initial, juce::dontSendNotification);
    };
    setup_distance(minDistanceSlider_, minDistanceLabel_, "spatial_preview_min_distance", 0.1, 50.0, 1.0);
    setup_distance(maxDistanceSlider_, maxDistanceLabel_, "spatial_preview_max_distance", 1.0, 200.0, 20.0);
    setup_distance(soundSizeSlider_, soundSizeLabel_, "spatial_preview_sound_size", 0.0, 50.0, 0.0);
    setup_distance(minExtentSlider_, minExtentLabel_, "spatial_preview_min_extent", 0.0, 360.0, 0.0);
    minExtentSlider_.setRange(0.0, 360.0, 1.0);
    minDistanceSlider_.onValueChange = [this]() {
        if (minDistanceSlider_.getValue() >= maxDistanceSlider_.getValue()) {
            maxDistanceSlider_.setValue(minDistanceSlider_.getValue() + 0.1, juce::dontSendNotification);
        }
        update_preview();
    };
    maxDistanceSlider_.onValueChange = [this]() {
        if (maxDistanceSlider_.getValue() <= minDistanceSlider_.getValue()) {
            minDistanceSlider_.setValue(maxDistanceSlider_.getValue() - 0.1, juce::dontSendNotification);
        }
        update_preview();
    };
    soundSizeSlider_.onValueChange = [this]() { update_preview(); };
    minExtentSlider_.onValueChange = [this]() { update_preview(); };

    addAndMakeVisible(rolloffLabel_);
    rolloffLabel_.setFont(juce::FontOptions(10.0f));
    rolloffLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    addAndMakeVisible(rolloffCombo_);
    rolloffCombo_.setComponentID("spatial_preview_rolloff");
    rolloffCombo_.addItem("Linear", 1);
    rolloffCombo_.addItem("Inverse", 2);
    rolloffCombo_.addItem("Quadratic", 3);
    rolloffCombo_.setSelectedId(1, juce::dontSendNotification);
    rolloffCombo_.onChange = [this]() { update_preview(); };
    update_preview();
}

Spatializer3DDeck::~Spatializer3DDeck()
{
    transport_.remove_listener(this);
    restore_instance();
}

void Spatializer3DDeck::visibilityChanged()
{
    if (!isVisible()) is_dragging_emitter_ = false;
    update_preview();
}

void Spatializer3DDeck::on_transport_state_changed(TransportState)
{
    update_preview();
}

/** Evaluates the output gain shared by the audition and the displayed curve. */
float Spatializer3DDeck::gain_at_distance(float distance) const
{
    const auto minimum = static_cast<float>(minDistanceSlider_.getValue());
    const auto maximum = static_cast<float>(maxDistanceSlider_.getValue());
    if (distance <= minimum) return 1.0f;
    if (distance >= maximum) return 0.0f;
    const float linear = (maximum - distance) / (maximum - minimum);
    switch (rolloffCombo_.getSelectedId()) {
        case 2: return linear * minimum / distance;
        case 3: return linear * linear;
        default: return linear;
    }
}

/** Releases the preview override on the same audition instance that received it. */
void Spatializer3DDeck::restore_instance()
{
    if (preview_instance_ != 0 && preview_instance_ == transport_.get_active_instance()) {
        oae_spatializer_settings off{};
        off.enabled = 0;
        off.rolloff = OAE_ROLLOFF_LINEAR;
        off.min_distance = 1.0f;
        off.max_distance = 20.0f;
        off.cone_inner_angle_deg = 360.0f;
        off.cone_outer_angle_deg = 360.0f;
        off.cone_outer_gain = 1.0f;
        oae_event_instance_set_spatializer_settings(preview_instance_, &off);
        oae_event_instance_set_volume(preview_instance_, instance_volume_);
        oae_event_instance_set_pan(preview_instance_, instance_pan_);
    }
    preview_instance_ = 0;
}

/** Applies the radar position and 3D spatializer settings only to the current local audition. */
void Spatializer3DDeck::update_preview()
{
    const float distance = std::sqrt(emitter_x_ * emitter_x_ + emitter_y_ * emitter_y_ + emitter_z_ * emitter_z_);
    const float gain = gain_at_distance(distance);
    coordsLabel_.setText(juce::String::formatted("Emitter X %+.1f, Y %+.1f, Z %+.1f m  |  %.1f m  |  %.0f%% gain",
                                                 emitter_x_, emitter_y_, emitter_z_, distance, gain * 100.0f),
                         juce::dontSendNotification);
    repaint();

    const auto instance = transport_.get_active_instance();
    if (!isVisible() || instance == 0 || transport_.is_remote_audition_active()) {
        restore_instance();
        return;
    }

    if (instance != preview_instance_) {
        preview_instance_ = 0;
        if (oae_event_instance_get_volume(instance, &instance_volume_) != OAE_OK ||
            oae_event_instance_get_pan(instance, &instance_pan_) != OAE_OK) return;
        preview_instance_ = instance;
    }

    oae_3d_attributes attrs{};
    attrs.position = { emitter_x_, emitter_y_, emitter_z_ };
    attrs.velocity = { 0.0f, 0.0f, 0.0f };
    attrs.forward = { 0.0f, 0.0f, -1.0f };
    attrs.up = { 0.0f, 1.0f, 0.0f };
    oae_event_instance_set_3d_attributes(instance, &attrs);

    oae_spatializer_settings settings{};
    settings.enabled = 1;
    switch (rolloffCombo_.getSelectedId()) {
        case 2: settings.rolloff = OAE_ROLLOFF_INVERSE; break;
        case 3: settings.rolloff = OAE_ROLLOFF_LINEAR_SQUARED; break;
        default: settings.rolloff = OAE_ROLLOFF_LINEAR; break;
    }
    settings.min_distance = static_cast<float>(minDistanceSlider_.getValue());
    settings.max_distance = static_cast<float>(maxDistanceSlider_.getValue());
    settings.sound_size = static_cast<float>(soundSizeSlider_.getValue());
    settings.min_extent_deg = static_cast<float>(minExtentSlider_.getValue());
    settings.cone_inner_angle_deg = 360.0f;
    settings.cone_outer_angle_deg = 360.0f;
    settings.cone_outer_gain = 1.0f;
    oae_event_instance_set_spatializer_settings(instance, &settings);
}

void Spatializer3DDeck::mouseDown(const juce::MouseEvent& e)
{
    const float radius = radar_area_.getWidth() * 0.5f;
    is_dragging_emitter_ = e.mods.isLeftButtonDown() && radius > 0.0f &&
        e.position.getDistanceFrom(radar_area_.getCentre()) <= radius;
    mouseDrag(e);
}

void Spatializer3DDeck::mouseDrag(const juce::MouseEvent& e)
{
    const float radius = radar_area_.getWidth() * 0.5f;
    if (!is_dragging_emitter_ || radius <= 0.0f) return;
    const auto delta = e.position - radar_area_.getCentre();
    const float maximum = static_cast<float>(maxDistanceSlider_.getValue());
    if (e.mods.isCtrlDown()) {
        emitter_y_ = std::clamp((-delta.y / radius) * maximum, -maximum, maximum);
    } else {
        const float scale = maximum / std::max(radius, delta.getDistanceFromOrigin());
        emitter_x_ = delta.x * scale;
        emitter_z_ = -delta.y * scale;
    }
    update_preview();
}

void Spatializer3DDeck::mouseUp(const juce::MouseEvent&)
{
    is_dragging_emitter_ = false;
}

void Spatializer3DDeck::mouseDoubleClick(const juce::MouseEvent& e)
{
    const float radius = radar_area_.getWidth() * 0.5f;
    if (radius > 0.0f && e.position.getDistanceFrom(radar_area_.getCentre()) <= radius) {
        emitter_x_ = 0.0f;
        emitter_y_ = 0.0f;
        emitter_z_ = 0.0f;
        update_preview();
    }
}

void Spatializer3DDeck::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (radar_area_.contains(e.position) && std::abs(wheel.deltaY) > 1.0e-5f) {
        const float maximum = static_cast<float>(maxDistanceSlider_.getValue());
        if (e.mods.isCtrlDown()) {
            emitter_y_ = std::clamp(emitter_y_ + wheel.deltaY * 2.0f, -maximum, maximum);
        } else {
            const float angle = wheel.deltaY * 0.25f;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float nx = emitter_x_ * c - emitter_z_ * s;
            const float nz = emitter_x_ * s + emitter_z_ * c;
            emitter_x_ = nx;
            emitter_z_ = nz;
        }
        update_preview();
    }
}

void Spatializer3DDeck::draw_radar(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto center = area.getCentre();
    const float radius = area.getWidth() * 0.5f;
    const auto maximum = static_cast<float>(maxDistanceSlider_.getValue());
    const juce::Point<float> offset(emitter_x_, -emitter_z_);
    const auto emitter = center + offset * (radius / std::max(maximum, offset.getDistanceFromOrigin()));

    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillEllipse(area);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawEllipse(area, 1.0f);
    for (float fraction : { 0.25f, 0.50f, 0.75f }) {
        g.drawEllipse(area.withSizeKeepingCentre(area.getWidth() * fraction, area.getHeight() * fraction), 1.0f);
    }
    g.drawHorizontalLine(static_cast<int>(center.y), area.getX(), area.getRight());
    g.drawVerticalLine(static_cast<int>(center.x), area.getY(), area.getBottom());
    g.setColour(juce::Colour(OaeTheme::Colors::indicator));
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(center));
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
    g.drawLine({ center, emitter }, 1.5f);
    const float sound_size = static_cast<float>(soundSizeSlider_.getValue());
    if (sound_size > 0.05f && maximum > 0.0f) {
        const float size_px = std::clamp((sound_size / maximum) * radius * 2.0f, 10.0f, area.getWidth());
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(0.22f));
        g.fillEllipse(juce::Rectangle<float>(size_px, size_px).withCentre(emitter));
    }
    const float elev_px = (maximum > 0.0f) ? (emitter_y_ / maximum) * (radius * 0.25f) : 0.0f;
    const auto elev_point = emitter.translated(0.0f, -elev_px);
    if (std::abs(elev_px) > 1.0f) {
        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber).withAlpha(0.75f));
        g.drawLine({ emitter, elev_point }, 1.2f);
    }
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
    g.fillEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre(elev_point));
}

void Spatializer3DDeck::draw_attenuation_graph(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
    const auto plot = area.reduced(6.0f);
    const auto minimum = static_cast<float>(minDistanceSlider_.getValue());
    const auto maximum = static_cast<float>(maxDistanceSlider_.getValue());
    const float min_x = plot.getX() + minimum / maximum * plot.getWidth();
    g.setColour(juce::Colour(OaeTheme::Colors::accentGreen));
    g.drawVerticalLine(static_cast<int>(min_x), plot.getY(), plot.getBottom());

    juce::Path curve;
    curve.startNewSubPath(plot.getX(), plot.getY());
    constexpr int steps = 80;
    constexpr float floor_db = -60.0f;
    for (int i = 1; i <= steps; ++i) {
        const float fraction = static_cast<float>(i) / steps;
        const float gain = gain_at_distance(fraction * maximum);
        const float db = gain > 0.0f ? std::max(floor_db, 20.0f * std::log10(gain)) : floor_db;
        curve.lineTo(plot.getX() + fraction * plot.getWidth(),
                     plot.getY() + db / floor_db * plot.getHeight());
    }
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void Spatializer3DDeck::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    if (!radar_area_.isEmpty()) draw_radar(g, radar_area_);
    if (graph_area_.getWidth() > 12.0f && graph_area_.getHeight() > 12.0f) draw_attenuation_graph(g, graph_area_);
}

void Spatializer3DDeck::resized()
{
    const auto bounds = getLocalBounds().reduced(OaeTheme::Spacing::small).toFloat();

    juce::FlexBox headerBox;
    headerBox.flexDirection = juce::FlexBox::Direction::row;
    headerBox.alignItems = juce::FlexBox::AlignItems::center;
    headerBox.items.add(juce::FlexItem(headerLabel_).withFlex(0.5f).withHeight(20.0f));
    headerBox.items.add(juce::FlexItem(coordsLabel_).withFlex(0.5f).withHeight(20.0f));

    juce::FlexBox rootBox;
    rootBox.flexDirection = juce::FlexBox::Direction::column;
    rootBox.items.add(juce::FlexItem(headerBox).withFlex(0.0f, 0.0f, 20.0f).withMargin({ 0.0f, 0.0f, 4.0f, 0.0f }));
    rootBox.items.add(juce::FlexItem().withFlex(1.0f));
    rootBox.performLayout(bounds);

    juce::FlexBox mainBox;
    mainBox.flexDirection = juce::FlexBox::Direction::row;
    mainBox.items.add(juce::FlexItem().withFlex(1.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
    mainBox.items.add(juce::FlexItem().withFlex(0.35f, 1.0f, 260.0f).withMaxWidth(260.0f));
    mainBox.performLayout(rootBox.items[1].currentBounds);

    juce::FlexBox visualsBox;
    visualsBox.flexDirection = juce::FlexBox::Direction::row;
    visualsBox.items.add(juce::FlexItem().withFlex(1.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
    visualsBox.items.add(juce::FlexItem().withFlex(1.0f));
    visualsBox.performLayout(mainBox.items[0].currentBounds);

    const auto radarBounds = visualsBox.items[0].currentBounds;
    const float radarSide = std::clamp(std::min(radarBounds.getWidth(), radarBounds.getHeight()), 0.0f, 180.0f);
    radar_area_ = juce::Rectangle<float>(radarBounds.getX(), radarBounds.getY(), radarSide, radarSide);

    const auto graphBounds = visualsBox.items[1].currentBounds;
    graph_area_ = juce::Rectangle<float>(graphBounds.getX(), graphBounds.getY(),
                                         graphBounds.getWidth(), std::min(180.0f, graphBounds.getHeight()));

    const auto controlsBounds = mainBox.items[1].currentBounds;
    const float rowH = std::clamp(controlsBounds.getHeight() / 5.0f, 18.0f, 24.0f);
    juce::Grid controlsGrid;
    controlsGrid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
    controlsGrid.columnGap = juce::Grid::Px(4);
    const std::array<std::pair<juce::Label*, juce::Component*>, 5> rows = {{
        { &minDistanceLabel_, &minDistanceSlider_ },
        { &maxDistanceLabel_, &maxDistanceSlider_ },
        { &rolloffLabel_, &rolloffCombo_ },
        { &soundSizeLabel_, &soundSizeSlider_ },
        { &minExtentLabel_, &minExtentSlider_ }
    }};
    for (int r = 0; r < 5; ++r) {
        controlsGrid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Px(rowH)));
        controlsGrid.items.add(juce::GridItem(*rows[static_cast<size_t>(r)].first).withArea(r + 1, 1));
        controlsGrid.items.add(juce::GridItem(*rows[static_cast<size_t>(r)].second).withArea(r + 1, 2));
    }
    controlsGrid.performLayout(controlsBounds.toNearestInt());
}

std::vector<TestableTarget> Spatializer3DDeck::get_testable_targets() const
{
    const auto bounds = getLocalBounds();
    const auto radarInt = radar_area_.toNearestInt().getIntersection(bounds);
    if (radarInt.getWidth() <= 4 || radarInt.getHeight() <= 4) return {};
    const auto center = radar_area_.getCentre();
    const float radius = radar_area_.getWidth() * 0.5f;
    const auto maximum = static_cast<float>(maxDistanceSlider_.getValue());
    const juce::Point<float> offset(emitter_x_, -emitter_z_);
    const auto emitter = center + offset * (radius / std::max(maximum, offset.getDistanceFromOrigin()));
    const auto centerRect = juce::Rectangle<int>(
        static_cast<int>(std::round(center.x)) - 10,
        static_cast<int>(std::round(center.y)) - 10,
        20, 20).getIntersection(radarInt);
    const auto emitterRect = juce::Rectangle<int>(
        static_cast<int>(std::round(emitter.x)) - 8,
        static_cast<int>(std::round(emitter.y)) - 8,
        16, 16).getIntersection(radarInt);
    std::vector<TestableTarget> out;
    if (!centerRect.isEmpty()) out.push_back({ "radar_center", "double_click", centerRect, isEnabled() });
    if (!emitterRect.isEmpty()) out.push_back({ "radar_emitter", "drag", emitterRect, isEnabled() });
    out.push_back({ "radar_area", "wheel", radarInt, isEnabled() });
    return out;
}

} // namespace oae::studio
