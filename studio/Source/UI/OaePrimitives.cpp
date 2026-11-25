#include "OaePrimitives.h"

#include <algorithm>
#include <cmath>

namespace oae::studio {

OaeToolbarButton::OaeToolbarButton(const juce::String& text, Variant variant)
    : juce::TextButton(text), variant_(variant)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void OaeToolbarButton::setVariant(Variant variant)
{
    if (variant_ == variant) return;
    variant_ = variant;
    repaint();
}

int OaeToolbarButton::preferredWidth(int minWidth, int horizontalPadding) const
{
    const auto font = juce::Font(juce::FontOptions(OaeTheme::Typography::compact, juce::Font::bold));
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, getButtonText(), 0.0f, 0.0f);
    const int textW = static_cast<int>(std::ceil(glyphs.getBoundingBox(0, -1, false).getWidth()));
    return std::max(minWidth, textW + horizontalPadding);
}

void OaeToolbarButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
    const float radius = OaeTheme::Edges::cornerRadius;

    juce::Colour fill(OaeTheme::Colors::bgSurfaceRaised);
    juce::Colour border(OaeTheme::Colors::borderSubtle);
    juce::Colour textCol(OaeTheme::Colors::textPrimary);

    switch (variant_) {
        case Variant::Neutral:
            if (getToggleState()) {
                fill = juce::Colour(OaeTheme::Colors::tintCyan);
                border = juce::Colour(OaeTheme::Colors::accentCyan);
                textCol = juce::Colour(OaeTheme::Colors::accentCyan);
            } else if (isMouseOverButton) {
                border = juce::Colour(OaeTheme::Colors::borderActive);
            }
            break;
        case Variant::AccentCyan:
            fill = juce::Colour(OaeTheme::Colors::bgSurfaceRaised);
            border = juce::Colour(OaeTheme::Colors::accentCyan);
            textCol = juce::Colour(OaeTheme::Colors::accentCyan);
            break;
        case Variant::TintCyan:
            fill = juce::Colour(OaeTheme::Colors::tintCyan);
            border = juce::Colour(OaeTheme::Colors::accentCyan);
            textCol = juce::Colour(OaeTheme::Colors::accentCyan);
            break;
        case Variant::AccentPurple:
            fill = juce::Colour(OaeTheme::Colors::tintPurple);
            border = juce::Colour(OaeTheme::Colors::accentPurple);
            textCol = juce::Colour(OaeTheme::Colors::accentPurple);
            break;
    }

    if (isButtonDown) {
        fill = fill.darker(OaeTheme::States::pressedDarkening);
    } else if (isMouseOverButton) {
        fill = fill.brighter(OaeTheme::States::hoverBrightening);
    }

    if (!isEnabled()) {
        fill = juce::Colour(OaeTheme::Colors::bgSurface);
        border = juce::Colour(OaeTheme::Colors::borderSubtle);
        textCol = juce::Colour(OaeTheme::Colors::textMuted);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, radius, OaeTheme::Edges::borderStroke);

    g.setColour(textCol);
    g.setFont(juce::FontOptions(OaeTheme::Typography::compact,
                                variant_ == Variant::Neutral ? juce::Font::plain : juce::Font::bold));
    g.drawFittedText(getButtonText(),
                     getLocalBounds().reduced(OaeTheme::Edges::buttonTextInsetX, 0),
                     juce::Justification::centred,
                     1);
}

OaeStatusBadge::OaeStatusBadge(const juce::String& text, Variant variant, bool showDot)
    : juce::TextButton(text), variant_(variant), showDot_(showDot)
{
    setInterceptsMouseClicks(false, false);
}

void OaeStatusBadge::setBadgeStyle(Variant variant, bool showDot)
{
    if (variant_ == variant && showDot_ == showDot) return;
    variant_ = variant;
    showDot_ = showDot;
    repaint();
}

void OaeStatusBadge::setInteractive(bool interactive)
{
    interactive_ = interactive;
    setInterceptsMouseClicks(interactive_, false);
    setMouseCursor(interactive_ ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

int OaeStatusBadge::preferredWidth(int minWidth) const
{
    const auto font = juce::Font(juce::FontOptions(OaeTheme::Typography::micro, juce::Font::bold));
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, getButtonText(), 0.0f, 0.0f);
    const int textW = static_cast<int>(std::ceil(glyphs.getBoundingBox(0, -1, false).getWidth()));
    return std::max(minWidth, textW + (showDot_ ? 22 : 14));
}

void OaeStatusBadge::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
    const float radius = std::min(OaeTheme::Edges::pillCornerRadius, bounds.getHeight() * 0.5f);

    juce::Colour fill(OaeTheme::Colors::bgSurfaceRaised);
    juce::Colour border(OaeTheme::Colors::borderSubtle);
    juce::Colour textCol(OaeTheme::Colors::textSecondary);

    switch (variant_) {
        case Variant::Cyan:
            fill = juce::Colour(OaeTheme::Colors::tintCyan);
            border = juce::Colour(OaeTheme::Colors::accentCyan);
            textCol = juce::Colour(OaeTheme::Colors::accentCyan);
            break;
        case Variant::Green:
            fill = juce::Colour(OaeTheme::Colors::tintGreen);
            border = juce::Colour(OaeTheme::Colors::accentGreen);
            textCol = juce::Colour(OaeTheme::Colors::accentGreen);
            break;
        case Variant::Amber:
            fill = juce::Colour(OaeTheme::Colors::tintAmber);
            border = juce::Colour(OaeTheme::Colors::accentAmber);
            textCol = juce::Colour(OaeTheme::Colors::accentAmber);
            break;
        case Variant::Purple:
            fill = juce::Colour(OaeTheme::Colors::tintPurple);
            border = juce::Colour(OaeTheme::Colors::accentPurple);
            textCol = juce::Colour(OaeTheme::Colors::accentPurple);
            break;
        case Variant::Neutral:
            break;
    }

    if (interactive_) {
        if (isButtonDown) fill = fill.darker(OaeTheme::States::pressedDarkening);
        else if (isMouseOverButton) fill = fill.brighter(OaeTheme::States::hoverBrightening);
    }

    if (!isEnabled()) {
        fill = juce::Colour(OaeTheme::Colors::bgSurface);
        border = juce::Colour(OaeTheme::Colors::borderSubtle);
        textCol = juce::Colour(OaeTheme::Colors::textMuted);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, radius, OaeTheme::Edges::borderStroke);

    auto content = getLocalBounds().reduced(6, 0);
    g.setColour(textCol);
    if (showDot_) {
        const float dotSize = 4.5f;
        const float dotY = content.toFloat().getCentreY() - dotSize * 0.5f;
        g.fillEllipse(static_cast<float>(content.getX()), dotY, dotSize, dotSize);
        content.removeFromLeft(8);
    }

    g.setFont(juce::FontOptions(OaeTheme::Typography::micro, juce::Font::bold));
    g.drawFittedText(getButtonText(), content, juce::Justification::centred, 1);
}

OaeSegmentedControl::OaeSegmentedControl() = default;

void OaeSegmentedControl::addOption(const juce::String& text,
                                    const juce::String& componentId,
                                    juce::Colour activeAccent,
                                    juce::Colour activeTint)
{
    const int idx = static_cast<int>(options_.size());
    auto btn = std::make_unique<juce::TextButton>(text);
    if (componentId.isNotEmpty()) {
        btn->setComponentID(componentId);
    }
    btn->setClickingTogglesState(false);
    btn->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    btn->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    btn->setColour(juce::TextButton::textColourOffId, juce::Colour(OaeTheme::Colors::textSecondary));
    btn->setColour(juce::TextButton::textColourOnId, activeAccent);
    btn->onClick = [this, idx]() {
        setSelectedIndex(idx, juce::sendNotification);
    };

    addAndMakeVisible(*btn);
    options_.push_back(std::move(btn));
    optionStyles_.push_back({ activeAccent, activeTint });
    setSelectedIndex(selectedIndex_, juce::dontSendNotification);
    resized();
}

void OaeSegmentedControl::clearOptions()
{
    options_.clear();
    optionStyles_.clear();
    selectedIndex_ = 0;
    repaint();
}

void OaeSegmentedControl::setSelectedIndex(int index, juce::NotificationType notification)
{
    if (options_.empty()) {
        selectedIndex_ = 0;
        return;
    }

    const int clamped = juce::jlimit(0, static_cast<int>(options_.size()) - 1, index);
    selectedIndex_ = clamped;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        const bool active = (i == selectedIndex_);
        options_[static_cast<size_t>(i)]->setToggleState(active, juce::dontSendNotification);
        options_[static_cast<size_t>(i)]->setColour(
            juce::TextButton::textColourOffId,
            active ? optionStyles_[static_cast<size_t>(i)].activeAccent
                   : juce::Colour(OaeTheme::Colors::textSecondary));
    }
    repaint();

    if (notification != juce::dontSendNotification && onSelectionChanged) {
        onSelectionChanged(selectedIndex_);
    }
}

void OaeSegmentedControl::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
    g.setColour(juce::Colour(OaeTheme::Colors::bgDark));
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::switcherCornerRadius);
    g.setColour(juce::Colour(OaeTheme::Colors::borderActive));
    g.drawRoundedRectangle(bounds, OaeTheme::Edges::switcherCornerRadius, OaeTheme::Edges::borderStroke);

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(options_.size())) {
        const auto& btn = options_[static_cast<size_t>(selectedIndex_)];
        const auto& style = optionStyles_[static_cast<size_t>(selectedIndex_)];
        const auto pill = btn->getBounds().toFloat().reduced(0.5f);
        g.setColour(style.activeTint);
        g.fillRoundedRectangle(pill, OaeTheme::Edges::cornerRadius);
        g.setColour(style.activeAccent);
        g.drawRoundedRectangle(pill, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);
    }
}

void OaeSegmentedControl::resized()
{
    if (options_.empty()) return;
    auto area = getLocalBounds().reduced(2);
    const int n = static_cast<int>(options_.size());
    const int cellW = area.getWidth() / n;
    for (int i = 0; i < n; ++i) {
        auto cell = (i == n - 1) ? area : area.removeFromLeft(cellW);
        options_[static_cast<size_t>(i)]->setBounds(cell);
    }
}

OaeValueBoxSlider::OaeValueBoxSlider(const juce::String& componentName)
    : OaeSlider(componentName)
{
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void OaeValueBoxSlider::setDisplayPrefix(const juce::String& prefix)
{
    if (displayPrefix_ == prefix) return;
    displayPrefix_ = prefix;
    repaint();
}

void OaeValueBoxSlider::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(OaeTheme::Edges::borderStroke * 0.5f);
    const bool active = isMouseOverOrDragging() || hasKeyboardFocus(false);

    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);

    g.setColour(juce::Colour(active ? OaeTheme::Colors::accentCyan : OaeTheme::Colors::borderSubtle));
    g.drawRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);

    const double span = getMaximum() - getMinimum();
    if (span > 0.0) {
        const float norm = static_cast<float>(juce::jlimit(0.0, 1.0, (getValue() - getMinimum()) / span));
        const auto barArea = bounds.reduced(3.0f, 1.0f).removeFromBottom(2.0f);
        g.setColour(findColour(juce::Slider::trackColourId).withAlpha(0.55f));
        g.fillRoundedRectangle(barArea.withWidth(barArea.getWidth() * norm), 1.0f);
    }

    const auto text = displayPrefix_ + getTextFromValue(getValue());
    g.setColour(juce::Colour(isEnabled() ? OaeTheme::Colors::textPrimary : OaeTheme::Colors::textMuted));
    g.setFont(juce::FontOptions(OaeTheme::Typography::compact));
    g.drawFittedText(text, getLocalBounds().reduced(6, 0), juce::Justification::centredLeft, 1);
}

} // namespace oae::studio
