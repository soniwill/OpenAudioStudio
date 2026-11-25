#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "VectorIcons.h"

namespace oae::studio {

/** Typed property inspector hosting specialized editors for each project element type. */
class InspectorPanel : public juce::Component, public AuthoringModel::Listener {
public:
    explicit InspectorPanel(AuthoringModel& model);
    ~InspectorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;
    void on_item_renamed(ItemType type, const std::string& old_id, const std::string& new_id) override;
    void on_item_deleted(ItemType type, const std::string& id) override;

    void update_view();

    /** Returns true if the specified Inspector section card is currently expanded. */
    bool is_card_expanded(const std::string& card_id) const noexcept;

    /** Sets the expansion state for the specified Inspector section card and refreshes layout. */
    void set_card_expanded(const std::string& card_id, bool expanded);

    /** Returns true if the Inspector selection is currently locked. */
    bool is_locked() const noexcept { return is_locked_; }

    /** Sets the selection lock state for the InspectorPanel. */
    void set_locked(bool locked);

    /** Hides the internal header bar when embedded solo under a DockHeader. */
    void set_embedded_header_mode(bool embedded);
    bool is_embedded_header_mode() const noexcept { return embeddedHeaderMode_; }

    /** Sets whether the stage is currently in Layers mode (reveals MaskTransitionCard). */
    void set_layers_mode(bool layersMode);
    bool is_layers_mode() const noexcept { return layersMode_; }

    std::function<void(bool locked)> onLockChanged;
    std::function<void()> onSwitchToTimelineRequested;

    AuthoringModel& model() noexcept { return model_; }

private:
    AuthoringModel& model_;
    juce::Label headerLabel_{ "header", "PROPERTY INSPECTOR" };
    VectorIconButton lockButton_{ "Lock Inspector", VectorIconType::LockOpen, juce::Colour(OaeTheme::Colors::accentCyan) };
    bool is_locked_{ false };
    bool embeddedHeaderMode_{ false };
    bool layersMode_{ false };
    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> currentEditor_;

    bool clip_card_expanded_{ true };
    bool track_card_expanded_{ true };
    bool mask_card_expanded_{ true };
    bool event_card_expanded_{ true };

    // Typed editor view builders
    std::unique_ptr<juce::Component> create_event_editor(const std::string& path);
    std::unique_ptr<juce::Component> create_snapshot_editor(const std::string& path);
    std::unique_ptr<juce::Component> create_bus_editor(const std::string& path);
    std::unique_ptr<juce::Component> create_vca_editor(const std::string& path);
    std::unique_ptr<juce::Component> create_param_editor(const std::string& name);
    std::unique_ptr<juce::Component> create_sample_editor(const std::string& id);
    std::unique_ptr<juce::Component> create_empty_editor();
    void schedule_refresh();

    int edit_depth_{ 0 };
    bool refresh_pending_{ false };
    ItemSelection last_selection_{};
};

} // namespace oae::studio
