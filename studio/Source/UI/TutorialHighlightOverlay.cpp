#include "TutorialHighlightOverlay.h"

#include <algorithm>
#include <cmath>

#include "Theme.h"

namespace oae::studio {

TutorialHighlightOverlay::TutorialHighlightOverlay()
{
    setComponentID("tutorial_highlight_overlay");
    setInterceptsMouseClicks(false, false);
    setWantsKeyboardFocus(false);
    setOpaque(false);
    setVisible(false);
}

bool TutorialHighlightOverlay::hitTest(int, int)
{
    return false;
}

bool TutorialHighlightOverlay::is_visible_under_root(const juce::Component* comp, const juce::Component* root) noexcept
{
    if (!comp || !root) {
        return false;
    }

    const juce::Component* cur = comp;
    while (cur != nullptr && cur != root) {
        if (!cur->isVisible()) {
            return false;
        }
        cur = cur->getParentComponent();
    }
    return cur == root;
}

void TutorialHighlightOverlay::set_highlight(juce::Component* target,
                                           const juce::String& label,
                                           float dimOpacity,
                                           HighlightColor color)
{
    active_ = (target != nullptr);
    target_ = target;
    label_ = label.trim();
    dimOpacity_ = std::isfinite(dimOpacity) ? std::clamp(dimOpacity, 0.0f, 0.85f) : 0.45f;
    color_ = color;

    if (auto* parent = getParentComponent()) {
        setBounds(parent->getLocalBounds());
        toFront(false);
        const bool visible = active_
            && is_visible_under_root(target, parent)
            && target->getWidth() > 0
            && target->getHeight() > 0;
        setVisible(visible);
        repaint();
        parent->repaint();
    } else {
        setVisible(false);
    }
}

void TutorialHighlightOverlay::clear()
{
    active_ = false;
    target_ = nullptr;
    label_.clear();
    setVisible(false);
    repaint();
    if (auto* parent = getParentComponent()) {
        parent->repaint();
    }
}

void TutorialHighlightOverlay::refresh_target_visibility()
{
    if (!active_) {
        if (isVisible()) {
            setVisible(false);
            repaint();
        }
        return;
    }

    auto* parent = getParentComponent();
    auto* target = target_.getComponent();
    const bool shouldShow = parent != nullptr
        && target != nullptr
        && is_visible_under_root(target, parent)
        && target->getWidth() > 0
        && target->getHeight() > 0;

    if (isVisible() != shouldShow) {
        setVisible(shouldShow);
        repaint();
    } else if (shouldShow) {
        repaint();
    }
}

juce::Colour TutorialHighlightOverlay::resolve_accent(HighlightColor color) noexcept
{
    switch (color) {
        case HighlightColor::Amber:
            return juce::Colour(OaeTheme::Colors::accentAmber);
        case HighlightColor::Green:
            return juce::Colour(OaeTheme::Colors::accentGreen);
        case HighlightColor::Cyan:
        default:
            return juce::Colour(OaeTheme::Colors::accentCyan);
    }
}

juce::Rectangle<int> TutorialHighlightOverlay::compute_pill_bounds(const juce::Rectangle<int>& fullBounds,
                                                                   const juce::Rectangle<int>& targetRect,
                                                                   int pillWidth,
                                                                   int pillHeight,
                                                                   int margin) noexcept
{
    const auto safeArea = fullBounds.reduced(margin);
    if (safeArea.isEmpty()) {
        return {};
    }

    const int clampedW = std::min(pillWidth, safeArea.getWidth());
    const int clampedH = std::min(pillHeight, safeArea.getHeight());
    const int gap = OaeTheme::Spacing::small;

    const auto clamp_rect = [&safeArea, clampedW, clampedH](int x, int y) {
        const int cx = std::clamp(x, safeArea.getX(), safeArea.getRight() - clampedW);
        const int cy = std::clamp(y, safeArea.getY(), safeArea.getBottom() - clampedH);
        return juce::Rectangle<int>(cx, cy, clampedW, clampedH);
    };

    const int centeredX = targetRect.getCentreX() - clampedW / 2;

    if (targetRect.getY() - gap - clampedH >= safeArea.getY()) {
        return clamp_rect(centeredX, targetRect.getY() - gap - clampedH);
    }

    if (targetRect.getBottom() + gap + clampedH <= safeArea.getBottom()) {
        return clamp_rect(centeredX, targetRect.getBottom() + gap);
    }

    const int centeredY = targetRect.getCentreY() - clampedH / 2;
    if (targetRect.getRight() + gap + clampedW <= safeArea.getRight()) {
        return clamp_rect(targetRect.getRight() + gap, centeredY);
    }

    if (targetRect.getX() - gap - clampedW >= safeArea.getX()) {
        return clamp_rect(targetRect.getX() - gap - clampedW, centeredY);
    }

    return clamp_rect(targetRect.getX() + gap, targetRect.getY() + gap);
}

void TutorialHighlightOverlay::paint(juce::Graphics& g)
{
    if (!active_) {
        return;
    }

    auto* parent = getParentComponent();
    auto* target = target_.getComponent();
    if (!parent || !target || !is_visible_under_root(target, parent)) {
        return;
    }
    if (target->getWidth() <= 0 || target->getHeight() <= 0) {
        return;
    }

    const auto full = getLocalBounds();
    const auto rawTarget = parent->getLocalArea(target, target->getLocalBounds());
    const auto targetRect = rawTarget.expanded(OaeTheme::Spacing::unit).getIntersection(full);
    if (targetRect.isEmpty()) {
        return;
    }

    if (dimOpacity_ > 0.0f) {
        g.setColour(juce::Colour(OaeTheme::Colors::bgDark).withAlpha(dimOpacity_));

        const juce::Rectangle<int> topRect(
            full.getX(), full.getY(), full.getWidth(), targetRect.getY() - full.getY());
        const juce::Rectangle<int> bottomRect(
            full.getX(), targetRect.getBottom(), full.getWidth(), full.getBottom() - targetRect.getBottom());
        const juce::Rectangle<int> leftRect(
            full.getX(), targetRect.getY(), targetRect.getX() - full.getX(), targetRect.getHeight());
        const juce::Rectangle<int> rightRect(
            targetRect.getRight(), targetRect.getY(), full.getRight() - targetRect.getRight(), targetRect.getHeight());

        if (!topRect.isEmpty()) g.fillRect(topRect);
        if (!bottomRect.isEmpty()) g.fillRect(bottomRect);
        if (!leftRect.isEmpty()) g.fillRect(leftRect);
        if (!rightRect.isEmpty()) g.fillRect(rightRect);
    }

    const auto accent = resolve_accent(color_);
    const auto borderBounds = targetRect.toFloat().reduced(1.0f);
    if (!borderBounds.isEmpty()) {
        g.setColour(accent.withAlpha(0.28f));
        g.drawRoundedRectangle(borderBounds.expanded(1.0f), OaeTheme::Edges::cardCornerRadius + 2.0f, 4.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(borderBounds, OaeTheme::Edges::cardCornerRadius, 2.0f);
    }

    if (label_.isNotEmpty()) {
        const auto font = juce::Font(juce::FontOptions(OaeTheme::Typography::control, juce::Font::bold));
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, label_, 0.0f, 0.0f);
        const int textW = static_cast<int>(std::ceil(glyphs.getBoundingBox(0, -1, false).getWidth()));
        constexpr int padX = 10;
        constexpr int pillH = 24;
        constexpr int margin = 8;
        const int maxPillW = std::max(48, full.getWidth() - margin * 2);
        const int pillW = std::clamp(textW + padX * 2, 48, maxPillW);

        const auto pillBounds = compute_pill_bounds(full, targetRect, pillW, pillH, margin);
        if (!pillBounds.isEmpty()) {
            const auto pillFloat = pillBounds.toFloat();
            g.setColour(juce::Colour(OaeTheme::Colors::bgSurfaceRaised).withAlpha(0.96f));
            g.fillRoundedRectangle(pillFloat, OaeTheme::Edges::pillCornerRadius);
            g.setColour(accent);
            g.drawRoundedRectangle(pillFloat, OaeTheme::Edges::pillCornerRadius, OaeTheme::Edges::selectedBorderStroke);
            g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
            g.setFont(font);
            g.drawFittedText(label_, pillBounds.reduced(padX, 0), juce::Justification::centred, 1);
        }
    }
}

} // namespace oae::studio
