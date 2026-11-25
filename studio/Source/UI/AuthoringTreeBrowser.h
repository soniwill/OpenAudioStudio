#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "DockTabButton.h"

namespace oae::studio {

/** Hierarchical tree browser for project events, snapshots, buses, VCAs, assets and parameters. */
class AuthoringTreeBrowser : public juce::Component,
                             public AuthoringModel::Listener,
                             public juce::TextEditor::Listener,
                             public juce::FileDragAndDropTarget {
public:
    enum class BrowserTab {
        Events = 0,
        Snapshots = 1,
        Buses = 2,
        Assets = 3
    };

    explicit AuthoringTreeBrowser(AuthoringModel& model);
    ~AuthoringTreeBrowser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_active_tab(BrowserTab tab);
    BrowserTab get_active_tab() const noexcept { return activeTab_; }

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;
    void on_item_renamed(ItemType type, const std::string& old_id, const std::string& new_id) override;
    void on_item_deleted(ItemType type, const std::string& id) override;

    // juce::TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& editor) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Tree actions
    void rebuild_tree();
    void rename_selected_item();
    void duplicate_selected_item();
    void delete_selected_item();
    void import_audio_dialog();
    void show_tab_context_menu();
    void show_parameter_context_menu(const std::string& paramName);

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseUp(const juce::MouseEvent& event) override;

    AuthoringModel& model() noexcept { return model_; }

private:
    class BrowserTreeItem;
    class ParametersListComponent;

    void rebuild_parameters_list();

    AuthoringModel& model_;
    BrowserTab activeTab_{ BrowserTab::Events };
    std::array<std::unique_ptr<DockTabButton>, 4> tabButtons_;
    juce::TextEditor searchBox_{ "search" };
    juce::TreeView treeView_;
    std::unique_ptr<BrowserTreeItem> rootItem_;
    juce::Label paramsHeaderLabel_{ "params_header", "PROJECT PARAMETERS (0)" };
    juce::Viewport paramsViewport_;
    std::unique_ptr<ParametersListComponent> paramsList_;
    std::unique_ptr<juce::FileChooser> activeFileChooser_;
};

using EventTreePanel = AuthoringTreeBrowser;

} // namespace oae::studio
