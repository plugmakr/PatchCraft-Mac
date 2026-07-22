# PatchCraft Studio — macOS

Synced from [Patchcraft-2](https://github.com/plugmakr/Patchcraft-2) branch **`PatchCraftRF`** for macOS builds.

## Requirements

- macOS 10.15+ (Apple Silicon or Intel)
- Xcode 14+ (Command Line Tools)
- CMake 3.22+
- Ninja (recommended) or Xcode generator

## Build (Studio + Player)

```bash
git clone https://github.com/plugmakr/PatchCraft-Mac.git
cd PatchCraft-Mac
git checkout PatchCraftRF
git pull

cmake -S . -B build-mac \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPATCHCRAFT_ENABLE_AI_STUDIO=OFF

cmake --build build-mac --target PatchCraftStudio PatchCraftPlayer PatchCraftPlayerFX -j$(sysctl -n hw.ncpu)
```

Apps / plugs land under `build-mac/`:

- `PatchCraftStudio_artefacts/Release/PatchCraft Studio.app`
- `PatchCraftPlayer_artefacts/Release/VST3/PatchCraft Player.vst3`
- `PatchCraftPlayerFX_artefacts/Release/VST3/PatchCraft Player FX.vst3`

Xcode generator alternative:

```bash
cmake -S . -B build-xcode -G Xcode -DPATCHCRAFT_ENABLE_AI_STUDIO=OFF
cmake --build build-xcode --config Release --target PatchCraftStudio
```

## Notes

- This tree is the **same product code** as Windows `PatchCraftRF`. CMake has `APPLE` `.app` naming support; packaging/notarization is still a separate Mac release step.
- Plugin.club Device Auth + `sellerImport` / `activateLicense` are included — see `docs/PLUGIN_CLUB_PUBLISH_CONTRACT.md`.
- Prefer building on a real Mac; Windows-side sync only updates sources.

## Sync source

Windows: `M:/AudiCode/PCraft` → this repo, branch `PatchCraftRF`.
