#!/usr/bin/env python3
"""
Create simple, working installer that actually runs
"""

import os
import sys
from pathlib import Path

def create_simple_working_installer(source_exe, output_dir, version="1.0.0"):
    """Create simple installer that works"""
    
    print(f"Creating working installer for PatchCraft v{version}")
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    # Copy main executable
    source_path = Path(source_exe)
    if not source_path.exists():
        print(f"❌ Source executable not found: {source_exe}")
        return False
    
    dest_exe = output_path / "PatchCraft Studio.exe"
    import shutil
    shutil.copy2(source_path, dest_exe)
    print(f"✅ Copied PatchCraft Studio.exe ({source_path.stat().st_size / 1024 / 1024:.1f} MB)")
    
    # Create simple installer script
    installer_script = f'''@echo off
title PatchCraft Studio v{version} Installer
color 0A
echo.
echo ========================================
echo   PatchCraft Studio v{version}
echo   AudiCode - Beta Testing
echo ========================================
echo.

REM Check if PatchCraft Studio.exe exists
if not exist "PatchCraft Studio.exe" (
    echo ERROR: PatchCraft Studio.exe not found!
    echo Please ensure it's in the same folder as this installer.
    echo.
    pause
    exit /b 1
)

echo Found PatchCraft Studio.exe
echo.

REM Ask for installation directory
set INSTALL_DIR=C:\\Program Files\\PatchCraft
set /p INSTALL_DIR="Enter installation directory (default: C:\\Program Files\\PatchCraft): "

if "%INSTALL_DIR%"=="" set INSTALL_DIR=C:\\Program Files\\PatchCraft

echo Installing to: %INSTALL_DIR%
echo.

REM Create installation directory
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo Created installation directory
)

REM Copy files
echo Installing PatchCraft Studio...
copy "PatchCraft Studio.exe" "%INSTALL_DIR%\\" >nul

if exist "%INSTALL_DIR%\\PatchCraft Studio.exe" (
    echo [OK] PatchCraft Studio.exe installed successfully
) else (
    echo [ERROR] Failed to install PatchCraft Studio.exe
    pause
    exit /b 1
)

REM Create desktop shortcut
echo Creating desktop shortcut...
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Save()" >nul 2>&1

if exist "%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk" (
    echo [OK] Desktop shortcut created
) else (
    echo [WARNING] Could not create desktop shortcut
)

echo.
echo ========================================
echo   Installation Complete!
echo ========================================
echo.
echo PatchCraft Studio has been installed to:
echo %INSTALL_DIR%
echo.
echo Launch Options:
echo - Desktop shortcut: PatchCraft Studio
echo - Direct run: %INSTALL_DIR%\\PatchCraft Studio.exe
echo.
echo Features:
echo - Multi-instrument layering
echo - Enhanced component library
echo - Visual modulation matrix
echo - SFZ import support
echo - Preset randomization
echo - A/B compare
echo - Computer keyboard input
echo.
echo Support: support@patchcraft.com
echo.
echo Press any key to exit...
pause >nul
'''
    
    # Save installer script
    installer_file = output_path / "Install.bat"
    with open(installer_file, 'w') as f:
        f.write(installer_script)
    print(f"✅ Created installer: {installer_file}")
    
    # Create simple README
    readme_content = f'''PatchCraft Studio v{version} - Installation Guide
================================================

INSTALLATION:

Option 1: Automated Installation (Recommended)
1. Run Install.bat
2. Follow on-screen instructions
3. Choose installation directory (default: C:\\Program Files\\PatchCraft)
4. Desktop shortcut will be created automatically

Option 2: Manual Installation
1. Create folder: C:\\Program Files\\PatchCraft
2. Copy PatchCraft Studio.exe to that folder
3. Create desktop shortcut manually if desired

SYSTEM REQUIREMENTS:
- Windows 10/11 (64-bit)
- 4GB RAM minimum (8GB recommended)
- Audio interface recommended for low latency
- VST3-compatible DAW for plugin use

BETA TESTING:
This is a beta version for testing purposes only.
No warranty is provided. Use at your own risk.

FEATURES:
- Multi-instrument layering (guitar + piano combined sounds)
- Enhanced component library with drag-drop functionality
- Visual modulation matrix for DSP routing
- SFZ import support for sample maps
- Preset randomization and A/B compare
- Computer keyboard input for testing
- Professional installer with AudiCode branding

SUPPORT:
Email: support@patchcraft.com
Website: https://patchcraft.com

COMPANY:
AudiCode
Professional Audio Software Development
'''
    
    readme_file = output_path / "README.txt"
    with open(readme_file, 'w') as f:
        f.write(readme_content)
    print(f"✅ Created README: {readme_file}")
    
    # Create EULA
    eula_content = f'''PatchCraft Studio End User License Agreement v{version}
============================================================

IMPORTANT NOTICE: This software is for beta testing purposes only.

LICENSE TERMS:

1. LICENSE GRANT
   AudiCode grants you a non-exclusive, non-transferable license to use
   PatchCraft Studio for beta testing purposes.

2. RESTRICTIONS
   - You may not redistribute, sell, or commercialize this beta software
   - You may not reverse engineer, decompile, or modify software
   - All copyright notices must remain intact

3. BETA TESTING
   - This software may contain bugs and unfinished features
   - No warranty is provided for beta software
   - Use at your own risk
   - AudiCode is not liable for any damages

4. TERM
   This beta license terminates upon release of commercial version
   or at AudiCode's discretion.

5. CONTACT
   For support: support@patchcraft.com
   For licensing: licensing@patchcraft.com

By installing this software, you agree to these terms.

COMPANY INFORMATION:
AudiCode
Professional Audio Software Development
https://patchcraft.com
'''
    
    eula_file = output_path / "EULA.txt"
    with open(eula_file, 'w') as f:
        f.write(eula_content)
    print(f"✅ Created EULA: {eula_file}")
    
    # Calculate total size
    total_size = sum(f.stat().st_size for f in output_path.rglob("*") if f.is_file())
    size_mb = total_size / (1024 * 1024)
    
    print(f"\n📦 Working Installer Package Created")
    print(f"📍 Location: {output_path}")
    print(f"📏 Total Size: {size_mb:.1f} MB")
    print(f"\n📋 Package Contents:")
    print(f"   • PatchCraft Studio.exe (Main application)")
    print(f"   • Install.bat (Simple installer)")
    print(f"   • README.txt (Installation guide)")
    print(f"   • EULA.txt (License agreement)")
    
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_working_installer.py <source_exe> <output_dir> [version]")
        print("Example: python create_working_installer.py ./PatchCraft_Studio.exe ./dist 1.0.0")
        sys.exit(1)
    
    source_exe = sys.argv[1]
    output_dir = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_exe).exists():
        print(f"❌ Source executable not found: {source_exe}")
        sys.exit(1)
    
    if create_simple_working_installer(source_exe, output_dir, version):
        print("✅ Working installer package created successfully!")
    else:
        print("❌ Failed to create installer package")

if __name__ == "__main__":
    main()
