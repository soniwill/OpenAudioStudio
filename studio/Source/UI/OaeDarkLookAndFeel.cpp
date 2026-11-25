#include "OaeDarkLookAndFeel.h"
#include <algorithm>

namespace oae::studio {

namespace {
using Edges = OaeTheme::Edges;
using Icons = OaeTheme::Icons;
using Menus = OaeTheme::Menus;
using Scrollbars = OaeTheme::Scrollbars;
using Sliders = OaeTheme::Sliders;
using States = OaeTheme::States;
} // namespace

OaeDarkLookAndFeel::OaeDarkLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    setColour(juce::TextButton::buttonColourId, juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(States::selectedFillOpacity));
    setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::textPrimary));
    setColour(juce::TextButton::textColourOnId, juce::Colour(OaeTheme::Colors::accentCyan));

    setColour(juce::Slider::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurface));
    setColour(juce::Slider::thumbColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    setColour(juce::Slider::trackColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(OaeTheme::Colors::accentCyan));

    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(OaeTheme::Colors::borderActive));

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurface));
    setColour(juce::ComboBox::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(OaeTheme::Colors::borderSubtle));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(OaeTheme::Colors::textSecondary));

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    setColour(juce::PopupMenu::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(States::menuHighlightOpacity));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(OaeTheme::Colors::indicator));

    setColour(juce::TextEditor::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurface));
    setColour(juce::TextEditor::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(OaeTheme::Colors::borderSubtle));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(OaeTheme::Colors::accentCyan));

    setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    setColour(juce::TreeView::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurface));
}

void OaeDarkLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(Edges::borderStroke / 2);
    const float cornerRadius = Edges::cornerRadius;

    juce::Colour base = backgroundColour;
    if (shouldDrawButtonAsDown) {
        base = base.darker(States::pressedDarkening);
    } else if (shouldDrawButtonAsHighlighted) {
        base = base.brighter(States::hoverBrightening);
    }

    g.setColour(base);
    g.fillRoundedRectangle(bounds, cornerRadius);

    juce::Colour borderCol = juce::Colour(OaeTheme::Colors::borderSubtle);
    if (button.getToggleState()) {
        borderCol = juce::Colour(OaeTheme::Colors::accentCyan);
    } else if (shouldDrawButtonAsHighlighted) {
        borderCol = juce::Colour(OaeTheme::Colors::borderActive);
    }

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, cornerRadius, Edges::borderStroke);
}

void OaeDarkLookAndFeel::drawButtonText(juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
{
    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    juce::Colour textCol = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                    : juce::TextButton::textColourOffId);
    if (!button.isEnabled()) {
        textCol = juce::Colour(OaeTheme::Colors::textMuted);
    }

    g.setColour(textCol);
    g.drawFittedText(button.getButtonText(),
                     button.getLocalBounds().reduced(Edges::buttonTextInsetX, Edges::buttonTextInsetY),
                     juce::Justification::centred,
                     1);
}

void OaeDarkLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float minSliderPos,
                                          float maxSliderPos,
                                          juce::Slider::SliderStyle style,
                                          juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                         minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    if (bounds.isEmpty()) return;

    const auto accent = slider.findColour(juce::Slider::trackColourId);
    const auto groove = slider.findColour(juce::Slider::backgroundColourId);
    const bool horizontal = style == juce::Slider::LinearHorizontal;

    if (horizontal) {
        const bool flatBar = static_cast<bool>(slider.getProperties().getWithDefault("oaeFlatBar", false));
        const bool largeSlider = static_cast<bool>(slider.getProperties().getWithDefault("oaeLargeSlider", false));
        const float targetThickness = largeSlider ? Sliders::largeTrackThickness : Sliders::trackThickness;
        const float targetDiameter = largeSlider ? Sliders::largeThumbDiameter : Sliders::thumbDiameter;

        if (flatBar) {
            const float thickness = std::min(targetThickness, bounds.getHeight());
            const float norm = static_cast<float>(juce::jlimit(
                0.0, 1.0,
                (slider.getMaximum() > slider.getMinimum())
                    ? (slider.getValue() - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum())
                    : 0.0));
            const auto track = juce::Rectangle<float>(bounds.getX(),
                                                      bounds.getCentreY() - thickness * 0.5f,
                                                      bounds.getWidth(),
                                                      thickness);
            g.setColour(groove);
            g.fillRoundedRectangle(track, thickness * 0.5f);
            g.setColour(accent);
            g.fillRoundedRectangle(track.withWidth(track.getWidth() * norm), thickness * 0.5f);
            return;
        }

        const float thickness = std::min(targetThickness, bounds.getHeight());
        const float diameter = std::min(targetDiameter, std::max(1.0f, bounds.getHeight()));
        const float radius = diameter * 0.5f;
        const float centre = juce::jlimit(bounds.getX(), bounds.getRight(), sliderPos);
        const auto track = juce::Rectangle<float>(bounds.getX() - radius,
                                                  bounds.getCentreY() - thickness * 0.5f,
                                                  bounds.getWidth() + diameter,
                                                  thickness);
        g.setColour(groove);
        g.fillRoundedRectangle(track, thickness / 2);
        g.setColour(accent);
        g.fillRoundedRectangle(track.withRight(centre), thickness / 2);
        const auto thumbCol = largeSlider ? slider.findColour(juce::Slider::thumbColourId)
                                          : juce::Colour(OaeTheme::Colors::indicator);
        g.setColour(thumbCol);
        g.fillEllipse(juce::Rectangle<float>(diameter, diameter).withCentre({centre, bounds.getCentreY()}));
        return;
    }

    const float thickness = std::min(Sliders::trackThickness, bounds.getWidth());
    const float faderWidth = std::min(Sliders::faderWidth, bounds.getWidth());
    const float faderHeight = std::min(Sliders::faderHeight, std::max(1.0f, bounds.getHeight()));
    const float halfHandle = faderHeight * 0.5f;
    const float centre = juce::jlimit(bounds.getY(), bounds.getBottom(), sliderPos);
    const auto track = juce::Rectangle<float>(bounds.getCentreX() - thickness * 0.5f,
                                              bounds.getY() - halfHandle,
                                              thickness,
                                              bounds.getHeight() + faderHeight);
    g.setColour(groove);
    g.fillRoundedRectangle(track, thickness / 2);
    g.setColour(accent);
    g.fillRoundedRectangle(track.withTop(centre), thickness / 2);

    const auto handle = juce::Rectangle<float>(faderWidth, faderHeight).withCentre({bounds.getCentreX(), centre});
    const float radius = std::min(thickness, faderHeight) / 2;
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    g.fillRoundedRectangle(handle, radius);
    g.setColour(accent);
    g.drawRoundedRectangle(handle.reduced(Edges::borderStroke / 2), radius, Edges::borderStroke);
    if (faderWidth > 2 * Sliders::faderLineInset)
        g.drawLine(handle.getX() + Sliders::faderLineInset, centre,
                   handle.getRight() - Sliders::faderLineInset, centre, Edges::borderStroke);
}

int OaeDarkLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    if (slider.isVertical()) {
        return static_cast<int>(std::round(Sliders::faderHeight * 0.5f));
    }
    if (static_cast<bool>(slider.getProperties().getWithDefault("oaeFlatBar", false))) {
        return 0;
    }
    if (static_cast<bool>(slider.getProperties().getWithDefault("oaeLargeSlider", false))) {
        return static_cast<int>(std::round(Sliders::largeThumbDiameter * 0.5f));
    }
    return static_cast<int>(std::round(Sliders::thumbDiameter * 0.5f));
}

juce::Slider::SliderLayout OaeDarkLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    return juce::LookAndFeel_V4::getSliderLayout(slider);
}

void OaeDarkLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPosProportional,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    if (bounds.isEmpty()) return;

    const float size = std::min(bounds.getWidth(), bounds.getHeight());
    const float inset = std::min(Sliders::rotaryInset, size / 8);
    const float stroke = std::min(Sliders::arcStroke, inset);
    const float radius = size / 2 - inset;
    const auto centre = bounds.getCentre();
    const float position = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
    const float angle = juce::jmap(position, rotaryStartAngle, rotaryEndAngle);
    const juce::PathStrokeType arcStroke(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path rail;
    rail.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.strokePath(rail, arcStroke);

    if (position > 0.0f) {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(valueArc, arcStroke);
    }

    const float dialRadius = radius - std::min(Sliders::dialGap, radius / 3);
    const auto dial = bounds.withSizeKeepingCentre(2 * dialRadius, 2 * dialRadius);
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    g.fillEllipse(dial);

    const auto tip = centre.getPointOnCircumference(dialRadius * Sliders::needleFraction, angle);
    g.setColour(juce::Colour(OaeTheme::Colors::indicator));
    g.drawLine({centre, tip}, std::min(Sliders::needleStroke, dialRadius));
}

void OaeDarkLookAndFeel::drawScrollbar(juce::Graphics& g,
                                       juce::ScrollBar& scrollbar,
                                       int x, int y, int width, int height,
                                       bool isScrollbarVertical,
                                       int thumbStartPosition,
                                       int thumbSize,
                                       bool isMouseOver,
                                       bool isMouseDown)
{
    const auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    if (track.isEmpty()) return;

    g.setColour(scrollbar.findColour(juce::ScrollBar::backgroundColourId));
    g.fillRect(track);

    const float crossSize = isScrollbarVertical ? track.getWidth() : track.getHeight();
    const float inset = std::min(static_cast<float>(Scrollbars::thumbInset), crossSize / 4);
    const auto thumb = isScrollbarVertical
        ? juce::Rectangle<float>(track.getX() + inset, static_cast<float>(thumbStartPosition),
                                 crossSize - 2 * inset, static_cast<float>(thumbSize))
        : juce::Rectangle<float>(static_cast<float>(thumbStartPosition), track.getY() + inset,
                                 static_cast<float>(thumbSize), crossSize - 2 * inset);
    const auto visibleThumb = thumb.getIntersection(track);
    auto colour = scrollbar.findColour(juce::ScrollBar::thumbColourId);
    if (isMouseDown)
        colour = juce::Colour(OaeTheme::Colors::accentCyan);
    else if (isMouseOver)
        colour = colour.brighter(States::scrollbarHoverBrightening);

    g.setColour(colour);
    g.fillRoundedRectangle(visibleThumb, std::min(Scrollbars::thumbRadius, (crossSize - 2 * inset) / 2));
}

void OaeDarkLookAndFeel::drawComboBox(juce::Graphics& g,
                                      int width, int height,
                                      bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(Edges::borderStroke / 2);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, Edges::cornerRadius);

    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, Edges::cornerRadius, Edges::borderStroke);

    // Chevron arrow
    const float arrowX = static_cast<float>(width) - Icons::comboRightInset;
    const float arrowY = static_cast<float>(height) / 2 - Icons::chevronSize / 4;
    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY);
    arrow.lineTo(arrowX + Icons::chevronSize / 2, arrowY + Icons::chevronSize / 2);
    arrow.lineTo(arrowX + Icons::chevronSize, arrowY);

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(Edges::iconStroke));
}

void OaeDarkLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator,
                                           bool isActive,
                                           bool isHighlighted,
                                           bool isTicked,
                                           bool hasSubMenu,
                                           const juce::String& text,
                                           const juce::String& shortcutKeyText,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* textColour)
{
    if (isSeparator) {
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
        return;
    }

    if (isHighlighted && isActive) {
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(States::menuHighlightOpacity));
        g.fillRect(area);
        g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
        g.drawVerticalLine(area.getX(), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    juce::Colour textCol = textColour ? *textColour : juce::Colour(OaeTheme::Colors::textPrimary);
    if (!isActive) textCol = juce::Colour(OaeTheme::Colors::textMuted);

    g.setColour(textCol);
    g.setFont(juce::FontOptions(OaeTheme::Typography::control));

    auto textArea = area.reduced(Menus::textInset, 0);
    if (isTicked) {
        g.drawText(juce::String::charToString(0x2713), textArea.removeFromLeft(Menus::tickWidth), juce::Justification::centredLeft);
    }

    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty()) {
        g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
        g.drawText(shortcutKeyText, textArea, juce::Justification::centredRight, true);
    }

    if (hasSubMenu) {
        const float arrowX = static_cast<float>(area.getRight()) - Icons::menuRightInset;
        const float arrowY = static_cast<float>(area.getCentreY()) - Icons::chevronSize / 2;
        juce::Path arrow;
        arrow.startNewSubPath(arrowX, arrowY);
        arrow.lineTo(arrowX + Icons::chevronSize / 2, arrowY + Icons::chevronSize / 2);
        arrow.lineTo(arrowX, arrowY + Icons::chevronSize);
        g.setColour(textCol);
        g.strokePath(arrow, juce::PathStrokeType(Edges::iconStroke));
    }
}

void OaeDarkLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    g.fillRoundedRectangle(bounds, Edges::popupCornerRadius);
    g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
    g.drawRoundedRectangle(bounds.reduced(Edges::borderStroke / 2), Edges::popupCornerRadius, Edges::borderStroke);
}

void OaeDarkLookAndFeel::drawTreeviewPlusMinusBox(juce::Graphics& g,
                                                  const juce::Rectangle<float>& area,
                                                  juce::Colour /*backgroundColour*/,
                                                  bool isOpen,
                                                  bool /*isMouseOver*/)
{
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    juce::Path p;

    if (isOpen) {
        // Downward triangle
        p.startNewSubPath(cx - Icons::triangleWidth / 2, cy - Icons::triangleBack);
        p.lineTo(cx + Icons::triangleWidth / 2, cy - Icons::triangleBack);
        p.lineTo(cx, cy + Icons::triangleTip);
        p.closeSubPath();
    } else {
        // Rightward triangle
        p.startNewSubPath(cx - Icons::triangleBack, cy - Icons::triangleWidth / 2);
        p.lineTo(cx + Icons::triangleTip, cy);
        p.lineTo(cx - Icons::triangleBack, cy + Icons::triangleWidth / 2);
        p.closeSubPath();
    }

    g.setColour(juce::Colour(OaeTheme::Colors::textSecondary));
    g.fillPath(p);
}

juce::Font OaeDarkLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}

juce::Rectangle<int> OaeDarkLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                          juce::Point<int> screenPos,
                                                          juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    s.append(tipText, juce::Font(juce::FontOptions(OaeTheme::Typography::control)));

    juce::TextLayout tl;
    tl.createLayout(s, 400.0f);

    const int w = static_cast<int>(std::ceil(tl.getWidth())) + 16;
    const int h = static_cast<int>(std::ceil(tl.getHeight())) + 10;

    return juce::Rectangle<int>(screenPos.x, screenPos.y + 18, w, h)
        .constrainedWithin(parentArea);
}

void OaeDarkLookAndFeel::drawTooltip(juce::Graphics& g,
                                     const juce::String& text,
                                     int width,
                                     int height)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised));
    g.fillRoundedRectangle(bounds, Edges::popupCornerRadius);
    g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
    g.drawRoundedRectangle(bounds.reduced(Edges::borderStroke * 0.5f),
                           Edges::popupCornerRadius,
                           Edges::borderStroke);

    g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
    g.setFont(juce::Font(juce::FontOptions(OaeTheme::Typography::control)));
    g.drawFittedText(text,
                     juce::Rectangle<int>(0, 0, width, height).reduced(8, 4),
                     juce::Justification::centredLeft,
                     3);
}

} // namespace oae::studio
