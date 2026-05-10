#!/usr/bin/env python3
"""
PatchCraft Installer Builder
Creates distribution packages for beta testing and release
"""

import os
import json
import shutil
import subprocess
import sys
from pathlib import Path

def create_installer(project_dir, output_dir, version="1.0.0"):
    """Create a simple installer for PatchCraft Studio"""
    
    installer_dir = Path(output_dir) / "PatchCraftInstaller"
    installer_dir.mkdir(exist_ok=True)
    
    # Copy essential files
    essential_files = [
        ("PatchCraft Studio.exe", "PatchCraftStudio.exe"),
        ("PatchCraft Player.vst3", "PatchCraftPlayer.vst3"),
        ("README.txt", "README.txt"),
        ("LICENSE.txt", "LICENSE.txt")
    ]
    
    for src, dst in essential_files:
        src_path = Path(project_dir) / "build" / src
        if src_path.exists():
            shutil.copy2(src_path, installer_dir / dst)
            print(f"Copied {src} to installer")
    
    # Create installer script
    installer_script = f"""@echo off
echo Installing PatchCraft {version}...
echo.

REM Create program files directory
if not exist "%PROGRAMFILES%\\PatchCraft" (
    mkdir "%PROGRAMFILES%\\PatchCraft"
)

REM Copy files
copy "PatchCraft Studio.exe" "%PROGRAMFILES%\\PatchCraft\\"
copy "PatchCraft Player.vst3" "%PROGRAMFILES%\\PatchCraft\\"
copy "README.txt" "%PROGRAMFILES%\\PatchCraft\\"
copy "LICENSE.txt" "%PROGRAMFILES%\\PatchCraft\\"

REM Create desktop shortcut
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $WshShell.CreateShortcut('%PROGRAMFILES%\\PatchCraft\\PatchCraft Studio.exe', '%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk')"

echo Installation complete!
echo.
echo PatchCraft {version} has been installed to:
echo %PROGRAMFILES%\\PatchCraft
echo.
echo A desktop shortcut has been created.
pause
"""
    
    with open(installer_dir / "install.bat", "w") as f:
        f.write(installer_script)
    
    # Create version info
    version_info = {
        "version": version,
        "build_date": "2026-05-10",
        "features": [
            "Multi-instrument layering",
            "Enhanced component library", 
            "Visual modulation matrix",
            "SFZ import support",
            "Copy protection system",
            "Preset randomization",
            "A/B compare",
            "Drag-and-drop loading",
            "Computer keyboard input"
        ]
    }
    
    with open(installer_dir / "version.json", "w") as f:
        json.dump(version_info, f, indent=2)
    
    print(f"Created installer package in {installer_dir}")
    return installer_dir

def create_standalone_zip(project_dir, output_dir):
    """Create standalone ZIP for distribution"""
    
    zip_dir = Path(output_dir) / "PatchCraft-Standalone"
    zip_dir.mkdir(exist_ok=True)
    
    # Copy build artifacts
    build_dir = Path(project_dir) / "build"
    if build_dir.exists():
        # Copy Studio executable
        studio_build = build_dir / "PatchCraftStudio_artefacts" / "Release"
        if studio_build.exists():
            for item in studio_build.glob("*"):
                if item.is_file():
                    shutil.copy2(item, zip_dir / item.name)
        
        # Copy VST3 plugin
        player_build = build_dir / "PatchCraftPlayer_artefacts" / "Release" / "VST3"
        if player_build.exists():
            vst3_dir = zip_dir / "VST3"
            vst3_dir.mkdir(exist_ok=True)
            for item in player_build.glob("*"):
                if item.is_file():
                    shutil.copy2(item, vst3_dir)
    
    # Add documentation
    docs_dir = zip_dir / "Documentation"
    docs_dir.mkdir(exist_ok=True)
    
    # Copy README and LICENSE
    for doc_file in ["README.md", "LICENSE"]:
        src = Path(project_dir) / doc_file
        if src.exists():
            shutil.copy2(src, docs_dir / doc_file)
    
    # Create info file
    info = {
        "name": "PatchCraft Standalone",
        "version": "1.0.0",
        "type": "standalone",
        "components": ["Studio", "Player VST3"]
    }
    
    with open(zip_dir / "info.json", "w") as f:
        json.dump(info, f, indent=2)
    
    # Create ZIP
    import zipfile
    zip_path = Path(output_dir) / "PatchCraft-Standalone.zip"
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file_path in zip_dir.rglob("*"):
            if file_path.is_file():
                zipf.write(file_path, os.path.relpath(file_path, zip_dir))
    
    print(f"Created standalone ZIP: {zip_path}")

def build_project(project_dir):
    """Build the PatchCraft project"""
    
    print("Building PatchCraft...")
    
    # Build using CMake
    build_dir = Path(project_dir) / "build"
    build_dir.mkdir(exist_ok=True)
    
    try:
        subprocess.run([
            "cmake", "-B", "build", 
            "-DPATCHCRAFT_JUCE_PATH=../JUCE",
            project_dir
        ], check=True, cwd=project_dir)
        
        subprocess.run([
            "cmake", "--build", "build", 
            "--config", "Release"
        ], check=True, cwd=project_dir)
        
        print("Build completed successfully!")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"Build failed: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python build_installer.py <project_dir> [output_dir]")
        sys.exit(1)
    
    project_dir = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "dist"
    
    if not os.path.exists(project_dir):
        print(f"Project directory not found: {project_dir}")
        sys.exit(1)
    
    # Build project first
    if build_project(project_dir):
        # Create distributions
        create_installer(project_dir, output_dir)
        create_standalone_zip(project_dir, output_dir)
        
        print(f"\nDistribution packages created in: {output_dir}")
        print("Files created:")
        print("  - PatchCraftInstaller/install.bat")
        print("  - PatchCraft-Standalone.zip")
    else:
        print("Build failed, skipping distribution creation")

if __name__ == "__main__":
    main()
