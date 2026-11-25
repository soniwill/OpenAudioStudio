#include "ContextualBottomDeck.h"
#include "OaeDarkLookAndFeel.h"

namespace oae::studio {

ContextualBottomDeck::ContextualBottomDeck(StudioEngine& engine,
                                           AuthoringModel& model,
                                           AuditionTransport& transport,
                                           juce::Component& diagnosticsPanel,
                                           DockingLayoutManager::TabSlot& slot)
    : engine_(engine), model_(model), transport_(transport), diagnosticsPanel_(diagnosticsPanel), slot_(slot)
{
    model_.add_listener(this);
    addAndMakeVisible(slot_);

    mixerDeck_.setComponentID("deck_mixer");
    diagnosticsPanel_.setComponentID("deck_diagnostics");
    playlistDeck_.setComponentID("deck_playlist");
    effectsDeck_.setComponentID("deck_effects");
    logicDeck_.setComponentID("deck_logic");
    spatializerDeck_.setComponentID("deck_spatializer");
    parametersDeck_.setComponentID("deck_parameters");

    slot_.add_panel(mixerDeck_, "Mixer", "btn_tab_mixer");
    slot_.add_panel(effectsDeck_, "Effect Chain", "btn_tab_effects");
    slot_.add_panel(logicDeck_, "Logic Conditions", "btn_tab_logic");
    slot_.add_panel(playlistDeck_, "Playlist", "btn_tab_playlist");
    slot_.add_panel(spatializerDeck_, "3D Spatializer", "btn_tab_spatializer");
    slot_.add_panel(parametersDeck_, "Parameters (0)", "btn_tab_parameters");
    slot_.add_panel(diagnosticsPanel_, "Diagnostics", "btn_tab_diagnostics");

    on_project_changed();

    parametersDeck_.onPopOutRequested = [this] {
        const int tabIdx = static_cast<int>(DeckTab::Parameters);
        if (parametersDeck_.is_floating()) {
            slot_.dock_back(tabIdx);
            parametersDeck_.set_floating(false);
        } else {
            slot_.pop_out(tabIdx);
            parametersDeck_.set_floating(true);
        }
    };
}

ContextualBottomDeck::~ContextualBottomDeck()
{
    slot_.clear();
    model_.remove_listener(this);
}

void ContextualBottomDeck::set_active_tab(DeckTab tab)
{
    slot_.select(static_cast<int>(tab));
}

DeckTab ContextualBottomDeck::get_active_tab() const noexcept
{
    return static_cast<DeckTab>(slot_.selected_panel());
}


void ContextualBottomDeck::on_project_changed()
{
    int count = 0;
    const auto& root = model_.document().get_root_json();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        count = static_cast<int>(root["global_parameters"].size());
    }
    slot_.set_panel_title(static_cast<int>(DeckTab::Parameters), "Parameters (" + juce::String(count) + ")");
}

void ContextualBottomDeck::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Bus) {
        set_active_tab(DeckTab::Effects);
        effectsDeck_.set_target_bus(selection.id_or_path);
    } else if (selection.type == ItemType::Snapshot) {
        set_active_tab(DeckTab::Mixer);
    } else if (selection.type == ItemType::Event) {
        if (selection.logic_index >= 0) {
            set_active_tab(DeckTab::Logic);
        } else if (playlistDeck_.has_selected_instrument(selection)) {
            set_active_tab(DeckTab::Playlist);
        } else if (selection.track_index >= 0 && selection.instrument_index < 0) {
            set_active_tab(DeckTab::Effects);
        }
    }
}

void ContextualBottomDeck::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaePalette::bgDark));

    // Bottom deck outer border
    g.setColour(juce::Colour(OaePalette::borderSubtle));
    g.drawRect(getLocalBounds(), 1);
}

void ContextualBottomDeck::resized()
{
    juce::FlexBox layout;
    layout.items.add(juce::FlexItem(slot_).withFlex(1));
    layout.performLayout(getLocalBounds());
}


} // namespace oae::studio
