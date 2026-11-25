#include "MainComponent.h"
#include "Testing/UiTestHooks.h"
#include "UI/DockTabButton.h"
#include "UI/OaePrimitives.h"
#include <algorithm>

namespace oae::studio {

DiagnosticsLogPanel::DiagnosticsLogPanel()
{
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(logEditor);
    logEditor.setComponentID("diagnostics_log_editor");

    headerLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::darkgrey);

    logEditor.setMultiLine(true);
    logEditor.setReadOnly(true);
    logEditor.setCaretVisible(false);
    logEditor.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
    logEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    logEditor.setColour(juce::TextEditor::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
}

void DiagnosticsLogPanel::resized()
{
    auto area = getLocalBounds().reduced(2);
    headerLabel.setBounds(area.removeFromTop(20));
    logEditor.setBounds(area);
}

void DiagnosticsLogPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRect(getLocalBounds(), 1);
}

void DiagnosticsLogPanel::append_message(const juce::String& text)
{
    logEditor.moveCaretToEnd();
    logEditor.insertTextAtCaret(text + "\n");
}

void DiagnosticsLogPanel::clear()
{
    logEditor.clear();
}

namespace {

fs::path find_bankc()
{
    const auto app_file = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto dir = fs::u8path(app_file.getParentDirectory().getFullPathName().toStdString());
    const std::vector<fs::path> candidates = {
        dir / "oae_bankc.exe",
        dir / "oae_bankc",
        dir / "Debug" / "oae_bankc.exe",
        dir / "Release" / "oae_bankc.exe",
        dir / ".." / "Debug" / "oae_bankc.exe",
        dir / ".." / "Release" / "oae_bankc.exe"
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) return fs::canonical(c);
    }
    return {};
}

} // namespace

class MainComponent::WorkspaceBar : public juce::Component {
public:
    explicit WorkspaceBar(MainComponent& owner)
        : owner_(owner)
    {
        setComponentID("workspace_bar");

        addAndMakeVisible(titleLabel_);
        titleLabel_.setText("WORKSPACE", juce::dontSendNotification);
        titleLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
        titleLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));

        const std::array<std::tuple<DockPanelId, const char*, const char*>, 5> visDefs = {{
            { DockPanelId::Transport, "Trn", "btn_vis_transport" },
            { DockPanelId::Browser, "Brw", "btn_vis_browser" },
            { DockPanelId::Timeline, "Stg", "btn_vis_timeline" },
            { DockPanelId::Inspector, "Ins", "btn_vis_inspector" },
            { DockPanelId::BottomDeck, "Dck", "btn_vis_bottom_deck" }
        }};
        for (size_t i = 0; i < visDefs.size(); ++i) {
            const auto pid = std::get<0>(visDefs[i]);
            auto& btn = visButtons_[i];
            btn.setButtonText(std::get<1>(visDefs[i]));
            btn.setComponentID(std::get<2>(visDefs[i]));
            btn.setTooltip(DockingLayoutManager::is_toolbar_panel(pid)
                ? juce::String("Minimize/Expand or Focus ") + DockingLayoutManager::panel_label(pid)
                : juce::String("Toggle ") + DockingLayoutManager::panel_label(pid));
            btn.onClick = [this, pid]() { toggle_panel(pid); };
            addAndMakeVisible(btn);
        }

        addAndMakeVisible(presetLabel_);
        presetLabel_.setText("Preset:", juce::dontSendNotification);
        presetLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption));
        presetLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textMuted));
        presetLabel_.setJustificationType(juce::Justification::centredRight);

        presetCombo_.setComponentID("combo_workspace_preset");
        presetCombo_.setTooltip("Active workspace layout preset");
        presetCombo_.onChange = [this]() {
            if (syncing_ || presetCombo_.getSelectedId() <= 0) return;
            const auto chosen = presetCombo_.getText().toStdString();
            if (!chosen.empty()) {
                owner_.docking_layout().apply_preset(chosen);
                owner_.sync_workspace_layout();
            }
        };
        addAndMakeVisible(presetCombo_);

        savePresetButton_.setComponentID("btn_preset_save");
        savePresetButton_.setTooltip("Save current arrangement as custom workspace preset");
        savePresetButton_.onClick = [this]() { owner_.prompt_save_custom_preset(); };
        addAndMakeVisible(savePresetButton_);

        menuButton_.setComponentID("btn_workspace_menu");
        menuButton_.setTooltip("Workspace actions: Log, Prj, Hide All / Restore, Dock, Delete & Reset");
        menuButton_.onClick = [this]() { show_workspace_menu(); };
        addAndMakeVisible(menuButton_);
    }

    void toggle_panel(DockPanelId pid)
    {
        auto& dl = owner_.docking_layout();
        if (DockingLayoutManager::is_toolbar_panel(pid)) {
            if (dl.get_panel_slot(pid) == DockSlotId::Floating) {
                dl.dock_back_workspace_panel(pid);
            } else if (pid == DockPanelId::Transport) {
                owner_.transportBar_.set_minimized(!owner_.transportBar_.is_minimized());
            } else if (pid == DockPanelId::ProjectToolbar) {
                owner_.projectToolbar_.set_minimized(!owner_.projectToolbar_.is_minimized());
            }
            owner_.panel_component(pid).grabKeyboardFocus();
        } else {
            dl.set_panel_visible(pid, !dl.is_panel_visible(pid));
        }
        owner_.sync_workspace_layout();
    }

    void toggle_hide_all()
    {
        auto& dl = owner_.docking_layout();
        if (dl.are_all_panels_hidden()) {
            dl.restore_all_panels();
        } else {
            dl.hide_all_panels();
        }
        owner_.sync_workspace_layout();
    }

    void delete_active_preset()
    {
        auto& dl = owner_.docking_layout();
        if (dl.delete_custom_preset(dl.get_active_preset_name())) {
            owner_.sync_workspace_layout();
        }
    }

    void reset_layout()
    {
        owner_.docking_layout().reset_layout();
        owner_.sync_workspace_layout();
    }

    void sync()
    {
        syncing_ = true;
        const auto& dl = owner_.docking_layout();
        const std::array<DockPanelId, 5> pillPids = {
            DockPanelId::Transport, DockPanelId::Browser, DockPanelId::Timeline,
            DockPanelId::Inspector, DockPanelId::BottomDeck
        };
        for (size_t i = 0; i < pillPids.size(); ++i) {
            const bool vis = dl.is_panel_visible(pillPids[i]);
            visButtons_[i].setToggleState(vis, juce::dontSendNotification);
            visButtons_[i].setVariant(vis ? OaeToolbarButton::Variant::TintCyan
                                          : OaeToolbarButton::Variant::Neutral);
        }

        presetCombo_.clear(juce::dontSendNotification);
        const auto names = dl.get_preset_names();
        const auto& active = dl.get_active_preset_name();
        int activeId = 0;
        for (size_t i = 0; i < names.size(); ++i) {
            const int id = static_cast<int>(i + 1);
            presetCombo_.addItem(juce::String::fromUTF8(names[i].c_str()), id);
            if (names[i] == active) activeId = id;
        }
        if (activeId > 0) {
            presetCombo_.setSelectedId(activeId, juce::dontSendNotification);
        }
        syncing_ = false;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
        g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 2);
        juce::FlexBox row;
        row.alignItems = juce::FlexBox::AlignItems::center;
        row.items.add(juce::FlexItem(titleLabel_).withWidth(72.0f).withHeight(20.0f).withMargin({ 0, 8, 0, 0 }));
        for (size_t i = 0; i < visButtons_.size(); ++i) {
            row.items.add(juce::FlexItem(visButtons_[i]).withWidth(34.0f).withHeight(20.0f).withMargin({ 0, 4, 0, 0 }));
        }
        row.items.add(juce::FlexItem().withFlex(1.0f));
        row.items.add(juce::FlexItem(presetLabel_).withWidth(44.0f).withHeight(20.0f).withMargin({ 0, 4, 0, 4 }));
        row.items.add(juce::FlexItem(presetCombo_).withWidth(164.0f).withHeight(24.0f).withMargin({ 0, 6, 0, 0 }));
        row.items.add(juce::FlexItem(savePresetButton_).withWidth(92.0f).withHeight(24.0f).withMargin({ 0, 4, 0, 0 }));
        row.items.add(juce::FlexItem(menuButton_).withWidth(24.0f).withHeight(24.0f));
        row.performLayout(area.toFloat());
    }

private:
    juce::PopupMenu build_dock_submenu() const
    {
        juce::PopupMenu menu;
        const auto& dl = owner_.docking_layout();
        const std::array<DockPanelId, dockPanelCount> pids = {
            DockPanelId::ProjectToolbar, DockPanelId::Transport, DockPanelId::Browser,
            DockPanelId::Timeline, DockPanelId::Inspector, DockPanelId::BottomDeck,
            DockPanelId::Diagnostics
        };
        const std::array<DockSlotId, 6> slots = {
            DockSlotId::Top, DockSlotId::Left, DockSlotId::Center,
            DockSlotId::Right, DockSlotId::Bottom, DockSlotId::Floating
        };

        int itemId = 100;
        for (DockPanelId pid : pids) {
            juce::PopupMenu sub;
            if (DockingLayoutManager::can_close_panel(pid)) {
                sub.addItem(itemId, "Visible", true, dl.is_panel_visible(pid));
            }
            itemId++;
            if (DockingLayoutManager::can_close_panel(pid)) {
                sub.addSeparator();
            }
            const DockSlotId curSlot = dl.get_panel_slot(pid);
            for (DockSlotId s : slots) {
                sub.addItem(itemId++,
                            juce::String("Slot: ") + DockingLayoutManager::slot_label(s),
                            true, curSlot == s);
            }
            menu.addSubMenu(DockingLayoutManager::panel_label(pid), sub);
        }
        return menu;
    }

    void show_workspace_menu()
    {
        const auto& dl = owner_.docking_layout();
        const auto& active = dl.get_active_preset_name();
        const bool canDelete = !dl.is_builtin_preset(active) && !active.empty();

        juce::PopupMenu menu;
        menu.addItem(1, "Diagnostics & Log (Log)", true, dl.is_panel_visible(DockPanelId::Diagnostics));
        menu.addItem(2, "Project Toolbar (Prj)", true, !owner_.projectToolbar_.is_minimized());
        menu.addSeparator();
        menu.addItem(3, dl.are_all_panels_hidden() ? "Restore All Panels" : "Hide All Panels");
        menu.addSubMenu("Dock Slots", build_dock_submenu());
        menu.addSeparator();
        menu.addItem(4, "Delete Custom Preset", canDelete);
        menu.addItem(5, "Reset Layout");

        juce::Component::SafePointer<MainComponent> safeOwner(&owner_);
        UiTestDialogSeam::show_popup_menu("workspace_menu", menu,
            juce::PopupMenu::Options().withTargetComponent(menuButton_),
            [safeOwner](int res) {
                if (!safeOwner || res <= 0) return;
                if (res == 1) {
                    safeOwner->workspaceBar_->toggle_panel(DockPanelId::Diagnostics);
                } else if (res == 2) {
                    safeOwner->workspaceBar_->toggle_panel(DockPanelId::ProjectToolbar);
                } else if (res == 3) {
                    safeOwner->workspaceBar_->toggle_hide_all();
                } else if (res == 4) {
                    safeOwner->workspaceBar_->delete_active_preset();
                } else if (res == 5) {
                    safeOwner->workspaceBar_->reset_layout();
                } else if (res >= 100) {
                    const std::array<DockPanelId, dockPanelCount> pids = {
                        DockPanelId::ProjectToolbar, DockPanelId::Transport, DockPanelId::Browser,
                        DockPanelId::Timeline, DockPanelId::Inspector, DockPanelId::BottomDeck,
                        DockPanelId::Diagnostics
                    };
                    const std::array<DockSlotId, 6> slots = {
                        DockSlotId::Top, DockSlotId::Left, DockSlotId::Center,
                        DockSlotId::Right, DockSlotId::Bottom, DockSlotId::Floating
                    };
                    const int offset = res - 100;
                    const int stride = 1 + static_cast<int>(slots.size());
                    const int pIdx = offset / stride;
                    const int sIdx = offset % stride;
                    if (pIdx >= 0 && pIdx < static_cast<int>(pids.size())) {
                        const auto pid = pids[static_cast<size_t>(pIdx)];
                        if (sIdx == 0) {
                            safeOwner->docking_layout().set_panel_visible(
                                pid, !safeOwner->docking_layout().is_panel_visible(pid));
                        } else if (sIdx - 1 < static_cast<int>(slots.size())) {
                            safeOwner->docking_layout().set_panel_slot(pid, slots[static_cast<size_t>(sIdx - 1)]);
                        }
                        safeOwner->sync_workspace_layout();
                    }
                }
            });
    }

    MainComponent& owner_;
    juce::Label titleLabel_;
    std::array<OaeToolbarButton, 5> visButtons_;
    juce::Label presetLabel_;
    juce::ComboBox presetCombo_;
    OaeToolbarButton savePresetButton_{ "+ Save Preset", OaeToolbarButton::Variant::Neutral };
    OaeToolbarButton menuButton_{ "...", OaeToolbarButton::Variant::Neutral };
    bool syncing_{ false };
};

class MainComponent::SlotHeaderBar : public juce::Component {
public:
    SlotHeaderBar(MainComponent& owner, int group)
        : owner_(owner), group_(group)
    {
        const auto key = group <= 5
            ? juce::String(DockingLayoutManager::slot_key(static_cast<DockSlotId>(group - 1)))
            : juce::String(group);
        setComponentID("slot_header_" + key);
        moveButton_.setComponentID("btn_slot_" + key + "_move");
        moveButton_.setTitle("Move active panel");
        moveButton_.setTooltip("Move active panel to another edge or group");
        moveButton_.onClick = [this] { show_move_menu(); };
        addAndMakeVisible(moveButton_);

        popOutButton_.setComponentID("btn_slot_" + key + "_popout");
        popOutButton_.setTitle("Pop out active panel");
        popOutButton_.setTooltip("Pop out panel into floating window");
        popOutButton_.onClick = [this] {
            owner_.dockingLayout_.set_panel_slot(activePanel_, DockSlotId::Floating);
            owner_.schedule_workspace_sync();
        };
        addAndMakeVisible(popOutButton_);

        closeButton_.setComponentID("btn_slot_" + key + "_close");
        closeButton_.setTitle("Hide active panel");
        closeButton_.setTooltip("Hide active panel");
        closeButton_.onClick = [this] {
            owner_.dockingLayout_.set_panel_visible(activePanel_, false);
            owner_.schedule_workspace_sync();
        };
        addAndMakeVisible(closeButton_);

        metaLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption));
        metaLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textMuted));
        metaLabel_.setJustificationType(juce::Justification::centredRight);
        addChildComponent(metaLabel_);

        dockLockButton_.setComponentID("dock_inspector_lock_button");
        dockLockButton_.set_button_style(VectorIconButton::Style::OutlineTint);
        dockLockButton_.setTooltip("Lock Inspector");
        dockLockButton_.onClick = [this] {
            owner_.inspectorPanel_.set_locked(!owner_.inspectorPanel_.is_locked());
            sync_lock_state();
        };
        addChildComponent(dockLockButton_);
    }

    static juce::String section_title(DockPanelId panel)
    {
        switch (panel) {
            case DockPanelId::Browser: return "BROWSER";
            case DockPanelId::Timeline: return "STAGE";
            case DockPanelId::Inspector: return "PROPERTY INSPECTOR";
            case DockPanelId::BottomDeck: return "BOTTOM DECK";
            case DockPanelId::Diagnostics: return "DIAGNOSTICS & LOG";
            default: return juce::String(DockingLayoutManager::panel_label(panel)).toUpperCase();
        }
    }

    void sync_lock_state()
    {
        const bool locked = owner_.inspectorPanel_.is_locked();
        dockLockButton_.set_icon_type(locked ? VectorIconType::LockClosed : VectorIconType::LockOpen);
        dockLockButton_.setToggleState(locked, juce::dontSendNotification);
        dockLockButton_.set_semantic_active(locked);
        dockLockButton_.setTooltip(locked ? "Unlock Inspector" : "Lock Inspector");
    }

    void update_panels(const std::vector<DockPanelId>& panels, DockPanelId active)
    {
        activePanel_ = active;
        const bool solo = panels.size() == 1;
        if (panels_ != panels) {
            panels_ = panels;
            tabButtons_.clear();
            for (auto panel : panels_) {
                auto button = std::make_unique<DockTabButton>(solo ? section_title(panel)
                                                                   : DockingLayoutManager::panel_label(panel));
                button->setComponentID(juce::String("btn_slot_header_tab_") + DockingLayoutManager::panel_key(panel));
                button->setTooltip(DockingLayoutManager::panel_label(panel));
                button->onClick = [this, panel] {
                    owner_.dockingLayout_.select_workspace_tab(group_, panel);
                    owner_.layout_workspace_components();
                };
                button->onTabDrag = [this, panel](const juce::MouseEvent& event) {
                    owner_.drag_workspace_tab(panel, event);
                };
                button->onTabDrop = [this, panel](const juce::MouseEvent& event) {
                    owner_.drop_workspace_tab(panel, event);
                };
                addAndMakeVisible(*button);
                tabButtons_.push_back(std::move(button));
            }
            resized();
        }

        if (solo && tabButtons_.size() == 1) {
            tabButtons_[0]->setButtonText(section_title(panels_[0]));
        } else {
            for (size_t i = 0; i < tabButtons_.size(); ++i) {
                tabButtons_[i]->setButtonText(DockingLayoutManager::panel_label(panels_[i]));
            }
        }

        const bool showSlotActions = !solo || isHovered_;
        moveButton_.setVisible(showSlotActions);
        popOutButton_.setVisible(showSlotActions);
        closeButton_.setVisible(showSlotActions && DockingLayoutManager::can_close_panel(activePanel_));

        if (solo && activePanel_ == DockPanelId::Browser) {
            metaLabel_.setText("1 Bank", juce::dontSendNotification);
            metaLabel_.setVisible(!isHovered_);
        } else if (solo && activePanel_ == DockPanelId::Timeline) {
            const auto& ev = owner_.timelinePanel_.get_event_path();
            metaLabel_.setText(juce::String::fromUTF8(ev.c_str()), juce::dontSendNotification);
            metaLabel_.setVisible(!isHovered_ && !ev.empty());
        } else {
            metaLabel_.setVisible(false);
        }

        dockLockButton_.setVisible(solo && activePanel_ == DockPanelId::Inspector);
        if (dockLockButton_.isVisible()) {
            sync_lock_state();
        }

        for (size_t i = 0; i < tabButtons_.size(); ++i) {
            const bool selected = panels_[i] == activePanel_;
            tabButtons_[i]->setNeutralTitle(solo);
            tabButtons_[i]->setTabSelected(selected && !solo);
            tabButtons_[i]->setToggleState(selected, juce::dontSendNotification);
        }
        resized();
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        if (!isHovered_) {
            isHovered_ = true;
            update_panels(panels_, activePanel_);
        }
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        if (!getLocalBounds().contains(event.getEventRelativeTo(this).getPosition()) && isHovered_) {
            isHovered_ = false;
            update_panels(panels_, activePanel_);
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu()) {
            show_move_menu();
        }
    }

    std::pair<int, int> insertion_at(int x) const
    {
        for (size_t i = 0; i < tabButtons_.size(); ++i) {
            const auto bounds = tabButtons_[i]->getBounds();
            if (x < bounds.getCentreX()) return { static_cast<int>(i), bounds.getX() };
        }
        return { static_cast<int>(tabButtons_.size()),
                 tabButtons_.empty() ? 0 : tabButtons_.back()->getRight() };
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(OaeTheme::Colors::dockHeaderBg));
        graphics.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
        graphics.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6, 2);
        if (moveButton_.isVisible() || popOutButton_.isVisible() || closeButton_.isVisible()) {
            if (closeButton_.isVisible()) {
                closeButton_.setBounds(area.removeFromRight(20));
                area.removeFromRight(2);
            }
            popOutButton_.setBounds(area.removeFromRight(22));
            area.removeFromRight(2);
            moveButton_.setBounds(area.removeFromRight(22));
            area.removeFromRight(4);
        }
        if (dockLockButton_.isVisible()) {
            dockLockButton_.setBounds(area.removeFromRight(20));
            area.removeFromRight(4);
        }
        if (metaLabel_.isVisible()) {
            metaLabel_.setBounds(area.removeFromRight(std::min(160, std::max(60, area.getWidth() / 2))));
            area.removeFromRight(4);
        }

        int remaining = static_cast<int>(tabButtons_.size());
        for (auto& button : tabButtons_) {
            const int width = remaining == 1 && panels_.size() == 1
                ? area.getWidth()
                : std::min(132, area.getWidth() / remaining);
            --remaining;
            button->setBounds(area.removeFromLeft(width));
        }
    }

private:
    void show_move_menu()
    {
        juce::PopupMenu menu;
        const std::array<DockSlotId, 5> targets = {
            DockSlotId::Top, DockSlotId::Left, DockSlotId::Center, DockSlotId::Right, DockSlotId::Bottom
        };
        for (size_t i = 0; i < targets.size(); ++i)
            menu.addItem(static_cast<int>(i + 1),
                juce::String("Move to ") + DockingLayoutManager::slot_label(targets[i]));
        const DockPanelId panel = activePanel_;
        juce::Component::SafePointer<MainComponent> owner(&owner_);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(moveButton_),
            [owner, panel, targets](int result) {
                if (!owner || result <= 0 || static_cast<size_t>(result) > targets.size()) return;
                owner->dockingLayout_.set_panel_slot(panel, targets[static_cast<size_t>(result - 1)]);
                owner->sync_workspace_layout();
            });
    }

    MainComponent& owner_;
    int group_;
    std::vector<DockPanelId> panels_;
    DockPanelId activePanel_{ DockPanelId::Timeline };
    std::vector<std::unique_ptr<DockTabButton>> tabButtons_;
    DockTabButton moveButton_{ "<>", DockTabButton::Mode::action };
    DockTabButton popOutButton_{ "^", DockTabButton::Mode::action };
    DockTabButton closeButton_{ "X", DockTabButton::Mode::action };
    juce::Label metaLabel_;
    VectorIconButton dockLockButton_{ "Lock Inspector", VectorIconType::LockOpen, juce::Colour(OaeTheme::Colors::accentCyan) };
    bool isHovered_{ false };
};

MainComponent::MainComponent()
    : MainComponent(MainComponentOptions{})
{
}

MainComponent::MainComponent(MainComponentOptions options)
    : options_(std::move(options))
{
    setWantsKeyboardFocus(true);
    setLookAndFeel(&lookAndFeel_);
    tooltipWindow_ = std::make_unique<juce::TooltipWindow>(this, 500);

    workspaceBar_ = std::make_unique<WorkspaceBar>(*this);
    addAndMakeVisible(*workspaceBar_);
    addAndMakeVisible(projectToolbar_);
    projectToolbar_.onLayoutChanged = [this] { resized(); };
    addAndMakeVisible(transportBar_);
    transportBar_.setComponentID("transport_bar");
    transportBar_.onLayoutChanged = [this] { resized(); };
    transportBar_.onOpenGlobalParameters = [this] {
        contextualDeck_.set_active_tab(DeckTab::Parameters);
    };
    inspectorPanel_.onLockChanged = [this](bool) {
        update_workspace_chrome();
    };
    inspectorPanel_.onSwitchToTimelineRequested = [this]() {
        timelinePanel_.set_view_mode(TimelinePanel::StageViewMode::Timeline);
    };
    timelinePanel_.onViewModeChanged = [this](TimelinePanel::StageViewMode mode) {
        inspectorPanel_.set_layers_mode(mode == TimelinePanel::StageViewMode::Layers);
    };

    const auto wireGrip = [this](GripHandle& grip, DockPanelId pid) {
        grip.onDragStart = [this, pid](const juce::MouseEvent& e) {
            dockingDragCancelled_ = false;
            drag_workspace_tab(pid, e);
        };
        grip.onDragMove = [this, pid](const juce::MouseEvent& e) {
            drag_workspace_tab(pid, e);
        };
        grip.onDragEnd = [this, pid](const juce::MouseEvent& e) {
            drop_workspace_tab(pid, e);
        };
        grip.onToggleFloating = [this, pid]() {
            if (dockingLayout_.get_panel_slot(pid) == DockSlotId::Floating) {
                dockingLayout_.dock_back_workspace_panel(pid);
            } else {
                dockingLayout_.set_panel_slot(pid, DockSlotId::Floating);
            }
            sync_workspace_layout();
        };
    };
    wireGrip(projectToolbar_.grip_handle(), DockPanelId::ProjectToolbar);
    wireGrip(transportBar_.grip_handle(), DockPanelId::Transport);
    addAndMakeVisible(eventTreePanel_);
    eventTreePanel_.setComponentID("browser_panel");
    addAndMakeVisible(timelinePanel_);
    addAndMakeVisible(inspectorPanel_);
    inspectorPanel_.setComponentID("inspector_panel");
    addAndMakeVisible(contextualDeck_);
    contextualDeck_.setComponentID("bottom_deck");
    addChildComponent(highlightOverlay_);
    dockingLayout_.bottom_slot().onFloatingKeyPressed = [this](const juce::KeyPress& key) { return keyPressed(key); };

    dockingLayout_.bottom_slot().onHeaderDrag = [this](const juce::MouseEvent& event) {
        drag_workspace_tab(DockPanelId::BottomDeck, event);
    };
    dockingLayout_.bottom_slot().onHeaderDrop = [this](const juce::MouseEvent& event) {
        drop_workspace_tab(DockPanelId::BottomDeck, event);
    };
    dockingLayout_.bottom_slot().onDiagnosticsDrag = [this](const juce::MouseEvent& event) {
        drag_workspace_tab(DockPanelId::Diagnostics, event);
    };
    dockingLayout_.bottom_slot().onDiagnosticsDrop = [this](const juce::MouseEvent& event) {
        drop_workspace_tab(DockPanelId::Diagnostics, event);
    };
    dockingLayout_.bottom_slot().onDiagnosticsPopOut = [this] {
        dockingLayout_.set_panel_slot(DockPanelId::Diagnostics, DockSlotId::Floating);
        schedule_workspace_sync();
    };

    const auto app_file = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto app_dir = fs::u8path(app_file.getParentDirectory().getFullPathName().toStdString());
    dockingLayout_.load(resolved_layout_file_path());
    sync_workspace_layout();

    contextualDeck_.mixer_deck().set_timers_enabled(options_.enableTimers);
    contextualDeck_.parameters_deck().set_timers_enabled(options_.enableTimers);
    previewRebuilder_.set_auto_rebuild_enabled(options_.enableTimers);

    oae_result res = OAE_OK;
    if (options_.headlessAudio) {
        res = engine_.init_headless();
        diagnosticsPanel_.append_message("[Audio] Headless audio engine initialized.");
    } else {
        res = engine_.init_device();
        if (res != OAE_OK) {
            diagnosticsPanel_.append_message("[Audio] Physical audio device unavailable, falling back to headless mode.");
            res = engine_.init_headless();
        } else {
            diagnosticsPanel_.append_message("[Audio] Native audio output device initialized successfully.");
        }
    }

    const auto bankc = find_bankc();
    if (!bankc.empty()) {
        previewRebuilder_.set_bankc_executable(bankc);
    }

    timelinePanel_.onPlayheadScrubbed = [this](double t_ms) {
        transport_.seek(t_ms);
    };

    previewRebuilder_.onStatusChanged = [this](const std::string& st) {
        transportBar_.update_status(st);
    };

    previewRebuilder_.onRebuildFinished = [this](bool success, const std::string& diag) {
        if (success) {
            diagnosticsPanel_.append_message("[Preview] Live preview hot swapped.");
        } else {
            diagnosticsPanel_.append_message("[Preview Build Error]\n" + juce::String(diag));
        }
    };

    projectToolbar_.newProjectButton.onClick = [this]() { new_project(); };
    projectToolbar_.openProjectButton.onClick = [this]() { open_project(); };
    projectToolbar_.saveProjectButton.onClick = [this]() { save_project(); };
    projectToolbar_.buildBankButton.onClick = [this]() { build_bank(); };
    projectToolbar_.helpButton.onClick = [this]() { open_help(); };

    if (options_.autoLoadExample) {
        const std::vector<fs::path> candidate_projects = {
            app_dir / "example_project" / "project.json",
            app_dir / ".." / "example_project" / "project.json"
        };
        for (const auto& cp : candidate_projects) {
            if (fs::exists(cp)) {
                if (document_.open(cp)) {
                    const auto event_paths = authoringModel_.get_event_paths();
                    if (!event_paths.empty()) {
                        authoringModel_.select(ItemSelection{ ItemType::Event, event_paths[0] });
                    }
                    authoringModel_.notify_project_changed();
                    previewRebuilder_.rebuild_synchronously();
                    diagnosticsPanel_.append_message("[Project] Auto-loaded project: " + juce::String::fromUTF8(cp.u8string().c_str()));
                    break;
                }
            }
        }
    }

    setSize(1280, 800);

    if (options_.enableLiveBridge) {
        transport_.set_live_bridge(&liveBridge_);
        liveBridge_.add_listener(this);
        if (liveBridge_.start(LiveBridgeServer::kDefaultPort)) {
            diagnosticsPanel_.append_message("[Live] Live-Linking TCP bridge listening on 127.0.0.1:" + juce::String(LiveBridgeServer::kDefaultPort));
        } else {
            diagnosticsPanel_.append_message("[Live] Port " + juce::String(LiveBridgeServer::kDefaultPort) + " unavailable (another instance may be active).");
        }
    } else {
        transport_.set_live_bridge(nullptr);
    }

    if (options_.enableTimers) {
        startTimerHz(30);
    }

    diagnosticsPanel_.append_message("[System] OpenAudio Studio Ready.");
}

MainComponent::~MainComponent()
{
    for (auto& fw : floatingWindows_) {
        fw.reset();
    }
    dockingLayout_.bottom_slot().clear();
    stopTimer();
    transport_.set_live_bridge(nullptr);
    if (options_.enableLiveBridge) {
        liveBridge_.remove_listener(this);
        liveBridge_.stop();
    }
    transport_.stop(true);
    engine_.shutdown();

    dockingLayout_.save(resolved_layout_file_path());

    setLookAndFeel(nullptr);
}

std::filesystem::path MainComponent::resolved_layout_file_path() const
{
    if (!options_.layoutFile.empty()) {
        return options_.layoutFile;
    }
    const auto app_file = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto app_dir = fs::u8path(app_file.getParentDirectory().getFullPathName().toStdString());
    return app_dir / "oae_studio_layout.json";
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaePalette::bgDark));
}

juce::Component& MainComponent::panel_component(DockPanelId id) noexcept
{
    switch (id) {
        case DockPanelId::Transport: return transportBar_;
        case DockPanelId::Browser: return eventTreePanel_;
        case DockPanelId::Timeline: return timelinePanel_;
        case DockPanelId::Inspector: return inspectorPanel_;
        case DockPanelId::BottomDeck: return contextualDeck_;
        case DockPanelId::Diagnostics: return diagnosticsPanel_;
        case DockPanelId::ProjectToolbar: return projectToolbar_;
        default: return timelinePanel_;
    }
}

void MainComponent::toggle_hide_all()
{
    if (workspaceBar_) {
        workspaceBar_->toggle_hide_all();
    }
}

void MainComponent::sync_workspace_layout()
{
    const bool embedded = dockingLayout_.workspace_group(DockPanelId::Diagnostics) == 0
                       && dockingLayout_.get_panel_slot(DockPanelId::Diagnostics) != DockSlotId::Floating;
    auto& deck = dockingLayout_.bottom_slot();
    if (!embedded || (!dockingLayout_.are_all_panels_hidden()
        && !dockingLayout_.is_panel_visible(DockPanelId::Diagnostics)))
        deck.set_panel_available(static_cast<int>(DeckTab::Diagnostics), false);
    for (size_t i = 0; i < dockPanelCount; ++i) {
        const auto panel = static_cast<DockPanelId>(i);
        const bool floating = dockingLayout_.get_panel_slot(panel) == DockSlotId::Floating;
        auto& component = panel_component(panel);

        if (floating && dockingLayout_.is_panel_visible(panel)) {
            if (!floatingWindows_[i]) {
                removeChildComponent(&component);
                floatingWindows_[i] = DockingLayoutManager::create_floating_host(
                    DockingLayoutManager::panel_label(panel), component, lookAndFeel_,
                    [owner = juce::Component::SafePointer<MainComponent>(this), panel] {
                        if (!owner) return;
                        owner->dockingLayout_.dock_back_workspace_panel(panel);
                        owner->sync_workspace_layout();
                    }, [this](const juce::KeyPress& key) { return keyPressed(key); },
                    DockingLayoutManager::is_toolbar_panel(panel));
            }
            component.setVisible(true);
        } else {
            floatingWindows_[i].reset();
            if (panel == DockPanelId::Diagnostics && embedded) {
                removeChildComponent(&component);
                if (!dockingLayout_.are_all_panels_hidden())
                    deck.set_panel_available(static_cast<int>(DeckTab::Diagnostics),
                                             dockingLayout_.is_panel_visible(panel));
            } else if (component.getParentComponent() != this) {
                addChildComponent(component);
            }
        }
    }

    projectToolbar_.set_floating(dockingLayout_.get_panel_slot(DockPanelId::ProjectToolbar) == DockSlotId::Floating);
    transportBar_.set_floating(dockingLayout_.get_panel_slot(DockPanelId::Transport) == DockSlotId::Floating);

    update_workspace_chrome();
    layout_workspace_components();
}

void MainComponent::schedule_workspace_sync()
{
    juce::MessageManager::callAsync([owner = juce::Component::SafePointer<MainComponent>(this)] {
        if (owner) owner->sync_workspace_layout();
    });
}

void MainComponent::update_workspace_chrome()
{
    if (workspaceBar_) {
        workspaceBar_->sync();
    }
}

void MainComponent::prompt_save_custom_preset()
{
    int seamResult = 0;
    std::string seamText;
    if (UiTestDialogSeam::intercept_alert("save_custom_preset", "Save Workspace Layout", seamResult, seamText)) {
        if (seamResult == 1 && !seamText.empty()) {
            dockingLayout_.save_custom_preset(seamText);
            sync_workspace_layout();
        }
        return;
    }

    auto* aw = new juce::AlertWindow(
        "Save Workspace Layout",
        "Enter a name for this custom workspace layout:",
        juce::MessageBoxIconType::QuestionIcon);
    aw->addTextEditor("layout_name", "Custom " + juce::String(static_cast<int>(dockingLayout_.get_preset_names().size() - 2)), "Name:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis = juce::Component::SafePointer<MainComponent>(this),
         safeAw = juce::Component::SafePointer<juce::AlertWindow>(aw)](int result) {
            if (safeThis && safeAw && result == 1) {
                const auto name = safeAw->getTextEditorContents("layout_name").trim().toStdString();
                if (!name.empty()) {
                    safeThis->dockingLayout_.save_custom_preset(name);
                    safeThis->sync_workspace_layout();
                }
            }
        }), true);
}

void MainComponent::layout_workspace_components()
{
    auto area = getLocalBounds();
    constexpr int workspaceBarHeight = 28;
    constexpr int headerHeight = 24;
    if (workspaceBar_) workspaceBar_->setBounds(area.removeFromTop(workspaceBarHeight));
    workspaceArea_ = area;
    const int topHeight = std::max(projectToolbar_.preferred_height(),
                                   transportBar_.preferred_height(std::max(200, area.getWidth() - projectToolbar_.preferred_width())));
    workspaceGeometry_ = dockingLayout_.workspace_geometry(area, topHeight);

    for (size_t index = 0; index < dockPanelCount; ++index) {
        auto& component = panel_component(static_cast<DockPanelId>(index));
        if (component.getParentComponent() != this) continue;
        const auto active = std::find_if(workspaceGeometry_.groups.begin(), workspaceGeometry_.groups.end(),
            [index](const auto& group) { return static_cast<size_t>(group.active) == index; });
        component.setVisible(active != workspaceGeometry_.groups.end());
        if (active == workspaceGeometry_.groups.end()) component.setBounds({});
    }

    for (auto it = slotHeaders_.begin(); it != slotHeaders_.end();) {
        const auto match = std::find_if(workspaceGeometry_.groups.begin(), workspaceGeometry_.groups.end(),
            [id = it->first](const auto& group) {
                const bool suppressHeader = group.tabs.size() == 1
                    && (DockingLayoutManager::is_toolbar_panel(group.active)
                        || group.active == DockPanelId::BottomDeck);
                return group.id == id && !suppressHeader;
            });
        if (match != workspaceGeometry_.groups.end()) ++it;
        else it = slotHeaders_.erase(it);
    }
    bool soloInspectorDocked = false;
    bool soloBottomDeckDocked = false;
    for (const auto& group : workspaceGeometry_.groups) {
        const bool suppressHeader = group.tabs.size() == 1
            && (DockingLayoutManager::is_toolbar_panel(group.active)
                || group.active == DockPanelId::BottomDeck);
        if (suppressHeader) {
            if (group.active == DockPanelId::BottomDeck) {
                soloBottomDeckDocked = true;
            }
            panel_component(group.active).setBounds(group.bounds);
            continue;
        }
        if (group.tabs.size() == 1 && group.active == DockPanelId::Inspector) {
            soloInspectorDocked = true;
        }
        auto& header = slotHeaders_[group.id];
        if (!header) {
            header = std::make_unique<SlotHeaderBar>(*this, group.id);
            addAndMakeVisible(*header);
        }
        auto bounds = group.bounds;
        header->setBounds(bounds.removeFromTop(headerHeight));
        header->update_panels(group.tabs, group.active);
        panel_component(group.active).setBounds(bounds);
    }
    inspectorPanel_.set_embedded_header_mode(soloInspectorDocked);
    dockingLayout_.bottom_slot().set_embedded_dock_header(soloBottomDeckDocked);

    for (auto it = workspaceSplitters_.begin(); it != workspaceSplitters_.end();) {
        const auto isUsed = std::any_of(workspaceGeometry_.dividers.begin(), workspaceGeometry_.dividers.end(),
            [id = it->first](const auto& divider) { return divider.id == id; });
        if (isUsed) ++it;
        else it = workspaceSplitters_.erase(it);
    }
    for (const auto& divider : workspaceGeometry_.dividers) {
        auto& bar = workspaceSplitters_[divider.id];
        const auto orientation = divider.vertical ? SplitterBar::Orientation::Horizontal
                                                   : SplitterBar::Orientation::Vertical;
        if (!bar || bar->get_orientation() != orientation) {
            bar = std::make_unique<SplitterBar>(orientation);
            bar->setComponentID("dock_splitter_" + juce::String(divider.id));
            bar->onSplitterDragged = [this, id = divider.id](int delta) {
                const auto current = std::find_if(workspaceGeometry_.dividers.begin(), workspaceGeometry_.dividers.end(),
                    [id](const auto& item) { return item.id == id; });
                if (current == workspaceGeometry_.dividers.end()) return;
                dockingLayout_.resize_workspace_split(*current, delta);
                resized();
            };
            addAndMakeVisible(*bar);
        }
        bar->setBounds(divider.bounds);
    }

    highlightOverlay_.setBounds(getLocalBounds());
    highlightOverlay_.toFront(false);
    highlightOverlay_.refresh_target_visibility();
}

void MainComponent::drag_workspace_tab(DockPanelId panel, const juce::MouseEvent& event)
{
    if (dockingDragCancelled_) return;
    draggingPanel_ = panel;
    workspaceDrop_.reset();
    dockingPreview_ = {};
    const auto point = event.getEventRelativeTo(this).getPosition();
    if (workspaceArea_.contains(point)) {
        constexpr int outerEdge = 10;
        const auto edge_at = [point](juce::Rectangle<int> bounds, int horizontal, int vertical)
            -> std::optional<DockEdge> {
            if (point.x < bounds.getX() + horizontal) return DockEdge::Left;
            if (point.x >= bounds.getRight() - horizontal) return DockEdge::Right;
            if (point.y < bounds.getY() + vertical) return DockEdge::Top;
            if (point.y >= bounds.getBottom() - vertical) return DockEdge::Bottom;
            return {};
        };

        if (const auto edge = edge_at(workspaceArea_, outerEdge, outerEdge)) {
            workspaceDrop_ = WorkspaceDrop{ 0, *edge, 0 };
        } else {
            for (const auto& group : workspaceGeometry_.groups) {
                if (!group.bounds.contains(point)) continue;
                const auto headerIt = slotHeaders_.find(group.id);
                if (headerIt != slotHeaders_.end() && headerIt->second && headerIt->second->getBounds().contains(point)) {
                    const auto [index, x] = headerIt->second->insertion_at(point.x - headerIt->second->getX());
                    workspaceDrop_ = WorkspaceDrop{ group.id, DockEdge::Tabs, index };
                    dockingPreview_ = { headerIt->second->getX() + x - 1, headerIt->second->getY(), 3, headerIt->second->getHeight() };
                } else if (const auto groupEdge = edge_at(group.bounds,
                               std::min(32, group.bounds.getWidth() / 4),
                               std::min(32, group.bounds.getHeight() / 4))) {
                    workspaceDrop_ = WorkspaceDrop{ group.id, *groupEdge, 0 };
                }
                break;
            }
        }
        if (workspaceDrop_ && workspaceDrop_->edge != DockEdge::Tabs) {
            const int topHeight = std::max(projectToolbar_.preferred_height(),
                transportBar_.preferred_height(std::max(200, workspaceArea_.getWidth() - projectToolbar_.preferred_width())));
            dockingPreview_ = dockingLayout_.preview_workspace_split(panel, *workspaceDrop_, workspaceArea_, topHeight);
            if (dockingPreview_.isEmpty()) workspaceDrop_.reset();
        }
    }
    repaint();
}

void MainComponent::drop_workspace_tab(DockPanelId panel, const juce::MouseEvent& event)
{
    if (!dockingDragCancelled_ && draggingPanel_ == panel) {
        drag_workspace_tab(panel, event);
        if (workspaceDrop_ && dockingLayout_.move_workspace_panel(panel, *workspaceDrop_))
            schedule_workspace_sync();
    }
    draggingPanel_.reset();
    workspaceDrop_.reset();
    dockingPreview_ = {};
    dockingDragCancelled_ = false;
    repaint();
}

void MainComponent::paintOverChildren(juce::Graphics& graphics)
{
    if (dockingPreview_.isEmpty()) return;
    const auto colour = juce::Colour(OaeTheme::Colors::accentCyan);
    graphics.setColour(colour.withAlpha(0.18f));
    graphics.fillRect(dockingPreview_);
    graphics.setColour(colour);
    graphics.drawRect(dockingPreview_, 2);
}

void MainComponent::resized()
{
    layout_workspace_components();
}

void MainComponent::timerCallback()
{
    liveBridge_.poll_incoming();
    engine_.update();
    transport_.update_tick();

    const double pos_ms = transport_.get_current_position_ms();
    transportBar_.update_timecode(pos_ms);
    timelinePanel_.set_playhead_time_ms(pos_ms);

    oae_bus masterBus = 0;
    if (engine_.get_bus("bus:/", &masterBus) == OAE_OK) {
        oae_meter m{};
        if (engine_.get_bus_meter(masterBus, &m) == OAE_OK) {
            transportBar_.update_meter(m.peak[0], m.peak[1], m.rms[0], m.rms[1]);
        }
    }

    if (highlightOverlay_.is_active()) {
        highlightOverlay_.refresh_target_visibility();
    }
}

void MainComponent::on_live_client_connected(const std::string& client_name, uint32_t client_crc32, bool crc_match)
{
    if (crc_match) {
        transportBar_.update_status("Live: " + client_name + " Connected");
        diagnosticsPanel_.append_message("[Live] Connected to " + juce::String(client_name) +
                                         " (bank_crc32=0x" + juce::String::toHexString(static_cast<int>(client_crc32)) + ", match=OK).");
    } else {
        transportBar_.update_status("Live: CRC Mismatch");
        diagnosticsPanel_.append_message("[Live] WARNING: Connected to " + juce::String(client_name) +
                                         " with bank_crc32 mismatch (client=0x" + juce::String::toHexString(static_cast<int>(client_crc32)) +
                                         ", studio=0x" + juce::String::toHexString(static_cast<int>(liveBridge_.get_studio_bank_crc32())) + ").");
    }
}

void MainComponent::on_live_client_disconnected()
{
    transportBar_.update_status("Live: Offline (Local Engine)");
    diagnosticsPanel_.append_message("[Live] Client disconnected; reverted to local StudioEngine.");
}

void MainComponent::new_project()
{
    fs::path chosenPath;
    if (UiTestDialogSeam::intercept_file_chooser("new_project", "Select Directory for New Project", chosenPath)) {
        if (!chosenPath.empty()) {
            const std::string name = chosenPath.filename().u8string();
            if (document_.create_empty(chosenPath, name)) {
                authoringModel_.select(ItemSelection{});
                authoringModel_.notify_project_changed();
                previewRebuilder_.rebuild_synchronously();
                diagnosticsPanel_.append_message("[Project] Created new project in: " + juce::String::fromUTF8(chosenPath.u8string().c_str()));
            } else {
                diagnosticsPanel_.append_message("[Error] Failed to create project in: " + juce::String::fromUTF8(chosenPath.u8string().c_str()));
            }
        }
        return;
    }

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Select Directory for New Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*"
    );

    const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectDirectories;
    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        const auto chosen = fc.getResult();
        if (chosen != juce::File{}) {
            const fs::path dir = fs::u8path(chosen.getFullPathName().toStdString());
            const std::string name = chosen.getFileName().toStdString();
            if (document_.create_empty(dir, name)) {
                authoringModel_.select(ItemSelection{});
                authoringModel_.notify_project_changed();
                previewRebuilder_.rebuild_synchronously();
                diagnosticsPanel_.append_message("[Project] Created new project in: " + chosen.getFullPathName());
            } else {
                diagnosticsPanel_.append_message("[Error] Failed to create project in: " + chosen.getFullPathName());
            }
        }
    });
}

bool MainComponent::open_project_file(const fs::path& proj_path)
{
    if (document_.open(proj_path)) {
        const auto event_paths = authoringModel_.get_event_paths();
        if (!event_paths.empty()) {
            authoringModel_.select(ItemSelection{ ItemType::Event, event_paths[0] });
        } else {
            authoringModel_.select(ItemSelection{});
        }
        authoringModel_.notify_project_changed();
        previewRebuilder_.rebuild_synchronously();
        diagnosticsPanel_.append_message("[Project] Loaded project: " + juce::String::fromUTF8(proj_path.u8string().c_str()));
        return true;
    }
    diagnosticsPanel_.append_message("[Error] Failed to load project from: " + juce::String::fromUTF8(proj_path.u8string().c_str()));
    return false;
}

void MainComponent::open_project()
{
    fs::path chosenPath;
    if (UiTestDialogSeam::intercept_file_chooser("open_project", "Open OpenAudio Project", chosenPath)) {
        if (!chosenPath.empty() && fs::is_regular_file(chosenPath)) {
            open_project_file(chosenPath);
        }
        return;
    }

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Open OpenAudio Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json"
    );

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        const auto chosen = fc.getResult();
        if (chosen.existsAsFile()) {
            open_project_file(fs::u8path(chosen.getFullPathName().toStdString()));
        }
    });
}

void MainComponent::save_project()
{
    if (document_.save()) {
        diagnosticsPanel_.append_message("[Project] Project saved successfully.");
    } else {
        diagnosticsPanel_.append_message("[Error] Failed to save project.");
    }
}

void MainComponent::build_bank()
{
    if (!document_.is_loaded()) {
        diagnosticsPanel_.append_message("[Error] No project loaded to build.");
        return;
    }
    const auto bankc_local = find_bankc();
    if (bankc_local.empty()) {
        diagnosticsPanel_.append_message("[Build Error] oae_bankc executable not found.");
        return;
    }
    if (publisher_.is_running()) {
        diagnosticsPanel_.append_message("[Build] A bank publication is already running.");
        return;
    }
    diagnosticsPanel_.append_message("[Build] Starting bank compilation...");
    const fs::path target_bank = document_.get_project_dir() / "build" / (document_.get_project_name() + ".bank");
    publisher_.start_publish(bankc_local, authoringModel_.create_immutable_snapshot(), document_.get_project_dir(), target_bank,
        [self = juce::Component::SafePointer<MainComponent>(this)](const PublishResult& r) {
            juce::MessageManager::callAsync([self, r] {
                if (!self) return;
                if (r.success) {
                    self->diagnosticsPanel_.append_message("[Build] Bank compiled successfully!");
                } else {
                    self->diagnosticsPanel_.append_message("[Build Error] Exit code: " + juce::String(r.exit_code));
                    self->diagnosticsPanel_.append_message(juce::String(r.diagnostics));
                }
            });
        });
}

void MainComponent::open_help()
{
    auto directory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    while (true) {
        const auto manual = directory.getChildFile("docs/manual/index.html");
        if (manual.existsAsFile()) {
            if (UiTestDialogSeam::intercept_external_launch("open_help", manual.getFullPathName().toStdString())) {
                return;
            }
            if (!manual.startAsProcess())
                diagnosticsPanel_.append_message("[Help] Could not open " + manual.getFullPathName());
            return;
        }
        const auto parent = directory.getParentDirectory();
        if (parent == directory)
            break;
        directory = parent;
    }
    diagnosticsPanel_.append_message("[Help] docs/manual/index.html was not found. Restore the manual folder from the Studio bundle.");
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && draggingPanel_) {
        dockingDragCancelled_ = true;
        draggingPanel_.reset();
        workspaceDrop_.reset();
        dockingPreview_ = {};
        repaint();
        return true;
    }
    if (key == juce::KeyPress::F1Key) {
        open_help();
        return true;
    }
    if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0)) {
        new_project();
        return true;
    }
    if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0)) {
        open_project();
        return true;
    }
    if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) {
        save_project();
        return true;
    }
    if (key == juce::KeyPress('b', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress::F7Key) {
        build_bank();
        return true;
    }
    if (key == juce::KeyPress::spaceKey) {
        if (transport_.get_state() == TransportState::Playing) {
            transport_.pause();
        } else {
            transport_.play();
        }
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
        if (authoringModel_.undo()) {
            diagnosticsPanel_.append_message("[Edit] Undo performed.");
            return true;
        }
    }
    if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) {
        if (authoringModel_.redo()) {
            diagnosticsPanel_.append_message("[Edit] Redo performed.");
            return true;
        }
    }
    return Component::keyPressed(key);
}

bool MainComponent::dump_ui_screenshots(const std::filesystem::path& out_dir)
{
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        return false;
    }

    setSize(1440, 900);
    resized();
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->runDispatchLoopUntil(250);
    }

    const auto& root = document_.get_root_json();
    if (root.contains("events") && root["events"].is_array() && !root["events"].empty()) {
        const auto& ev0 = root["events"][0];
        if (ev0.contains("path") && ev0["path"].is_string()) {
            ItemSelection sel;
            sel.type = ItemType::Event;
            sel.id_or_path = ev0["path"].get<std::string>();
            authoringModel_.select(sel);
            if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
                mm->runDispatchLoopUntil(250);
            }
        }
    }

    auto save_comp = [&out_dir](juce::Component& comp, const std::string& filename) -> bool {
        if (comp.getWidth() <= 0 || comp.getHeight() <= 0) {
            return false;
        }
        auto img = comp.createComponentSnapshot(comp.getLocalBounds(), true, 1.0f);
        const auto target = out_dir / filename;
        juce::File file(juce::String::fromUTF8(target.u8string().c_str()));
        if (file.existsAsFile()) {
            file.deleteFile();
        }
        juce::FileOutputStream stream(file);
        if (!stream.openedOk()) {
            return false;
        }
        juce::PNGImageFormat png;
        return png.writeImageToStream(img, stream);
    };

    bool ok = true;
    ok &= save_comp(*this, "studio-overview.png");
    ok &= save_comp(transportBar_, "studio-transport.png");
    ok &= save_comp(eventTreePanel_, "studio-tree.png");
    ok &= save_comp(timelinePanel_, "studio-timeline.png");
    ok &= save_comp(timelinePanel_, "studio-timeline-params-expanded.png");
    timelinePanel_.parameter_strip().set_expanded(false);
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->runDispatchLoopUntil(80);
    }
    ok &= save_comp(timelinePanel_, "studio-timeline-params-collapsed.png");
    timelinePanel_.parameter_strip().set_expanded(true);
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->runDispatchLoopUntil(80);
    }
    ok &= save_comp(inspectorPanel_, "studio-inspector.png");

    const std::pair<DeckTab, std::string> tabs[] = {
        { DeckTab::Mixer, "studio-deck-mixer.png" },
        { DeckTab::Effects, "studio-deck-effects.png" },
        { DeckTab::Playlist, "studio-deck-playlist.png" },
        { DeckTab::Logic, "studio-deck-logic.png" },
        { DeckTab::Spatializer, "studio-deck-spatializer.png" },
        { DeckTab::Diagnostics, "studio-deck-diagnostics.png" },
        { DeckTab::Parameters, "studio-deck-parameters.png" },
        { DeckTab::Parameters, "deck-tab7-parameters.png" }
    };
    for (const auto& [tab, name] : tabs) {
        contextualDeck_.set_active_tab(tab);
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
            mm->runDispatchLoopUntil(80);
        }
        ok &= save_comp(contextualDeck_, name);
    }

    return ok;
}

} // namespace oae::studio
