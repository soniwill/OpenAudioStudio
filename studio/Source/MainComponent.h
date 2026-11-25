#pragma once

#include <filesystem>
#include <map>
#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>
#include "StudioEngine.h"
#include "Project/ProjectDocument.h"
#include "Project/AsyncBankPublisher.h"
#include "Model/AuthoringModel.h"
#include "Transport/AuditionTransport.h"
#include "Transport/PreviewRebuilder.h"
#include "UI/AuthoringTreeBrowser.h"
#include "UI/InspectorPanel.h"
#include "UI/TimelinePanel.h"
#include "UI/ProjectToolbar.h"
#include "UI/TransportBar.h"
#include "UI/MixerDeckPanel.h"
#include "UI/ContextualBottomDeck.h"
#include "UI/OaeDarkLookAndFeel.h"
#include "UI/DockingLayoutManager.h"
#include "UI/TutorialHighlightOverlay.h"
#include "Live/LiveBridgeServer.h"

namespace oae::studio {

class DiagnosticsLogPanel : public juce::Component {
public:
    DiagnosticsLogPanel();
    void resized() override;
    void paint(juce::Graphics& g) override;

    void append_message(const juce::String& text);
    void clear();
    juce::String get_log_text() const { return logEditor.getText(); }

    juce::Label headerLabel{ "header", "DIAGNOSTICS & BUILD LOG" };
    juce::TextEditor logEditor;
};

/** Startup options for configuring headless audio, layout isolation, and timers in MainComponent. */
struct MainComponentOptions {
    bool headlessAudio{ false };
    std::filesystem::path layoutFile;
    bool autoLoadExample{ true };
    bool enableLiveBridge{ true };
    bool enableTimers{ true };
};

/** Root studio workspace component hosting toolbars, dockable panels, engine, and live bridge. */
class MainComponent : public juce::Component, public juce::Timer, public LiveBridgeServer::Listener {
public:
    MainComponent();
    explicit MainComponent(MainComponentOptions options);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void sync_workspace_layout();
    void toggle_hide_all();

    // LiveBridgeServer::Listener
    void on_live_client_connected(const std::string& client_name, uint32_t client_crc32, bool crc_match) override;
    void on_live_client_disconnected() override;

    bool keyPressed(const juce::KeyPress& key) override;

    void new_project();
    void open_project();
    bool open_project_file(const std::filesystem::path& project_file);
    void save_project();
    void build_bank();
    /** Opens the bundled HTML manual in the system browser. */
    void open_help();
    /** Renders software UI snapshots of the main window and individual panels to PNG files in out_dir. */
    bool dump_ui_screenshots(const std::filesystem::path& out_dir);

    StudioEngine& engine() noexcept { return engine_; }
    ProjectDocument& document() noexcept { return document_; }
    AuthoringModel& authoring_model() noexcept { return authoringModel_; }
    AsyncBankPublisher& publisher() noexcept { return publisher_; }
    AuditionTransport& transport() noexcept { return transport_; }
    PreviewRebuilder& preview_rebuilder() noexcept { return previewRebuilder_; }
    DiagnosticsLogPanel& diagnostics() noexcept { return diagnosticsPanel_; }
    MixerDeckPanel& mixer_deck() noexcept { return contextualDeck_.mixer_deck(); }
    ContextualBottomDeck& contextual_deck() noexcept { return contextualDeck_; }
    DockingLayoutManager& docking_layout() noexcept { return dockingLayout_; }
    LiveBridgeServer& live_bridge() noexcept { return liveBridge_; }
    /** Returns the non-interactive spotlight overlay hosted over the main workspace. */
    TutorialHighlightOverlay& highlight_overlay() noexcept { return highlightOverlay_; }
    ProjectToolbar& project_toolbar() noexcept { return projectToolbar_; }
    TransportBar& transport_bar() noexcept { return transportBar_; }
    AuthoringTreeBrowser& browser_panel() noexcept { return eventTreePanel_; }
    TimelinePanel& timeline_panel() noexcept { return timelinePanel_; }
    InspectorPanel& inspector_panel() noexcept { return inspectorPanel_; }

private:
    class WorkspaceBar;
    class SlotHeaderBar;

    std::filesystem::path resolved_layout_file_path() const;
    juce::Component& panel_component(DockPanelId id) noexcept;
    void schedule_workspace_sync();
    void drag_workspace_tab(DockPanelId panel, const juce::MouseEvent& event);
    void drop_workspace_tab(DockPanelId panel, const juce::MouseEvent& event);
    void layout_workspace_components();
    void update_workspace_chrome();
    void prompt_save_custom_preset();

    OaeDarkLookAndFeel lookAndFeel_;
    MainComponentOptions options_;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow_;
    DockingLayoutManager dockingLayout_;

    StudioEngine engine_;
    ProjectDocument document_;
    AuthoringModel authoringModel_{ document_ };
    AsyncBankPublisher publisher_;
    AsyncBankPublisher previewPublisher_;
    AuditionTransport transport_{ engine_, authoringModel_ };
    PreviewRebuilder previewRebuilder_{ engine_, authoringModel_, document_, transport_, previewPublisher_ };

    std::unique_ptr<WorkspaceBar> workspaceBar_;
    std::map<int, std::unique_ptr<SlotHeaderBar>> slotHeaders_;
    std::map<int, std::unique_ptr<SplitterBar>> workspaceSplitters_;
    WorkspaceGeometry workspaceGeometry_;
    juce::Rectangle<int> workspaceArea_;
    juce::Rectangle<int> dockingPreview_;
    std::optional<DockPanelId> draggingPanel_;
    std::optional<WorkspaceDrop> workspaceDrop_;
    bool dockingDragCancelled_{ false };
    std::array<std::unique_ptr<juce::DocumentWindow>, dockPanelCount> floatingWindows_;

    ProjectToolbar projectToolbar_;
    TransportBar transportBar_{ authoringModel_, transport_ };
    AuthoringTreeBrowser eventTreePanel_{ authoringModel_ };
    TimelinePanel timelinePanel_{ authoringModel_, &transport_ };
    InspectorPanel inspectorPanel_{ authoringModel_ };
    DiagnosticsLogPanel diagnosticsPanel_;
    ContextualBottomDeck contextualDeck_{ engine_, authoringModel_, transport_, diagnosticsPanel_, dockingLayout_.bottom_slot() };
    LiveBridgeServer liveBridge_;
    TutorialHighlightOverlay highlightOverlay_;

    std::unique_ptr<juce::FileChooser> activeFileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace oae::studio
