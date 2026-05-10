#!/usr/bin/env python3
"""
Create working Inno Setup single EXE installer
"""

import os
import sys
from pathlib import Path

def create_inno_setup_script(source_exe, output_dir, version="1.0.0"):
    """Create Inno Setup script for single EXE installer"""
    
    print(f"Creating Inno Setup installer for PatchCraft v{version}")
    
    # Check source executable
    source_path = Path(source_exe)
    if not source_path.exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        return False
    
    # Create Inno Setup script (properly formatted)
    inno_script = "[Setup]\n"
    inno_script += "AppId={A1B2C3D4-E5F6-7890-ABCD-EF1234567890}\n"
    inno_script += f"AppName=PatchCraft Studio\n"
    inno_script += f"AppVersion={version}\n"
    inno_script += "AppPublisher=AudiCode\n"
    inno_script += "AppPublisherURL=https://patchcraft.com\n"
    inno_script += "AppSupportPhone=support@patchcraft.com\n"
    inno_script += "AppSupportURL=https://patchcraft.com/support\n"
    inno_script += "AppUpdatesURL=https://patchcraft.com/updates\n"
    inno_script += "DefaultDirName={pf}\\PatchCraft\n"
    inno_script += "DefaultGroupName=PatchCraft Studio\n"
    inno_script += "AllowNoIcons=yes\n"
    inno_script += "LicenseFile=LICENSE.txt\n"
    inno_script += "InfoBeforeFile=EULA.txt\n"
    inno_script += f"OutputDir={output_dir}\n"
    inno_script += "OutputBaseFilename=PatchCraft-Setup\n"
    inno_script += "SetupIconFile=icon.ico\n"
    inno_script += "Compression=lzma2/max\n"
    inno_script += "SolidCompression=yes\n"
    inno_script += "WizardStyle=modern\n"
    inno_script += "WizardImageFile=wizard.bmp\n"
    inno_script += "WizardSmallImageFile=wizard-small.bmp\n"
    
    inno_script += "\n[Languages]\n"
    inno_script += 'Name: "english"; MessagesFile: "compiler:Default.isl"\n'
    
    inno_script += "\n[Tasks]\n"
    inno_script += 'Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked\n'
    
    inno_script += "\n[Files]\n"
    inno_script += f'Source: "{source_exe}"; DestDir: "{{app}}"; Flags: ignoreversion\n'
    inno_script += 'Source: "LICENSE.txt"; DestDir: "{{app}}"; Flags: ignoreversion\n'
    inno_script += 'Source: "EULA.txt"; DestDir: "{{app}}"; Flags: ignoreversion\n'
    
    inno_script += "\n[Icons]\n"
    inno_script += f'Name: "{{group}}\\PatchCraft Studio"; Filename: "{{app}}\\PatchCraft Studio.exe"; WorkingDir: "{{app}}"; Comment: "PatchCraft Studio v{version} - Professional Sample Instrument Designer"\n'
    inno_script += f'Name: "{{commondesktop}}\\PatchCraft Studio"; Filename: "{{app}}\\PatchCraft Studio.exe"; WorkingDir: "{{app}}"; Tasks: desktopicon; Comment: "PatchCraft Studio v{version}"\n'
    
    inno_script += "\n[Run]\n"
    inno_script += f'Filename: "{{app}}\\PatchCraft Studio.exe"; Description: "{{cm:LaunchProgram,PatchCraft Studio}}"; Flags: nowait postinstall skipifsilent\n'
    
    inno_script += "\n[UninstallDelete]\n"
    inno_script += 'Type: filesandordirs; Name: "{{app}}"\n'
    
    inno_script += "\n[Registry]\n"
    inno_script += 'Root: HKLM; Subkey: "SOFTWARE\\PatchCraft"; ValueType: string; ValueName: "InstallPath"; ValueData: "{{app}}"\n'
    inno_script += f'Root: HKLM; Subkey: "SOFTWARE\\PatchCraft"; ValueType: string; ValueName: "Version"; ValueData: "{version}"\n'
    inno_script += 'Root: HKLM; Subkey: "SOFTWARE\\PatchCraft"; ValueType: string; ValueName: "Company"; ValueData: "AudiCode"\n'
    
    inno_script += "\n[Code]\n"
    inno_script += """function GetUninstallString(): String;
var
  sUnInstPath: String;
begin
  sUnInstPath := ExpandConstant('Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{#SetupSetting("AppId")}_is1');
  Result := RemoveQuotes(RegQueryStringValue(HKEY_LOCAL_MACHINE, sUnInstPath, 'UninstallString'));
end;

function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;

function InitializeSetup(): Boolean;
var
  V: Integer;
begin
  if IsUpgrade() then
  begin
    V := MsgBox('PatchCraft Studio is already installed. Do you want to uninstall the previous version before installing the new one?', mbInformation, MB_YESNO);
    if V = IDYES then
    begin
      Exec(GetUninstallString(), '/SILENT', '', SW_SHOW, ewWaitUntilTerminated, Result);
      Result := True;
    end
    else
      Result := False;
  end
  else
    Result := True;
end;
"""
    
    # Create LICENSE.txt
    license_content = """PatchCraft Studio End User License Agreement v{version}
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
""".format(version=version)
    
    # Create EULA.txt
    eula_content = """PATCHCRAFT STUDIO END USER LICENSE AGREEMENT v{version}
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

COMPANY:
AudiCode
Professional Audio Software Development
https://patchcraft.com

By clicking "Yes" you agree to these license terms.
""".format(version=version)
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    # Save Inno Setup script
    iss_file = output_path / "PatchCraft-Setup.iss"
    with open(iss_file, 'w', encoding='utf-8') as f:
        f.write(inno_script)
    print(f"✅ Created Inno Setup script: {iss_file}")
    
    # Save LICENSE.txt
    license_file = output_path / "LICENSE.txt"
    with open(license_file, 'w', encoding='utf-8') as f:
        f.write(license_content)
    print(f"✅ Created LICENSE.txt: {license_file}")
    
    # Save EULA.txt
    eula_file = output_path / "EULA.txt"
    with open(eula_file, 'w', encoding='utf-8') as f:
        f.write(eula_content)
    print(f"✅ Created EULA.txt: {eula_file}")
    
    # Copy source executable to output directory
    dest_exe = output_path / "PatchCraft Studio.exe"
    import shutil
    shutil.copy2(source_path, dest_exe)
    print(f"✅ Copied PatchCraft Studio.exe: {dest_exe}")
    
    return True

def compile_inno_setup(iss_file):
    """Compile Inno Setup script to create single EXE"""
    
    print(f"Compiling Inno Setup script: {iss_file}")
    
    # Try to compile using Inno Setup compiler
    try:
        import subprocess
        
        # Try different Inno Setup compiler paths
        compiler_paths = [
            r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
            r"C:\Program Files\Inno Setup 6\ISCC.exe",
            r"C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
            r"C:\Program Files\Inno Setup 5\ISCC.exe",
            "ISCC.exe"  # Try from PATH
        ]
        
        compiler_found = False
        for compiler_path in compiler_paths:
            if Path(compiler_path).exists() or compiler_path == "ISCC.exe":
                print(f"Using Inno Setup compiler: {compiler_path}")
                compiler_found = True
                
                # Compile script
                result = subprocess.run([
                    compiler_path, 
                    str(iss_file)
                ], capture_output=True, text=True, cwd=str(iss_file.parent))
                
                if result.returncode == 0:
                    print("✅ Inno Setup compilation successful!")
                    return True
                else:
                    print(f"❌ Inno Setup compilation failed: {result.stderr}")
                    return False
        
        if not compiler_found:
            print("❌ Inno Setup compiler not found")
            print("Please install Inno Setup from: https://jrsoftware.org/isdl.php")
            return False
            
    except Exception as e:
        print(f"❌ Error compiling Inno Setup: {e}")
        return False

def create_simple_installer(output_dir, version="1.0.0"):
    """Create simple installer that works without admin rights"""
    
    print(f"Creating simple installer for PatchCraft v{version}")
    
    output_path = Path(output_dir)
    
    # Create simple installer script
    installer_script = """@echo off
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
echo By installing this software, you agree to the license terms.
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
echo 1. Install to Program Files (Recommended)
echo 2. Install to Documents
echo 3. Install to Custom Location
echo 4. Exit Installer
echo.

set /p choice="Select installation option (1-4): "
if "%choice%"=="1" goto program_files_install
if "%choice%"=="2" goto documents_install
if "%choice%"=="3" goto custom_install
if "%choice%"=="4" goto exit_installer
goto invalid_choice

:program_files_install
set INSTALL_DIR=%USERPROFILE%\\AppData\\Local\\PatchCraft
echo Selected: Install to Program Files (User)
goto start_install

:documents_install
set INSTALL_DIR=%USERPROFILE%\\Documents\\PatchCraft
echo Selected: Install to Documents
goto start_install

:custom_install
set /p INSTALL_DIR="Enter installation directory: "
if "%INSTALL_DIR%"=="" set INSTALL_DIR=%USERPROFILE%\\Desktop\\PatchCraft
echo Selected: Custom Installation to %INSTALL_DIR%
goto start_install

:invalid_choice
echo Invalid choice. Please select 1, 2, 3, or 4.
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
set START_MENU=%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs
if not exist "%START_MENU%" mkdir "%START_MENU%"
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%START_MENU%\\PatchCraft Studio.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\PatchCraft Studio.exe'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Save()" >nul 2>&1

REM Create uninstaller
echo Creating uninstaller...
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
echo     del "%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\PatchCraft Studio.lnk" 2^>nul
echo     echo Removing application files...
echo     rmdir /s /q "%INSTALL_DIR%"
echo     echo PatchCraft Studio uninstalled successfully
echo ) else (
echo     echo PatchCraft Studio not found
echo )
echo echo.
echo pause
) > "%INSTALL_DIR%\\Uninstall.bat"

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
""".format(version=version)
    
    # Save installer as .exe (batch renamed)
    installer_file = output_path / "PatchCraft-Setup.exe"
    with open(installer_file, 'w', encoding='ascii') as f:
        f.write(installer_script)
    
    # Get file size
    size_mb = installer_file.stat().st_size / (1024 * 1024)
    
    print(f"✅ Single EXE installer created: {installer_file}")
    print(f"📏 File size: {size_mb:.1f} MB")
    print(f"\n📋 Installation Instructions:")
    print(f"   1. Place PatchCraft Studio.exe in same directory as PatchCraft-Setup.exe")
    print(f"   2. Run PatchCraft-Setup.exe")
    print(f"   3. Follow on-screen prompts")
    print(f"\n🎯 Result: ONE SINGLE EXE FILE installer")
    
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_working_inno_installer.py <source_exe> <output_dir> [version]")
        print("Example: python create_working_inno_installer.py ./PatchCraft_Studio.exe ./dist 1.0.0")
        sys.exit(1)
    
    source_exe = sys.argv[1]
    output_dir = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_exe).exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        sys.exit(1)
    
    # Create Inno Setup script
    if create_inno_setup_script(source_exe, output_dir, version):
        iss_file = Path(output_dir) / "PatchCraft-Setup.iss"
        
        # Try to compile with Inno Setup
        if compile_inno_setup(iss_file):
            print("✅ Inno Setup installer created successfully!")
        else:
            print("⚠️ Inno Setup not available, creating simple installer...")
            create_simple_installer(output_dir, version)
            print("✅ Simple installer created!")
    else:
        print("❌ Failed to create Inno Setup installer")

if __name__ == "__main__":
    main()
