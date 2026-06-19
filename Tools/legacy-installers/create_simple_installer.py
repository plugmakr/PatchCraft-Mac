#!/usr/bin/env python3
"""
Simple PatchCraft Installer - creates installer with existing executable
"""

import os
import sys
from pathlib import Path

def create_installer(exe_path, output_dir, version="1.0.0"):
    """Create a simple installer for PatchCraft"""
    
    installer_dir = Path(output_dir) / "PatchCraftInstaller"
    installer_dir.mkdir(exist_ok=True)
    
    # Copy executable
    exe_name = "PatchCraft Studio.exe"
    dest_exe = installer_dir / exe_name
    if Path(exe_path).exists():
        import shutil
        shutil.copy2(exe_path, dest_exe)
        print(f"Copied {exe_name} to installer")
    else:
        print(f"Source executable not found: {exe_path}")
        return False
    
    # Create README
    readme_content = f"""PatchCraft Studio v{version}

This installer contains PatchCraft Studio, a professional sample instrument design tool.

Features:
- Visual instrument designer with drag-and-drop interface
- Multi-instrument layering for combined sounds
- Component library with knobs, sliders, meters, and more
- SFZ import support
- Visual modulation matrix
- Preset randomization and A/B compare
- Drag-and-drop pack loading
- Computer keyboard input for testing

Installation:
1. Extract all files to your desired location
2. Run PatchCraft Studio.exe to start creating instruments

For more information visit: https://patchcraft.com
"""
    
    with open(installer_dir / "README.txt", "w") as f:
        f.write(readme_content)
    
    # Create installer script
    installer_script = f"""@echo off
echo Installing PatchCraft Studio v{version}...
echo.

REM Create program files directory
if not exist "%PROGRAMFILES%\\PatchCraft" (
    mkdir "%PROGRAMFILES%\\PatchCraft"
)

REM Copy executable
copy "{exe_name}" "%PROGRAMFILES%\\PatchCraft\\"

REM Create desktop shortcut
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $WshShell.CreateShortcut('%PROGRAMFILES%\\PatchCraft\\{exe_name}', '%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk')"

echo Installation complete!
echo.
echo PatchCraft Studio v{version} has been installed to:
echo %PROGRAMFILES%\\PatchCraft
echo.
echo A desktop shortcut has been created.
pause
"""
    
    with open(installer_dir / "install.bat", "w") as f:
        f.write(installer_script)
    
    print(f"Created simple installer in {installer_dir}")
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_simple_installer.py <exe_path> <output_dir> [version]")
        sys.exit(1)
    
    exe_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "dist"
    version = sys.argv[3] if len(sys.argv) > 2 else "1.0.0"
    
    if not Path(exe_path).exists():
        print(f"Executable not found: {exe_path}")
        sys.exit(1)
    
    if create_installer(exe_path, output_dir, version):
        print(f"Simple installer created successfully!")
    else:
        print("Failed to create installer")

if __name__ == "__main__":
    main()
