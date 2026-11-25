#pragma once

#include "../Model/AuthoringModel.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace oae::studio {

/** Interactive AHDSR envelope curve preview and node drag editor. */
class AhdsrCurveEditor : public juce::Component, public ITestableCustomControl {
public:
    using ChangeCallback = std::function<void(double a, double h, double d, double s, double r)>;

    AhdsrCurveEditor(AuthoringModel& model, int& edit_depth, std::function<void()> on_edit_end,
                     double a_ms, double h_ms, double d_ms, double sustain, double r_ms,
                     bool release_on_key_off, ChangeCallback on_change)
        : model_(model), edit_depth_(edit_depth), on_edit_end_(std::move(on_edit_end)),
          a_ms_(std::max(0.0, a_ms)), h_ms_(std::max(0.0, h_ms)), d_ms_(std::max(0.0, d_ms)),
          sustain_(std::clamp(sustain, 0.0, 1.0)), r_ms_(std::max(0.0, r_ms)),
          key_off_(release_on_key_off), on_change_(std::move(on_change))
    {
        setComponentID("ahdsr_curve_editor");
    }

    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(6.0f, 6.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return;

        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle).withAlpha(OaeTheme::States::curveGridOpacity));
        for (float frac : { 0.25f, 0.50f, 0.75f }) {
            g.drawHorizontalLine(static_cast<int>(area.getY() + area.getHeight() * frac), area.getX(), area.getRight());
        }

        const double total_ms = std::max(100.0, a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_);
        const auto to_x = [&](double ms) {
            return area.getX() + static_cast<float>(ms / total_ms) * area.getWidth();
        };
        const auto to_y = [&](double amp) {
            return area.getBottom() - static_cast<float>(std::clamp(amp, 0.0, 1.0)) * area.getHeight();
        };

        const float x0 = area.getX();
        const float xA = to_x(a_ms_);
        const float xH = to_x(a_ms_ + h_ms_);
        const float xD = to_x(a_ms_ + h_ms_ + d_ms_);
        const float xK = to_x(a_ms_ + h_ms_ + d_ms_ + 250.0);
        const float xR = to_x(a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_);

        const float y0 = to_y(0.0);
        const float y1 = to_y(1.0);
        const float yS = to_y(sustain_);

        juce::Path p;
        p.startNewSubPath(x0, y0);
        p.cubicTo(x0 + (xA - x0) * 0.2f, y1, x0 + (xA - x0) * 0.6f, y1, xA, y1);
        p.lineTo(xH, y1);
        p.cubicTo(xH + (xD - xH) * 0.25f, yS, xH + (xD - xH) * 0.65f, yS, xD, yS);
        p.lineTo(xK, yS);
        p.cubicTo(xK + (xR - xK) * 0.25f, y0, xK + (xR - xK) * 0.65f, y0, xR, y0);

        const auto cyan = juce::Colour(OaeTheme::Colors::accentCyan);
        juce::Path ribbon = p;
        ribbon.lineTo(xR, y0);
        ribbon.lineTo(x0, y0);
        ribbon.closeSubPath();
        juce::ColourGradient grad(cyan.withAlpha(OaeTheme::States::curveRibbonTopOpacity), 0.0f, area.getY(),
                                  cyan.withAlpha(OaeTheme::States::curveRibbonBottomOpacity), 0.0f, y0, false);
        g.setGradientFill(grad);
        g.fillPath(ribbon);

        g.setColour(cyan);
        g.strokePath(p, juce::PathStrokeType(1.75f));

        // Key Off marker
        g.setColour(juce::Colour(OaeTheme::Colors::accentAmber).withAlpha(0.75f));
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(xK, area.getY(), xK, area.getBottom()), dashes, 2, 1.0f);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(key_off_ ? "Key Off" : "Hold", static_cast<int>(xK) - 24, static_cast<int>(area.getY()), 48, 12,
                   juce::Justification::centred, false);

        const juce::Point<float> nodes[4] = { { xA, y1 }, { xH, y1 }, { xD, yS }, { xR, y0 } };
        for (int i = 0; i < 4; ++i) {
            const float haloAlpha = drag_node_ == i ? OaeTheme::States::nodeHoverHaloOpacity : 0.15f;
            g.setColour(cyan.withAlpha(haloAlpha));
            g.fillEllipse(nodes[i].x - 6.0f, nodes[i].y - 6.0f, 12.0f, 12.0f);

            g.setColour(juce::Colour(drag_node_ == i ? OaeTheme::Colors::textPrimary : OaeTheme::Colors::accentCyan));
            g.fillEllipse(nodes[i].x - 3.5f, nodes[i].y - 3.5f, 7.0f, 7.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto area = getLocalBounds().toFloat().reduced(6.0f, 6.0f);
        const double total_ms = std::max(100.0, a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_);
        const auto to_x = [&](double ms) { return area.getX() + static_cast<float>(ms / total_ms) * area.getWidth(); };
        const auto to_y = [&](double amp) { return area.getBottom() - static_cast<float>(std::clamp(amp, 0.0, 1.0)) * area.getHeight(); };

        const juce::Point<float> nodes[4] = {
            { to_x(a_ms_), to_y(1.0) },
            { to_x(a_ms_ + h_ms_), to_y(1.0) },
            { to_x(a_ms_ + h_ms_ + d_ms_), to_y(sustain_) },
            { to_x(a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_), to_y(0.0) }
        };
        drag_node_ = -1;
        float best_dist = 14.0f;
        for (int i = 0; i < 4; ++i) {
            const float d = e.position.getDistanceFrom(nodes[i]);
            if (d < best_dist) { best_dist = d; drag_node_ = i; }
        }
        if (drag_node_ >= 0) {
            drag_total_ms_ = total_ms;
            ++edit_depth_;
            model_.begin_gesture();
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drag_node_ < 0) return;
        const auto area = getLocalBounds().toFloat().reduced(6.0f, 6.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return;
        const double ms = std::clamp((e.position.x - area.getX()) / static_cast<double>(area.getWidth()), 0.0, 1.0) * drag_total_ms_;
        const double amp = std::clamp((area.getBottom() - e.position.y) / static_cast<double>(area.getHeight()), 0.0, 1.0);

        if (drag_node_ == 0) a_ms_ = std::max(0.0, ms);
        else if (drag_node_ == 1) h_ms_ = std::max(0.0, ms - a_ms_);
        else if (drag_node_ == 2) {
            d_ms_ = std::max(0.0, ms - (a_ms_ + h_ms_));
            sustain_ = amp;
        } else if (drag_node_ == 3) {
            r_ms_ = std::max(0.0, ms - (a_ms_ + h_ms_ + d_ms_ + 250.0));
        }
        if (on_change_) on_change_(a_ms_, h_ms_, d_ms_, sustain_, r_ms_);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (drag_node_ >= 0) {
            drag_node_ = -1;
            model_.end_gesture();
            --edit_depth_;
            if (on_edit_end_) on_edit_end_();
            repaint();
        }
    }

    std::vector<TestableTarget> get_testable_targets() const override
    {
        const auto bounds = getLocalBounds();
        const auto area = bounds.toFloat().reduced(6.0f, 6.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return {};
        const double total_ms = std::max(100.0, a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_);
        const auto to_x = [&](double ms) { return area.getX() + static_cast<float>(ms / total_ms) * area.getWidth(); };
        const auto to_y = [&](double amp) { return area.getBottom() - static_cast<float>(std::clamp(amp, 0.0, 1.0)) * area.getHeight(); };
        const struct { const char* id; juce::Point<float> pt; } nodes[4] = {
            { "node_attack", { to_x(a_ms_), to_y(1.0) } },
            { "node_hold", { to_x(a_ms_ + h_ms_), to_y(1.0) } },
            { "node_decay", { to_x(a_ms_ + h_ms_ + d_ms_), to_y(sustain_) } },
            { "node_release", { to_x(a_ms_ + h_ms_ + d_ms_ + 250.0 + r_ms_), to_y(0.0) } }
        };
        std::vector<TestableTarget> out;
        for (const auto& n : nodes) {
            const auto r = juce::Rectangle<int>(
                static_cast<int>(std::round(n.pt.x)) - 8,
                static_cast<int>(std::round(n.pt.y)) - 8,
                16, 16).getIntersection(bounds);
            if (!r.isEmpty()) {
                out.push_back({ n.id, "drag", r, isEnabled() });
            }
        }
        return out;
    }

private:
    AuthoringModel& model_;
    int& edit_depth_;
    std::function<void()> on_edit_end_;
    double a_ms_, h_ms_, d_ms_, sustain_, r_ms_;
    bool key_off_;
    ChangeCallback on_change_;
    int drag_node_{ -1 };
    double drag_total_ms_{ 1000.0 };
};

} // namespace oae::studio
