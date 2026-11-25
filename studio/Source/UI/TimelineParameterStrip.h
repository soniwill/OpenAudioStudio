#pragma once

#include "OaePrimitives.h"
#include "OaeSlider.h"
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../Transport/AuditionTransport.h"
#include <vector>
#include <memory>
#include <string>

namespace oae::studio {

/** Contextual parameter strip embedded in TimelinePanel displaying active event parameters. */
class TimelineParameterStrip : public juce::Component, public AuditionTransport::Listener, public AuthoringModel::Listener {
public:
    TimelineParameterStrip(AuthoringModel& model, AuditionTransport* transport);
    ~TimelineParameterStrip() override;

    void set_event_path(const std::string& path);
    const std::string& get_event_path() const noexcept { return eventPath_; }

    void set_transport(AuditionTransport* transport);

    /** Returns the required height of the strip (0 if no parameters, 24 if collapsed, 42 if expanded). */
    int preferred_height() const noexcept;

    bool is_expanded() const noexcept { return expanded_; }
    void set_expanded(bool expanded);

    size_t get_parameter_count() const noexcept { return items_.size(); }

    std::function<void()> onHeightChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AuditionTransport::Listener
    void on_parameter_changed(const std::string& name, float val, bool is_global) override;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

    void refresh();

    juce::TextButton toggleButton{ "[v] Parameters (0)" };

    OaeSlider* find_slider_by_name(const std::string& name, bool is_global) const;

private:
    struct Item {
        std::string name;
        bool is_global{ false };
        std::string kind;
        std::vector<std::string> labels;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<OaeSlider> slider;
        std::unique_ptr<OaeStatusBadge> boolBadge;
        juce::Rectangle<int> pillBounds;
    };

    class PillContainer : public juce::Component {
    public:
        explicit PillContainer(std::vector<Item>& items) : items_(items) {}
        void paint(juce::Graphics& g) override;
    private:
        std::vector<Item>& items_;
    };

    AuthoringModel& model_;
    AuditionTransport* transport_{ nullptr };
    std::string eventPath_;
    bool expanded_{ true };

    juce::Viewport viewport_;
    std::vector<Item> items_;
    PillContainer contentContainer_{ items_ };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineParameterStrip)
};

} // namespace oae::studio
