#include "DockingLayoutManager.h"
#include "OaeDarkLookAndFeel.h"
#include "DockTabButton.h"
#include "Testing/UiTestHooks.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace oae::studio {

namespace {

std::string slot_to_str(DockSlotId slot)
{
    switch (slot) {
        case DockSlotId::Top: return "top";
        case DockSlotId::Left: return "left";
        case DockSlotId::Center: return "center";
        case DockSlotId::Right: return "right";
        case DockSlotId::Bottom: return "bottom";
        case DockSlotId::Floating: return "floating";
        default: return "center";
    }
}

DockSlotId str_to_slot(const std::string& s, DockSlotId fallback)
{
    if (s == "top") return DockSlotId::Top;
    if (s == "left") return DockSlotId::Left;
    if (s == "center") return DockSlotId::Center;
    if (s == "right") return DockSlotId::Right;
    if (s == "bottom") return DockSlotId::Bottom;
    if (s == "floating") return DockSlotId::Floating;
    return fallback;
}

nlohmann::json dims_to_json_obj(const DockingDimensions& d)
{
    nlohmann::json j;
    j["version"] = 2;
    j["left_width"] = d.left_width;
    j["right_width"] = d.right_width;
    j["bottom_height"] = d.bottom_height;
    j["top_height"] = d.top_height;
    j["active_preset"] = d.active_preset;
    j["hide_all_active"] = d.hide_all_active;

    nlohmann::json preHide = nlohmann::json::array();
    for (bool v : d.pre_hide_visible) preHide.push_back(v);
    j["pre_hide_visible"] = preHide;

    nlohmann::json tabs = nlohmann::json::array();
    for (int t : d.active_tab_in_slot) tabs.push_back(t);
    j["active_tab_in_slot"] = tabs;

    nlohmann::json panelsJson = nlohmann::json::object();
    const std::array<DockPanelId, 5> ids = {
        DockPanelId::Transport, DockPanelId::Browser, DockPanelId::Timeline,
        DockPanelId::Inspector, DockPanelId::BottomDeck
    };
    for (size_t i = 0; i < ids.size(); ++i) {
        const char* k = DockingLayoutManager::panel_key(ids[i]);
        panelsJson[k] = {
            { "visible", d.panels[i].visible },
            { "slot", slot_to_str(d.panels[i].slot) }
        };
    }
    j["panels"] = panelsJson;
    return j;
}

bool json_obj_to_dims(const nlohmann::json& j, DockingDimensions& out)
{
    if (!j.is_object()) return false;
    if (j.contains("left_width") && j["left_width"].is_number_integer()) {
        out.left_width = j["left_width"].get<int>();
    }
    if (j.contains("right_width") && j["right_width"].is_number_integer()) {
        out.right_width = j["right_width"].get<int>();
    }
    if (j.contains("bottom_height") && j["bottom_height"].is_number_integer()) {
        out.bottom_height = j["bottom_height"].get<int>();
    }
    if (j.contains("top_height") && j["top_height"].is_number_integer()) {
        out.top_height = j["top_height"].get<int>();
    }
    if (j.contains("active_preset") && j["active_preset"].is_string()) {
        out.active_preset = j["active_preset"].get<std::string>();
    }
    if (j.contains("hide_all_active") && j["hide_all_active"].is_boolean()) {
        out.hide_all_active = j["hide_all_active"].get<bool>();
    }
    if (j.contains("pre_hide_visible") && j["pre_hide_visible"].is_array()) {
        for (size_t i = 0; i < std::min<size_t>(5, j["pre_hide_visible"].size()); ++i) {
            if (j["pre_hide_visible"][i].is_boolean()) {
                out.pre_hide_visible[i] = j["pre_hide_visible"][i].get<bool>();
            }
        }
    }
    if (j.contains("active_tab_in_slot") && j["active_tab_in_slot"].is_array()) {
        for (size_t i = 0; i < std::min<size_t>(5, j["active_tab_in_slot"].size()); ++i) {
            if (j["active_tab_in_slot"][i].is_number_integer()) {
                out.active_tab_in_slot[i] = std::clamp(j["active_tab_in_slot"][i].get<int>(), 0, 4);
            }
        }
    }
    if (j.contains("panels") && j["panels"].is_object()) {
        const auto& pj = j["panels"];
        const std::array<DockPanelId, 5> ids = {
            DockPanelId::Transport, DockPanelId::Browser, DockPanelId::Timeline,
            DockPanelId::Inspector, DockPanelId::BottomDeck
        };
        const std::array<DockSlotId, 5> defSlots = {
            DockSlotId::Top, DockSlotId::Left, DockSlotId::Center,
            DockSlotId::Right, DockSlotId::Bottom
        };
        for (size_t i = 0; i < ids.size(); ++i) {
            const char* k = DockingLayoutManager::panel_key(ids[i]);
            if (pj.contains(k) && pj[k].is_object()) {
                if (pj[k].contains("visible") && pj[k]["visible"].is_boolean()) {
                    out.panels[i].visible = pj[k]["visible"].get<bool>();
                }
                if (pj[k].contains("slot") && pj[k]["slot"].is_string()) {
                    out.panels[i].slot = str_to_slot(pj[k]["slot"].get<std::string>(), defSlots[i]);
                }
            }
        }
    }
    return true;
}

} // namespace

void DockingDimensions::clamp(int total_w, int total_h) noexcept
{
    const int available_w = std::max(min_left_width + min_right_width, total_w - 100);
    const int max_allowed_left = std::max(min_left_width, std::min(max_left_width, available_w / 2));
    const int max_allowed_right = std::max(min_right_width, std::min(max_right_width, available_w / 2));
    const int max_allowed_bottom = std::max(min_bottom_height, std::min(max_bottom_height, std::max(min_bottom_height, total_h - 150)));

    left_width = std::clamp(left_width, min_left_width, max_allowed_left);
    right_width = std::clamp(right_width, min_right_width, max_allowed_right);
    bottom_height = std::clamp(bottom_height, min_bottom_height, max_allowed_bottom);
    top_height = std::clamp(top_height, 32, std::max(64, total_h / 3));
}

std::string DockingDimensions::to_json() const
{
    return dims_to_json_obj(*this).dump(2);
}

bool DockingDimensions::from_json(const std::string& json_str)
{
    try {
        const auto j = nlohmann::json::parse(json_str);
        return json_obj_to_dims(j, *this);
    } catch (...) {
        return false;
    }
}

SplitterBar::SplitterBar(Orientation orientation)
    : orientation_(orientation)
{
    setRepaintsOnMouseActivity(true);
}

void SplitterBar::paint(juce::Graphics& g)
{
    juce::Colour lineCol = juce::Colour(OaePalette::borderSubtle);
    if (is_dragging_) {
        lineCol = juce::Colour(OaePalette::accentCyan);
    } else if (is_mouse_over_) {
        lineCol = juce::Colour(OaePalette::borderActive);
    }

    g.fillAll(lineCol);
}

void SplitterBar::mouseEnter(const juce::MouseEvent&)
{
    is_mouse_over_ = true;
    setMouseCursor(orientation_ == Orientation::Vertical ? juce::MouseCursor::LeftRightResizeCursor
                                                         : juce::MouseCursor::UpDownResizeCursor);
    repaint();
}

void SplitterBar::mouseExit(const juce::MouseEvent&)
{
    is_mouse_over_ = false;
    if (!is_dragging_) {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    repaint();
}

void SplitterBar::mouseDown(const juce::MouseEvent& e)
{
    is_dragging_ = true;
    drag_start_mouse_ = e.getPosition();
    repaint();
}

void SplitterBar::mouseDrag(const juce::MouseEvent& e)
{
    if (!is_dragging_) return;

    if (orientation_ == Orientation::Vertical) {
        const int delta_x = e.getPosition().x - drag_start_mouse_.x;
        if (delta_x != 0 && onSplitterDragged) {
            onSplitterDragged(delta_x);
        }
    } else {
        const int delta_y = e.getPosition().y - drag_start_mouse_.y;
        if (delta_y != 0 && onSplitterDragged) {
            onSplitterDragged(delta_y);
        }
    }
}

void SplitterBar::mouseUp(const juce::MouseEvent&)
{
    is_dragging_ = false;
    if (!is_mouse_over_) {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    repaint();
    if (onSplitterDragEnded) {
        onSplitterDragEnded();
    }
}

namespace {

/** Wrapper content for floating toolbar docks providing a dedicated Dock Back button. */
class ToolbarFloatingContent final : public juce::Component {
public:
    ToolbarFloatingContent(juce::Component& panel, std::function<void()> dockBack)
        : panel_(panel)
        , dockBackButton_("Dock Back")
    {
        addAndMakeVisible(panel_);
        dockBackButton_.setComponentID("btn_floating_dock_back");
        dockBackButton_.setTooltip("Re-attach toolbar to the top of the main window");
        dockBackButton_.onClick = [cb = std::move(dockBack)] {
            if (cb) {
                juce::MessageManager::callAsync(cb);
            }
        };
        addAndMakeVisible(dockBackButton_);
    }

    ~ToolbarFloatingContent() override
    {
        removeChildComponent(&panel_);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto actionArea = area.removeFromRight(88).reduced(4, 4);
        dockBackButton_.setBounds(actionArea);
        panel_.setBounds(area);
    }

private:
    juce::Component& panel_;
    juce::TextButton dockBackButton_;
};

/** Hosts a panel non-owningly. Closing the window requests reintegration on the message thread. */
class OaeFloatingPanelWindow final : public juce::DocumentWindow {
public:
    OaeFloatingPanelWindow(const juce::String& title,
                           juce::Component& panel,
                           juce::LookAndFeel& look,
                           std::function<void()> dockBack,
                           std::function<bool(const juce::KeyPress&)> handleKey,
                           bool isToolbar = false)
        : DocumentWindow(title,
                         juce::Colour(OaePalette::bgSurface),
                         isToolbar ? 0 : DocumentWindow::closeButton,
                         false),
          dockBack_(std::move(dockBack)),
          handleKey_(std::move(handleKey)),
          isToolbar_(isToolbar)
    {
        setLookAndFeel(&look);
        setUsingNativeTitleBar(true);
        if (isToolbar_) {
            toolbarContent_ = std::make_unique<ToolbarFloatingContent>(panel, dockBack_);
            setContentNonOwned(toolbarContent_.get(), false);
            setResizable(true, true);
            setResizeLimits(240, 42, 16384, 140);
            const int targetW = std::max(360, panel.getWidth() + 96);
            centreWithSize(targetW, 74);
        } else {
            setContentNonOwned(&panel, false);
            setResizable(true, true);
            setResizeLimits(480, 220, 16384, 16384);
            centreWithSize(800, 360);
        }
        addToDesktop();
        setVisible(true);
    }

    ~OaeFloatingPanelWindow() override
    {
        clearContentComponent();
        toolbarContent_.reset();
        setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override
    {
        if (isToolbar_) {
            return;
        }
        juce::Component::SafePointer<OaeFloatingPanelWindow> window(this);
        juce::MessageManager::callAsync([window] {
            if (window != nullptr) {
                const auto dockBack = window->dockBack_;
                dockBack();
            }
        });
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        return (handleKey_ && handleKey_(key)) || DocumentWindow::keyPressed(key);
    }

private:
    std::function<void()> dockBack_;
    std::function<bool(const juce::KeyPress&)> handleKey_;
    bool isToolbar_{ false };
    std::unique_ptr<ToolbarFloatingContent> toolbarContent_;
};

}

struct DockingLayoutManager::TabSlot::Panel {
    juce::Component::SafePointer<juce::Component> component;
    DockTabButton tab{ "" };
    bool available{ true };
    std::unique_ptr<OaeFloatingPanelWindow> window;
};

DockingLayoutManager::TabSlot::TabSlot()
{
    addAndMakeVisible(tabs_);
    addAndMakeVisible(content_);
    tabs_.setComponentID("deck_tab_bar");
    tabs_.addAndMakeVisible(tabViewport_);
    tabViewport_.setViewedComponent(&tabStrip_, false);
    tabViewport_.setScrollBarsShown(false, true);
    tabViewport_.setScrollBarThickness(OaeTheme::Spacing::unit);
    tabs_.addChildComponent(tabMenu_);
    tabs_.addAndMakeVisible(popOut_);
    tabs_.addMouseListener(this, true);
    popOut_.setComponentID("btn_dock_pop_out");
    tabMenu_.setComponentID("btn_dock_tabs_menu");

    auto dragBtn = std::make_unique<DockTabButton>("BOTTOM DECK");
    dragBtn->setNeutralTitle(true);
    dragBtn->setComponentID("btn_slot_tab_bottom_deck");
    dragBtn->setTooltip("Drag to dock Bottom Deck");
    dragBtn->onTabDrag = [this](const juce::MouseEvent& event) {
        if (onHeaderDrag) onHeaderDrag(event);
    };
    dragBtn->onTabDrop = [this](const juce::MouseEvent& event) {
        if (onHeaderDrop) onHeaderDrop(event);
    };
    tabs_.addChildComponent(*dragBtn);
    headerDragHandle_ = std::move(dragBtn);
    popOut_.onClick = [this] {
        if (selected_ < 0)
            return;
        const auto& selected = *panels_[static_cast<size_t>(selected_)];
        if (selected.tab.getComponentID() == "btn_tab_diagnostics" && onDiagnosticsPopOut) {
            onDiagnosticsPopOut();
            return;
        }
        if (selected.window)
            dock_back(selected_);
        else
            pop_out(selected_);
    };
    tabMenu_.onClick = [this] { show_tab_menu(); };
}

DockingLayoutManager::TabSlot::~TabSlot()
{
    clear();
}

void DockingLayoutManager::TabSlot::add_panel(juce::Component& component,
                                             const juce::String& name, const juce::String& tab_id)
{
    const int index = static_cast<int>(panels_.size());
    auto panel = std::make_unique<Panel>();
    panel->component = &component;
    panel->tab.setButtonText(name);
    panel->tab.setComponentID(tab_id);
    panel->tab.setActiveTextColour(juce::Colour(OaeTheme::Colors::accentCyan));
    if (tab_id == "btn_tab_diagnostics") {
        panel->tab.onTabDrag = [this](const juce::MouseEvent& event) {
            if (onDiagnosticsDrag) onDiagnosticsDrag(event);
        };
        panel->tab.onTabDrop = [this](const juce::MouseEvent& event) {
            if (onDiagnosticsDrop) onDiagnosticsDrop(event);
        };
    }
    panel->tab.onClick = [this, index] {
        select(index);
        if (auto* window = panels_[static_cast<size_t>(index)]->window.get())
            window->toFront(true);
    };
    tabStrip_.addAndMakeVisible(panel->tab);
    content_.addChildComponent(component);
    panels_.push_back(std::move(panel));
    order_.push_back(index);
    if (selected_ < 0)
        selected_ = index;
    update_selection();
}

void DockingLayoutManager::TabSlot::clear()
{
    for (auto& panel : panels_) {
        panel->window.reset();
        if (auto* component = panel->component.getComponent())
            content_.removeChildComponent(component);
    }
    panels_.clear();
    order_.clear();
    selected_ = -1;
    update_selection();
}

void DockingLayoutManager::TabSlot::set_panel_title(int panel_index, const juce::String& title)
{
    if (panel_index < 0 || static_cast<size_t>(panel_index) >= panels_.size())
        return;
    panels_[static_cast<size_t>(panel_index)]->tab.setButtonText(title);
    resized();
}

void DockingLayoutManager::TabSlot::set_embedded_dock_header(bool embedded)
{
    if (embeddedDockHeader_ == embedded)
        return;
    embeddedDockHeader_ = embedded;
    if (headerDragHandle_) {
        headerDragHandle_->setVisible(embeddedDockHeader_);
    }
    resized();
}

void DockingLayoutManager::TabSlot::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && tabs_.getBounds().contains(event.getEventRelativeTo(this).getPosition())) {
        show_tab_menu();
    }
}

void DockingLayoutManager::TabSlot::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(OaeTheme::Colors::dockHeaderBg));
    g.fillRect(0, 0, getWidth(), 34);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(33, 0.0f, static_cast<float>(getWidth()));
}

void DockingLayoutManager::TabSlot::set_panel_available(int index, bool available)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(panels_.size()))) return;
    auto& panel = *panels_[static_cast<size_t>(index)];
    if (panel.available == available) return;
    panel.available = available;
    panel.tab.setVisible(available);
    if (available) {
        if (auto* component = panel.component.getComponent()) content_.addChildComponent(component);
    } else {
        panel.window.reset();
        if (auto* component = panel.component.getComponent()) content_.removeChildComponent(component);
        if (selected_ == index) selected_ = -1;
    }
    if (selected_ < 0) {
        const auto next = std::find_if(order_.begin(), order_.end(),
            [this](int candidate) { return panels_[static_cast<size_t>(candidate)]->available; });
        if (next != order_.end()) selected_ = *next;
    }
    update_selection();
}

void DockingLayoutManager::TabSlot::select(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(panels_.size())))
        return;
    if (!panels_[static_cast<size_t>(index)]->available) return;
    selected_ = index;
    update_selection();
}

void DockingLayoutManager::TabSlot::pop_out(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(panels_.size())))
        return;
    auto& panel = *panels_[static_cast<size_t>(index)];
    if (!panel.available || panel.window || panel.component == nullptr)
        return;
    panel.window = std::make_unique<OaeFloatingPanelWindow>(
        panel.tab.getButtonText(), *panel.component, getLookAndFeel(),
        [this, index] { dock_back(index); }, onFloatingKeyPressed);
    update_selection();
}

void DockingLayoutManager::TabSlot::dock_back(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(panels_.size())))
        return;
    auto& panel = *panels_[static_cast<size_t>(index)];
    if (!panel.window)
        return;
    panel.window.reset();
    if (auto* component = panel.component.getComponent())
        content_.addChildComponent(component);
    select(index);
}

void DockingLayoutManager::TabSlot::move_tab(int offset)
{
    const auto current = std::find(order_.begin(), order_.end(), selected_);
    if (current == order_.end())
        return;
    const auto next = std::distance(order_.begin(), current) + offset;
    if (next >= 0 && next < static_cast<int>(order_.size()) && offset != 0) {
        std::rotate(std::min(current, order_.begin() + next),
                    offset < 0 ? current : std::next(current),
                    std::next(std::max(current, order_.begin() + next)));
        resized();
    }
}

void DockingLayoutManager::TabSlot::show_tab_menu()
{
    juce::PopupMenu menu;
    const auto current = std::find(order_.begin(), order_.end(), selected_);
    menu.addItem(1, "Move active tab left", current != order_.end() && current != order_.begin());
    menu.addItem(2, "Move active tab right", current != order_.end() && std::next(current) != order_.end());
    menu.addSeparator();
    for (int index : order_) {
        const auto& panel = *panels_[static_cast<size_t>(index)];
        if (!panel.available) continue;
        auto name = panel.tab.getButtonText();
        if (panel.window)
            name += " (floating)";
        menu.addItem(index + 3, name, true, index == selected_);
    }
    juce::Component::SafePointer<TabSlot> slot(this);
    UiTestDialogSeam::show_popup_menu("dock_tab_menu", menu,
        juce::PopupMenu::Options().withTargetComponent(tabMenu_), [slot](int result) {
        if (slot == nullptr || result == 0)
            return;
        if (result < 3)
            slot->move_tab(result == 1 ? -1 : 1);
        else if (juce::isPositiveAndBelow(result - 3, static_cast<int>(slot->panels_.size())))
            slot->panels_[static_cast<size_t>(result - 3)]->tab.triggerClick();
    });
}

void DockingLayoutManager::TabSlot::update_selection()
{
    for (size_t i = 0; i < panels_.size(); ++i) {
        auto& panel = *panels_[i];
        const bool active = static_cast<int>(i) == selected_;
        panel.tab.setTabSelected(active);
        panel.tab.setToggleState(active, juce::dontSendNotification);
        if (panel.available && !panel.window && panel.component != nullptr)
            panel.component->setVisible(active);
    }
    popOut_.setEnabled(selected_ >= 0);
    popOut_.setButtonText(selected_ >= 0 && panels_[static_cast<size_t>(selected_)]->window
        ? "Dock Back" : "Pop-Out ^");
    resized();
}

void DockingLayoutManager::TabSlot::resized()
{
    constexpr int gap = OaeTheme::Spacing::unit;
    constexpr int headerHeight = 34;
    constexpr int tabHeight = 22;
    auto area = getLocalBounds();
    tabs_.setBounds(area.removeFromTop(headerHeight).reduced(8, 6));
    content_.setBounds(area);

    juce::FlexBox header;
    header.alignItems = juce::FlexBox::AlignItems::center;
    if (embeddedDockHeader_ && headerDragHandle_) {
        header.items.add(juce::FlexItem(*headerDragHandle_).withWidth(94.0f).withHeight(static_cast<float>(tabHeight)).withMargin({ 0, 8, 0, 0 }));
    }
    header.items.add(juce::FlexItem(tabViewport_).withFlex(1).withHeight(static_cast<float>(tabHeight)).withMargin({ 0, gap, 0, 0 }));
    header.items.add(juce::FlexItem(popOut_).withWidth(78.0f).withHeight(24.0f));
    header.performLayout(tabs_.getLocalBounds());

    const juce::Font tabFont{ juce::FontOptions(OaeTheme::Typography::compact) };
    juce::GlyphArrangement glyphs;
    int tabStripWidth = 0;
    juce::FlexBox tabs;
    for (int index : order_) {
        auto& panel = *panels_[static_cast<size_t>(index)];
        if (panel.available) {
            glyphs.clear();
            glyphs.addLineOfText(tabFont, panel.tab.getButtonText(), 0.0f, 0.0f);
            const int w = static_cast<int>(std::clamp(glyphs.getBoundingBox(0, -1, false).getWidth() + 20.0f, 76.0f, 150.0f));
            tabs.items.add(juce::FlexItem(panel.tab).withWidth(static_cast<float>(w)).withHeight(static_cast<float>(tabHeight)).withMargin({ 0, gap, 0, 0 }));
            tabStripWidth += w + gap;
        }
    }
    tabStrip_.setSize(std::max(tabViewport_.getWidth(), tabStripWidth), tabHeight);
    tabs.performLayout(tabStrip_.getLocalBounds());

    if (selected_ >= 0) {
        auto& panel = *panels_[static_cast<size_t>(selected_)];
        if (panel.available && !panel.window && panel.component != nullptr) {
            juce::FlexBox body;
            body.items.add(juce::FlexItem(*panel.component).withFlex(1));
            body.performLayout(content_.getLocalBounds());
        }
    }
}

namespace {

using Bounds = juce::Rectangle<int>;
constexpr int divider_size = 4;

WorkspaceNode* find_node(WorkspaceNode& node, int id)
{
    if (node.id == id) return &node;
    for (auto& child : node.children)
        if (auto* result = find_node(child, id)) return result;
    return nullptr;
}

const WorkspaceNode* panel_group(const WorkspaceNode& node, DockPanelId panel)
{
    if (std::find(node.tabs.begin(), node.tabs.end(), panel) != node.tabs.end()) return &node;
    for (const auto& child : node.children)
        if (auto* result = panel_group(child, panel)) return result;
    return nullptr;
}

int max_id(const WorkspaceNode& node)
{
    int id = node.id;
    for (const auto& child : node.children) id = std::max(id, max_id(child));
    return id;
}

WorkspaceNode split_node(int id, bool vertical, WorkspaceNode first, WorkspaceNode second, int extent = 0)
{
    WorkspaceNode result;
    result.id = id;
    result.vertical = vertical;
    result.extent = extent;
    result.children.push_back(std::move(first));
    result.children.push_back(std::move(second));
    return result;
}

void collapse_empty_groups(WorkspaceNode& node)
{
    if (node.children.empty()) return;
    for (auto& child : node.children) collapse_empty_groups(child);
    node.children.erase(std::remove_if(node.children.begin(), node.children.end(),
        [](const auto& child) { return child.children.empty() && child.tabs.empty(); }), node.children.end());
    if (node.children.size() == 1) {
        auto child = std::move(node.children.front());
        node = std::move(child);
    }
}

bool move_panel(DockingDimensions& state, DockPanelId panel, WorkspaceDrop drop)
{
    const auto* source = panel_group(state.tree, panel);
    const int sourceId = source ? source->id : 0;
    const auto* target = drop.group == 0 ? &state.tree : find_node(state.tree, drop.group);
    if (!target || (drop.edge == DockEdge::Tabs && (drop.group == 0 || !target->children.empty())))
        return false;
    if (drop.edge != DockEdge::Tabs && source && target == source && source->tabs.size() == 1)
        return false;

    if (drop.edge == DockEdge::Tabs) {
        int visible = 0;
        int index = 0;
        for (auto tab : target->tabs) {
            const auto& item = state.panels[static_cast<size_t>(tab)];
            if (item.visible && item.slot != DockSlotId::Floating && visible++ == drop.index) break;
            ++index;
        }
        drop.index = index;
    }

    int nextId = max_id(state.tree) + 1;
    if (sourceId != 0) {
        auto& origin = *find_node(state.tree, sourceId);
        const auto pos = std::find(origin.tabs.begin(), origin.tabs.end(), panel);
        if (drop.edge == DockEdge::Tabs && drop.group == sourceId
            && drop.index > std::distance(origin.tabs.begin(), pos))
            --drop.index;
        origin.tabs.erase(pos);
        if (origin.active == panel && !origin.tabs.empty()) origin.active = origin.tabs.front();
    }

    if (drop.edge == DockEdge::Tabs) {
        auto& group = *find_node(state.tree, drop.group);
        const int index = std::clamp(drop.index, 0, static_cast<int>(group.tabs.size()));
        group.tabs.insert(group.tabs.begin() + index, panel);
        group.active = panel;
        collapse_empty_groups(state.tree);
    } else {
        collapse_empty_groups(state.tree);
        auto* destination = drop.group == 0 ? &state.tree : find_node(state.tree, drop.group);
        if (!destination) return false;
        WorkspaceNode group;
        group.id = nextId++;
        group.tabs = { panel };
        group.active = panel;
        if (destination->tabs.empty() && destination->children.empty()) {
            *destination = std::move(group);
        } else {
            auto old = std::move(*destination);
            const bool vertical = drop.edge == DockEdge::Top || drop.edge == DockEdge::Bottom;
            const bool first = drop.edge == DockEdge::Top || drop.edge == DockEdge::Left;
            *destination = first ? split_node(nextId, vertical, std::move(group), std::move(old))
                                 : split_node(nextId, vertical, std::move(old), std::move(group));
            destination->ratio = first ? 0.25 : 0.75;
        }
    }
    auto& panelState = state.panels[static_cast<size_t>(panel)];
    panelState.visible = true;
    if (panelState.slot == DockSlotId::Floating) panelState.slot = DockSlotId::Center;
    state.hide_all_active = false;
    return true;
}

bool docked_visible(const DockingDimensions& state, DockPanelId panel)
{
    const auto& item = state.panels[static_cast<size_t>(panel)];
    return item.visible && item.slot != DockSlotId::Floating;
}

juce::Point<int> minimum_size(const DockingDimensions& state, const WorkspaceNode& node)
{
    if (node.children.empty()) {
        juce::Point<int> minimum;
        for (auto panel : node.tabs) {
            if (!docked_visible(state, panel)) continue;
            const int width = panel == DockPanelId::Inspector ? DockingDimensions::min_right_width
                            : panel == DockPanelId::Browser ? DockingDimensions::min_left_width : 100;
            const int height = DockingLayoutManager::is_toolbar_panel(panel) ? 32 : 64;
            minimum.x = std::max(minimum.x, width);
            minimum.y = std::max(minimum.y, height);
        }
        return minimum;
    }
    auto a = minimum_size(state, node.children[0]);
    auto b = minimum_size(state, node.children[1]);
    if (a.x == 0) return b;
    if (b.x == 0) return a;
    return node.vertical ? juce::Point<int>{ std::max(a.x, b.x), a.y + b.y + divider_size }
                         : juce::Point<int>{ a.x + b.x + divider_size, std::max(a.y, b.y) };
}

void arrange(const DockingDimensions& state, const WorkspaceNode& node, Bounds bounds,
             int transport_height, WorkspaceGeometry& result)
{
    if (bounds.isEmpty()) return;
    if (node.children.empty()) {
        std::vector<DockPanelId> tabs;
        for (auto panel : node.tabs)
            if (docked_visible(state, panel)) tabs.push_back(panel);
        if (tabs.empty()) return;
        const auto active = std::find(tabs.begin(), tabs.end(), node.active) == tabs.end()
                          ? tabs.front() : node.active;
        result.groups.push_back({ node.id, bounds, std::move(tabs), active });
        return;
    }

    const auto a = minimum_size(state, node.children[0]);
    const auto b = minimum_size(state, node.children[1]);
    if (a.x == 0 || b.x == 0) {
        arrange(state, node.children[a.x == 0 ? 1 : 0], bounds, transport_height, result);
        return;
    }
    const int firstMin = node.vertical ? a.y : a.x;
    const int secondMin = node.vertical ? b.y : b.x;
    const int total = node.vertical ? bounds.getHeight() : bounds.getWidth();
    const int gap = std::min(divider_size, std::max(0, total - 2));
    const int available = total - gap;
    int extent = node.extent;
    const bool firstIsTopToolbar = node.vertical
        && !panel_group(node.children[0], DockPanelId::Timeline)
        && ((panel_group(node.children[0], DockPanelId::Transport) && docked_visible(state, DockPanelId::Transport))
            || (panel_group(node.children[0], DockPanelId::ProjectToolbar) && docked_visible(state, DockPanelId::ProjectToolbar)));
    if (extent > 0 && firstIsTopToolbar)
        extent = transport_height;
    const int preferred = extent > 0 ? extent : extent < 0 ? available + extent
                        : juce::roundToInt(node.ratio * available);
    const int min = available < firstMin + secondMin ? available * firstMin / (firstMin + secondMin) : firstMin;
    const int max = available < firstMin + secondMin ? min : available - secondMin;
    const int first = std::clamp(preferred, min, max);
    const auto before = node.vertical ? bounds.removeFromTop(first) : bounds.removeFromLeft(first);
    const auto divider = node.vertical ? bounds.removeFromTop(gap) : bounds.removeFromLeft(gap);
    result.dividers.push_back({ node.id, divider, node.vertical, available, first, min, max });
    arrange(state, node.children[0], before, transport_height, result);
    arrange(state, node.children[1], bounds, transport_height, result);
}

} // namespace

WorkspaceNode DockingLayoutManager::workspace_from_legacy(const DockingDimensions& state)
{
    std::array<WorkspaceNode, 5> leaves;
    for (size_t index = 0; index < leaves.size(); ++index) {
        leaves[index].id = static_cast<int>(index + 1);
        leaves[index].active = static_cast<DockPanelId>(state.active_tab_in_slot[index]);
    }
    for (size_t index = 0; index < 5; ++index) {
        const auto slot = state.panels[index].slot;
        const auto anchor = slot == DockSlotId::Floating ? index : static_cast<size_t>(slot);
        leaves[anchor].tabs.push_back(static_cast<DockPanelId>(index));
    }
    for (auto& leaf : leaves) {
        if (!leaf.tabs.empty() && !panel_group(leaf, leaf.active)) leaf.active = leaf.tabs.front();
    }
    WorkspaceNode projectLeaf;
    projectLeaf.id = 10;
    projectLeaf.active = DockPanelId::ProjectToolbar;
    projectLeaf.tabs.push_back(DockPanelId::ProjectToolbar);

    const auto projSlot = state.panels[static_cast<size_t>(DockPanelId::ProjectToolbar)].slot;
    WorkspaceNode topNode;
    if (projSlot == DockSlotId::Top || projSlot == DockSlotId::Floating) {
        topNode = split_node(11, false, std::move(projectLeaf), std::move(leaves[0]), 400);
    } else {
        const auto anchor = static_cast<size_t>(projSlot);
        leaves[anchor].tabs.push_back(DockPanelId::ProjectToolbar);
        topNode = std::move(leaves[0]);
    }

    auto middle = split_node(6, false, std::move(leaves[2]), std::move(leaves[3]),
                             -std::clamp(state.right_width, 180, 500));
    middle = split_node(7, false, std::move(leaves[1]), std::move(middle),
                        std::clamp(state.left_width, 160, 480));
    auto lower = split_node(8, true, std::move(middle), std::move(leaves[4]),
                            -std::clamp(state.bottom_height, 192, 520));
    return split_node(9, true, std::move(topNode), std::move(lower), std::max(32, state.top_height));
}

WorkspaceGeometry DockingLayoutManager::workspace_geometry(Bounds area, int transport_height) const
{
    WorkspaceGeometry result;
    arrange(dims_, dims_.tree, area, transport_height, result);
    return result;
}

int DockingLayoutManager::workspace_group(DockPanelId panel) const
{
    if (const auto* group = panel_group(dims_.tree, panel)) return group->id;
    return 0;
}

void DockingLayoutManager::select_workspace_tab(int group, DockPanelId panel)
{
    if (auto* node = find_node(dims_.tree, group); node && panel_group(*node, panel) == node)
        node->active = panel;
}

bool DockingLayoutManager::move_workspace_panel(DockPanelId panel, WorkspaceDrop target)
{
    auto candidate = dims_;
    if (!move_panel(candidate, panel, target)) return false;
    dims_ = std::move(candidate);
    return true;
}

Bounds DockingLayoutManager::preview_workspace_split(DockPanelId panel, WorkspaceDrop target,
                                                      Bounds area, int transport_height) const
{
    auto candidate = dims_;
    if (!move_panel(candidate, panel, target)) return {};
    const auto* group = panel_group(candidate.tree, panel);
    WorkspaceGeometry result;
    arrange(candidate, candidate.tree, area, transport_height, result);
    for (const auto& item : result.groups)
        if (item.id == group->id) return item.bounds;
    return {};
}

void DockingLayoutManager::resize_workspace_split(const WorkspaceDividerBounds& divider, int delta)
{
    if (divider.available <= 0 || delta == 0 || divider.min_first == divider.max_first) return;
    if (auto* node = find_node(dims_.tree, divider.id); node && !node->children.empty()) {
        const int first = std::clamp(divider.first + delta, divider.min_first, divider.max_first);
        if (first == divider.first) return;
        node->ratio = std::clamp(static_cast<double>(first) / divider.available, 0.01, 0.99);
        node->extent = 0;
    }
}

void DockingLayoutManager::dock_back_workspace_panel(DockPanelId panel)
{
    auto& state = dims_.panels[static_cast<size_t>(panel)];
    const int group = workspace_group(panel);
    state.slot = group >= 1 && group <= 5 ? static_cast<DockSlotId>(group - 1) : DockSlotId::Center;
    state.visible = true;
    select_workspace_tab(group, panel);
}

DockingLayoutManager::DockingLayoutManager()
{
    init_builtin_presets();
    reset_layout();
}

void DockingLayoutManager::init_builtin_presets()
{
    presets_.clear();

    {
        DockWorkspacePreset p;
        p.name = "Default (Authoring)";
        p.builtin = true;
        p.state = DockingDimensions{};
        p.state.active_preset = p.name;
        presets_.push_back(std::move(p));
    }

    {
        DockWorkspacePreset p;
        p.name = "Stage Focus";
        p.builtin = true;
        p.state = DockingDimensions{};
        p.state.right_width = 250;
        p.state.panels[static_cast<size_t>(DockPanelId::Transport)] = { true, DockSlotId::Top };
        p.state.panels[static_cast<size_t>(DockPanelId::Browser)] = { false, DockSlotId::Left };
        p.state.panels[static_cast<size_t>(DockPanelId::Timeline)] = { true, DockSlotId::Center };
        p.state.panels[static_cast<size_t>(DockPanelId::Inspector)] = { true, DockSlotId::Right };
        p.state.panels[static_cast<size_t>(DockPanelId::BottomDeck)] = { false, DockSlotId::Bottom };
        p.state.active_preset = p.name;
        presets_.push_back(std::move(p));
    }

    {
        DockWorkspacePreset p;
        p.name = "Mixing & Mastering";
        p.builtin = true;
        p.state = DockingDimensions{};
        p.state.left_width = 220;
        p.state.right_width = 250;
        p.state.bottom_height = 420;
        p.state.panels[static_cast<size_t>(DockPanelId::Transport)] = { true, DockSlotId::Top };
        p.state.panels[static_cast<size_t>(DockPanelId::Browser)] = { true, DockSlotId::Left };
        p.state.panels[static_cast<size_t>(DockPanelId::Timeline)] = { false, DockSlotId::Center };
        p.state.panels[static_cast<size_t>(DockPanelId::Inspector)] = { true, DockSlotId::Right };
        p.state.panels[static_cast<size_t>(DockPanelId::BottomDeck)] = { true, DockSlotId::Bottom };
        p.state.active_preset = p.name;
        presets_.push_back(std::move(p));
    }
    for (auto& preset : presets_) preset.state.tree = workspace_from_legacy(preset.state);
}

const char* DockingLayoutManager::panel_key(DockPanelId id) noexcept
{
    switch (id) {
        case DockPanelId::Transport: return "transport";
        case DockPanelId::Browser: return "browser";
        case DockPanelId::Timeline: return "timeline";
        case DockPanelId::Inspector: return "inspector";
        case DockPanelId::BottomDeck: return "bottom_deck";
        case DockPanelId::Diagnostics: return "diagnostics";
        case DockPanelId::ProjectToolbar: return "project_toolbar";
        default: return "timeline";
    }
}

const char* DockingLayoutManager::panel_label(DockPanelId id) noexcept
{
    switch (id) {
        case DockPanelId::Transport: return "Transport";
        case DockPanelId::Browser: return "Browser";
        case DockPanelId::Timeline: return "Timeline";
        case DockPanelId::Inspector: return "Inspector";
        case DockPanelId::BottomDeck: return "Bottom Deck";
        case DockPanelId::Diagnostics: return "Diagnostics & Log";
        case DockPanelId::ProjectToolbar: return "Project Toolbar";
        default: return "Panel";
    }
}

const char* DockingLayoutManager::slot_key(DockSlotId slot) noexcept
{
    switch (slot) {
        case DockSlotId::Top: return "top";
        case DockSlotId::Left: return "left";
        case DockSlotId::Center: return "center";
        case DockSlotId::Right: return "right";
        case DockSlotId::Bottom: return "bottom";
        case DockSlotId::Floating: return "floating";
        default: return "center";
    }
}

const char* DockingLayoutManager::slot_label(DockSlotId slot) noexcept
{
    switch (slot) {
        case DockSlotId::Top: return "Top";
        case DockSlotId::Left: return "Left";
        case DockSlotId::Center: return "Center";
        case DockSlotId::Right: return "Right";
        case DockSlotId::Bottom: return "Bottom";
        case DockSlotId::Floating: return "Floating";
        default: return "Center";
    }
}

std::unique_ptr<juce::DocumentWindow> DockingLayoutManager::create_floating_host(
    const juce::String& title,
    juce::Component& panel,
    juce::LookAndFeel& look,
    std::function<void()> on_close,
    std::function<bool(const juce::KeyPress&)> on_key,
    bool is_toolbar)
{
    return std::make_unique<OaeFloatingPanelWindow>(
        title, panel, look, std::move(on_close), std::move(on_key), is_toolbar);
}

bool DockingLayoutManager::is_panel_visible(DockPanelId id) const noexcept
{
    const size_t idx = static_cast<size_t>(id);
    return idx < dims_.panels.size() ? dims_.panels[idx].visible : false;
}

void DockingLayoutManager::set_panel_visible(DockPanelId id, bool visible)
{
    const size_t idx = static_cast<size_t>(id);
    if (idx >= dims_.panels.size()) return;
    if (!visible && !can_close_panel(id)) {
        dims_.panels[idx].visible = true;
        return;
    }
    dims_.panels[idx].visible = visible;
    if (visible) {
        select_workspace_tab(workspace_group(id), id);
        dims_.hide_all_active = false;
        const auto slot = dims_.panels[idx].slot;
        if (slot != DockSlotId::Floating) {
            dims_.active_tab_in_slot[static_cast<size_t>(slot)] = static_cast<int>(idx);
        }
    }
}

DockSlotId DockingLayoutManager::get_panel_slot(DockPanelId id) const noexcept
{
    const size_t idx = static_cast<size_t>(id);
    return idx < dims_.panels.size() ? dims_.panels[idx].slot : DockSlotId::Center;
}

void DockingLayoutManager::set_panel_slot(DockPanelId id, DockSlotId slot)
{
    const size_t idx = static_cast<size_t>(id);
    if (idx >= dims_.panels.size()) return;
    if (slot != DockSlotId::Floating) {
        const int group = static_cast<int>(slot) + 1;
        if (const auto* destination = find_node(dims_.tree, group); destination && destination->children.empty()) {
            move_workspace_panel(id, { group, DockEdge::Tabs, static_cast<int>(destination->tabs.size()) });
        } else {
            const std::array<DockEdge, 5> edges = {
                DockEdge::Top, DockEdge::Left, DockEdge::Right, DockEdge::Right, DockEdge::Bottom
            };
            move_workspace_panel(id, { 0, edges[static_cast<size_t>(slot)], 0 });
        }
        dims_.active_tab_in_slot[static_cast<size_t>(slot)] = static_cast<int>(idx);
    }
    dims_.panels[idx].slot = slot;
    dims_.panels[idx].visible = true;
    dims_.hide_all_active = false;
}

int DockingLayoutManager::get_active_tab_in_slot(DockSlotId slot) const noexcept
{
    return static_cast<int>(effective_active_panel_for_slot(slot));
}

void DockingLayoutManager::set_active_tab_in_slot(DockSlotId slot, DockPanelId id)
{
    select_workspace_tab(static_cast<int>(slot) + 1, id);
    const size_t sIdx = static_cast<size_t>(slot);
    if (sIdx < dims_.active_tab_in_slot.size()) {
        dims_.active_tab_in_slot[sIdx] = static_cast<int>(id);
    }
}

std::vector<DockPanelId> DockingLayoutManager::panels_in_slot(DockSlotId slot, bool visible_only) const
{
    std::vector<DockPanelId> out;
    for (size_t i = 0; i < static_cast<size_t>(DockPanelId::Diagnostics); ++i) {
        if (dims_.panels[i].slot == slot && (!visible_only || dims_.panels[i].visible)) {
            out.push_back(static_cast<DockPanelId>(i));
        }
    }
    return out;
}

DockPanelId DockingLayoutManager::effective_active_panel_for_slot(DockSlotId slot) const noexcept
{
    const size_t sIdx = static_cast<size_t>(slot);
    if (sIdx >= 5) return DockPanelId::Timeline;
    const int pref = dims_.active_tab_in_slot[sIdx];
    if (pref >= 0 && pref < 5
        && dims_.panels[static_cast<size_t>(pref)].slot == slot
        && dims_.panels[static_cast<size_t>(pref)].visible) {
        return static_cast<DockPanelId>(pref);
    }
    for (size_t i = 0; i < dims_.panels.size(); ++i) {
        if (dims_.panels[i].slot == slot && dims_.panels[i].visible) {
            return static_cast<DockPanelId>(i);
        }
    }
    return DockPanelId::Timeline;
}

void DockingLayoutManager::hide_all_panels()
{
    if (!dims_.hide_all_active) {
        for (size_t i = 0; i < dims_.panels.size(); ++i) {
            dims_.pre_hide_visible[i] = dims_.panels[i].visible;
            if (can_close_panel(static_cast<DockPanelId>(i))) {
                dims_.panels[i].visible = false;
            } else {
                dims_.panels[i].visible = true;
            }
        }
        dims_.hide_all_active = true;
    }
}

void DockingLayoutManager::restore_all_panels()
{
    bool anyRestored = false;
    for (size_t i = 0; i < dims_.panels.size(); ++i) {
        dims_.panels[i].visible = dims_.pre_hide_visible[i];
        if (dims_.panels[i].visible) anyRestored = true;
    }
    if (!anyRestored) {
        for (size_t i = 0; i < dims_.panels.size(); ++i) {
            dims_.panels[i].visible = true;
        }
    }
    dims_.hide_all_active = false;
}

std::vector<std::string> DockingLayoutManager::get_preset_names() const
{
    std::vector<std::string> names;
    names.reserve(presets_.size());
    for (const auto& p : presets_) {
        names.push_back(p.name);
    }
    return names;
}

bool DockingLayoutManager::is_builtin_preset(const std::string& name) const
{
    for (const auto& p : presets_) {
        if (p.name == name) return p.builtin;
    }
    return false;
}

bool DockingLayoutManager::apply_preset(const std::string& name)
{
    for (const auto& p : presets_) {
        if (p.name == name) {
            dims_ = p.state;
            dims_.active_preset = p.name;
            return true;
        }
    }
    return false;
}

bool DockingLayoutManager::save_custom_preset(const std::string& name)
{
    if (name.empty() || is_builtin_preset(name)) {
        return false;
    }
    DockingDimensions snap = dims_;
    snap.active_preset = name;
    for (auto& p : presets_) {
        if (p.name == name) {
            p.state = snap;
            dims_.active_preset = name;
            return true;
        }
    }
    DockWorkspacePreset created;
    created.name = name;
    created.builtin = false;
    created.state = snap;
    presets_.push_back(std::move(created));
    dims_.active_preset = name;
    return true;
}

bool DockingLayoutManager::delete_custom_preset(const std::string& name)
{
    if (name.empty() || is_builtin_preset(name)) {
        return false;
    }
    for (auto it = presets_.begin(); it != presets_.end(); ++it) {
        if (it->name == name && !it->builtin) {
            presets_.erase(it);
            if (dims_.active_preset == name) {
                dims_.active_preset = "Default (Authoring)";
            }
            return true;
        }
    }
    return false;
}

void DockingLayoutManager::reset_layout()
{
    apply_preset("Default (Authoring)");
}

void DockingLayoutManager::compute_slot_rects(juce::Rectangle<int> workspace_area,
                                              int transport_height,
                                              std::array<juce::Rectangle<int>, 5>& out_slot_rects,
                                              std::array<bool, 5>& out_slot_occupied,
                                              bool& out_show_left_splitter,
                                              bool& out_show_right_splitter,
                                              bool& out_show_bottom_splitter)
{
    dims_.clamp(workspace_area.getWidth(), workspace_area.getHeight());
    for (size_t s = 0; s < 5; ++s) {
        out_slot_rects[s] = {};
        out_slot_occupied[s] = !panels_in_slot(static_cast<DockSlotId>(s), true).empty();
    }
    out_show_left_splitter = false;
    out_show_right_splitter = false;
    out_show_bottom_splitter = false;

    auto area = workspace_area;
    constexpr int splitterThickness = 4;

    if (out_slot_occupied[static_cast<size_t>(DockSlotId::Top)]) {
        const bool has_transport_here =
            dims_.panels[static_cast<size_t>(DockPanelId::Transport)].visible
            && dims_.panels[static_cast<size_t>(DockPanelId::Transport)].slot == DockSlotId::Top;
        const int top_h = has_transport_here
            ? std::clamp(transport_height, 36, std::max(40, area.getHeight() / 2))
            : std::clamp(dims_.top_height, 36, std::max(60, area.getHeight() / 3));
        out_slot_rects[static_cast<size_t>(DockSlotId::Top)] = area.removeFromTop(top_h);
    }

    const bool has_left = out_slot_occupied[static_cast<size_t>(DockSlotId::Left)];
    const bool has_center = out_slot_occupied[static_cast<size_t>(DockSlotId::Center)];
    const bool has_right = out_slot_occupied[static_cast<size_t>(DockSlotId::Right)];
    const bool has_bottom = out_slot_occupied[static_cast<size_t>(DockSlotId::Bottom)];
    const bool has_middle = has_left || has_center || has_right;

    if (has_bottom) {
        if (!has_middle) {
            out_slot_rects[static_cast<size_t>(DockSlotId::Bottom)] = area;
            return;
        }
        const int avail_b = std::max(0, area.getHeight() - 100 - splitterThickness);
        const int bh = std::min(dims_.bottom_height, avail_b);
        out_slot_rects[static_cast<size_t>(DockSlotId::Bottom)] = area.removeFromBottom(bh);
        area.removeFromBottom(splitterThickness);
        out_show_bottom_splitter = true;
    }

    if (!has_middle) {
        return;
    }

    if (has_center) {
        if (has_left) {
            const int lw = std::min(dims_.left_width, std::max(60, area.getWidth() - splitterThickness - 120));
            out_slot_rects[static_cast<size_t>(DockSlotId::Left)] = area.removeFromLeft(lw);
            area.removeFromLeft(splitterThickness);
            out_show_left_splitter = true;
        }
        if (has_right) {
            const int rw = std::min(dims_.right_width, std::max(60, area.getWidth() - splitterThickness - 120));
            out_slot_rects[static_cast<size_t>(DockSlotId::Right)] = area.removeFromRight(rw);
            area.removeFromRight(splitterThickness);
            out_show_right_splitter = true;
        }
        out_slot_rects[static_cast<size_t>(DockSlotId::Center)] = area;
    } else {
        // Center is hidden: expand adjacent Left/Right panels cleanly across full width
        if (has_left && has_right) {
            const int lw = std::min(dims_.left_width, std::max(80, area.getWidth() - splitterThickness - 120));
            out_slot_rects[static_cast<size_t>(DockSlotId::Left)] = area.removeFromLeft(lw);
            area.removeFromLeft(splitterThickness);
            out_show_left_splitter = true;
            out_slot_rects[static_cast<size_t>(DockSlotId::Right)] = area;
        } else if (has_left) {
            out_slot_rects[static_cast<size_t>(DockSlotId::Left)] = area;
        } else if (has_right) {
            out_slot_rects[static_cast<size_t>(DockSlotId::Right)] = area;
        }
    }
}

void DockingLayoutManager::layout_bounds(juce::Rectangle<int> total_area,
                                        juce::Component* transport_bar,
                                        int transport_height,
                                        juce::Component* left_panel,
                                        SplitterBar* left_splitter,
                                        juce::Component* center_panel,
                                        SplitterBar* right_splitter,
                                        juce::Component* right_panel,
                                        SplitterBar* bottom_splitter,
                                        juce::Component* bottom_deck)
{
    std::array<juce::Rectangle<int>, 5> rects;
    std::array<bool, 5> occ;
    bool show_l = false, show_r = false, show_b = false;
    compute_slot_rects(total_area, transport_height, rects, occ, show_l, show_r, show_b);

    const auto assign = [](juce::Component* comp, const juce::Rectangle<int>& r, bool vis) {
        if (!comp) return;
        comp->setVisible(vis);
        comp->setBounds(vis ? r : juce::Rectangle<int>{});
    };

    assign(transport_bar, rects[static_cast<size_t>(DockSlotId::Top)], occ[static_cast<size_t>(DockSlotId::Top)]);
    assign(left_panel, rects[static_cast<size_t>(DockSlotId::Left)], occ[static_cast<size_t>(DockSlotId::Left)]);
    assign(center_panel, rects[static_cast<size_t>(DockSlotId::Center)], occ[static_cast<size_t>(DockSlotId::Center)]);
    assign(right_panel, rects[static_cast<size_t>(DockSlotId::Right)], occ[static_cast<size_t>(DockSlotId::Right)]);
    assign(bottom_deck, rects[static_cast<size_t>(DockSlotId::Bottom)], occ[static_cast<size_t>(DockSlotId::Bottom)]);

    constexpr int splitterThickness = 4;
    if (bottom_splitter) {
        bottom_splitter->setVisible(show_b);
        if (show_b) {
            const auto& br = rects[static_cast<size_t>(DockSlotId::Bottom)];
            bottom_splitter->setBounds(br.getX(), br.getY() - splitterThickness, br.getWidth(), splitterThickness);
        }
    }
    if (left_splitter) {
        left_splitter->setVisible(show_l);
        if (show_l) {
            const auto& lr = rects[static_cast<size_t>(DockSlotId::Left)];
            left_splitter->setBounds(lr.getRight(), lr.getY(), splitterThickness, lr.getHeight());
        }
    }
    if (right_splitter) {
        right_splitter->setVisible(show_r);
        if (show_r) {
            const auto& rr = rects[static_cast<size_t>(DockSlotId::Right)];
            right_splitter->setBounds(rr.getX() - splitterThickness, rr.getY(), splitterThickness, rr.getHeight());
        }
    }
}

} // namespace oae::studio
