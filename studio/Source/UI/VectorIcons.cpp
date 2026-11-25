#include "VectorIcons.h"

namespace oae::studio {

namespace {
constexpr float kRefSize = 16.0f;
} // namespace

juce::Path VectorIcons::create_fill_path(VectorIconType type)
{
    juce::Path p;
    switch (type) {
    case VectorIconType::Play:
        p.addTriangle(4.2f, 2.6f, 13.4f, 8.0f, 4.2f, 13.4f);
        break;
    case VectorIconType::Pause:
        p.addRoundedRectangle(3.6f, 2.8f, 3.2f, 10.4f, 0.8f);
        p.addRoundedRectangle(9.2f, 2.8f, 3.2f, 10.4f, 0.8f);
        break;
    case VectorIconType::Stop:
        p.addRoundedRectangle(3.6f, 3.6f, 8.8f, 8.8f, 1.0f);
        break;
    case VectorIconType::Loop: {
        // Top-right arrowhead
        p.addTriangle(11.4f, 2.0f, 14.0f, 4.6f, 11.4f, 6.8f);
        // Bottom-left arrowhead
        p.addTriangle(4.6f, 9.2f, 2.0f, 11.4f, 4.6f, 14.0f);
        break;
    }
    case VectorIconType::KeyOff:
        p.addRoundedRectangle(3.0f, 2.5f, 10.0f, 2.0f, 0.6f);
        break;
    case VectorIconType::Help:
        p.addEllipse(7.25f, 11.3f, 1.5f, 1.5f);
        break;
    case VectorIconType::DockBack:
        p.addTriangle(8.0f, 3.0f, 4.8f, 6.6f, 11.2f, 6.6f);
        break;
    case VectorIconType::MoreHorizontal:
        p.addEllipse(2.6f, 6.8f, 2.4f, 2.4f);
        p.addEllipse(6.8f, 6.8f, 2.4f, 2.4f);
        p.addEllipse(11.0f, 6.8f, 2.4f, 2.4f);
        break;
    case VectorIconType::LockClosed:
    case VectorIconType::LockOpen:
        p.addRoundedRectangle(3.4f, 7.2f, 9.2f, 6.8f, 1.4f);
        break;
    case VectorIconType::NewProject:
    case VectorIconType::OpenProject:
    case VectorIconType::SaveProject:
    case VectorIconType::BuildBank:
    case VectorIconType::ChevronRight:
    case VectorIconType::ChevronDown:
        break;
    }
    return p;
}

juce::Path VectorIcons::create_stroke_path(VectorIconType type)
{
    juce::Path p;
    switch (type) {
    case VectorIconType::Loop:
        p.startNewSubPath(3.2f, 7.2f);
        p.lineTo(3.2f, 5.4f);
        p.quadraticTo(3.2f, 4.4f, 4.4f, 4.4f);
        p.lineTo(11.6f, 4.4f);

        p.startNewSubPath(12.8f, 8.8f);
        p.lineTo(12.8f, 10.6f);
        p.quadraticTo(12.8f, 11.6f, 11.6f, 11.6f);
        p.lineTo(4.4f, 11.6f);
        break;

    case VectorIconType::KeyOff:
        p.startNewSubPath(8.0f, 13.5f);
        p.lineTo(8.0f, 6.2f);
        p.startNewSubPath(5.0f, 8.8f);
        p.lineTo(8.0f, 5.8f);
        p.lineTo(11.0f, 8.8f);
        break;

    case VectorIconType::NewProject:
        p.startNewSubPath(3.5f, 2.2f);
        p.lineTo(9.8f, 2.2f);
        p.lineTo(12.5f, 4.9f);
        p.lineTo(12.5f, 13.8f);
        p.lineTo(3.5f, 13.8f);
        p.closeSubPath();
        p.startNewSubPath(9.6f, 2.2f);
        p.lineTo(9.6f, 5.1f);
        p.lineTo(12.5f, 5.1f);
        p.startNewSubPath(8.0f, 7.2f);
        p.lineTo(8.0f, 11.6f);
        p.startNewSubPath(5.8f, 9.4f);
        p.lineTo(10.2f, 9.4f);
        break;

    case VectorIconType::OpenProject:
        p.startNewSubPath(2.2f, 4.0f);
        p.lineTo(6.2f, 4.0f);
        p.lineTo(7.6f, 5.5f);
        p.lineTo(13.8f, 5.5f);
        p.lineTo(13.8f, 12.8f);
        p.lineTo(2.2f, 12.8f);
        p.closeSubPath();
        p.startNewSubPath(2.2f, 7.2f);
        p.lineTo(13.8f, 7.2f);
        break;

    case VectorIconType::SaveProject:
        p.startNewSubPath(2.6f, 2.6f);
        p.lineTo(11.4f, 2.6f);
        p.lineTo(13.4f, 4.6f);
        p.lineTo(13.4f, 13.4f);
        p.lineTo(2.6f, 13.4f);
        p.closeSubPath();
        p.addRectangle(5.0f, 2.6f, 5.2f, 3.6f);
        p.addRectangle(4.8f, 8.8f, 6.4f, 4.6f);
        break;

    case VectorIconType::BuildBank:
        // Stacked bank/package glyph with build accent
        p.addRoundedRectangle(2.6f, 5.6f, 10.8f, 7.8f, 1.2f);
        p.startNewSubPath(2.6f, 8.4f);
        p.lineTo(13.4f, 8.4f);
        p.startNewSubPath(8.0f, 2.2f);
        p.lineTo(8.0f, 5.6f);
        p.startNewSubPath(5.8f, 3.8f);
        p.lineTo(8.0f, 5.8f);
        p.lineTo(10.2f, 3.8f);
        break;

    case VectorIconType::Help:
        p.addEllipse(2.4f, 2.4f, 11.2f, 11.2f);
        p.startNewSubPath(6.1f, 6.1f);
        p.cubicTo(6.1f, 4.6f, 9.9f, 4.6f, 9.9f, 6.3f);
        p.cubicTo(9.9f, 7.6f, 8.0f, 7.7f, 8.0f, 9.6f);
        break;

    case VectorIconType::DockBack:
        p.addRoundedRectangle(2.6f, 2.6f, 10.8f, 10.8f, 1.2f);
        p.startNewSubPath(8.0f, 11.2f);
        p.lineTo(8.0f, 6.2f);
        break;

    case VectorIconType::ChevronRight:
        p.startNewSubPath(6.0f, 4.0f);
        p.lineTo(10.0f, 8.0f);
        p.lineTo(6.0f, 12.0f);
        break;

    case VectorIconType::ChevronDown:
        p.startNewSubPath(4.0f, 6.0f);
        p.lineTo(8.0f, 10.0f);
        p.lineTo(12.0f, 6.0f);
        break;

    case VectorIconType::LockClosed:
        p.startNewSubPath(5.4f, 7.2f);
        p.lineTo(5.4f, 5.0f);
        p.quadraticTo(5.4f, 2.8f, 8.0f, 2.8f);
        p.quadraticTo(10.6f, 2.8f, 10.6f, 5.0f);
        p.lineTo(10.6f, 7.2f);
        break;

    case VectorIconType::LockOpen:
        p.startNewSubPath(5.4f, 7.2f);
        p.lineTo(5.4f, 4.2f);
        p.quadraticTo(5.4f, 2.0f, 8.0f, 2.0f);
        p.quadraticTo(10.6f, 2.0f, 10.6f, 4.2f);
        p.lineTo(10.6f, 5.2f);
        break;

    case VectorIconType::MoreHorizontal:
    case VectorIconType::Play:
    case VectorIconType::Pause:
    case VectorIconType::Stop:
        break;
    }
    return p;
}

void VectorIcons::draw_icon(juce::Graphics& g,
                            VectorIconType type,
                            juce::Rectangle<float> bounds,
                            juce::Colour colour)
{
    if (bounds.isEmpty()) {
        return;
    }

    const float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto target = juce::Rectangle<float>(side, side).withCentre(bounds.getCentre());
    const auto transform = juce::AffineTransform::scale(side / kRefSize, side / kRefSize)
                               .translated(target.getX(), target.getY());

    g.setColour(colour);

    auto fillPath = create_fill_path(type);
    if (!fillPath.isEmpty()) {
        fillPath.applyTransform(transform);
        g.fillPath(fillPath);
    }

    auto strokePath = create_stroke_path(type);
    if (!strokePath.isEmpty()) {
        strokePath.applyTransform(transform);
        const float strokeWidth = juce::jmax(1.2f, OaeTheme::Edges::iconStroke * (side / 16.0f));
        g.strokePath(strokePath,
                     juce::PathStrokeType(strokeWidth,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
}

VectorIconButton::VectorIconButton(const juce::String& name,
                                   VectorIconType iconType,
                                   juce::Colour activeBackground)
    : juce::Button(name)
    , iconType_(iconType)
    , activeBackground_(activeBackground)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void VectorIconButton::set_button_style(Style style)
{
    if (style_ != style) {
        style_ = style;
        repaint();
    }
}

void VectorIconButton::set_always_tinted(bool alwaysTinted)
{
    if (alwaysTinted_ != alwaysTinted) {
        alwaysTinted_ = alwaysTinted;
        repaint();
    }
}

void VectorIconButton::set_active_background(juce::Colour colour)
{
    if (activeBackground_ != colour) {
        activeBackground_ = colour;
        repaint();
    }
}

void VectorIconButton::set_icon_type(VectorIconType type)
{
    if (iconType_ != type) {
        iconType_ = type;
        repaint();
    }
}

void VectorIconButton::set_semantic_active(bool active)
{
    if (semanticActive_ != active) {
        semanticActive_ = active;
        repaint();
    }
}

void VectorIconButton::paintButton(juce::Graphics& g,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
    const bool isActive = semanticActive_ || getToggleState();

    if (style_ == Style::OutlineTint) {
        juce::Colour bg = isActive
                              ? activeBackground_.withAlpha(0.24f)
                              : (alwaysTinted_ ? activeBackground_.withAlpha(0.12f)
                                               : juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
        if (shouldDrawButtonAsDown) {
            bg = bg.darker(OaeTheme::States::pressedDarkening);
        } else if (shouldDrawButtonAsHighlighted) {
            bg = bg.brighter(OaeTheme::States::hoverBrightening);
        }

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);

        juce::Colour borderCol = isActive
                                     ? activeBackground_
                                     : (alwaysTinted_
                                            ? activeBackground_.withAlpha(shouldDrawButtonAsHighlighted ? 0.85f : 0.55f)
                                            : juce::Colour(shouldDrawButtonAsHighlighted ? OaeTheme::Colors::borderActive
                                                                                         : OaeTheme::Colors::borderSubtle));
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

        juce::Colour iconCol = isActive
                                   ? activeBackground_
                                   : (alwaysTinted_
                                          ? activeBackground_.withAlpha(0.92f)
                                          : juce::Colour(shouldDrawButtonAsHighlighted ? OaeTheme::Colors::textPrimary
                                                                                       : OaeTheme::Colors::textSecondary));
        if (!isEnabled()) {
            iconCol = juce::Colour(OaeTheme::Colors::textMuted);
        }

        const auto iconBounds = bounds.reduced(juce::jmin(6.0f, bounds.getWidth() * 0.25f),
                                               juce::jmin(5.0f, bounds.getHeight() * 0.25f));
        VectorIcons::draw_icon(g, iconType_, iconBounds, iconCol);
        return;
    }

    juce::Colour bg = isActive
                          ? activeBackground_
                          : juce::Colour(OaeTheme::Colors::bgSurfaceRaised);
    if (shouldDrawButtonAsDown) {
        bg = bg.darker(OaeTheme::States::pressedDarkening);
    } else if (shouldDrawButtonAsHighlighted) {
        bg = bg.brighter(OaeTheme::States::hoverBrightening);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);

    juce::Colour borderCol = juce::Colour(OaeTheme::Colors::borderSubtle);
    if (isActive) {
        borderCol = activeBackground_.brighter(0.2f);
    } else if (shouldDrawButtonAsHighlighted) {
        borderCol = juce::Colour(OaeTheme::Colors::borderActive);
    }
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

    juce::Colour iconCol = isActive
                               ? juce::Colour(OaeTheme::Colors::clipTitleTextDark)
                               : juce::Colour(OaeTheme::Colors::textPrimary);
    if (!isEnabled()) {
        iconCol = juce::Colour(OaeTheme::Colors::textMuted);
    }

    const auto iconBounds = bounds.reduced(juce::jmin(6.0f, bounds.getWidth() * 0.22f),
                                           juce::jmin(5.0f, bounds.getHeight() * 0.22f));
    VectorIcons::draw_icon(g, iconType_, iconBounds, iconCol);
}

} // namespace oae::studio
