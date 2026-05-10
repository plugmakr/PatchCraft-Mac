#!/usr/bin/env python3
"""
Create Inno Setup single EXE installer that works on Windows
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
    
    # Create Inno Setup script (using string formatting to avoid f-string issues)
    inno_script = "[Setup]\n"
    inno_script += "AppId={{{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}}\n"
    inno_script += f"AppName=PatchCraft Studio\n"
    inno_script += f"AppVersion={version}\n"
    inno_script += "AppPublisher=AudiCode\n"
    inno_script += "AppPublisherURL=https://patchcraft.com\n"
    inno_script += "AppSupportPhone=support@patchcraft.com\n"
    inno_script += "AppSupportURL=https://patchcraft.com/support\n"
    inno_script += "AppUpdatesURL=https://patchcraft.com/updates\n"
    inno_script += "DefaultDirName={{pf}}\\PatchCraft\n"
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
    inno_script += 'Name: "desktopicon"; Description: "{{cm:CreateDesktopIcon}}"; GroupDescription: "{{cm:AdditionalIcons}}"; Flags: unchecked\n'
    
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
    inno_script += '''function GetUninstallString(): String;
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
'''
    
    # Create LICENSE.txt
    license_content = f'''PatchCraft Studio End User License Agreement v{version}
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
    
    # Create EULA.txt
    eula_content = f'''PATCHCRAFT STUDIO END USER LICENSE AGREEMENT v{version}
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
'''
    
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

def create_self_contained_installer(output_dir, version="1.0.0"):
    """Create self-contained installer without requiring Inno Setup"""
    
    print(f"Creating self-contained installer for PatchCraft v{version}")
    
    output_path = Path(output_dir)
    
    # Create a simple self-extracting installer using PowerShell
    ps_installer = f'''# PatchCraft Studio Self-Contained Installer v{version}
# AudiCode - Beta Testing

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Main Form
$form = New-Object System.Windows.Forms.Form
$form.Text = "PatchCraft Studio v{version} Installer"
$form.Size = New-Object System.Drawing.Size(500, 400)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false

# Title Label
$titleLabel = New-Object System.Windows.Forms.Label
$titleLabel.Text = "PatchCraft Studio v{version}"
$titleLabel.Font = New-Object System.Drawing.Font("Arial", 16, [System.Drawing.FontStyle]::Bold)
$titleLabel.Location = New-Object System.Drawing.Point(50, 20)
$titleLabel.Size = New-Object System.Drawing.Size(400, 30)
$form.Controls.Add($titleLabel)

# Company Label
$companyLabel = New-Object System.Windows.Forms.Label
$companyLabel.Text = "AudiCode - Beta Testing Only"
$companyLabel.Font = New-Object System.Drawing.Font("Arial", 10)
$companyLabel.Location = New-Object System.Drawing.Point(50, 55)
$companyLabel.Size = New-Object System.Drawing.Size(400, 20)
$form.Controls.Add($companyLabel)

# EULA Textbox
$eulaTextBox = New-Object System.Windows.Forms.TextBox
$eulaTextBox.Multiline = $true
$eulaTextBox.ScrollBars = "Vertical"
$eulaTextBox.Text = @"
PATCHCRAFT STUDIO END USER LICENSE AGREEMENT v{version}
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

By clicking "Install" you agree to these license terms.
"@
$eulaTextBox.Location = New-Object System.Drawing.Point(20, 90)
$eulaTextBox.Size = New-Object System.Drawing.Size(450, 180)
$form.Controls.Add($eulaTextBox)

# Install Button
$installButton = New-Object System.Windows.Forms.Button
$installButton.Text = "Install"
$installButton.Location = New-Object System.Drawing.Point(200, 280)
$installButton.Size = New-Object System.Drawing.Size(100, 30)
$installButton.Add_Click({{
    # Check if PatchCraft Studio.exe exists
    $currentDir = Get-Location
    $appExe = Join-Path $currentDir "PatchCraft Studio.exe"
    
    if (-not (Test-Path $appExe)) {{
        [System.Windows.Forms.MessageBox]::Show("PatchCraft Studio.exe not found! Please ensure it's in the same directory as this installer.", "Error", "OK", "Error")
        return
    }}
    
    # Install to Program Files
    $installDir = Join-Path $env:ProgramFiles "PatchCraft"
    
    try {{
        # Create installation directory
        if (-not (Test-Path $installDir)) {{
            New-Item -ItemType Directory -Path $installDir -Force
        }}
        
        # Copy application
        Copy-Item $appExe (Join-Path $installDir "PatchCraft Studio.exe") -Force
        
        # Create desktop shortcut
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut((Join-Path $env:USERPROFILE "Desktop\PatchCraft Studio.lnk"))
        $shortcut.TargetPath = Join-Path $installDir "PatchCraft Studio.exe"
        $shortcut.WorkingDirectory = $installDir
        $shortcut.Description = "PatchCraft Studio v{version} - Professional Sample Instrument Designer"
        $shortcut.Save()
        
        # Create Start Menu shortcut
        $startMenu = Join-Path $env:PROGRAMDATA "Microsoft\Windows\Start Menu\Programs"
        if (-not (Test-Path $startMenu)) {{
            New-Item -ItemType Directory -Path $startMenu -Force
        }}
        $shortcut = $shell.CreateShortcut((Join-Path $startMenu "PatchCraft Studio.lnk"))
        $shortcut.TargetPath = Join-Path $installDir "PatchCraft Studio.exe"
        $shortcut.WorkingDirectory = $installDir
        $shortcut.Save()
        
        # Add to PATH
        $currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
        $newPath = $currentPath + ";" + $installDir
        [Environment]::SetEnvironmentVariable("PATH", $newPath, "Machine")
        
        # Create registry entries
        $regPath = "HKLM:\SOFTWARE\PatchCraft"
        if (-not (Test-Path $regPath)) {{
            New-Item -Path $regPath -Force
        }}
        Set-ItemProperty -Path $regPath -Name "InstallPath" -Value $installDir
        Set-ItemProperty -Path $regPath -Name "Version" -Value "{version}"
        Set-ItemProperty -Path $regPath -Name "Company" -Value "AudiCode"
        
        [System.Windows.Forms.MessageBox]::Show("PatchCraft Studio v{version} has been successfully installed!`n`nInstallation Directory: $installDir`n`nLaunch Options:`n• Desktop shortcut: PatchCraft Studio`n• Start Menu: Programs > PatchCraft Studio`n• Direct run: $installDir\PatchCraft Studio.exe", "Installation Complete", "OK", "Information")
        
    }} catch {{
        [System.Windows.Forms.MessageBox]::Show("Installation failed: $($_.Exception.Message)", "Error", "OK", "Error")
    }}
    
    $form.Close()
}})
$form.Controls.Add($installButton)

# Cancel Button
$cancelButton = New-Object System.Windows.Forms.Button
$cancelButton.Text = "Cancel"
$cancelButton.Location = New-Object System.Drawing.Point(320, 280)
$cancelButton.Size = New-Object System.Drawing.Size(100, 30)
$cancelButton.Add_Click({{ $form.Close() }})
$form.Controls.Add($cancelButton)

# Show form
$form.ShowDialog()
'''
    
    # Save PowerShell installer
    ps_file = output_path / "PatchCraft-Installer.ps1"
    with open(ps_file, 'w', encoding='utf-8') as f:
        f.write(ps_installer)
    print(f"✅ Created PowerShell installer: {ps_file}")
    
    # Create batch wrapper to run PowerShell
    batch_wrapper = f'''@echo off
title PatchCraft Studio v{version} Installer
echo PatchCraft Studio v{version} - AudiCode Beta Testing
echo.
echo Starting installer...
echo.

REM Check if PatchCraft Studio.exe exists
if not exist "PatchCraft Studio.exe" (
    echo ERROR: PatchCraft Studio.exe not found!
    echo Please ensure PatchCraft Studio.exe is in the same directory
    echo as this installer.
    echo.
    pause
    exit /b 1
)

REM Run PowerShell installer
powershell -ExecutionPolicy Bypass -File "PatchCraft-Installer.ps1"

echo.
echo Installer closed.
pause
'''
    
    # Save batch wrapper
    batch_file = output_path / "PatchCraft-Setup.bat"
    with open(batch_file, 'w', encoding='ascii') as f:
        f.write(batch_wrapper)
    print(f"✅ Created batch wrapper: {batch_file}")
    
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_inno_installer_fixed.py <source_exe> <output_dir> [version]")
        print("Example: python create_inno_installer_fixed.py ./PatchCraft_Studio.exe ./dist 1.0.0")
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
            print("⚠️ Inno Setup not available, creating self-contained installer...")
            create_self_contained_installer(output_dir, version)
            print("✅ Self-contained installer created!")
    else:
        print("❌ Failed to create Inno Setup installer")

if __name__ == "__main__":
    main()
