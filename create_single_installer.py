#!/usr/bin/env python3
"""
Create single-file installer for PatchCraft beta distribution
"""

import os
import sys
import zipfile
import tempfile
from pathlib import Path

def create_single_installer(source_dir, output_path, version="1.0.0"):
    """Create a single-file installer package"""
    
    print(f"Creating single-file installer for PatchCraft v{version}")
    
    # Create temporary directory for packaging
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Copy installer files
        installer_files = {
            "PatchCraft Studio.exe": "PatchCraft Studio.exe",
            "README.txt": "README.txt", 
            "install.bat": "install.bat"
        }
        
        for src_name, dst_name in installer_files.items():
            src_path = Path(source_dir) / src_name
            if src_path.exists():
                dst_path = temp_path / dst_name
                import shutil
                shutil.copy2(src_path, dst_path)
                print(f"Copied {src_name}")
            else:
                print(f"Warning: {src_name} not found")
        
        # Create version info
        version_info = f"""PatchCraft Studio v{version}
============================

Installation Instructions:
1. Extract all files to a temporary folder
2. Run install.bat to install to Program Files
3. Desktop shortcut will be created automatically

Features:
- Multi-instrument layering (guitar + piano combined)
- Enhanced component library with drag-drop
- Visual modulation matrix
- SFZ import support
- Copy protection and trial system
- Preset randomization and A/B compare
- Computer keyboard input support
- Professional installer with desktop integration

System Requirements:
- Windows 10/11 (64-bit)
- 4GB RAM minimum
- Audio interface recommended
- VST3-compatible DAW for plugin use

For support visit: https://patchcraft.com
"""
        
        with open(temp_path / "INSTALL.txt", "w") as f:
            f.write(version_info)
        
        # Create the ZIP file
        output_file = Path(output_path)
        with zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for file_path in temp_path.rglob("*"):
                if file_path.is_file():
                    arcname = file_path.relative_to(temp_path)
                    zipf.write(file_path, arcname)
                    print(f"Added {arcname}")
        
        # Get file size
        size_mb = output_file.stat().st_size / (1024 * 1024)
        print(f"\nSingle-file installer created: {output_file}")
        print(f"Size: {size_mb:.1f} MB")
        
        return output_file

def create_self_extracting_installer(zip_path, output_path):
    """Create a self-extracting installer using Windows built-in tools"""
    
    print("Creating self-extracting installer...")
    
    # Create a simple self-extractor using Windows IExpress
    # This creates a single .exe that extracts and runs install.bat
    
    config_content = f"""[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%License%
FinishMessage=%FinishMessage%
TargetName=%TargetName%
FriendlyName=%FriendlyName%
AppLaunched=%AppLaunched%
PostInstallCmd=%PostInstallCmd%
AdminQuietInstCmd=%AdminQuietInstCmd%
UserQuietInstCmd=%UserQuietInstCmd%
SourceFiles=SourceFiles
[Strings]
InstallPrompt=Installing PatchCraft Studio...
License=
FinishMessage=Installation complete!
TargetName=PatchCraftInstaller.exe
FriendlyName=PatchCraft Studio
AppLaunched=install.bat
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
[SourceFiles]
%SourceFiles%
"""
    
    # Write config file
    config_path = Path("installer_config.sed")
    with open(config_path, "w") as f:
        f.write(config_content)
    
    try:
        # Use IExpress to create self-extracting exe
        import subprocess
        result = subprocess.run([
            "iexpress", "/N", "/Q", config_path
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            print("Self-extracting installer created successfully!")
            return True
        else:
            print(f"IExpress failed: {result.stderr}")
            return False
            
    except Exception as e:
        print(f"Could not create self-extracting installer: {e}")
        return False
    finally:
        if config_path.exists():
            config_path.unlink()

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_single_installer.py <installer_dir> <output_file> [version]")
        sys.exit(1)
    
    installer_dir = sys.argv[1]
    output_file = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(installer_dir).exists():
        print(f"Installer directory not found: {installer_dir}")
        sys.exit(1)
    
    # Create ZIP installer
    zip_file = create_single_installer(installer_dir, output_file, version)
    
    # Try to create self-extracting version
    exe_file = Path(output_file).with_suffix(".exe")
    if create_self_extracting_installer(zip_file, exe_file):
        print(f"\nBoth installers created:")
        print(f"  ZIP: {zip_file}")
        print(f"  EXE: {exe_file}")
    else:
        print(f"\nZIP installer created: {zip_file}")
        print("Self-extracting installer requires Windows IExpress")

if __name__ == "__main__":
    main()
