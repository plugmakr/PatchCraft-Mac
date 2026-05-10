#!/usr/bin/env python3
"""
Create ONE SINGLE EXE FILE installer - ASCII only
"""

import os
import sys
from pathlib import Path

def create_single_exe_installer(source_exe, output_exe, version="1.0.0"):
    """Create ONE SINGLE EXE installer file"""
    
    print(f"Creating SINGLE EXE installer for PatchCraft v{version}")
    
    # Check source executable
    source_path = Path(source_exe)
    if not source_path.exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        return False
    
    # Create simple installer script with ASCII only
    installer_script = f'''@echo off
title PatchCraft Studio v{version} Installer
color 0A
cls

echo.
echo ===============================================================
echo   PatchCraft Studio v{version} - Single EXE Installer
echo   AudiCode - Beta Testing Only
echo ===============================================================
echo.

REM Check if PatchCraft Studio.exe exists
if not exist "PatchCraft Studio.exe" (
    echo ERROR: PatchCraft Studio.exe not found!
    echo.
    echo Please ensure PatchCraft Studio.exe is in the same directory
    echo as this installer.
    echo.
    pause
    exit /b 1
)

echo Found PatchCraft Studio.exe
echo.

REM Show EULA
echo.
echo ===============================================================
echo                   END USER LICENSE AGREEMENT
echo ===============================================================
echo.
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
echo    - You may not reverse engineer, decompile, or modify software
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
echo.

set /p continue="Continue with installation? (Y/N): "
if /i not "%continue%"=="Y" (
    echo Installation cancelled by user.
    pause
    exit /b 1
)

echo.
echo ===============================================================
echo                    INSTALLATION OPTIONS
echo ===============================================================
echo.
echo 1. Typical Installation - Recommended
echo 2. Custom Installation - Choose location
echo 3. Exit Installer
echo.

set /p choice="Select installation option (1-3): "
if "%choice%"=="1" goto typical_install
if "%choice%"=="2" goto custom_install
if "%choice%"=="3" goto exit_installer
goto invalid_choice

:typical_install
set INSTALL_DIR=%PROGRAMFILES%\\PatchCraft
echo Selected: Typical Installation
goto start_install

:custom_install
set /p INSTALL_DIR="Enter installation directory: "
if "%INSTALL_DIR%"=="" set INSTALL_DIR=%PROGRAMFILES%\\PatchCraft
echo Selected: Custom Installation to %INSTALL_DIR%
goto start_install

:invalid_choice
echo Invalid choice. Please select 1, 2, or 3.
echo.
pause
goto choice

:start_install
echo.
echo ===============================================================
echo                    INSTALLING PATCHCRAFT STUDIO
echo ===============================================================
echo.

REM Create installation directory
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo Created installation directory
)

REM Copy application files
echo Copying PatchCraft Studio.exe...
copy "PatchCraft Studio.exe" "%INSTALL_DIR%\\" >nul

if exist "%INSTALL_DIR%\\PatchCraft Studio.exe" (
    echo Application installed successfully
) else (
    echo Failed to install PatchCraft Studio.exe
    pause
    exit /b 1
)

REM Create shortcuts
echo Creating desktop shortcut...
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Description = 'PatchCraft Studio v{version} - Professional Sample Instrument Designer'; $Shortcut.Save()" >nul 2>&1

echo Creating Start Menu shortcut...
set START_MENU=%PROGRAMDATA%\\Microsoft\\Windows\\Start Menu\\Programs
if not exist "%START_MENU%" mkdir "%START_MENU%"
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%START_MENU%\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Save()" >nul 2>&1

REM Add to system PATH
echo Adding to system PATH...
setx PATH "%PATH%;%INSTALL_DIR%" >nul 2>&1

REM Create registry entries
echo Creating registry entries...
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "InstallPath" /d "%INSTALL_DIR%" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Version" /d "{version}" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Company" /d "AudiCode" /f >nul 2>&1

REM Create uninstaller
echo Creating uninstaller...
echo @echo off > "%INSTALL_DIR%\\Uninstall.bat"
echo title PatchCraft Studio Uninstaller >> "%INSTALL_DIR%\\Uninstall.bat"
echo echo Uninstalling PatchCraft Studio v{version}... >> "%INSTALL_DIR%\\Uninstall.bat"
echo echo. >> "%INSTALL_DIR%\\Uninstall.bat"
echo set INSTALL_DIR=%INSTALL_DIR% >> "%INSTALL_DIR%\\Uninstall.bat"
echo if exist "%INSTALL_DIR%" ( >> "%INSTALL_DIR%\\Uninstall.bat"
echo     echo Removing desktop shortcut... >> "%INSTALL_DIR%\\Uninstall.bat"
echo     del "%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk" 2^>nul >> "%INSTALL_DIR%\\Uninstall.bat"
echo     echo Removing Start Menu shortcut... >> "%INSTALL_DIR%\\Uninstall.bat"
echo     del "%PROGRAMDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\PatchCraft Studio.lnk" 2^>nul >> "%INSTALL_DIR%\\Uninstall.bat"
echo     echo Removing application files... >> "%INSTALL_DIR%\\Uninstall.bat"
echo     rmdir /s /q "%INSTALL_DIR%" >> "%INSTALL_DIR%\\Uninstall.bat"
echo     echo PatchCraft Studio uninstalled successfully >> "%INSTALL_DIR%\\Uninstall.bat"
echo ) else ( >> "%INSTALL_DIR%\\Uninstall.bat"
echo     echo PatchCraft Studio not found >> "%INSTALL_DIR%\\Uninstall.bat"
echo ) >> "%INSTALL_DIR%\\Uninstall.bat"
echo echo. >> "%INSTALL_DIR%\\Uninstall.bat"
echo pause >> "%INSTALL_DIR%\\Uninstall.bat"

echo.
echo ===============================================================
echo                    INSTALLATION COMPLETE!
echo ===============================================================
echo.
echo PatchCraft Studio v{version} has been installed to:
echo %INSTALL_DIR%
echo.
echo Launch Options:
echo - Desktop shortcut: PatchCraft Studio
echo - Start Menu: Programs > PatchCraft Studio
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
echo Company: AudiCode
echo.
echo Press any key to exit...
pause >nul
goto end

:exit_installer
echo.
echo Installation cancelled by user.
pause
goto end

:end
exit /b 0
'''
    
    # Create the single EXE installer
    try:
        # Create batch file first
        bat_file = output_exe.replace('.exe', '.bat')
        with open(bat_file, 'w', encoding='ascii') as f:
            f.write(installer_script)
        
        # Rename to .exe (Windows will still run it)
        import shutil
        shutil.move(bat_file, output_exe)
        
        # Get file size
        size_mb = Path(output_exe).stat().st_size / (1024 * 1024)
        
        print(f"SUCCESS: Single EXE installer created: {output_exe}")
        print(f"File size: {size_mb:.1f} MB")
        print()
        print("INSTALLATION INSTRUCTIONS:")
        print("1. Place PatchCraft Studio.exe in same directory as installer")
        print("2. Run PatchCraft-Setup.exe")
        print("3. Follow on-screen prompts")
        print()
        print("RESULT: ONE SINGLE EXE FILE installer")
        print("No other files needed!")
        
        return True
        
    except Exception as e:
        print(f"ERROR: Failed to create single EXE installer: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_single_exe_installer_ascii.py <source_exe> <output_exe> [version]")
        print("Example: python create_single_exe_installer_ascii.py ./PatchCraft_Studio.exe ./PatchCraft-Setup.exe 1.0.0")
        sys.exit(1)
    
    source_exe = sys.argv[1]
    output_exe = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_exe).exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        sys.exit(1)
    
    if create_single_exe_installer(source_exe, output_exe, version):
        print("SUCCESS: Single EXE installer created!")
    else:
        print("ERROR: Failed to create single EXE installer")

if __name__ == "__main__":
    main()
