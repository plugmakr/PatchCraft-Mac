cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if(NOT DEFINED STUDIO_EXE)
    message(FATAL_ERROR "STUDIO_EXE is required")
endif()

if(NOT DEFINED STUDIO_DIR)
    message(FATAL_ERROR "STUDIO_DIR is required")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

foreach(var SOURCE_DIR STUDIO_EXE STUDIO_DIR OUTPUT_DIR)
    string(REGEX REPLACE "^\"|\"$" "" ${var} "${${var}}")
endforeach()

function(require_exists path label)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing ${label}: ${path}")
    endif()
endfunction()

require_exists("${STUDIO_EXE}" "PatchCraft Studio executable")
require_exists("${STUDIO_DIR}/FactoryDemos" "FactoryDemos")
require_exists("${STUDIO_DIR}/Library" "Library")
require_exists("${STUDIO_DIR}/PlayerPlugins/PatchCraft Player.vst3" "PatchCraft Player VST3")
require_exists("${STUDIO_DIR}/PlayerPlugins/PatchCraft Player FX.vst3" "PatchCraft Player FX VST3")

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/docs")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/installer")

file(COPY "${STUDIO_EXE}" DESTINATION "${OUTPUT_DIR}")
file(COPY "${STUDIO_DIR}/FactoryDemos" DESTINATION "${OUTPUT_DIR}")
file(COPY "${STUDIO_DIR}/Library" DESTINATION "${OUTPUT_DIR}")
file(COPY "${STUDIO_DIR}/PlayerPlugins" DESTINATION "${OUTPUT_DIR}")

if(EXISTS "${SOURCE_DIR}/README.md")
    file(COPY "${SOURCE_DIR}/README.md" DESTINATION "${OUTPUT_DIR}/docs")
endif()
if(EXISTS "${SOURCE_DIR}/HANDOFF.md")
    file(COPY "${SOURCE_DIR}/HANDOFF.md" DESTINATION "${OUTPUT_DIR}/docs")
endif()
if(EXISTS "${SOURCE_DIR}/IMPLEMENTATION_ROADMAP.md")
    file(COPY "${SOURCE_DIR}/IMPLEMENTATION_ROADMAP.md" DESTINATION "${OUTPUT_DIR}/docs")
endif()
if(EXISTS "${SOURCE_DIR}/docs")
    file(COPY "${SOURCE_DIR}/docs/" DESTINATION "${OUTPUT_DIR}/docs")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

file(WRITE "${OUTPUT_DIR}/INSTALL.txt" "PatchCraft Studio Release Candidate
======================================

Contents:
- PatchCraftStudio.exe
- FactoryDemos/
- Library/
- PlayerPlugins/PatchCraft Player.vst3
- PlayerPlugins/PatchCraft Player FX.vst3
- docs/
- installer/PatchCraftStudio-Windows.iss
- installer/PatchCraftStudio-macOS-notes.md

Manual install:
1. Copy this folder to a writable application folder, such as C:\\Program Files\\PatchCraft or C:\\Users\\<you>\\Apps\\PatchCraft.
2. Run PatchCraftStudio.exe.
3. Keep FactoryDemos, Library, and PlayerPlugins beside the executable.
4. Copy PlayerPlugins/PatchCraft Player.vst3 and PatchCraft Player FX.vst3 to your per-user VST3 folder, or build installer/PatchCraftStudio-Windows.iss.
5. In Studio, open Launch and run Launch Doctor before exporting customer products.

VST3 deployment:
- PatchCraft Player and PatchCraft Player FX are included in the base Studio/Player system.
- The paid VST Expansion addon is packaged separately and installs PluginTemplate/ when purchased.
- Per-user VST3 fallback on Windows is %LOCALAPPDATA%\\Programs\\Common\\VST3.

Final release proof:
- Load exported packs in PatchCraft Player.
- Load Player and Player FX in FL Studio or another DAW.
- Verify tabs, labels, hardware MIDI, MIDI Learn, presets, pad/drum grids, and volume.
- Publish a Plugin.club draft through https://plugin.club/functions/sellerImport.
")

file(WRITE "${OUTPUT_DIR}/installer/PatchCraftStudio-Windows.iss" "; PatchCraft Studio generated Windows installer script
; Compile with Inno Setup 6 after code signing inputs are added.

#define ProductName \"PatchCraft Studio\"
#define ProductPublisher \"PatchCraft\"
#define ProductVersion \"0.1.0\"
#define SourceDir \"..\"

[Setup]
AppId={{B6302D57-AB6E-4F16-9873-2A7D8F3E9001}
AppName={#ProductName}
AppVersion={#ProductVersion}
AppPublisher={#ProductPublisher}
DefaultDirName={localappdata}\\Programs\\PatchCraft\\Studio
DefaultGroupName=PatchCraft Studio
OutputBaseFilename=PatchCraftStudio-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest

[Files]
Source: \"{#SourceDir}\\PatchCraftStudio.exe\"; DestDir: \"{app}\"; Flags: ignoreversion
Source: \"{#SourceDir}\\FactoryDemos\\*\"; DestDir: \"{app}\\FactoryDemos\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\Library\\*\"; DestDir: \"{app}\\Library\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\docs\\*\"; DestDir: \"{app}\\docs\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\PlayerPlugins\\PatchCraft Player.vst3\\*\"; DestDir: \"{localappdata}\\Programs\\Common\\VST3\\PatchCraft Player.vst3\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\PlayerPlugins\\PatchCraft Player FX.vst3\\*\"; DestDir: \"{localappdata}\\Programs\\Common\\VST3\\PatchCraft Player FX.vst3\"; Flags: recursesubdirs ignoreversion

[Icons]
Name: \"{group}\\PatchCraft Studio\"; Filename: \"{app}\\PatchCraftStudio.exe\"; WorkingDir: \"{app}\"

[Registry]
Root: HKCU; Subkey: \"Software\\PatchCraft\\Studio\"; ValueType: string; ValueName: \"InstallPath\"; ValueData: \"{app}\"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: \"Software\\PatchCraft\\Studio\"; ValueType: string; ValueName: \"Version\"; ValueData: \"{#ProductVersion}\"

[Run]
Filename: \"{app}\\PatchCraftStudio.exe\"; Description: \"Launch PatchCraft Studio\"; Flags: nowait postinstall skipifsilent
")

file(WRITE "${OUTPUT_DIR}/installer/PatchCraftStudio-macOS-notes.md" "# PatchCraft Studio macOS Installer Notes

Build the macOS release from the same payload:

1. Stage `PatchCraftStudio.app`, `FactoryDemos`, `Library`, and `docs` under `/Applications/PatchCraft Studio/`.
2. Stage `PatchCraft Player.vst3` and `PatchCraft Player FX.vst3` under `~/Library/Audio/Plug-Ins/VST3/` for per-user install, or `/Library/Audio/Plug-Ins/VST3/` for admin/system install.
3. Keep the paid VST Expansion addon separate. Its package installs `PluginTemplate/` beside the Studio app payload.
4. Sign and notarize the app, VST3 bundles, and final pkg before public distribution.
")

file(WRITE "${OUTPUT_DIR}/rc-manifest.json" "{
  \"name\": \"PatchCraft Studio RC\",
  \"created_at\": \"${BUILD_TIME}\",
  \"studio_executable\": \"PatchCraftStudio.exe\",
  \"required_runtime_folders\": [
    \"FactoryDemos\",
    \"Library\",
    \"PlayerPlugins/PatchCraft Player.vst3\",
    \"PlayerPlugins/PatchCraft Player FX.vst3\"
  ],
  \"vst_expansion_included\": false,
  \"vst_expansion_package_target\": \"PatchCraftVstExpansionPackage\",
  \"pluginclub_endpoint\": \"https://plugin.club/functions/sellerImport\",
  \"vst3_user_folder_windows\": \"%LOCALAPPDATA%/Programs/Common/VST3\",
  \"manual_qa_required\": [
    \"Studio preview audio\",
    \"Hardware MIDI note input\",
    \"Mod wheel/expression/sustain\",
    \"Player tab switching and text labels\",
    \"PatchCraft Player and Player FX scan in FL Studio\",
    \"Plugin.club draft publish\"
  ]
}
")

message(STATUS "PatchCraft RC bundle created at ${OUTPUT_DIR}")
