cmake_minimum_required(VERSION 3.22)

project(PatchCraft
    VERSION 0.1.0
    DESCRIPTION "PatchCraft - Instrument Builder + Player platform"
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(PATCHCRAFT_BUILD_STUDIO "Build PatchCraft Studio standalone app" ON)
option(PATCHCRAFT_BUILD_PLAYER "Build PatchCraft Player VST3 plugin"   ON)

# ---------------------------------------------------------------------------
# Locate JUCE
# ---------------------------------------------------------------------------
set(PATCHCRAFT_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
    CACHE PATH "Path to JUCE source tree")

if(NOT EXISTS "${PATCHCRAFT_JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${PATCHCRAFT_JUCE_PATH}. "
        "Set -DPATCHCRAFT_JUCE_PATH=/path/to/JUCE.")
endif()

add_subdirectory("${PATCHCRAFT_JUCE_PATH}" "${CMAKE_BINARY_DIR}/_juce" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Shared core library - used by Studio and Player
# ---------------------------------------------------------------------------
add_library(PatchCraftCore STATIC
    Source/Shared/PatchCraftTypes.cpp
    Source/Shared/PatchCraftProject.cpp
    Source/Shared/PatchCraftPackFormat.cpp
    Source/Shared/PatchCraftPackReader.cpp
    Source/Shared/PatchCraftPackWriter.cpp
    Source/Shared/PatchCraftLookAndFeel.cpp
    Source/Shared/SampleMap.cpp
    Source/Shared/SampleVoice.cpp
    Source/Shared/SampleSynthEngine.cpp
    Source/Shared/SynthEngine.cpp
    Source/Shared/AdvancedEqProcessor.cpp
    Source/Shared/AdvancedFxProcessor.cpp
    Source/Shared/AudioUtilityProcessor.cpp
    Source/Shared/EffectEngine.cpp
    Source/Shared/DspRoutingEngine.cpp
    Source/Shared/ArpeggiatorRuntime.cpp
    Source/Shared/MidiPlaygroundRuntime.cpp
    Source/Shared/MidiPlaygroundPattern.cpp
    Source/Shared/EngineFactory.cpp
    Source/Shared/InstrumentTemplates.cpp
    Source/Shared/ParameterModel.cpp
    Source/Shared/PresetGenerator.cpp
    Source/Shared/LayoutModel.cpp
    Source/Shared/LiveValueStore.cpp
    Source/Shared/AssetManager.cpp
    Source/Shared/AiAssistService.cpp
    Source/Shared/LibraryScanner.cpp
    Source/Shared/SfzImporter.cpp
    Source/Shared/LicenseValidator.cpp
)

target_include_directories(PatchCraftCore PUBLIC Source/Shared)

target_link_libraries(PatchCraftCore
    PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(PatchCraftCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

# ---------------------------------------------------------------------------
# Automated engine smoke tests
# ---------------------------------------------------------------------------
add_executable(PatchCraftAudioSmokeTests
    Source/Tests/AudioSmokeTests.cpp)
target_link_libraries(PatchCraftAudioSmokeTests PRIVATE PatchCraftCore)

# ---------------------------------------------------------------------------
# Studio standalone app
# ---------------------------------------------------------------------------
if(PATCHCRAFT_BUILD_STUDIO)
    juce_add_gui_app(PatchCraftStudio
        PRODUCT_NAME "PatchCraft Studio"
        COMPANY_NAME "PatchCraft"
        BUNDLE_ID "com.patchcraft.studio"
        VERSION "0.1.0")

    juce_generate_juce_header(PatchCraftStudio)

    target_sources(PatchCraftStudio PRIVATE
        Source/Studio/Main.cpp
        Source/Studio/StudioApplication.cpp
        Source/Studio/StudioMainComponent.cpp
        Source/Studio/TopToolbar.cpp
        Source/Studio/ElementPalette.cpp
        Source/Studio/BrandingLabPage.cpp
        Source/Studio/BuiltAssetLibraryComponent.cpp
        Source/Studio/ExpansionLibraryPanel.cpp
        Source/Studio/LayersPanel.cpp
        Source/Studio/CanvasEditor.cpp
        Source/Studio/InspectorPanel.cpp
        Source/Studio/BottomPanel.cpp
        Source/Studio/SampleMapperComponent.cpp
        Source/Studio/KeyzonesComponent.cpp
        Source/Studio/ParametersComponent.cpp
        Source/Studio/KnobBuilderComponent.cpp
        Source/Studio/SliderBuilderComponent.cpp
        Source/Studio/MeterBuilderComponent.cpp
        Source/Studio/PresetsComponent.cpp
        Source/Studio/InstrumentPreviewComponent.cpp
        Source/Studio/StudioInstrumentRenderer.cpp
        Source/Studio/TestPage.cpp
        Source/Studio/MidiPlaygroundPage.cpp
        Source/Studio/DspPage.cpp
        Source/Studio/CanvasToolbar.cpp
        Source/Studio/ControlBuilderComponent.cpp
        Source/Studio/StudioAudioService.cpp
        Source/Studio/SettingsDialog.cpp
        Source/Studio/LibraryMetadataComponent.cpp
        Source/Studio/SampleWaveformViewer.cpp
        Source/Studio/VelocityMapViewer.cpp
        Source/Studio/SampleMapEditor.cpp)

    target_link_libraries(PatchCraftStudio PRIVATE PatchCraftCore)
endif()

# ---------------------------------------------------------------------------
# Player VST3 instrument plugin (sample + synth packs)
# ---------------------------------------------------------------------------
if(PATCHCRAFT_BUILD_PLAYER)
    juce_add_plugin(PatchCraftPlayer
        PRODUCT_NAME "PatchCraft Player"
        COMPANY_NAME "PatchCraft"
        BUNDLE_ID "com.patchcraft.player"
        PLUGIN_MANUFACTURER_CODE "Pcrf"
        PLUGIN_CODE "Pcpl"
        FORMATS VST3 Standalone
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Instrument Sampler
        COPY_PLUGIN_AFTER_BUILD FALSE
        VERSION "0.1.0")

    juce_generate_juce_header(PatchCraftPlayer)

    target_sources(PatchCraftPlayer PRIVATE
        Source/Player/PluginProcessor.cpp
        Source/Player/PluginEditor.cpp
        Source/Player/PlayerGuiRenderer.cpp
        Source/Player/PlayerLookAndFeel.cpp
        Source/Player/LibraryBrowser.cpp)

    target_compile_definitions(PatchCraftPlayer PRIVATE PATCHCRAFT_PLAYER_FX=0)
    target_link_libraries(PatchCraftPlayer PRIVATE PatchCraftCore)
endif()

# ---------------------------------------------------------------------------
# Player FX VST3 audio effect plugin (effect packs)
# ---------------------------------------------------------------------------
option(PATCHCRAFT_BUILD_PLAYER_FX "Build PatchCraft Player FX VST3 (audio effect)" ON)
if(PATCHCRAFT_BUILD_PLAYER_FX)
    juce_add_plugin(PatchCraftPlayerFX
        PRODUCT_NAME "PatchCraft Player FX"
        COMPANY_NAME "PatchCraft"
        BUNDLE_ID "com.patchcraft.playerfx"
        PLUGIN_MANUFACTURER_CODE "Pcrf"
        PLUGIN_CODE "Pcfx"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx
        COPY_PLUGIN_AFTER_BUILD FALSE
        VERSION "0.1.0")

    # JUCE's effect target can keep this property false in generated metadata
    # unless it is explicitly set on the target. FX patches still need MIDI
    # for MIDI Learn, mod wheel, aftertouch, and CC-driven automation.
    set_target_properties(PatchCraftPlayerFX PROPERTIES JUCE_NEEDS_MIDI_INPUT TRUE)

    juce_generate_juce_header(PatchCraftPlayerFX)

    target_sources(PatchCraftPlayerFX PRIVATE
        Source/Player/PluginProcessor.cpp
        Source/Player/PluginEditor.cpp
        Source/Player/PlayerGuiRenderer.cpp
        Source/Player/PlayerLookAndFeel.cpp
        Source/Player/LibraryBrowser.cpp)

    target_compile_definitions(PatchCraftPlayerFX PRIVATE PATCHCRAFT_PLAYER_FX=1)
    target_link_libraries(PatchCraftPlayerFX PRIVATE PatchCraftCore)
endif()

# ---------------------------------------------------------------------------
# Release checklist target
# ---------------------------------------------------------------------------
set(PATCHCRAFT_RELEASE_CHECK_TARGETS PatchCraftAudioSmokeTests)
if(PATCHCRAFT_BUILD_STUDIO)
    list(APPEND PATCHCRAFT_RELEASE_CHECK_TARGETS PatchCraftStudio)
endif()
if(PATCHCRAFT_BUILD_PLAYER)
    list(APPEND PATCHCRAFT_RELEASE_CHECK_TARGETS PatchCraftPlayer_Standalone PatchCraftPlayer_VST3)
endif()
if(PATCHCRAFT_BUILD_PLAYER_FX)
    list(APPEND PATCHCRAFT_RELEASE_CHECK_TARGETS PatchCraftPlayerFX_Standalone PatchCraftPlayerFX_VST3)
endif()

add_custom_target(PatchCraftReleaseChecklist
    DEPENDS ${PATCHCRAFT_RELEASE_CHECK_TARGETS})
