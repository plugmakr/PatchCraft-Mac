#!/usr/bin/env python3
"""
Create simple distribution package: main app exe, EULA, and README
"""

import os
import sys
from pathlib import Path

def create_simple_distribution(source_exe, output_dir, version="1.0.0"):
    """Create simple distribution package with main app exe, EULA, and README"""
    
    print(f"Creating simple distribution package for PatchCraft v{version}")
    
    # Check source executable
    source_path = Path(source_exe)
    if not source_path.exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        return False
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    # Copy main application executable
    dest_exe = output_path / "PatchCraft Studio.exe"
    import shutil
    shutil.copy2(source_path, dest_exe)
    print(f"✅ Copied PatchCraft Studio.exe ({source_path.stat().st_size / 1024 / 1024:.1f} MB)")
    
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
    with open(eula_file, 'w', encoding='utf-8') as f:
        f.write(eula_content)
    print(f"✅ Created EULA.txt")
    
    # Create README
    readme_content = f'''PatchCraft Studio v{version} - Installation Guide
================================================

INSTALLATION:

SIMPLE INSTALLATION:
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

LAUNCH OPTIONS:
- Direct run: C:\\Program Files\\PatchCraft\\PatchCraft Studio.exe
- Create desktop shortcut to PatchCraft Studio.exe

SUPPORT:
Email: support@patchcraft.com
Website: https://patchcraft.com

COMPANY:
AudiCode
Professional Audio Software Development
https://patchcraft.com

VERSION INFORMATION:
Version: {version}
Release: Beta Testing Only
Build Date: {os.path.getmtime(source_exe)}

FILE INFORMATION:
- PatchCraft Studio.exe: Main application
- EULA.txt: End User License Agreement
- README.txt: This installation guide

Thank you for testing PatchCraft Studio!
'''
    
    readme_file = output_path / "README.txt"
    with open(readme_file, 'w', encoding='utf-8') as f:
        f.write(readme_content)
    print(f"✅ Created README.txt")
    
    # Calculate total package size
    total_size = sum(f.stat().st_size for f in output_path.rglob("*") if f.is_file())
    size_mb = total_size / (1024 * 1024)
    
    print(f"\n📦 Simple Distribution Package Created")
    print(f"📍 Location: {output_path}")
    print(f"📏 Total Size: {size_mb:.1f} MB")
    print(f"\n📋 Package Contents:")
    print(f"   • PatchCraft Studio.exe (Main application)")
    print(f"   • EULA.txt (License agreement)")
    print(f"   • README.txt (Installation guide)")
    print(f"\n🎯 Ready for manual ZIP creation")
    
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python create_simple_distribution.py <source_exe> <output_dir> [version]")
        print("Example: python create_simple_distribution.py ./PatchCraft_Studio.exe ./dist 1.0.0")
        sys.exit(1)
    
    source_exe = sys.argv[1]
    output_dir = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    
    if not Path(source_exe).exists():
        print(f"ERROR: Source executable not found: {source_exe}")
        sys.exit(1)
    
    if create_simple_distribution(source_exe, output_dir, version):
        print("✅ Simple distribution package created successfully!")
    else:
        print("❌ Failed to create distribution package")

if __name__ == "__main__":
    main()
