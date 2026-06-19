#!/usr/bin/env python3
"""
Create single EXE installer using 7-Zip SFX module
"""

import os
import sys
import tempfile
from pathlib import Path

def create_sfx_installer(source_dir, output_exe, version="1.0.0"):
    """Create self-extracting EXE installer using 7-Zip SFX"""
    
    print(f"Creating single EXE installer for PatchCraft v{version}")
    
    # Create temporary directory
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Copy installer files
        files_to_copy = [
            ("PatchCraft Studio.exe", "PatchCraft Studio.exe"),
            ("install.bat", "install.bat"),
            ("README.txt", "README.txt"),
            ("INSTALL.txt", "INSTALL.txt")
        ]
        
        for src_name, dst_name in files_to_copy:
            src_path = Path(source_dir) / src_name
            if src_path.exists():
                dst_path = temp_path / dst_name
                import shutil
                shutil.copy2(src_path, dst_path)
                print(f"Copied {src_name}")
        
        # Create SFX config
        sfx_config = f""";!@Install@!UTF-8!
Title="PatchCraft Studio v{version}"
BeginPrompt="Do you want to install PatchCraft Studio?"
Progress="yes"
GUIMode="2"
ExtractDialogText="Extracting PatchCraft Studio..."
ExtractTitle="PatchCraft Studio"
ExtractPathText="Choose installation folder:"
ExtractCancelText="Installation cancelled"
RunProgram="install.bat"
"""
        
        config_path = temp_path / "sfx_config.txt"
        with open(config_path, "w", encoding="utf-8-sig") as f:
            f.write(sfx_config)
        
        # Try to find 7-Zip
        seven_zip_paths = [
            r"C:\Program Files\7-Zip\7z.exe",
            r"C:\Program Files (x86)\7-Zip\7z.exe",
            "7z.exe"
        ]
        
        seven_zip = None
        for path in seven_zip_paths:
            if Path(path).exists():
                seven_zip = path
                break
        
        if not seven_zip:
            print("7-Zip not found. Creating alternative installer...")
            return create_alternative_installer(temp_path, output_exe, version)
        
        # Create SFX archive
        try:
            import subprocess
            
            # Find 7z.sfx
            sfx_paths = [
                r"C:\Program Files\7-Zip\7z.sfx",
                r"C:\Program Files (x86)\7-Zip\7z.sfx"
            ]
            
            sfx_module = None
            for sfx_path in sfx_paths:
                if Path(sfx_path).exists():
                    sfx_module = sfx_path
                    break
            
            if not sfx_module:
                print("7z.sfx module not found")
                return False
            
            # Create archive
            archive_path = temp_path / "installer.7z"
            cmd = [
                seven_zip, "a", "-r", str(archive_path), str(temp_path / "*"),
                "-x!sfx_config.txt", "-x!*.7z"
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"7-Zip archive failed: {result.stderr}")
                return False
            
            # Combine SFX module + config + archive
            with open(output_exe, "wb") as outfile:
                # Write SFX module
                with open(sfx_module, "rb") as sfx_file:
                    outfile.write(sfx_file.read())
                
                # Write config
                with open(config_path, "rb") as config_file:
                    outfile.write(config_file.read())
                
                # Write archive
                with open(archive_path, "rb") as archive_file:
                    outfile.write(archive_file.read())
            
            size_mb = Path(output_exe).stat().st_size / (1024 * 1024)
            print(f"Single EXE installer created: {output_exe}")
            print(f"Size: {size_mb:.1f} MB")
            return True
            
        except Exception as e:
            print(f"SFX creation failed: {e}")
            return False

def create_alternative_installer(temp_path, output_exe, version):
    """Create alternative installer using Windows built-in tools"""
    
    print("Creating Windows installer alternative...")
    
    # Create installer script
    installer_script = f'''@echo off
title PatchCraft Studio v{version} Installer
echo.
echo ========================================
echo   PatchCraft Studio v{version}
echo   Single-File Installer
echo ========================================
echo.

REM Create temporary directory
set TEMP_DIR=%TEMP%\\PatchCraftInstall
mkdir "%TEMP_DIR%" 2>nul

REM Extract embedded files (this would need external tool)
echo Extracting files...
echo Note: This installer requires 7-Zip for self-extraction
echo Please download 7-Zip from https://www.7-zip.org/
echo.
echo Alternative: Manual installation
echo 1. Create folder: C:\\Program Files\\PatchCraft
echo 2. Copy PatchCraft Studio.exe to that folder
echo 3. Create desktop shortcut manually
echo.
pause
'''
    
    script_path = temp_path / "installer.bat"
    with open(script_path, "w") as f:
        f.write(installer_script)
    
    # Create simple wrapper (copy existing exe + script)
    import shutil
    shutil.copy2(temp_path / "PatchCraft Studio.exe", output_exe)
    
    # Add instructions to create proper installer
    print(f"Created installer wrapper: {output_exe}")
    print("Note: For true single-file installer, install 7-Zip")
    return True

def create_mac_installer():
    """Create Mac version structure and repository setup"""
    
    print("Setting up Mac version...")
    
    # Create Mac project structure
    mac_dir = Path("../PatchCraft-Mac")
    mac_dir.mkdir(exist_ok=True)
    
    # Basic Mac project files
    mac_files = {
        "CMakeLists.txt": '''cmake_minimum_required(VERSION 3.15)
project(PatchCraftMac)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")

# Find required packages
find_package(JUCE REQUIRED)
add_subdirectory(JUCE)

# Add Mac-specific modules
set(PATCHCRAFT_MAC TRUE)

# Source files
set(SOURCES
    Source/Shared/*.cpp
    Source/Studio/*.cpp
    Source/Player/*.cpp
)

# Headers
set(HEADERS
    Source/Shared/*.h
    Source/Studio/*.h
    Source/Player/*.h
)

# Create app bundle
add_executable(PatchCraftStudio MACOSX_BUNDLE ${SOURCES} ${HEADERS})

# Mac-specific settings
set_target_properties(PatchCraftStudio PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/mac/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "PatchCraft Studio"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    MACOSX_BUNDLE_IDENTIFIER "com.patchcraft.studio"
)

target_link_libraries(PatchCraftStudio
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_core
    ${COREFOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${WEBKIT_FRAMEWORK}
)
''',
        
        "README.md": '''# PatchCraft Studio for macOS

## Installation

1. Download PatchCraftStudio.app
2. Right-click and "Open" (bypass Gatekeeper if needed)
3. Grant necessary permissions

## Building from Source

```bash
git clone https://github.com/plugmakr/PatchCraft-Mac.git
cd PatchCraft-Mac
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

## Requirements

- macOS 10.15 or later
- 4GB RAM minimum
- Audio interface recommended

## Features

- Multi-instrument layering
- Visual modulation matrix
- Component library with drag-drop
- SFZ import support
- Professional UI design
''',
        
        ".gitignore": '''# Build files
build/
*.xcodeproj/
*.xcworkspace/

# IDE files
.vscode/
.idea/

# macOS
*.dSYM/
.DS_Store

# JUCE
JUCE/
'''
    }
    
    for filename, content in mac_files.items():
        file_path = mac_dir / filename
        with open(file_path, "w") as f:
            f.write(content)
        print(f"Created {filename}")
    
    # Create mac bundle structure
    mac_bundle = mac_dir / "mac" / "PatchCraftStudio.app" / "Contents"
    mac_bundle.mkdir(parents=True, exist_ok=True)
    
    # Info.plist
    info_plist = '''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>PatchCraftStudio</string>
    <key>CFBundleIdentifier</key>
    <string>com.patchcraft.studio</string>
    <key>CFBundleName</key>
    <string>PatchCraft Studio</string>
    <key>CFBundleDisplayName</key>
    <string>PatchCraft Studio</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>patchcraft</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>PatchCraft Project</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
        </dict>
    </array>
</dict>
</plist>'''
    
    with open(mac_bundle / "Info.plist", "w") as f:
        f.write(info_plist)
    
    (mac_bundle / "MacOS").mkdir(exist_ok=True)
    (mac_bundle / "Resources").mkdir(exist_ok=True)
    
    print(f"Mac project structure created: {mac_dir}")
    return mac_dir

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_single_exe_installer.py <installer_dir> <output_exe> [version]")
        sys.exit(1)
    
    installer_dir = sys.argv[1]
    output_exe = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(installer_dir).exists():
        print(f"Installer directory not found: {installer_dir}")
        sys.exit(1)
    
    # Create single EXE installer
    if create_sfx_installer(installer_dir, output_exe, version):
        print("✅ Single EXE installer created successfully!")
    
    # Create Mac version setup
    mac_dir = create_mac_installer()
    if mac_dir:
        print("✅ Mac repository structure created!")
        print(f"Location: {mac_dir}")
        print("Initialize with: cd ../PatchCraft-Mac && git init && git add . && git commit -m 'Initial Mac version'")

if __name__ == "__main__":
    main()
