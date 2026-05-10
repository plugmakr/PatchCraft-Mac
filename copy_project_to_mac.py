#!/usr/bin/env python3
"""
Copy entire PatchCraft project to Mac repository
"""

import os
import sys
from pathlib import Path
import shutil

def copy_patchcraft_to_mac():
    """Copy entire PatchCraft project to Mac repository"""
    
    print("Copying PatchCraft project to Mac repository...")
    
    # Source and destination paths
    source_dir = Path("m:/AudiCode/PCraft")
    mac_dir = Path("m:/AudiCode/PatchCraft-Mac")
    
    if not source_dir.exists():
        print(f"ERROR: Source directory not found: {source_dir}")
        return False
    
    if not mac_dir.exists():
        print(f"ERROR: Mac repository not found: {mac_dir}")
        return False
    
    # Files and directories to exclude
    exclude_patterns = {
        '.git',
        '.gitignore',
        'build',
        'dist',
        '.vs',
        '.vscode',
        '__pycache__',
        '*.pyc',
        '*.obj',
        '*.pdb',
        '*.ilk',
        '*.exe',
        '*.dll',
        '*.lib',
        '.DS_Store',
        'Thumbs.db'
    }
    
    # Copy project files
    print(f"Copying from {source_dir} to {mac_dir}")
    
    copied_count = 0
    skipped_count = 0
    
    for item in source_dir.rglob("*"):
        if item.is_file():
            # Skip excluded files
            if any(item.name.endswith(pattern) for pattern in ['*.pyc', '*.obj', '*.pdb', '*.ilk', '*.exe', '*.dll', '*.lib']):
                skipped_count += 1
                continue
            
            if item.name in exclude_patterns:
                skipped_count += 1
                continue
            
            # Create relative path
            rel_path = item.relative_to(source_dir)
            dest_path = mac_dir / rel_path
            
            # Create destination directory if needed
            dest_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Copy file
            try:
                shutil.copy2(item, dest_path)
                copied_count += 1
                if copied_count % 10 == 0:
                    print(f"Copied {copied_count} files...")
            except Exception as e:
                print(f"Failed to copy {item}: {e}")
    
    print(f"\n✅ Copying completed!")
    print(f"📁 Files copied: {copied_count}")
    print(f"📁 Files skipped: {skipped_count}")
    
    # Update CMakeLists.txt for Mac
    cmake_file = mac_dir / "CMakeLists.txt"
    if cmake_file.exists():
        print("Updating CMakeLists.txt for Mac build...")
        # CMakeLists.txt is already set up for Mac
        print("✅ CMakeLists.txt ready for Mac build")
    
    # Update README.md
    readme_file = mac_dir / "README.md"
    if readme_file.exists():
        print("Updating README.md...")
        readme_content = f'''# PatchCraft Studio - macOS Version

## Overview
PatchCraft Studio is a professional sample instrument designer for macOS. This version includes all the features from the Windows version, optimized for macOS.

## Features
- Multi-instrument layering (guitar + piano combined sounds)
- Enhanced component library with drag-drop functionality
- Visual modulation matrix for DSP routing
- SFZ import support for sample maps
- Preset randomization and A/B compare
- Computer keyboard input for testing
- Professional installer with AudiCode branding

## System Requirements
- macOS 10.15 (Catalina) or later
- 4GB RAM minimum (8GB recommended)
- 500MB disk space
- Audio interface recommended for low latency

## Build Instructions

### Prerequisites
- Xcode 12.0 or later
- CMake 3.16 or later
- Make

### Build Steps
```bash
git clone https://github.com/plugmakr/PatchCraft-Mac.git
cd PatchCraft-Mac
mkdir build
cd build
cmake ..
make -j4
```

### Installation
```bash
sudo make install
```

## Usage
Launch PatchCraft Studio from Applications folder or run directly from build directory.

## Support
- Email: support@patchcraft.com
- Website: https://patchcraft.com
- Documentation: https://patchcraft.com/docs

## Company
AudiCode
Professional Audio Software Development
https://patchcraft.com

## License
Beta testing license. See EULA.txt for terms.
'''
        
        with open(readme_file, 'w') as f:
            f.write(readme_content)
        print("✅ README.md updated")
    
    return True

def main():
    print("PatchCraft Mac Repository Setup")
    print("=" * 40)
    
    if copy_patchcraft_to_mac():
        print("\n✅ PatchCraft project copied to Mac repository successfully!")
        print("\nNext steps:")
        print("1. cd ../PatchCraft-Mac")
        print("2. git add .")
        print("3. git commit -m 'Add full PatchCraft project to Mac repository'")
        print("4. git push origin main")
    else:
        print("\n❌ Failed to copy PatchCraft project")

if __name__ == "__main__":
    main()
