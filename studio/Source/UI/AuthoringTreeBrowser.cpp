#include "AuthoringTreeBrowser.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"

namespace oae::studio {

class AuthoringTreeBrowser::BrowserTreeItem : public juce::TreeViewItem {
public:
    enum class Kind {
        Root,
        Category,
        Folder,
        Leaf
    };

    BrowserTreeItem(AuthoringTreeBrowser& owner, Kind kind, ItemType type,
                    const juce::String& name, const juce::String& fullPath)
        : owner_(owner), kind_(kind), type_(type), name_(name), fullPath_(fullPath)
    {
    }

    bool mightContainSubItems() override
    {
        return kind_ != Kind::Leaf;
    }

    juce::String getUniqueName() const override
    {
        return fullPath_ + "_" + name_;
    }

    void itemClicked(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) {
            show_context_menu();
            return;
        }

        if (kind_ == Kind::Leaf) {
            owner_.model().select({ type_, fullPath_.toStdString() });
        }
    }

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (kind_ == Kind::Leaf) {
            owner_.rename_selected_item();
        }
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width) - 2.0f, static_cast<float>(height) - 2.0f);
        if (isSelected()) {
            g.setColour(juce::Colour(OaeTheme::Colors::tintCyan));
            g.fillRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius);
            g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
            g.drawRoundedRectangle(bounds, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);
        }

        auto area = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

        // Icon indicator
        juce::Colour iconCol(OaeTheme::Colors::textMuted);
        if (kind_ == Kind::Category || kind_ == Kind::Folder) {
            iconCol = juce::Colour(OaeTheme::Colors::accentAmber);
        } else {
            switch (type_) {
                case ItemType::Event: iconCol = juce::Colour(OaeTheme::Colors::accentGreen); break;
                case ItemType::Snapshot: iconCol = juce::Colour(OaeTheme::Colors::accentAmber); break;
                case ItemType::Bus: iconCol = juce::Colour(OaeTheme::Colors::accentPurple); break;
                case ItemType::Vca: iconCol = juce::Colour(OaeTheme::Colors::accentCyan); break;
                case ItemType::Sample: iconCol = juce::Colour(OaeTheme::Colors::waveformEvent); break;
                case ItemType::GlobalParameter: iconCol = juce::Colour(OaeTheme::Colors::accentCoral); break;
                default: break;
            }
        }

        auto iconArea = area.removeFromLeft(16);
        g.setColour(iconCol);
        if (kind_ == Kind::Leaf) {
            g.fillRoundedRectangle(iconArea.toFloat().reduced(3.0f), 2.0f);
        } else {
            g.drawRect(iconArea.reduced(2), 1);
        }

        area.removeFromLeft(4);
        g.setColour(juce::Colour(isSelected() ? OaeTheme::Colors::accentCyan : OaeTheme::Colors::textSecondary));
        g.setFont(juce::FontOptions(OaeTheme::Typography::compact));
        g.drawText(name_, area, juce::Justification::centredLeft, true);
    }

    void show_context_menu()
    {
        juce::PopupMenu menu;
        if (kind_ == Kind::Leaf) {
            menu.addItem(1, "Rename (F2)");
            menu.addItem(2, "Duplicate (Ctrl+D)");
            if (type_ == ItemType::Sample) {
                menu.addItem(11, "Import Audio...");
            }
            menu.addSeparator();
            menu.addItem(3, "Delete (Del)");
        } else if (kind_ == Kind::Category) {
            if (type_ == ItemType::Sample) {
                menu.addItem(11, "Import Audio...");
            } else {
                menu.addItem(10, "Add Item...");
            }
        }

        const int result = UiTestDialogSeam::show_modal_menu("browser_tree_context", menu);
        if (result == 1) {
            owner_.rename_selected_item();
        } else if (result == 2) {
            owner_.duplicate_selected_item();
        } else if (result == 3) {
            owner_.delete_selected_item();
        } else if (result == 10) {
            if (type_ == ItemType::Event) owner_.model().create_event("event:/new_event");
            else if (type_ == ItemType::Snapshot) owner_.model().create_snapshot("snapshot:/new_snapshot");
            else if (type_ == ItemType::Bus) owner_.model().create_bus("bus:/new_bus");
            else if (type_ == ItemType::Vca) owner_.model().create_vca("vca:/new_vca");
            else if (type_ == ItemType::GlobalParameter) owner_.model().create_global_parameter("NewParam");
        } else if (result == 11) {
            owner_.import_audio_dialog();
        }
    }

    bool filter(const juce::String& query)
    {
        if (query.isEmpty()) {
            return true;
        }

        bool anyChildMatches = false;
        for (int i = 0; i < getNumSubItems(); ++i) {
            auto* child = dynamic_cast<BrowserTreeItem*>(getSubItem(i));
            if (child != nullptr && child->filter(query)) {
                anyChildMatches = true;
            }
        }

        const bool selfMatches = name_.containsIgnoreCase(query) || fullPath_.containsIgnoreCase(query);
        const bool visible = selfMatches || anyChildMatches;

        if (visible && anyChildMatches) {
            setOpen(true);
        }

        return visible;
    }

    Kind kind_{ Kind::Leaf };
    ItemType type_{ ItemType::Unknown };
    juce::String name_;
    juce::String fullPath_;
    AuthoringTreeBrowser& owner_;
};

class AuthoringTreeBrowser::ParametersListComponent : public juce::Component, public ITestableCustomControl {
public:
    explicit ParametersListComponent(AuthoringTreeBrowser& owner)
        : owner_(owner)
    {
    }

    struct Row {
        std::string name;
        std::string domainText;
        bool selected{ false };
    };

    void set_rows(std::vector<Row> rows)
    {
        rows_ = std::move(rows);
        const int h = std::max(24, static_cast<int>(rows_.size()) * 24);
        setSize(std::max(120, getWidth()), h);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(OaeTheme::Colors::bgSurface));
        int y = 0;
        for (const auto& row : rows_) {
            const auto box = juce::Rectangle<float>(4.0f, static_cast<float>(y), static_cast<float>(getWidth() - 8), 22.0f);
            g.setColour(juce::Colour(row.selected ? OaeTheme::Colors::tintCyan : OaeTheme::Colors::bgDark));
            g.fillRoundedRectangle(box, OaeTheme::Edges::cornerRadius);
            if (row.selected) {
                g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
                g.drawRoundedRectangle(box, OaeTheme::Edges::cornerRadius, OaeTheme::Edges::borderStroke);
            }

            auto textArea = box.toNearestInt().reduced(8, 0);
            g.setFont(juce::FontOptions(OaeTheme::Typography::compact, juce::Font::bold));
            g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
            const int domainW = 76;
            g.drawFittedText(juce::String::fromUTF8(row.name.c_str()),
                             textArea.withTrimmedRight(domainW + 4),
                             juce::Justification::centredLeft, 1);

            g.setFont(juce::FontOptions(OaeTheme::Typography::caption));
            g.setColour(juce::Colour(OaeTheme::Colors::textMuted));
            g.drawFittedText(juce::String::fromUTF8(row.domainText.c_str()),
                             textArea.removeFromRight(domainW),
                             juce::Justification::centredRight, 1);

            y += 24;
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const int idx = event.y / 24;
        std::string paramName;
        if (idx >= 0 && idx < static_cast<int>(rows_.size())) {
            paramName = rows_[static_cast<size_t>(idx)].name;
            owner_.model().select({ ItemType::GlobalParameter, paramName, -1, -1 });
        }
        if (event.mods.isPopupMenu()) {
            owner_.show_parameter_context_menu(paramName);
        }
    }

    std::vector<TestableTarget> get_testable_targets() const override
    {
        std::vector<TestableTarget> out;
        const auto bounds = getLocalBounds();
        if (bounds.isEmpty()) return out;
        for (size_t i = 0; i < rows_.size(); ++i) {
            const auto r = juce::Rectangle<int>(4, static_cast<int>(i) * 24, std::max(1, getWidth() - 8), 22).getIntersection(bounds);
            if (!r.isEmpty()) {
                out.push_back({ "row_" + std::to_string(i), "row", r, isEnabled() });
            }
        }
        return out;
    }

private:
    AuthoringTreeBrowser& owner_;
    std::vector<Row> rows_;
};

AuthoringTreeBrowser::AuthoringTreeBrowser(AuthoringModel& model)
    : model_(model)
{
    model_.add_listener(this);

    const std::array<std::tuple<BrowserTab, const char*, const char*>, 4> tabDefs = {{
        { BrowserTab::Events, "Events", "btn_browser_tab_events" },
        { BrowserTab::Snapshots, "Snapshots", "btn_browser_tab_snapshots" },
        { BrowserTab::Buses, "Buses", "btn_browser_tab_buses" },
        { BrowserTab::Assets, "Assets", "btn_browser_tab_assets" }
    }};
    for (size_t i = 0; i < tabDefs.size(); ++i) {
        const auto tab = std::get<0>(tabDefs[i]);
        auto btn = std::make_unique<DockTabButton>(std::get<1>(tabDefs[i]));
        btn->setComponentID(std::get<2>(tabDefs[i]));
        btn->setActiveTextColour(juce::Colour(OaeTheme::Colors::accentCyan));
        btn->onClick = [this, tab]() { set_active_tab(tab); };
        addAndMakeVisible(*btn);
        tabButtons_[i] = std::move(btn);
    }

    addAndMakeVisible(searchBox_);
    searchBox_.setComponentID("browser_search_box");
    searchBox_.setTextToShowWhenEmpty("Search events, stems, tags...", juce::Colour(OaeTheme::Colors::textMuted));
    searchBox_.addListener(this);
    searchBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    searchBox_.setColour(juce::TextEditor::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    searchBox_.setColour(juce::TextEditor::outlineColourId, juce::Colour(OaeTheme::Colors::borderSubtle));

    addAndMakeVisible(treeView_);
    treeView_.setComponentID("browser_tree_view");
    treeView_.setDefaultOpenness(true);
    treeView_.setRootItemVisible(false);
    treeView_.setColour(juce::TreeView::backgroundColourId, juce::Colour(OaeTheme::Colors::bgSurface));
    treeView_.addMouseListener(this, false);

    paramsHeaderLabel_.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
    paramsHeaderLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textMuted));
    addAndMakeVisible(paramsHeaderLabel_);

    paramsList_ = std::make_unique<ParametersListComponent>(*this);
    paramsList_->setComponentID("browser_params_list");
    paramsViewport_.setViewedComponent(paramsList_.get(), false);
    paramsViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(paramsViewport_);

    set_active_tab(BrowserTab::Events);
}

AuthoringTreeBrowser::~AuthoringTreeBrowser()
{
    model_.remove_listener(this);
    treeView_.setRootItem(nullptr);
}

void AuthoringTreeBrowser::set_active_tab(BrowserTab tab)
{
    activeTab_ = tab;
    for (size_t i = 0; i < tabButtons_.size(); ++i) {
        const bool selected = (static_cast<int>(activeTab_) == static_cast<int>(i));
        tabButtons_[i]->setTabSelected(selected);
        tabButtons_[i]->setToggleState(selected, juce::dontSendNotification);
    }
    rebuild_tree();
}

void AuthoringTreeBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgSurface));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(34, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(paramsHeaderLabel_.getY() - 2, 0.0f, static_cast<float>(getWidth()));
    g.drawRect(getLocalBounds(), 1);
}

void AuthoringTreeBrowser::resized()
{
    auto area = getLocalBounds();

    auto tabsBar = area.removeFromTop(34).reduced(6, 6);
    const int gap = 4;
    const int tabW = std::max(40, (tabsBar.getWidth() - 3 * gap) / 4);
    for (size_t i = 0; i < tabButtons_.size(); ++i) {
        auto cell = (i == 3) ? tabsBar : tabsBar.removeFromLeft(tabW);
        tabButtons_[i]->setBounds(cell);
        if (i < 3) tabsBar.removeFromLeft(gap);
    }

    auto searchRow = area.removeFromTop(34).reduced(6, 4);
    searchBox_.setBounds(searchRow);

    int paramCount = 0;
    const auto& root = model_.document().get_root_json();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        paramCount = static_cast<int>(root["global_parameters"].size());
    }
    const int maxParamsH = std::max(72, static_cast<int>(getHeight() * 0.35f));
    const int desiredParamsH = std::min(maxParamsH, std::max(72, 26 + paramCount * 24));
    auto paramsArea = area.removeFromBottom(desiredParamsH);
    paramsHeaderLabel_.setBounds(paramsArea.removeFromTop(22).reduced(8, 2));
    paramsViewport_.setBounds(paramsArea.reduced(2, 2));
    if (paramsList_) {
        paramsList_->setSize(std::max(100, paramsViewport_.getWidth() - 8), std::max(24, paramCount * 24));
    }

    treeView_.setBounds(area.reduced(2, 0));
}

void AuthoringTreeBrowser::show_tab_context_menu()
{
    juce::PopupMenu menu;
    if (activeTab_ == BrowserTab::Events) {
        menu.addItem(1, "New Event");
    } else if (activeTab_ == BrowserTab::Snapshots) {
        menu.addItem(2, "New Snapshot");
    } else if (activeTab_ == BrowserTab::Buses) {
        menu.addItem(3, "New Bus");
        menu.addItem(4, "New VCA");
    } else if (activeTab_ == BrowserTab::Assets) {
        menu.addItem(6, "Import Audio...");
    }

    const int res = UiTestDialogSeam::show_modal_menu("browser_tab_context", menu);
    if (res == 1) model_.create_event("event:/new_event");
    else if (res == 2) model_.create_snapshot("snapshot:/new_snapshot");
    else if (res == 3) model_.create_bus("bus:/new_bus");
    else if (res == 4) model_.create_vca("vca:/new_vca");
    else if (res == 6) import_audio_dialog();
}

void AuthoringTreeBrowser::show_parameter_context_menu(const std::string& paramName)
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Global Parameter");
    if (!paramName.empty()) {
        menu.addSeparator();
        menu.addItem(2, "Rename (F2)");
        menu.addItem(3, "Duplicate (Ctrl+D)");
        menu.addItem(4, "Delete (Del)");
    }

    const int res = UiTestDialogSeam::show_modal_menu("browser_param_context", menu);
    if (res == 1) model_.create_global_parameter("NewParam");
    else if (res == 2) rename_selected_item();
    else if (res == 3) duplicate_selected_item();
    else if (res == 4) delete_selected_item();
}

void AuthoringTreeBrowser::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && event.eventComponent == &treeView_) {
        show_tab_context_menu();
    }
}

void AuthoringTreeBrowser::rebuild_tree()
{
    treeView_.setRootItem(nullptr);
    rootItem_ = std::make_unique<BrowserTreeItem>(*this, BrowserTreeItem::Kind::Root, ItemType::Unknown, "Project", "");

    // Helper to add a hierarchical path to a category node
    const auto add_path_hierarchy = [&](BrowserTreeItem* categoryNode, ItemType type, const std::string& fullPath, const std::string& prefix) {
        std::string sub = fullPath;
        if (!prefix.empty() && sub.rfind(prefix, 0) == 0) {
            sub = sub.substr(prefix.length());
        }

        juce::StringArray segments;
        segments.addTokens(juce::String(sub), "/", "");

        BrowserTreeItem* currentParent = categoryNode;
        for (int i = 0; i < segments.size(); ++i) {
            const juce::String seg = segments[i];
            if (seg.isEmpty()) continue;

            const bool isLast = (i == segments.size() - 1);
            if (isLast) {
                currentParent->addSubItem(new BrowserTreeItem(*this, BrowserTreeItem::Kind::Leaf, type, seg, fullPath));
            } else {
                BrowserTreeItem* folder = nullptr;
                for (int c = 0; c < currentParent->getNumSubItems(); ++c) {
                    auto* existing = dynamic_cast<BrowserTreeItem*>(currentParent->getSubItem(c));
                    if (existing && existing->kind_ == BrowserTreeItem::Kind::Folder && existing->name_ == seg) {
                        folder = existing;
                        break;
                    }
                }
                if (!folder) {
                    folder = new BrowserTreeItem(*this, BrowserTreeItem::Kind::Folder, type, seg, "");
                    currentParent->addSubItem(folder);
                }
                currentParent = folder;
            }
        }
    };

    if (activeTab_ == BrowserTab::Events) {
        for (const auto& ev : model_.get_event_paths()) {
            add_path_hierarchy(rootItem_.get(), ItemType::Event, ev, "event:/");
        }
    } else if (activeTab_ == BrowserTab::Snapshots) {
        for (const auto& sn : model_.get_snapshot_paths()) {
            add_path_hierarchy(rootItem_.get(), ItemType::Snapshot, sn, "snapshot:/");
        }
    } else if (activeTab_ == BrowserTab::Buses) {
        auto* busesCat = new BrowserTreeItem(*this, BrowserTreeItem::Kind::Category, ItemType::Bus, "Buses", "");
        for (const auto& b : model_.get_bus_paths()) {
            add_path_hierarchy(busesCat, ItemType::Bus, b, "bus:/");
        }
        rootItem_->addSubItem(busesCat);

        auto* vcasCat = new BrowserTreeItem(*this, BrowserTreeItem::Kind::Category, ItemType::Vca, "VCAs", "");
        for (const auto& v : model_.get_vca_paths()) {
            add_path_hierarchy(vcasCat, ItemType::Vca, v, "vca:/");
        }
        rootItem_->addSubItem(vcasCat);
    } else if (activeTab_ == BrowserTab::Assets) {
        for (const auto& s : model_.get_sample_ids()) {
            rootItem_->addSubItem(new BrowserTreeItem(*this, BrowserTreeItem::Kind::Leaf, ItemType::Sample, s, s));
        }
    }

    treeView_.setRootItem(rootItem_.get());
    rootItem_->setOpen(true);
    for (int i = 0; i < rootItem_->getNumSubItems(); ++i) {
        rootItem_->getSubItem(i)->setOpen(true);
    }

    // Apply any active search filter
    textEditorTextChanged(searchBox_);
    rebuild_parameters_list();
    resized();
}

void AuthoringTreeBrowser::rebuild_parameters_list()
{
    std::vector<ParametersListComponent::Row> rows;
    const auto& root = model_.document().get_root_json();
    const auto& sel = model_.get_selection();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        for (const auto& p : root["global_parameters"]) {
            const std::string name = p.value("name", "");
            if (name.empty()) continue;
            const std::string type = p.value("type", "continuous");
            std::string domain;
            if (type == "labeled") {
                const auto labels = p.contains("labels") && p["labels"].is_array() ? p["labels"].size() : 0;
                domain = (labels == 2) ? "Bool" : "Label";
            } else if (type == "discrete") {
                domain = std::to_string(static_cast<int>(p.value("min", 0.0))) + " .. " +
                         std::to_string(static_cast<int>(p.value("max", 1.0)));
            } else {
                domain = juce::String::formatted("%.1f .. %.1f", p.value("min", 0.0), p.value("max", 1.0)).toStdString();
            }
            const bool isSel = (sel.type == ItemType::GlobalParameter && sel.id_or_path == name);
            rows.push_back({ name, domain, isSel });
        }
    }
    paramsHeaderLabel_.setText("PROJECT PARAMETERS (" + juce::String(rows.size()) + ")", juce::dontSendNotification);
    if (paramsList_) {
        paramsList_->set_rows(std::move(rows));
    }
}

void AuthoringTreeBrowser::textEditorTextChanged(juce::TextEditor& editor)
{
    const auto query = editor.getText().trim();
    if (rootItem_ != nullptr) {
        rootItem_->filter(query);
        treeView_.repaint();
    }
}

void AuthoringTreeBrowser::rename_selected_item()
{
    const auto sel = model_.get_selection();
    if (!sel.is_valid()) return;

    int seamResult = 0;
    std::string seamText;
    if (UiTestDialogSeam::intercept_alert("browser_rename", "Rename Item", seamResult, seamText)) {
        if (seamResult == 1 && !seamText.empty() && seamText != sel.id_or_path) {
            model_.rename_item(sel.type, sel.id_or_path, seamText);
        }
        return;
    }

    juce::AlertWindow dialog("Rename Item", "Enter new name/path for " + sel.id_or_path, juce::AlertWindow::QuestionIcon);
    dialog.addTextEditor("name", sel.id_or_path);
    dialog.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() == 1) {
        const auto newName = dialog.getTextEditorContents("name").trim().toStdString();
        if (!newName.empty() && newName != sel.id_or_path) {
            model_.rename_item(sel.type, sel.id_or_path, newName);
        }
    }
}

void AuthoringTreeBrowser::duplicate_selected_item()
{
    const auto sel = model_.get_selection();
    if (sel.is_valid()) {
        model_.duplicate_item(sel.type, sel.id_or_path);
    }
}

void AuthoringTreeBrowser::delete_selected_item()
{
    const auto sel = model_.get_selection();
    if (!sel.is_valid()) return;

    int seamResult = 0;
    std::string seamText;
    if (UiTestDialogSeam::intercept_alert("browser_delete", "Delete Item", seamResult, seamText)) {
        if (seamResult == 1) {
            model_.delete_item(sel.type, sel.id_or_path);
        }
        return;
    }

    if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Delete Item",
        "Are you sure you want to delete '" + sel.id_or_path + "'?", "Delete", "Cancel")) {
        model_.delete_item(sel.type, sel.id_or_path);
    }
}

bool AuthoringTreeBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::F2Key) {
        rename_selected_item();
        return true;
    }
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0)) {
        duplicate_selected_item();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        delete_selected_item();
        return true;
    }
    return Component::keyPressed(key);
}

void AuthoringTreeBrowser::on_project_changed()
{
    rebuild_tree();
}

void AuthoringTreeBrowser::on_selection_changed(const ItemSelection&)
{
    treeView_.repaint();
    rebuild_parameters_list();
}

void AuthoringTreeBrowser::on_item_renamed(ItemType, const std::string&, const std::string&)
{
    rebuild_tree();
}

void AuthoringTreeBrowser::on_item_deleted(ItemType, const std::string&)
{
    rebuild_tree();
}

void AuthoringTreeBrowser::import_audio_dialog()
{
    fs::path chosenPath;
    if (UiTestDialogSeam::intercept_file_chooser("browser_import_audio", "Import Audio...", chosenPath)) {
        if (!chosenPath.empty() && fs::is_regular_file(chosenPath)) {
            model_.begin_gesture();
            model_.import_audio_asset(chosenPath);
            model_.end_gesture();
            rebuild_tree();
        }
        return;
    }

    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Import Audio...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.ogg;*.mp3;*.flac"
    );

    const auto chooserFlags = juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectMultipleItems;

    activeFileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        const auto results = fc.getResults();
        if (results.isEmpty()) return;

        model_.begin_gesture();
        for (const auto& file : results) {
            model_.import_audio_asset(fs::u8path(file.getFullPathName().toStdString()));
        }
        model_.end_gesture();
        rebuild_tree();
    });
}

bool AuthoringTreeBrowser::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files) {
        const juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            return true;
        }
    }
    return false;
}

void AuthoringTreeBrowser::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    repaint();
}

void AuthoringTreeBrowser::fileDragMove(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
}

void AuthoringTreeBrowser::fileDragExit(const juce::StringArray& /*files*/)
{
    repaint();
}

void AuthoringTreeBrowser::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    model_.begin_gesture();
    for (const auto& file : files) {
        const juce::File jFile(file);
        const juce::String ext = jFile.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            model_.import_audio_asset(fs::path(jFile.getFullPathName().toStdString()));
        }
    }
    model_.end_gesture();
    rebuild_tree();
}

} // namespace oae::studio
