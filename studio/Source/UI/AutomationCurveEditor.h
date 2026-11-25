#pragma once

#include "../Model/AuthoringModel.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace oae::studio {

/** Interactive multipoint parameter automation curve editor (1..32 points, 5 segment shapes). */
class AutomationCurveEditor : public juce::Component, public ITestableCustomControl {
public:
    using PointsChanged = std::function<void(const nlohmann::json&)>;
    using PointSelected = std::function<void(int)>;

    AutomationCurveEditor(AuthoringModel& model, int& edit_depth, std::function<void()> on_edit_end,
                          double x_min, double x_max, double y_min, double y_max,
                          nlohmann::json points, int selected_index,
                          PointsChanged on_points_changed, PointSelected on_point_selected)
        : model_(model), edit_depth_(edit_depth), on_edit_end_(std::move(on_edit_end)),
          x_min_(x_min), x_max_(x_max > x_min ? x_max : x_min + 1.0),
          y_min_(y_min), y_max_(y_max > y_min ? y_max : y_min + 1.0),
          points_(points.is_array() ? std::move(points) : nlohmann::json::array()),
          selected_(selected_index),
          on_points_changed_(std::move(on_points_changed)),
          on_point_selected_(std::move(on_point_selected))
    {
        setComponentID("automation_curve_editor");
    }

    void set_selected_shape(const std::string& shape)
    {
        if (selected_ < 0 || static_cast<size_t>(selected_) >= points_.size()) return;
        points_[static_cast<size_t>(selected_)]["shape"] = shape;
        if (on_points_changed_) on_points_changed_(points_);
        repaint();
    }

    std::string selected_shape() const
    {
        if (selected_ < 0 || static_cast<size_t>(selected_) >= points_.size()) return "linear";
        return points_[static_cast<size_t>(selected_)].value("shape", "linear");
    }

    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return;

        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle).withAlpha(OaeTheme::States::curveGridOpacity));
        for (float frac : { 0.25f, 0.50f, 0.75f }) {
            g.drawHorizontalLine(static_cast<int>(area.getY() + area.getHeight() * frac), area.getX(), area.getRight());
            g.drawVerticalLine(static_cast<int>(area.getX() + area.getWidth() * frac), area.getY(), area.getBottom());
        }

        if (points_.empty()) return;

        const auto to_x = [&](double x) {
            return area.getX() + static_cast<float>((x - x_min_) / (x_max_ - x_min_)) * area.getWidth();
        };
        const auto to_y = [&](double y) {
            return area.getBottom() - static_cast<float>((y - y_min_) / (y_max_ - y_min_)) * area.getHeight();
        };

        juce::Path p;
        float firstX = 0.0f;
        float lastX = 0.0f;
        for (size_t i = 0; i < points_.size(); ++i) {
            const float px = to_x(points_[i].value("x", x_min_));
            const float py = to_y(points_[i].value("y", y_min_));
            if (i == 0) {
                p.startNewSubPath(px, py);
                firstX = px;
                lastX = px;
                continue;
            }
            const float prev_x = to_x(points_[i - 1].value("x", x_min_));
            const float prev_y = to_y(points_[i - 1].value("y", y_min_));
            const auto shape = points_[i - 1].value("shape", "linear");
            const float dx = px - prev_x;
            if (shape == "hold") {
                p.lineTo(px, prev_y);
                p.lineTo(px, py);
            } else if (shape == "ease_in") {
                p.cubicTo(prev_x + dx * 0.45f, prev_y, prev_x + dx * 0.85f, prev_y, px, py);
            } else if (shape == "ease_out") {
                p.cubicTo(prev_x + dx * 0.15f, py, prev_x + dx * 0.55f, py, px, py);
            } else if (shape == "s_curve") {
                p.cubicTo(prev_x + dx * 0.5f, prev_y, prev_x + dx * 0.5f, py, px, py);
            } else {
                p.lineTo(px, py);
            }
            lastX = px;
        }

        const auto purple = juce::Colour(OaeTheme::Colors::accentPurple);
        if (points_.size() >= 2) {
            juce::Path ribbon = p;
            ribbon.lineTo(lastX, area.getBottom());
            ribbon.lineTo(firstX, area.getBottom());
            ribbon.closeSubPath();
            juce::ColourGradient grad(purple.withAlpha(OaeTheme::States::curveRibbonTopOpacity), 0.0f, area.getY(),
                                      purple.withAlpha(OaeTheme::States::curveRibbonBottomOpacity), 0.0f, area.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(ribbon);
        }

        g.setColour(purple);
        g.strokePath(p, juce::PathStrokeType(1.75f));

        for (size_t i = 0; i < points_.size(); ++i) {
            const float px = to_x(points_[i].value("x", x_min_));
            const float py = to_y(points_[i].value("y", y_min_));
            const bool is_sel = static_cast<int>(i) == selected_;
            g.setColour(juce::Colour(OaeTheme::Colors::accentCyan)
                            .withAlpha(is_sel ? OaeTheme::States::nodeHoverHaloOpacity : 0.15f));
            g.fillEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f);

            g.setColour(juce::Colour(is_sel ? OaeTheme::Colors::textPrimary : OaeTheme::Colors::accentCyan));
            g.fillEllipse(px - 3.5f, py - 3.5f, 7.0f, 7.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto area = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return;
        const auto to_x = [&](double x) { return area.getX() + static_cast<float>((x - x_min_) / (x_max_ - x_min_)) * area.getWidth(); };
        const auto to_y = [&](double y) { return area.getBottom() - static_cast<float>((y - y_min_) / (y_max_ - y_min_)) * area.getHeight(); };

        int hit = -1;
        float best = 10.0f;
        for (size_t i = 0; i < points_.size(); ++i) {
            const juce::Point<float> pt(to_x(points_[i].value("x", x_min_)), to_y(points_[i].value("y", y_min_)));
            const float d = e.position.getDistanceFrom(pt);
            if (d < best) { best = d; hit = static_cast<int>(i); }
        }

        if (e.mods.isPopupMenu()) {
            if (hit >= 0 && points_.size() > 1) {
                points_.erase(points_.begin() + hit);
                selected_ = std::min(hit, static_cast<int>(points_.size()) - 1);
                if (on_points_changed_) on_points_changed_(points_);
                if (on_point_selected_) on_point_selected_(selected_);
                repaint();
            }
            return;
        }

        if (hit < 0 && points_.size() < 32) {
            const double nx = x_min_ + std::clamp((e.position.x - area.getX()) / static_cast<double>(area.getWidth()), 0.0, 1.0) * (x_max_ - x_min_);
            const double ny = y_min_ + std::clamp((area.getBottom() - e.position.y) / static_cast<double>(area.getHeight()), 0.0, 1.0) * (y_max_ - y_min_);
            points_.push_back({ { "x", nx }, { "y", ny }, { "shape", "linear" } });
            std::stable_sort(points_.begin(), points_.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
                return a.value("x", 0.0) < b.value("x", 0.0);
            });
            for (size_t i = 0; i < points_.size(); ++i) {
                if (std::abs(points_[i].value("x", 0.0) - nx) < 1e-9) { hit = static_cast<int>(i); break; }
            }
            if (on_points_changed_) on_points_changed_(points_);
        }

        if (hit >= 0) {
            selected_ = hit;
            dragging_ = true;
            ++edit_depth_;
            model_.begin_gesture();
            if (on_point_selected_) on_point_selected_(selected_);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging_ || selected_ < 0 || static_cast<size_t>(selected_) >= points_.size()) return;
        const auto area = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return;
        double nx = x_min_ + std::clamp((e.position.x - area.getX()) / static_cast<double>(area.getWidth()), 0.0, 1.0) * (x_max_ - x_min_);
        const double ny = y_min_ + std::clamp((area.getBottom() - e.position.y) / static_cast<double>(area.getHeight()), 0.0, 1.0) * (y_max_ - y_min_);
        const size_t idx = static_cast<size_t>(selected_);
        const double eps = (x_max_ - x_min_) * 1e-4;
        if (idx > 0) nx = std::max(nx, points_[idx - 1].value("x", x_min_) + eps);
        if (idx + 1 < points_.size()) nx = std::min(nx, points_[idx + 1].value("x", x_max_) - eps);
        points_[idx]["x"] = nx;
        points_[idx]["y"] = ny;
        if (on_points_changed_) on_points_changed_(points_);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging_) {
            dragging_ = false;
            model_.end_gesture();
            --edit_depth_;
            if (on_edit_end_) on_edit_end_();
            repaint();
        }
    }

    std::vector<TestableTarget> get_testable_targets() const override
    {
        const auto bounds = getLocalBounds();
        const auto area = bounds.toFloat().reduced(8.0f, 8.0f);
        if (area.getWidth() <= 8.0f || area.getHeight() <= 8.0f) return {};
        std::vector<TestableTarget> out;
        out.push_back({ "canvas", "click", area.toNearestInt().getIntersection(bounds), isEnabled() });
        const auto to_x = [&](double x) { return area.getX() + static_cast<float>((x - x_min_) / (x_max_ - x_min_)) * area.getWidth(); };
        const auto to_y = [&](double y) { return area.getBottom() - static_cast<float>((y - y_min_) / (y_max_ - y_min_)) * area.getHeight(); };
        for (size_t i = 0; i < points_.size(); ++i) {
            const float px = to_x(points_[i].value("x", x_min_));
            const float py = to_y(points_[i].value("y", y_min_));
            const auto r = juce::Rectangle<int>(
                static_cast<int>(std::round(px)) - 6,
                static_cast<int>(std::round(py)) - 6,
                12, 12).getIntersection(bounds);
            if (!r.isEmpty()) {
                out.push_back({ "point_" + std::to_string(i), "drag", r, isEnabled() });
            }
        }
        return out;
    }

private:
    AuthoringModel& model_;
    int& edit_depth_;
    std::function<void()> on_edit_end_;
    double x_min_, x_max_, y_min_, y_max_;
    nlohmann::json points_;
    int selected_{ 0 };
    PointsChanged on_points_changed_;
    PointSelected on_point_selected_;
    bool dragging_{ false };
};

} // namespace oae::studio
