#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace oae::studio {

/** Logical dockable workspace panels managed by DockingLayoutManager. */
enum class DockPanelId {
    Transport = 0,
    Browser = 1,
    Timeline = 2,
    Inspector = 3,
    BottomDeck = 4,
    Diagnostics = 5,
    ProjectToolbar = 6,
    Count
};

inline constexpr size_t dockPanelCount = static_cast<size_t>(DockPanelId::Count);

/** Dockable anchor regions in the Studio main workspace. */
enum class DockSlotId {
    Top = 0,
    Left = 1,
    Center = 2,
    Right = 3,
    Bottom = 4,
    Floating = 5
};

/** Per-panel visibility and assigned dock slot. */
struct DockPanelState {
    bool visible{ true };
    DockSlotId slot{ DockSlotId::Center };
};

/** A binary split or an ordered group of dock tabs. */
struct WorkspaceNode {
    int id{ 0 };
    bool vertical{ false };
    double ratio{ 0.5 };
    int extent{ 0 };
    std::vector<WorkspaceNode> children;
    std::vector<DockPanelId> tabs;
    DockPanelId active{ DockPanelId::Timeline };
};

enum class DockEdge { Tabs, Left, Right, Top, Bottom };

struct WorkspaceDrop {
    int group{ 0 };
    DockEdge edge{ DockEdge::Tabs };
    int index{ 0 };
};

struct WorkspaceGroupBounds {
    int id;
    juce::Rectangle<int> bounds;
    std::vector<DockPanelId> tabs;
    DockPanelId active;
};

struct WorkspaceDividerBounds {
    int id;
    juce::Rectangle<int> bounds;
    bool vertical;
    int available;
    int first;
    int min_first;
    int max_first;
};

struct WorkspaceGeometry {
    std::vector<WorkspaceGroupBounds> groups;
    std::vector<WorkspaceDividerBounds> dividers;
};

/** Persistent dimensions and panel configuration for the Studio workspace. */
struct DockingDimensions {
    int left_width{ 260 };
    int right_width{ 280 };
    int bottom_height{ 240 };
    int top_height{ 34 };

    std::array<DockPanelState, dockPanelCount> panels = {{
        { true, DockSlotId::Top },
        { true, DockSlotId::Left },
        { true, DockSlotId::Center },
        { true, DockSlotId::Right },
        { true, DockSlotId::Bottom },
        { true, DockSlotId::Bottom },
        { true, DockSlotId::Top }
    }};
    std::array<int, 5> active_tab_in_slot = {{ 0, 1, 2, 3, 4 }};
    bool hide_all_active{ false };
    std::array<bool, dockPanelCount> pre_hide_visible = {{ true, true, true, true, true, true, true }};
    std::string active_preset{ "Default (Authoring)" };
    WorkspaceNode tree;

    static constexpr int min_left_width{ 160 };
    static constexpr int max_left_width{ 480 };

    static constexpr int min_right_width{ 180 };
    static constexpr int max_right_width{ 500 };

    static constexpr int min_bottom_height{ 192 };
    static constexpr int max_bottom_height{ 520 };

    void clamp(int total_w, int total_h) noexcept;
    std::string to_json() const;
    bool from_json(const std::string& json_str);
};

/** Named workspace preset entry (built-in read-only templates or user-created layouts). */
struct DockWorkspacePreset {
    std::string name;
    bool builtin{ false };
    DockingDimensions state;
};

/** Interactive resizable splitter bar between panels with hovering and cursor feedback. */
class SplitterBar : public juce::Component {
public:
    enum class Orientation {
        Vertical,   // Resizes columns horizontally (cursor: LeftRightResize)
        Horizontal  // Resizes rows vertically (cursor: UpDownResize)
    };

    explicit SplitterBar(Orientation orientation);
    ~SplitterBar() override = default;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override;

    Orientation get_orientation() const noexcept { return orientation_; }
    bool is_dragging() const noexcept { return is_dragging_; }

    std::function<void(int delta_px)> onSplitterDragged;
    std::function<void()> onSplitterDragEnded;

private:
    Orientation orientation_;
    bool is_mouse_over_{ false };
    bool is_dragging_{ false };
    juce::Point<int> drag_start_mouse_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplitterBar)
};

/** Docking Layout Manager arranging panels and persisting state to oae_studio_layout.json. */
class DockingLayoutManager {
public:
    /** Tabbed dock slot. Registered panels remain owned by the caller and must outlive clear(). */
    class TabSlot : public juce::Component {
    public:
        TabSlot();
        ~TabSlot() override;
        void add_panel(juce::Component& panel, const juce::String& name, const juce::String& tab_id);
        void clear();
        void select(int panel_index);
        int selected_panel() const noexcept { return selected_; }
        void pop_out(int panel_index);
        void dock_back(int panel_index);
        void move_tab(int offset);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseUp(const juce::MouseEvent& event) override;
        void set_panel_available(int panel_index, bool available);
        void set_panel_title(int panel_index, const juce::String& title);
        void set_embedded_dock_header(bool embedded);

        std::function<bool(const juce::KeyPress&)> onFloatingKeyPressed;
        std::function<void(const juce::MouseEvent&)> onHeaderDrag;
        std::function<void(const juce::MouseEvent&)> onHeaderDrop;
        std::function<void(const juce::MouseEvent&)> onDiagnosticsDrag;
        std::function<void(const juce::MouseEvent&)> onDiagnosticsDrop;
        std::function<void()> onDiagnosticsPopOut;

    private:
        struct Panel;
        void update_selection();
        void show_tab_menu();

        juce::Component tabs_;
        juce::Component tabStrip_;
        juce::Viewport tabViewport_;
        juce::Component content_;
        std::unique_ptr<juce::TextButton> headerDragHandle_;
        juce::TextButton popOut_{ "Pop-Out ^" };
        juce::TextButton tabMenu_{ "Tabs" };
        bool embeddedDockHeader_{ false };
        std::vector<std::unique_ptr<Panel>> panels_;
        std::vector<int> order_;
        int selected_{ -1 };
    };

    DockingLayoutManager();
    ~DockingLayoutManager() = default;

    void load(const fs::path& layout_file_path);
    void save(const fs::path& layout_file_path) const;

    const DockingDimensions& dimensions() const noexcept { return dims_; }
    DockingDimensions& dimensions() noexcept { return dims_; }

    void set_left_width(int w) noexcept { dims_.left_width = w; }
    void set_right_width(int w) noexcept { dims_.right_width = w; }
    void set_bottom_height(int h) noexcept { dims_.bottom_height = h; }
    TabSlot& bottom_slot() noexcept { return bottomSlot_; }

    bool is_panel_visible(DockPanelId id) const noexcept;
    void set_panel_visible(DockPanelId id, bool visible);
    DockSlotId get_panel_slot(DockPanelId id) const noexcept;
    void set_panel_slot(DockPanelId id, DockSlotId slot);
    int get_active_tab_in_slot(DockSlotId slot) const noexcept;
    void set_active_tab_in_slot(DockSlotId slot, DockPanelId id);
    std::vector<DockPanelId> panels_in_slot(DockSlotId slot, bool visible_only = true) const;
    static WorkspaceNode workspace_from_legacy(const DockingDimensions& state);
    WorkspaceGeometry workspace_geometry(juce::Rectangle<int> area, int transport_height) const;
    int workspace_group(DockPanelId panel) const;
    void select_workspace_tab(int group, DockPanelId panel);
    bool move_workspace_panel(DockPanelId panel, WorkspaceDrop target);
    juce::Rectangle<int> preview_workspace_split(DockPanelId panel, WorkspaceDrop target,
                                                 juce::Rectangle<int> area, int transport_height) const;
    void resize_workspace_split(const WorkspaceDividerBounds& divider, int delta);
    void dock_back_workspace_panel(DockPanelId panel);

    void hide_all_panels();
    void restore_all_panels();
    bool are_all_panels_hidden() const noexcept { return dims_.hide_all_active; }

    std::vector<std::string> get_preset_names() const;
    bool is_builtin_preset(const std::string& name) const;
    const std::string& get_active_preset_name() const noexcept { return dims_.active_preset; }
    bool apply_preset(const std::string& name);
    bool save_custom_preset(const std::string& name);
    bool delete_custom_preset(const std::string& name);
    void reset_layout();

    /** Computes dynamic rectangles for slots Top..Bottom, collapsing hidden slots completely. */
    void compute_slot_rects(juce::Rectangle<int> workspace_area,
                            int transport_height,
                            std::array<juce::Rectangle<int>, 5>& out_slot_rects,
                            std::array<bool, 5>& out_slot_occupied,
                            bool& out_show_left_splitter,
                            bool& out_show_right_splitter,
                            bool& out_show_bottom_splitter);

    std::string serialize_state_json() const;
    bool deserialize_state_json(const std::string& json_str);

    static const char* panel_key(DockPanelId id) noexcept;
    static const char* panel_label(DockPanelId id) noexcept;
    static const char* slot_key(DockSlotId slot) noexcept;
    static const char* slot_label(DockSlotId slot) noexcept;
    static bool is_toolbar_panel(DockPanelId id) noexcept
    {
        return id == DockPanelId::Transport || id == DockPanelId::ProjectToolbar;
    }
    static bool can_close_panel(DockPanelId id) noexcept
    {
        return !is_toolbar_panel(id);
    }
    static std::unique_ptr<juce::DocumentWindow> create_floating_host(
        const juce::String& title,
        juce::Component& panel,
        juce::LookAndFeel& look,
        std::function<void()> on_close,
        std::function<bool(const juce::KeyPress&)> on_key,
        bool is_toolbar = false);

    void layout_bounds(juce::Rectangle<int> total_area,
                       juce::Component* transport_bar,
                       int transport_height,
                       juce::Component* left_panel,
                       SplitterBar* left_splitter,
                       juce::Component* center_panel,
                       SplitterBar* right_splitter,
                       juce::Component* right_panel,
                       SplitterBar* bottom_splitter,
                       juce::Component* bottom_deck);

private:
    void init_builtin_presets();
    DockPanelId effective_active_panel_for_slot(DockSlotId slot) const noexcept;

    DockingDimensions dims_;
    TabSlot bottomSlot_;
    std::vector<DockWorkspacePreset> presets_;
};

} // namespace oae::studio
