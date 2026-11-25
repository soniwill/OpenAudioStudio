#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/AuthoringModel.h"
#include "../StudioEngine.h"
#include "../Transport/AuditionTransport.h"
#include "MixerDeckPanel.h"
#include "PlaylistDeck.h"
#include "EffectChainDeck.h"
#include "LogicConditionsDeck.h"
#include "Spatializer3DDeck.h"
#include "GlobalParametersDeck.h"
#include "DockingLayoutManager.h"
#include <memory>
#include <string>

namespace oae::studio {

enum class DeckTab {
    Mixer = 0,
    Effects = 1,
    Logic = 2,
    Playlist = 3,
    Spatializer = 4,
    Parameters = 5,
    Diagnostics = 6
};

/** Contextual Bottom Deck hosting specialized sub-editors with seamless switching (PRO-UI-02). */
class ContextualBottomDeck : public juce::Component, public AuthoringModel::Listener {
public:
    ContextualBottomDeck(StudioEngine& engine,
                         AuthoringModel& model,
                         AuditionTransport& transport,
                         juce::Component& diagnosticsPanel,
                         DockingLayoutManager::TabSlot& slot);
    ~ContextualBottomDeck() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_active_tab(DeckTab tab);
    DeckTab get_active_tab() const noexcept;

    MixerDeckPanel& mixer_deck() noexcept { return mixerDeck_; }
    PlaylistDeck& playlist_deck() noexcept { return playlistDeck_; }
    EffectChainDeck& effects_deck() noexcept { return effectsDeck_; }
    LogicConditionsDeck& logic_deck() noexcept { return logicDeck_; }
    Spatializer3DDeck& spatializer_deck() noexcept { return spatializerDeck_; }
    GlobalParametersDeck& parameters_deck() noexcept { return parametersDeck_; }

    // AuthoringModel::Listener
    void on_project_changed() override;
    void on_selection_changed(const ItemSelection& selection) override;

private:
    StudioEngine& engine_;
    AuthoringModel& model_;
    AuditionTransport& transport_;
    juce::Component& diagnosticsPanel_;

    MixerDeckPanel mixerDeck_{ engine_, model_, transport_ };
    PlaylistDeck playlistDeck_{ model_ };
    EffectChainDeck effectsDeck_{ engine_, model_ };
    LogicConditionsDeck logicDeck_{ model_, transport_ };
    Spatializer3DDeck spatializerDeck_{ transport_ };
    GlobalParametersDeck parametersDeck_{ model_, transport_ };

    DockingLayoutManager::TabSlot& slot_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContextualBottomDeck)
};

} // namespace oae::studio
