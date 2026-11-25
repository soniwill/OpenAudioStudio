#pragma once

#include "IInspectorCard.h"
#include "InspectorCardHeader.h"
#include "../../Model/AuthoringModel.h"
#include "../Theme.h"
#include "../OaePrimitives.h"
#include "../OaeSlider.h"
#include "../AhdsrCurveEditor.h"
#include "../AutomationCurveEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace oae::studio {

class InspectorEditor : public juce::Component {
public:
    struct LayoutItem {
        juce::Component* component;
        int group;
        int width;
        int height;
    };

    class Row : public juce::Component {
    public:
        Row(std::vector<LayoutItem> itemsToLayout, int verticalGap);
        int heightForWidth(int width) const;
        void resized() override;
        int gapBefore{ 0 };

    private:
        std::vector<LayoutItem> items;
    };

    void addLayoutItem(juce::Component* component, int group, int width, int height);
    void resized() override;
    virtual int contentHeightForWidth(int width);
    ~InspectorEditor() override = default;

protected:
    void ensureRowsCreated() { createRowsIfNeeded(); }

private:
    void createRowsIfNeeded();
    std::vector<LayoutItem> layoutItems;
    std::vector<std::unique_ptr<Row>> rows;
};

void register_layout_item(juce::Component& parent, juce::Component* component,
                          int y, int width, int height);

void resize_viewport_content(juce::Viewport& viewport, juce::Component& content);

std::pair<double, double> parameter_json_domain(const nlohmann::json& p);

const nlohmann::json* find_live_param(const nlohmann::json& root, bool is_local,
                                      const std::string& event_path, const std::string& name);

const nlohmann::json* resolve_event_param_any_scope(const nlohmann::json& root,
                                                    const nlohmann::json& ev,
                                                    const std::string& name);

const nlohmann::json* find_live_instrument(const nlohmann::json& root,
                                           const std::string& event_path,
                                           size_t track_index,
                                           size_t inst_index);

bool has_suffix(const std::string& s, const std::string& suffix);
bool is_internal_user_prop(const std::string& key);
std::string kind_badge_label(const std::string& kind);
std::vector<std::string> parse_comma_labels(const juce::String& text);

juce::Slider* create_gesture_slider(AuthoringModel& model, int& edit_depth, std::function<void()> on_edit_end,
                                    double min, double max, double val,
                                    const juce::String& suffix, std::function<void(double)> on_change);

void add_labeled_row(juce::Component& parent, std::vector<std::unique_ptr<juce::Component>>& owned,
                     int& y, const juce::String& text, juce::Component* comp, int compHeight = 22);

void add_section_header(juce::Component& parent, std::vector<std::unique_ptr<juce::Component>>& owned,
                        int& y, const juce::String& title);

void append_parameter_editor_controls(juce::Component& view,
                                      std::vector<std::unique_ptr<juce::Component>>& comps,
                                      int& y,
                                      AuthoringModel& model,
                                      int& edit_depth,
                                      const std::function<void()>& on_edit_end,
                                      const nlohmann::json& param,
                                      bool is_local,
                                      const std::string& event_path,
                                      const std::string& id_prefix);

void append_modulation_section(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                               int& y, AuthoringModel& model, int& edit_depth,
                               std::function<void()> on_edit_end, const ItemSelection& owner,
                               const nlohmann::json& owner_json, const juce::String& title_prefix,
                               const std::string& id_prefix);

void append_param_automations_section(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                                      int& y, AuthoringModel& model, int& edit_depth,
                                      std::function<void()> on_edit_end, const std::string& event_path,
                                      int track_index, const nlohmann::json& owner_json,
                                      const std::string& id_prefix);

void append_bus_effect_automations(juce::Component& view, std::vector<std::unique_ptr<juce::Component>>& comps,
                                   int& y, AuthoringModel& model, int& edit_depth,
                                   std::function<void()> on_edit_end, const std::string& bus_path,
                                   const nlohmann::json& bus_json,
                                   const std::string& id_prefix);

class TrackColorSwatchButton : public juce::Button {
public:
    TrackColorSwatchButton(size_t index, juce::Colour col, bool active);
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    juce::Colour colour_;
    bool active_{ false };
};

class TrackColorSwatchStrip : public juce::Component {
public:
    TrackColorSwatchStrip(AuthoringModel& model,
                          const std::string& event_path,
                          size_t track_index,
                          const std::string& active_hex);
    void resized() override;

private:
    std::vector<std::unique_ptr<TrackColorSwatchButton>> buttons_;
};

/** Reusable base class for inspector cards providing standard card chrome, header, and body layout. */
class InspectorCardBase : public IInspectorCard {
public:
    static constexpr int kBodyPadX = 10;
    static constexpr int kBodyPadTop = 10;
    static constexpr int kBodyPadBottom = 10;

    InspectorCardBase(const juce::String& cardId,
                      std::string_view cardTypeId,
                      const juce::String& title,
                      const juce::String& badge,
                      juce::Colour accentColour,
                      bool expanded,
                      AuthoringModel& model,
                      int& edit_depth,
                      std::function<void()> on_edit_end,
                      std::function<void(bool)> onToggle = nullptr);

    ~InspectorCardBase() override = default;

    std::string_view get_card_type_id() const override { return cardTypeId_; }
    bool is_card_expanded() const noexcept override { return expanded_; }
    void set_card_expanded(bool expanded) override;
    int height_for_width(int width) override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    InspectorEditor& body() noexcept { return body_; }
    std::vector<std::unique_ptr<juce::Component>>& comps() noexcept { return comps_; }
    int& cursor_y() noexcept { return y_; }
    AuthoringModel& model() noexcept { return model_; }
    int& edit_depth() noexcept { return edit_depth_; }
    const std::function<void()>& on_edit_end() const noexcept { return on_edit_end_; }

    void add_labeled_row(const juce::String& text, juce::Component* comp, int compHeight = 22) {
        studio::add_labeled_row(body_, comps_, y_, text, comp, compHeight);
    }

    void add_section_header(const juce::String& title) {
        studio::add_section_header(body_, comps_, y_, title);
    }

    juce::Slider* create_slider(double min, double max, double val, const juce::String& suffix, std::function<void(double)> on_change) {
        return create_gesture_slider(model_, edit_depth_, on_edit_end_, min, max, val, suffix, std::move(on_change));
    }

protected:
    std::string cardTypeId_;
    juce::Colour accentColour_;
    bool expanded_{ true };
    AuthoringModel& model_;
    int& edit_depth_;
    std::function<void()> on_edit_end_;
    std::function<void(bool)> onToggle_;

    InspectorCardHeader header_;
    InspectorEditor body_;
    std::vector<std::unique_ptr<juce::Component>> comps_;
    int y_{ 4 };
};

class CardStackEditor : public InspectorEditor {
public:
    void addCard(std::unique_ptr<IInspectorCard> card);
    int contentHeightForWidth(int width) override;
    void resized() override;

private:
    std::vector<std::unique_ptr<IInspectorCard>> cards_;
};

} // namespace oae::studio
