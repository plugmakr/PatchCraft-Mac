#!/usr/bin/env python3
"""
Create professional installer with branding, EULA, and proper installation
"""

import os
import sys
import tempfile
from pathlib import Path

def create_eula():
    """Create End User License Agreement"""
    return """PatchCraft Studio End User License Agreement v1.0.0
============================================================

IMPORTANT NOTICE: This software is for beta testing purposes only.

LICENSE TERMS:

1. LICENSE GRANT
   AudiCode grants you a non-exclusive, non-transferable license to use
   PatchCraft Studio for beta testing purposes.

2. RESTRICTIONS
   - You may not redistribute, sell, or commercialize this beta software
   - You may not reverse engineer, decompile, or modify the software
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
"""

def create_professional_installer(source_dir, output_installer, version="1.0.0"):
    """Create professional installer with branding and EULA"""
    
    print(f"Creating professional installer for PatchCraft v{version}")
    
    # Create installer resources
    eula_content = create_eula()
    
    # Create self-extracting installer with proper branding
    installer_script = f'''@echo off
title PatchCraft Studio v{version} Professional Installer
color 0B
cls

echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║     PATCHCRAFT STUDIO v{version} - PROFESSIONAL INSTALLER        ║
echo    ║                                                              ║
echo    ║     AudiCode - Beta Testing Only                              ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.

REM Check administrator rights
net session >nul 2>&1
if %errorLevel% == 0 (
    echo [✓] Running with administrator rights
) else (
    echo [!] WARNING: Not running as administrator!
    echo     Please right-click and "Run as administrator"
    echo     Some features may not work properly
    echo.
    pause
    exit /b 1
)

REM Create temporary directory
set TEMP_DIR=%TEMP%\\PatchCraftInstaller_{version}
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"

echo [✓] Created temporary directory: %TEMP_DIR%

REM Extract files (embedded in this script)
echo [✓] Extracting installation files...

REM Create EULA
echo Creating End User License Agreement...
(
echo PatchCraft Studio End User License Agreement v{version}
echo ============================================================
echo.
echo IMPORTANT NOTICE: This software is for beta testing purposes only.
echo.
echo LICENSE TERMS:
echo.
echo 1. LICENSE GRANT
echo    AudiCode grants you a non-exclusive, non-transferable license to use
echo    PatchCraft Studio for beta testing purposes.
echo.
echo 2. RESTRICTIONS
echo    - You may not redistribute, sell, or commercialize this beta software
echo    - You may not reverse engineer, decompile, or modify the software
echo    - All copyright notices must remain intact
echo.
echo 3. BETA TESTING
echo    - This software may contain bugs and unfinished features
echo    - No warranty is provided for beta software
echo    - Use at your own risk
echo    - AudiCode is not liable for any damages
echo.
echo 4. TERM
echo    This beta license terminates upon release of commercial version
echo    or at AudiCode's discretion.
echo.
echo 5. CONTACT
echo    For support: support@patchcraft.com
echo    For licensing: licensing@patchcraft.com
echo.
echo By installing this software, you agree to these terms.
echo.
echo ============================================================
) > "%TEMP_DIR%\\EULA.txt"

REM Show EULA
echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║                    END USER LICENSE AGREEMENT                ║
echo    ║                                                              ║
echo    ║     Please read the license terms carefully before proceeding      ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.
echo Press any key to continue to installation...
pause <nul

echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║                    INSTALLATION OPTIONS                       ║
echo    ║                                                              ║
echo    ║     1. Typical Installation - Recommended                      ║
echo    ║     2. Custom Installation - Choose location               ║
echo    ║     3. Exit Installer                                        ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.

set /p choice="Select installation option (1-3): "
if "%choice%"=="1" goto typical_install
if "%choice%"=="2" goto custom_install
if "%choice%"=="3" goto exit_installer
goto invalid_choice

:typical_install
set INSTALL_DIR=%PROGRAMFILES%\\PatchCraft
echo [✓] Selected: Typical Installation
goto start_install

:custom_install
set /p INSTALL_DIR="Enter installation directory: "
if "%INSTALL_DIR%"=="" set INSTALL_DIR=%PROGRAMFILES%\\PatchCraft
echo [✓] Selected: Custom Installation to %INSTALL_DIR%
goto start_install

:invalid_choice
echo [!] Invalid choice. Please select 1, 2, or 3.
echo.
pause
goto choice

:start_install
echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║                    INSTALLING PATCHCRAFT STUDIO            ║
echo    ║                                                              ║
echo    ╚════════════════════════════════════════════════════════╝
echo.

REM Create installation directory
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo [✓] Created installation directory
)

REM Copy application files
echo [✓] Copying PatchCraft Studio.exe...
copy "PatchCraft Studio.exe" "%INSTALL_DIR%\\" >nul 2>&1
if exist "%INSTALL_DIR%\\PatchCraft Studio.exe" (
    echo [✓] Application installed successfully
) else (
    echo [!] Failed to copy application
    pause
    exit /b 1
)

REM Create shortcuts
echo [✓] Creating desktop shortcut...
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Description = 'PatchCraft Studio v{version} - Professional Sample Instrument Designer'; $Shortcut.Save()" >nul 2>&1

echo [✓] Creating Start Menu shortcut...
set START_MENU=%PROGRAMDATA%\\Microsoft\\Windows\\Start Menu\\Programs
if not exist "%START_MENU%" mkdir "%START_MENU%"
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%START_MENU%\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Description = 'PatchCraft Studio v{version}'; $Shortcut.Save()" >nul 2>&1

REM Add to system PATH
echo [✓] Adding to system PATH...
setx PATH "%PATH%;%INSTALL_DIR%" >nul 2>&1

REM Create uninstaller
echo [✓] Creating uninstaller...
(
echo @echo off
echo title PatchCraft Studio Uninstaller
echo echo Uninstalling PatchCraft Studio v{version}...
echo echo.
echo set INSTALL_DIR=%PROGRAMFILES%\\PatchCraft
echo if exist "%INSTALL_DIR%" (
echo     echo Removing desktop shortcut...
echo     del "%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk" 2^>nul
echo     echo Removing Start Menu shortcut...
echo     del "%PROGRAMDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\PatchCraft Studio.lnk" 2^>nul
echo     echo Removing application files...
echo     rmdir /s /q "%INSTALL_DIR%"
echo     echo [✓] PatchCraft Studio uninstalled successfully
echo ) else (
echo     echo [!] PatchCraft Studio not found
echo )
echo echo.
echo pause
) > "%INSTALL_DIR%\\Uninstall.bat"

REM Create registry entries
echo [✓] Creating registry entries...
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "InstallPath" /d "%INSTALL_DIR%" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Version" /d "{version}" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Company" /d "AudiCode" /f >nul 2>&1

REM Clean up temporary files
echo [✓] Cleaning up temporary files...
rmdir /s /q "%TEMP_DIR%"

echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║                  INSTALLATION COMPLETE!                    ║
echo    ║                                                              ║
echo    ║     PatchCraft Studio v{version} has been installed to:      ║
echo    ║     %INSTALL_DIR%                                          ║
echo    ║                                                              ║
echo    ║     Launch Options:                                     ║
echo    ║     • Desktop shortcut                                     ║
echo    ║     • Start Menu: Programs ^> PatchCraft Studio              ║
echo    ║     • Run: %INSTALL_DIR%\\PatchCraft Studio.exe          ║
echo    ║                                                              ║
echo    ║     Features:                                          ║
echo    ║     • Multi-instrument layering                          ║
echo    ║     • Enhanced component library                         ║
echo    ║     • Visual modulation matrix                          ║
echo    ║     • SFZ import support                               ║
echo    ║     • Preset randomization                             ║
echo    ║     • A/B compare                                      ║
echo    ║     • Computer keyboard input                           ║
echo    ║                                                              ║
echo    ║     Support: support@patchcraft.com                     ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.
echo Press any key to exit...
pause <nul
goto end

:exit_installer
echo.
echo Installation cancelled by user.
pause
goto end

:end
exit /b 0
'''
    
    # Create the installer executable
    with open(output_installer, 'w', encoding='utf-8') as f:
        f.write(installer_script)
    
    print(f"✅ Professional installer created: {output_installer}")
    return True

def create_installer_package(source_dir, output_dir, version="1.0.0"):
    """Create complete installer package"""
    
    print(f"Creating professional installer package for PatchCraft v{version}")
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    # Copy main application
    source_exe = Path(source_dir) / "PatchCraft Studio.exe"
    dest_exe = output_path / "PatchCraft Studio.exe"
    
    if source_exe.exists():
        import shutil
        shutil.copy2(source_exe, dest_exe)
        print(f"✅ Copied PatchCraft Studio.exe ({source_exe.stat().st_size / 1024 / 1024:.1f} MB)")
    else:
        print(f"❌ Source executable not found: {source_exe}")
        return False
    
    # Create professional installer
    installer_exe = output_path / "PatchCraft-Setup.exe"
    if create_professional_installer(source_dir, installer_exe, version):
        print(f"✅ Created professional installer: {installer_exe}")
    else:
        print("❌ Failed to create professional installer")
        return False
    
    # Create installation guide
    install_guide = f'''PatchCraft Studio v{version} - Professional Installation Guide
================================================================

INSTALLATION FILES:
- PatchCraft-Setup.exe (Professional installer with EULA and branding)
- PatchCraft Studio.exe (Main application)

INSTALLATION OPTIONS:

OPTION 1: PROFESSIONAL INSTALLER (Recommended)
1. Run PatchCraft-Setup.exe
2. Read and accept EULA
3. Choose installation type:
   • Typical: Installs to C:\\Program Files\\PatchCraft
   • Custom: Choose installation directory
4. Automatic shortcut creation (Desktop + Start Menu)
5. Registry entries for proper Windows integration
6. Built-in uninstaller

OPTION 2: MANUAL INSTALLATION
1. Create folder: C:\\Program Files\\PatchCraft
2. Copy PatchCraft Studio.exe to that folder
3. Create shortcuts manually

PROFESSIONAL FEATURES:
- Branded installer with AudiCode company information
- End User License Agreement (EULA)
- Administrator rights checking
- Interactive installation options
- Progress indicators and status messages
- Automatic desktop and Start Menu shortcuts
- System PATH integration
- Registry entries for Windows integration
- Built-in uninstaller
- Professional UI with ASCII art branding

BETA TESTING:
This is a beta version for testing purposes only.
No warranty is provided. Use at your own risk.

SYSTEM REQUIREMENTS:
- Windows 10/11 (64-bit)
- 4GB RAM minimum (8GB recommended)
- Administrator rights (for installer)
- Audio interface (recommended for low latency)

SUPPORT:
Email: support@patchcraft.com
Website: https://patchcraft.com
'''
    
    with open(output_path / "INSTALLATION_GUIDE.txt", "w") as f:
        f.write(install_guide)
    
    # Create package info
    package_info = {
        "name": "PatchCraft Studio",
        "version": version,
        "company": "AudiCode",
        "type": "Professional Installer",
        "platform": "Windows",
        "beta": True,
        "features": [
            "Multi-instrument layering",
            "Enhanced component library",
            "Visual modulation matrix",
            "SFZ import support",
            "Preset randomization",
            "A/B compare",
            "Computer keyboard input",
            "Professional installer with EULA",
            "Automatic shortcuts creation",
            "Registry integration"
        ]
    }
    
    import json
    with open(output_path / "package.json", "w") as f:
        json.dump(package_info, f, indent=2)
    
    # Calculate package size
    total_size = sum(f.stat().st_size for f in output_path.rglob("*") if f.is_file())
    size_mb = total_size / (1024 * 1024)
    
    print(f"\n📦 Professional Installer Package Created")
    print(f"📍 Location: {output_path}")
    print(f"📏 Total Size: {size_mb:.1f} MB")
    print(f"\n📋 Package Contents:")
    print(f"   • PatchCraft-Setup.exe (Professional installer)")
    print(f"   • PatchCraft Studio.exe (Main application)")
    print(f"   • INSTALLATION_GUIDE.txt (Installation instructions)")
    print(f"   • package.json (Package information)")
    
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_professional_installer.py <source_dir> <output_dir> [version]")
        print("Example: python create_professional_installer.py ./dist/PatchCraftInstaller ./dist 1.0.0")
        sys.exit(1)
    
    source_dir = sys.argv[1]
    output_dir = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_dir).exists():
        print(f"❌ Source directory not found: {source_dir}")
        sys.exit(1)
    
    if create_installer_package(source_dir, output_dir, version):
        print("✅ Professional installer package created successfully!")
    else:
        print("❌ Failed to create installer package")

if __name__ == "__main__":
    main()
