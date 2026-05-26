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
option(PATCHCRAFT_BUILD_PLAYER_FX "Build PatchCraft Player FX VST3 (audio effect)" ON)
option(PATCHCRAFT_STAGE_VST_EXPANSION "Stage the paid VST Expansion templates inside the Studio runtime" OFF)
option(PATCHCRAFT_ENABLE_AI_STUDIO "Enable the future paid AI Studio expansion UI surfaces" OFF)

if(PATCHCRAFT_ENABLE_AI_STUDIO)
    set(PATCHCRAFT_ENABLE_AI_STUDIO_VALUE 1)
else()
    set(PATCHCRAFT_ENABLE_AI_STUDIO_VALUE 0)
endif()

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
    Source/Shared/MultiInstrumentEngine.cpp
    Source/Shared/InstrumentTemplates.cpp
    Source/Shared/ParameterModel.cpp
    Source/Shared/PresetGenerator.cpp
    Source/Shared/LayoutModel.cpp
    Source/Shared/LiveValueStore.cpp
    Source/Shared/AssetManager.cpp
    Source/Shared/PcexpManager.cpp
    Source/Shared/AudiLockSecurity.cpp
    Source/Shared/AiAssistService.cpp
    Source/Shared/AiExpansionModule.cpp
    Source/Shared/AiImageService.cpp
    Source/Shared/LibraryScanner.cpp
    Source/Shared/SfzImporter.cpp
    Source/Shared/LicenseValidator.cpp
    Source/Shared/PluginClubPublisher.cpp
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

# ==============================================================================
# AI Expansion Packaging Target
# ==============================================================================
add_custom_target(PatchCraftAiExpansion
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CreatePatchCraftAiExpansionPackage.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building AI Expansion addon payload..."
)

target_compile_definitions(PatchCraftCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        PATCHCRAFT_ENABLE_AI_STUDIO=${PATCHCRAFT_ENABLE_AI_STUDIO_VALUE})

# ---------------------------------------------------------------------------
# Automated engine smoke tests
# ---------------------------------------------------------------------------
add_executable(PatchCraftAudioSmokeTests
    Source/Tests/AudioSmokeTests.cpp)
target_link_libraries(PatchCraftAudioSmokeTests PRIVATE PatchCraftCore)

# ---------------------------------------------------------------------------
# VST export smoke test
#
# Exercises VstExportModule::exportPlugin headlessly and verifies the
# produced .vst3 bundle has the structure a host expects. Skipped at
# runtime if the PatchCraft Player VST3 template isn't built yet.
# ---------------------------------------------------------------------------
add_executable(PatchCraftVstExportSmokeTest
    Source/Tests/VstExportSmokeTest.cpp
    Source/Studio/VstExportModule.cpp)
target_include_directories(PatchCraftVstExportSmokeTest PRIVATE Source/Studio)
target_link_libraries(PatchCraftVstExportSmokeTest PRIVATE PatchCraftCore)
# After the test exe is built, point it at the freshly-built Player VST3
# template via an env var so the harness doesn't need to know paths.
if(PATCHCRAFT_BUILD_PLAYER)
    add_dependencies(PatchCraftVstExportSmokeTest PatchCraftPlayer_VST3)
endif()
if(PATCHCRAFT_BUILD_PLAYER_FX)
    add_dependencies(PatchCraftVstExportSmokeTest PatchCraftPlayerFX_VST3)
endif()

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
    if(APPLE)
        # Match CFBundleExecutable in the generated Info.plist so Finder can
        # launch the .app bundle instead of reporting it as incomplete.
        set_target_properties(PatchCraftStudio PROPERTIES OUTPUT_NAME "PatchCraft Studio")
    else()
        set_target_properties(PatchCraftStudio PROPERTIES OUTPUT_NAME "PatchCraftStudio")
    endif()

    target_sources(PatchCraftStudio PRIVATE
        Source/Studio/Main.cpp
        Source/Studio/StudioApplication.cpp
        Source/Studio/StudioMainComponent.cpp
        Source/Player/PluginProcessor.cpp
        Source/Player/PluginEditor.cpp
        Source/Player/PlayerGuiRenderer.cpp
        Source/Player/LibraryBrowser.cpp
        Source/Studio/TopToolbar.cpp
        Source/Studio/ElementPalette.cpp
        Source/Studio/AiImageBuilderComponent.cpp
        Source/Studio/AnimationLabPage.cpp
        Source/Studio/BrandingLabPage.cpp
        Source/Studio/BuiltAssetLibraryComponent.cpp
        Source/Studio/ExpansionLibraryPanel.cpp
        Source/Studio/LayersPanel.cpp
        Source/Studio/CanvasEditor.cpp
        Source/Studio/WorkflowPage.cpp
        Source/Studio/LaunchCenterPage.cpp
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
        Source/Studio/OneShotMakerPage.cpp
        Source/Studio/MidiPlaygroundPage.cpp
        Source/Studio/DspPage.cpp
        Source/Studio/PromptToPluginComponent.cpp
        Source/Studio/CanvasToolbar.cpp
        Source/Studio/ControlBuilderComponent.cpp
        Source/Studio/StudioAudioService.cpp
        Source/Studio/SettingsDialog.cpp
        Source/Studio/LibraryMetadataComponent.cpp
        Source/Studio/SampleWaveformViewer.cpp
        Source/Studio/VelocityMapViewer.cpp
        Source/Studio/SampleMapEditor.cpp
        Source/Studio/VstExportModule.cpp)

    target_link_libraries(PatchCraftStudio PRIVATE PatchCraftCore)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/FactoryDemos")
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/FactoryDemos"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/FactoryDemos"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library/Backgrounds"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library/Templates"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library/Assets/knobs"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library/Assets/sliders"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library/Assets/meters"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Library"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/Library"
            COMMENT "Staging PatchCraft factory demo instruments"
            VERBATIM)
    endif()

    # Base runtime: installable PatchCraft Player plugins are part of the
    # Studio/Player system. They are staged separately from PluginTemplate so
    # the paid VST Expansion exporter can be packaged/unlocked independently.
    if(PATCHCRAFT_BUILD_PLAYER)
        add_dependencies(PatchCraftStudio PatchCraftPlayer_VST3)
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PlayerPlugins"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayer_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PlayerPlugins/PatchCraft Player.vst3"
            COMMENT "Staging PatchCraft Player VST3 for the base Studio/Player installer"
            VERBATIM)
    endif()
    if(PATCHCRAFT_BUILD_PLAYER_FX)
        add_dependencies(PatchCraftStudio PatchCraftPlayerFX_VST3)
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PlayerPlugins"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayerFX_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PlayerPlugins/PatchCraft Player FX.vst3"
            COMMENT "Staging PatchCraft Player FX VST3 for the base Studio/Player installer"
            VERBATIM)
    endif()

    # Paid VST Expansion addon: only staged into the app runtime when this
    # option is enabled. Release builds keep these templates out of the base
    # installer and ship them via the separate PatchCraftVstExpansionPackage.
    if(PATCHCRAFT_STAGE_VST_EXPANSION AND PATCHCRAFT_BUILD_PLAYER)
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayer_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate/PatchCraft Player.vst3"
            COMMENT "Staging paid VST Expansion instrument template"
            VERBATIM)
    endif()
    if(PATCHCRAFT_STAGE_VST_EXPANSION AND PATCHCRAFT_BUILD_PLAYER_FX)
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayerFX_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate/PatchCraft Player FX.vst3"
            COMMENT "Staging paid VST Expansion FX template"
            VERBATIM)
    endif()
    if(NOT PATCHCRAFT_STAGE_VST_EXPANSION)
        add_custom_command(TARGET PatchCraftStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf
                "$<TARGET_FILE_DIR:PatchCraftStudio>/PluginTemplate"
            COMMENT "Removing paid VST Expansion templates from the base Studio runtime"
            VERBATIM)
    endif()
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
if(PATCHCRAFT_STAGE_VST_EXPANSION)
    list(APPEND PATCHCRAFT_RELEASE_CHECK_TARGETS PatchCraftVstExportSmokeTest)
endif()
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

if(PATCHCRAFT_BUILD_STUDIO)
    add_custom_target(PatchCraftRcBundle
        DEPENDS PatchCraftReleaseChecklist
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            -D STUDIO_EXE="$<TARGET_FILE:PatchCraftStudio>"
            -D STUDIO_DIR="$<TARGET_FILE_DIR:PatchCraftStudio>"
            -D OUTPUT_DIR="${CMAKE_BINARY_DIR}/dist/PatchCraftStudio-RC"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreatePatchCraftRcBundle.cmake"
        COMMENT "Creating PatchCraft Studio release-candidate bundle"
        VERBATIM)
endif()

if(PATCHCRAFT_BUILD_PLAYER AND PATCHCRAFT_BUILD_PLAYER_FX)
    add_custom_target(PatchCraftVstExpansionPackage
        DEPENDS PatchCraftPlayer_VST3 PatchCraftPlayerFX_VST3
        COMMAND ${CMAKE_COMMAND}
            -D PLAYER_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayer_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
            -D PLAYER_FX_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:PatchCraftPlayerFX_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
            -D OUTPUT_DIR="${CMAKE_BINARY_DIR}/dist/PatchCraftVstExpansion"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreatePatchCraftVstExpansionPackage.cmake"
        COMMENT "Creating separate paid PatchCraft VST Expansion installer package"
        VERBATIM)
endif()
