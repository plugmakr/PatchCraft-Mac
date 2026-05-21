cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED PLAYER_VST3)
    message(FATAL_ERROR "PLAYER_VST3 is required")
endif()

if(NOT DEFINED PLAYER_FX_VST3)
    message(FATAL_ERROR "PLAYER_FX_VST3 is required")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

foreach(var PLAYER_VST3 PLAYER_FX_VST3 OUTPUT_DIR)
    string(REGEX REPLACE "^\"|\"$" "" ${var} "${${var}}")
endforeach()

function(require_exists path label)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing ${label}: ${path}")
    endif()
endfunction()

require_exists("${PLAYER_VST3}" "PatchCraft Player VST3 template")
require_exists("${PLAYER_FX_VST3}" "PatchCraft Player FX VST3 template")

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/PluginTemplate")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/Extension/PatchCraftVstExpansion")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/installer")

file(COPY "${PLAYER_VST3}" DESTINATION "${OUTPUT_DIR}/PluginTemplate")
file(COPY "${PLAYER_FX_VST3}" DESTINATION "${OUTPUT_DIR}/PluginTemplate")

string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

file(WRITE "${OUTPUT_DIR}/Extension/PatchCraftVstExpansion/manifest.json" "{
  \"format\": \"PatchCraft Extension\",
  \"formatVersion\": 1,
  \"id\": \"com.patchcraft.vst-expansion\",
  \"name\": \"PatchCraft VST Expansion\",
  \"version\": \"1.0.0\",
  \"kind\": \"exporter\",
  \"author\": \"PatchCraft\",
  \"description\": \"Enables standalone branded VST3 export from PatchCraft Studio projects.\",
  \"minPatchCraftVersion\": \"0.1.0\",
  \"productId\": \"patchcraft-vst-expansion\",
  \"licenseMode\": \"external\",
  \"capabilities\": [
    \"export.vst3\",
    \"white_label.vst3\",
    \"pluginclub.publish.vst3\"
  ],
  \"tags\": [
    \"vst3\",
    \"export\",
    \"white-label\"
  ],
  \"builtAt\": \"${BUILD_TIME}\"
}
")

file(WRITE "${OUTPUT_DIR}/INSTALL.txt" "PatchCraft VST Expansion
========================

This is the paid addon package for standalone VST3 export.

Contents:
- PluginTemplate/PatchCraft Player.vst3
- PluginTemplate/PatchCraft Player FX.vst3
- Extension/PatchCraftVstExpansion/manifest.json
- installer/PatchCraftVstExpansion-Windows.iss
- installer/PatchCraftVstExpansion-macOS-notes.md

What it unlocks:
- Export a PatchCraft project as a dedicated standalone VST3 plugin.
- Export branded client/customer plugins with an embedded pack.
- Publish VST3 plugin artifacts to Plugin.club when enabled in Studio.

Install behavior:
- The Windows installer reads HKCU\\Software\\PatchCraft\\Studio\\InstallPath from the base Studio installer.
- It installs PluginTemplate/ beside PatchCraftStudio.exe.
- It installs the extension manifest under %APPDATA%\\PatchCraft\\Extensions.
")

file(WRITE "${OUTPUT_DIR}/installer/PatchCraftVstExpansion-Windows.iss" "; PatchCraft VST Expansion generated Windows installer script
; Compile with Inno Setup 6 after adding signing and license checks.

#define ProductName \"PatchCraft VST Expansion\"
#define ProductPublisher \"PatchCraft\"
#define ProductVersion \"1.0.0\"
#define SourceDir \"..\"

[Setup]
AppId={{C6F7363C-AF55-4F31-8E5D-2A7D8F3E9002}
AppName={#ProductName}
AppVersion={#ProductVersion}
AppPublisher={#ProductPublisher}
DefaultDirName={code:GetStudioInstallPath}
DefaultGroupName=PatchCraft
OutputBaseFilename=PatchCraftVstExpansion-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest

[Files]
Source: \"{#SourceDir}\\PluginTemplate\\PatchCraft Player.vst3\\*\"; DestDir: \"{app}\\PluginTemplate\\PatchCraft Player.vst3\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\PluginTemplate\\PatchCraft Player FX.vst3\\*\"; DestDir: \"{app}\\PluginTemplate\\PatchCraft Player FX.vst3\"; Flags: recursesubdirs ignoreversion
Source: \"{#SourceDir}\\Extension\\PatchCraftVstExpansion\\manifest.json\"; DestDir: \"{userappdata}\\PatchCraft\\Extensions\\PatchCraftVstExpansion\"; Flags: ignoreversion

[Code]
function GetStudioInstallPath(Param: String): String;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Software\\PatchCraft\\Studio', 'InstallPath', Result) then
    Result := ExpandConstant('{localappdata}\\Programs\\PatchCraft\\Studio');
end;
")

file(WRITE "${OUTPUT_DIR}/installer/PatchCraftVstExpansion-macOS-notes.md" "# PatchCraft VST Expansion macOS Installer Notes

1. Locate the base Studio install path.
2. Install `PluginTemplate/PatchCraft Player.vst3` and `PluginTemplate/PatchCraft Player FX.vst3` beside the Studio executable/app payload.
3. Install `Extension/PatchCraftVstExpansion/manifest.json` to `~/Library/Application Support/PatchCraft/Extensions/PatchCraftVstExpansion/manifest.json`.
4. Gate the installer through the Plugin.club entitlement for `patchcraft-vst-expansion`.
5. Sign and notarize the package before distribution.
")

file(WRITE "${OUTPUT_DIR}/vst-expansion-manifest.json" "{
  \"name\": \"PatchCraft VST Expansion\",
  \"created_at\": \"${BUILD_TIME}\",
  \"paid_addon\": true,
  \"base_product_required\": \"PatchCraft Studio\",
  \"installs\": [
    \"PluginTemplate/PatchCraft Player.vst3\",
    \"PluginTemplate/PatchCraft Player FX.vst3\",
    \"%APPDATA%/PatchCraft/Extensions/PatchCraftVstExpansion/manifest.json\"
  ],
  \"capabilities\": [
    \"export.vst3\",
    \"white_label.vst3\",
    \"pluginclub.publish.vst3\"
  ]
}
")

message(STATUS "PatchCraft VST Expansion package created at ${OUTPUT_DIR}")
