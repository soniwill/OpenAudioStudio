cmake_minimum_required(VERSION 3.25)
project(OpenAudioStudioLanguages C CXX)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/juce/CMakeLists.txt")
    add_subdirectory(juce)
else()
    message(FATAL_ERROR "JUCE 8.0.12 directory not found at studio/juce. Run: git submodule update --init --recursive")
endif()

juce_add_gui_app(oae_studio
    PRODUCT_NAME "OpenAudio Studio"
    VERSION "1.0.0"
)

target_sources(oae_studio PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    ../native/src/miniaudio_impl.c
    Source/StudioEngine.h
    Source/StudioEngine.cpp
    Source/Audio/AcousticMeterProcessor.h
    Source/Audio/AcousticMeterProcessor.cpp
    Source/Model/AuthoringModel.h
    Source/Model/AuthoringModel.cpp
    Source/UI/AuthoringTreeBrowser.h
    Source/UI/AuthoringTreeBrowser.cpp
    Source/UI/Inspector/IInspectorCard.h
    Source/UI/Inspector/InspectorCardBase.h
    Source/UI/Inspector/InspectorCardBase.cpp
    Source/UI/Inspector/InspectorCardHeader.h
    Source/UI/Inspector/InspectorCardHeader.cpp
    Source/UI/Inspector/InspectorClipboard.h
    Source/UI/Inspector/InspectorClipboard.cpp
    Source/UI/Inspector/EventHeaderCard.h
    Source/UI/Inspector/EventHeaderCard.cpp
    Source/UI/Inspector/TrackCard.h
    Source/UI/Inspector/TrackCard.cpp
    Source/UI/Inspector/ClipCard.h
    Source/UI/Inspector/ClipCard.cpp
    Source/UI/Inspector/MaskTransitionCard.h
    Source/UI/Inspector/MaskTransitionCard.cpp
    Source/UI/Inspector/BusCard.h
    Source/UI/Inspector/BusCard.cpp
    Source/UI/Inspector/ParameterCard.h
    Source/UI/Inspector/ParameterCard.cpp
    Source/UI/Inspector/SnapshotCard.h
    Source/UI/Inspector/SnapshotCard.cpp
    Source/UI/Inspector/VcaCard.h
    Source/UI/Inspector/VcaCard.cpp
    Source/UI/Inspector/SampleCard.h
    Source/UI/Inspector/SampleCard.cpp
    Source/UI/InspectorPanel.h
    Source/UI/InspectorPanel.cpp
    Source/UI/TimelineRuler.h
    Source/UI/TimelineRuler.cpp
    Source/UI/TrackHeaderPanel.h
    Source/UI/TrackHeaderPanel.cpp
    Source/UI/TimelineTrackLane.h
    Source/UI/TimelineTrackLane.cpp
    Source/UI/TimelinePanel.h
    Source/UI/TimelinePanel.cpp
    Source/UI/TimelineParameterStrip.h
    Source/UI/TimelineParameterStrip.cpp
    Source/UI/LayerStackView.h
    Source/UI/LayerStackView.cpp
    Source/Timeline/TimelineSnap.h
    Source/Timeline/TimelineSnap.cpp
    Source/Timeline/WaveformCache.h
    Source/Timeline/WaveformCache.cpp
    Source/Transport/AuditionTransport.h
    Source/Transport/AuditionTransport.cpp
    Source/Transport/PreviewRebuilder.h
    Source/Transport/PreviewRebuilder.cpp
    Source/UI/TransportBar.h
    Source/UI/TransportBar.cpp
    Source/UI/ProjectToolbar.h
    Source/UI/ProjectToolbar.cpp
    Source/UI/MixerDeckPanel.h
    Source/UI/MixerDeckPanel.cpp
    Source/UI/Theme.h
    Source/UI/OaePrimitives.h
    Source/UI/OaePrimitives.cpp
    Source/UI/OaeSlider.h
    Source/UI/OaeMeterComponent.h
    Source/UI/OaeMeterComponent.cpp
    Source/UI/OaeDarkLookAndFeel.h
    Source/UI/OaeDarkLookAndFeel.cpp
    Source/UI/VectorIcons.h
    Source/UI/VectorIcons.cpp
    Source/UI/GripHandle.h
    Source/UI/GripHandle.cpp
    Source/UI/DockingLayoutManager.h
    Source/UI/DockingLayoutManager.cpp
    Source/UI/DockingWorkspacePersistence.cpp
    Source/UI/PlaylistDeck.h
    Source/UI/PlaylistDeck.cpp
    Source/UI/EffectChainDeck.h
    Source/UI/EffectChainDeck.cpp
    Source/UI/LogicConditionsDeck.h
    Source/UI/LogicConditionsDeck.cpp
    Source/UI/Spatializer3DDeck.h
    Source/UI/Spatializer3DDeck.cpp
    Source/UI/TimelineOverviewMinimap.h
    Source/UI/TimelineOverviewMinimap.cpp
    Source/UI/GlobalParametersDeck.h
    Source/UI/GlobalParametersDeck.cpp
    Source/UI/ContextualBottomDeck.h
    Source/UI/ContextualBottomDeck.cpp
    Source/UI/TutorialHighlightOverlay.h
    Source/UI/TutorialHighlightOverlay.cpp
    Source/Project/ProjectDocument.h
    Source/Project/ProjectDocument.cpp
    Source/Project/AsyncBankPublisher.h
    Source/Project/AsyncBankPublisher.cpp
    Source/MCP/McpProtocol.h
    Source/MCP/McpServer.h
    Source/MCP/McpServer.cpp
    Source/Live/LiveBridgeServer.h
    Source/Live/LiveBridgeServer.cpp
    Source/Testing/UiTestHooks.h
    Source/Testing/HeadlessAudioProbe.h
    Source/Testing/HeadlessAudioProbe.cpp
    Source/Testing/UiInteractionDriver.h
    Source/Testing/UiInteractionDriver.cpp
    Source/Testing/UiControlAuditor.h
    Source/Testing/UiControlAuditor.cpp
    Source/Testing/UiControlTestFixture.h
    Source/Testing/UiControlTestFixture.cpp
    Source/Testing/UiControlContracts.h
    Source/Testing/UiControlContractsShell.cpp
    Source/Testing/UiControlContractsAudioCore.cpp
    Source/Testing/UiControlContractsTimelineLayers.cpp
    Source/Testing/UiControlContractsInspector.cpp
    Source/Testing/UiControlContractsDecks.cpp
    Source/Testing/UiControlTestRunner.h
    Source/Testing/UiControlTestRunner.cpp
)

target_include_directories(oae_studio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/external"
    Source
)

target_compile_definitions(oae_studio PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(oae_studio PRIVATE
    oae_audio
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
)

if(MSVC)
    target_compile_options(oae_studio PRIVATE /W4 /utf-8 $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
endif()

# Post-build copy of oae_audio.dll and oae_bankc.exe side-by-side with oae_studio.exe
add_custom_command(TARGET oae_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:oae_audio>
        $<TARGET_FILE_DIR:oae_studio>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:oae_bankc>
        $<TARGET_FILE_DIR:oae_studio>
    COMMENT "Copying oae_audio.dll and oae_bankc.exe next to oae_studio"
)
