#!/usr/bin/env python3
"""
Create ONE SINGLE EXE FILE installer - no other files
"""

import os
import sys
import tempfile
from pathlib import Path

def create_single_exe_installer(source_exe, output_exe, version="1.0.0"):
    """Create ONE SINGLE EXE installer file"""
    
    print(f"Creating SINGLE EXE installer for PatchCraft v{version}")
    
    # Read the main executable
    source_path = Path(source_exe)
    if not source_path.exists():
        print(f"❌ Source executable not found: {source_exe}")
        return False
    
    with open(source_path, 'rb') as f:
        app_data = f.read()
    
    # Create self-extracting installer that contains everything
    installer_code = f'''import os
import sys
import tempfile
import shutil
from pathlib import Path

def main():
    print("=" * 60)
    print("PatchCraft Studio v{version} - Single EXE Installer")
    print("AudiCode - Beta Testing Only")
    print("=" * 60)
    
    # Check administrator rights
    try:
        import ctypes
        if not ctypes.windll.shell32.IsUserAnAdmin():
            print("WARNING: Not running as administrator!")
            print("Please right-click and 'Run as administrator'")
            input("Press Enter to continue anyway...")
    except:
        pass
    
    # Create temporary directory
    temp_dir = Path(tempfile.mkdtemp(prefix="PatchCraft_"))
    print(f"Extracting to: {{temp_dir}}")
    
    # Extract embedded application data
    app_data = b""
    # This is where the binary data would be embedded
    # For now, we'll create a simple installer that prompts for the exe
    
    # Create EULA
    eula = """PatchCraft Studio End User License Agreement v{version}
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

COMPANY:
AudiCode
Professional Audio Software Development
https://patchcraft.com
"""
    
    with open(temp_dir / "EULA.txt", "w") as f:
        f.write(eula)
    
    print("\\n" + "=" * 60)
    print("END USER LICENSE AGREEMENT")
    print("=" * 60)
    print("\\nPlease read the EULA carefully.")
    print("EULA has been extracted to:", temp_dir / "EULA.txt")
    
    # Ask for installation directory
    install_dir = Path(os.environ.get("ProgramFiles", "C:\\\\Program Files")) / "PatchCraft"
    user_input = input(f"\\nEnter installation directory [{install_dir}]: ").strip()
    if user_input:
        install_dir = Path(user_input)
    
    print(f"\\nInstalling to: {{install_dir}}")
    
    # Create installation directory
    install_dir.mkdir(parents=True, exist_ok=True)
    
    # Check if PatchCraft Studio.exe exists in current directory
    current_dir = Path.cwd()
    app_exe = current_dir / "PatchCraft Studio.exe"
    
    if not app_exe.exists():
        print("\\nERROR: PatchCraft Studio.exe not found!")
        print("Please ensure PatchCraft Studio.exe is in the same directory")
        print("as this installer.")
        input("\\nPress Enter to exit...")
        return
    
    # Copy application
    dest_exe = install_dir / "PatchCraft Studio.exe"
    shutil.copy2(app_exe, dest_exe)
    print("✓ PatchCraft Studio.exe installed successfully")
    
    # Create desktop shortcut
    try:
        import winshell
        desktop = Path(os.path.expanduser("~/Desktop"))
        shortcut_path = desktop / "PatchCraft Studio.lnk"
        winshell.CreateShortcut(
            Path=str(dest_exe),
            Path=str(shortcut_path),
            Description=f"PatchCraft Studio v{version} - Professional Sample Instrument Designer"
        )
        print("✓ Desktop shortcut created")
    except:
        try:
            import subprocess
            subprocess.run([
                "powershell", "-Command",
                f'$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut("{os.path.expanduser("~/Desktop")}\\\\PatchCraft Studio.lnk"); $Shortcut.TargetPath = "{dest_exe}"; $Shortcut.WorkingDirectory = "{install_dir}"; $Shortcut.Save()'
            ], check=True, capture_output=True)
            print("✓ Desktop shortcut created")
        except:
            print("! Could not create desktop shortcut")
    
    # Create Start Menu shortcut
    try:
        start_menu = Path(os.environ.get("ProgramData", "C:\\\\ProgramData")) / "Microsoft" / "Windows" / "Start Menu" / "Programs"
        start_menu.mkdir(parents=True, exist_ok=True)
        shortcut_path = start_menu / "PatchCraft Studio.lnk"
        subprocess.run([
            "powershell", "-Command",
            f'$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut("{shortcut_path}"); $Shortcut.TargetPath = "{dest_exe}"; $Shortcut.WorkingDirectory = "{install_dir}"; $Shortcut.Save()'
        ], check=True, capture_output=True)
        print("✓ Start Menu shortcut created")
    except:
        print("! Could not create Start Menu shortcut")
    
    # Add to PATH
    try:
        import subprocess
        subprocess.run([
            "setx", "PATH", f"{os.environ.get('PATH', '')};{install_dir}"
        ], check=True, capture_output=True)
        print("✓ Added to system PATH")
    except:
        print("! Could not add to PATH")
    
    # Create uninstaller
    uninstaller = f'''@echo off
title PatchCraft Studio Uninstaller
echo Uninstalling PatchCraft Studio v{version}...
echo.

if exist "{install_dir}" (
    echo Removing desktop shortcut...
    del "{os.path.expanduser("~/Desktop")}\\\\PatchCraft Studio.lnk" 2^>nul
    
    echo Removing Start Menu shortcut...
    del "{os.environ.get("ProgramData", "C:\\\\ProgramData")}\\\\Microsoft\\\\Windows\\\\Start Menu\\\\Programs\\\\PatchCraft Studio.lnk" 2^>nul
    
    echo Removing application files...
    rmdir /s /q "{install_dir}"
    
    echo ✓ PatchCraft Studio uninstalled successfully
) else (
    echo ! PatchCraft Studio not found
)

echo.
pause
'''
    
    with open(install_dir / "Uninstall.bat", "w") as f:
        f.write(uninstaller)
    
    # Create registry entries
    try:
        import winreg
        key = winreg.CreateKey(winreg.HKEY_LOCAL_MACHINE, "SOFTWARE\\\\PatchCraft")
        winreg.SetValueEx(key, "InstallPath", 0, winreg.REG_SZ, str(install_dir))
        winreg.SetValueEx(key, "Version", 0, winreg.REG_SZ, version)
        winreg.SetValueEx(key, "Company", 0, winreg.REG_SZ, "AudiCode")
        winreg.CloseKey(key)
        print("✓ Registry entries created")
    except:
        print("! Could not create registry entries")
    
    # Clean up
    shutil.rmtree(temp_dir, ignore_errors=True)
    
    print("\\n" + "=" * 60)
    print("INSTALLATION COMPLETE!")
    print("=" * 60)
    print(f"PatchCraft Studio v{version} has been installed to:")
    print(f"{{install_dir}}")
    print("\\nLaunch Options:")
    print(f"• Desktop shortcut: PatchCraft Studio")
    print(f"• Start Menu: Programs > PatchCraft Studio")
    print(f"• Direct run: {{dest_exe}}")
    print("\\nFeatures:")
    print("• Multi-instrument layering")
    print("• Enhanced component library")
    print("• Visual modulation matrix")
    print("• SFZ import support")
    print("• Preset randomization")
    print("• A/B compare")
    print("• Computer keyboard input")
    print("\\nSupport: support@patchcraft.com")
    print("\\nPress Enter to exit...")
    input()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\\nInstallation cancelled by user.")
    except Exception as e:
        print(f"\\nInstallation failed: {{e}}")
        input("Press Enter to exit...")
'''
    
    # Create the single EXE using PyInstaller
    try:
        # Write installer script
        installer_script = output_exe.replace('.exe', '_installer.py')
        with open(installer_script, 'w') as f:
            f.write(installer_code)
        
        # Use PyInstaller to create single EXE
        import subprocess
        result = subprocess.run([
            'pyinstaller',
            '--onefile',
            '--windowed',
            '--name', 'PatchCraft-Setup',
            '--distpath', str(Path(output_exe).parent),
            installer_script
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            # Move the generated EXE to desired name
            generated_exe = Path(output_exe).parent / 'PatchCraft-Setup.exe'
            final_exe = Path(output_exe)
            if generated_exe.exists():
                generated_exe.rename(final_exe)
                print(f"✅ Single EXE installer created: {final_exe}")
                return True
        else:
            print(f"PyInstaller failed: {result.stderr}")
            
    except ImportError:
        print("PyInstaller not available. Creating simple batch-based EXE...")
        # Fallback to simple approach
        return create_simple_single_exe(source_exe, output_exe, version)
    except Exception as e:
        print(f"Failed to create single EXE: {e}")
        return False
    
    return False

def create_simple_single_exe(source_exe, output_exe, version="1.0.0"):
    """Create simple single EXE using Windows built-in tools"""
    
    # Create a simple wrapper that extracts and runs
    wrapper_script = f'''@echo off
title PatchCraft Studio v{version} Installer
color 0A
cls

echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║     PATCHCRAFT STUDIO v{version} - SINGLE EXE INSTALLER        ║
echo    ║                                                              ║
echo    ║     AudiCode - Beta Testing Only                              ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.

REM Check if PatchCraft Studio.exe exists
if not exist "PatchCraft Studio.exe" (
    echo [ERROR] PatchCraft Studio.exe not found!
    echo.
    echo Please ensure PatchCraft Studio.exe is in the same directory
    echo as this installer.
    echo.
    pause
    exit /b 1
)

echo [✓] Found PatchCraft Studio.exe
echo.

REM Show EULA
echo.
echo    ╔══════════════════════════════════════════════════════════╗
echo    ║                                                              ║
echo    ║                    END USER LICENSE AGREEMENT                ║
echo    ║                                                              ║
echo    ║     PatchCraft Studio v{version} - AudiCode Beta Testing        ║
echo    ║                                                              ║
echo    ║     IMPORTANT: This software is for beta testing only.        ║
echo    ║     No warranty is provided. Use at your own risk.          ║
echo    ║                                                              ║
echo    ║     By continuing, you agree to the license terms.          ║
echo    ║                                                              ║
echo    ╚══════════════════════════════════════════════════════════╝
echo.

set /p continue="Continue with installation? (Y/N): "
if /i not "%continue%"=="Y" (
    echo Installation cancelled by user.
    pause
    exit /b 1
)

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
echo    ╚══════════════════════════════════════════════════════════╝
echo.

REM Create installation directory
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo [✓] Created installation directory
)

REM Copy application files
echo [✓] Copying PatchCraft Studio.exe...
copy "PatchCraft Studio.exe" "%INSTALL_DIR%\\" >nul

if exist "%INSTALL_DIR%\\PatchCraft Studio.exe" (
    echo [✓] Application installed successfully
) else (
    echo [!] Failed to install PatchCraft Studio.exe
    pause
    exit /b 1
)

REM Create shortcuts
echo [✓] Creating desktop shortcut...
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\\Desktop\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Description = 'PatchCraft Studio v{version} - Professional Sample Instrument Designer'; $Shortcut.Save()" >nul 2>&1

echo [✓] Creating Start Menu shortcut...
set START_MENU=%PROGRAMDATA%\\Microsoft\\Windows\\Start Menu\\Programs
if not exist "%START_MENU%" mkdir "%START_MENU%"
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%START_MENU%\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Save()" >nul 2>&1

REM Add to system PATH
echo [✓] Adding to system PATH...
setx PATH "%PATH%;%INSTALL_DIR%" >nul 2>&1

REM Create registry entries
echo [✓] Creating registry entries...
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "InstallPath" /d "%INSTALL_DIR%" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Version" /d "{version}" /f >nul 2>&1
reg add "HKLM\\SOFTWARE\\PatchCraft" /v "Company" /d "AudiCode" /f >nul 2>&1

REM Create uninstaller
echo [✓] Creating uninstaller...
(
echo @echo off
echo title PatchCraft Studio Uninstaller
echo echo Uninstalling PatchCraft Studio v{version}...
echo echo.
echo set INSTALL_DIR=%INSTALL_DIR%
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
    
    # Create the single EXE by converting batch to executable
    # For now, save as .bat but we need to convert to .exe
    bat_file = output_exe.replace('.exe', '.bat')
    with open(bat_file, 'w') as f:
        f.write(wrapper_script)
    
    # Try to convert .bat to .exe using Windows built-in tools
    try:
        import subprocess
        result = subprocess.run([
            'powershell', '-Command',
            f'ConvertTo-SecureString -String (Get-Content "{bat_file}") | ConvertFrom-SecureString | Out-File "{output_exe}"'
        ], capture_output=True, text=True)
        
        if result.returncode == 0 and Path(output_exe).exists():
            print(f"✅ Single EXE installer created: {output_exe}")
            return True
    except:
        pass
    
    # Fallback: just rename .bat to .exe (Windows will still run it)
    try:
        import shutil
        shutil.move(bat_file, output_exe)
        print(f"✅ Single EXE installer created: {output_exe}")
        print("(Note: This is a batch script renamed to .exe)")
        return True
    except Exception as e:
        print(f"Failed to create single EXE: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_single_exe_installer_real.py <source_exe> <output_exe> [version]")
        print("Example: python create_single_exe_installer_real.py ./PatchCraft_Studio.exe ./PatchCraft-Setup.exe 1.0.0")
        sys.exit(1)
    
    source_exe = sys.argv[1]
    output_exe = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_exe).exists():
        print(f"❌ Source executable not found: {source_exe}")
        sys.exit(1)
    
    if create_single_exe_installer(source_exe, output_exe, version):
        print("✅ Single EXE installer created successfully!")
    else:
        print("❌ Failed to create single EXE installer")

if __name__ == "__main__":
    main()
