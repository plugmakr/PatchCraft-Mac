# PatchCraft Studio - macOS Version

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
