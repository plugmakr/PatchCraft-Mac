# PatchCraft `.pcexp` Extension System

PatchCraft extensions extend the application itself. They are separate from sellable instrument expansion packs, which contain presets, samples, patches, and artwork for a specific instrument.

## Package Types

- `.pcexp` application extension: exporters, script runtimes, synth modules, DSP modules, validators, workflow tools, templates, and integrations.
- Patch/instrument expansion pack: sellable content made inside PatchCraft and exported for Player/instruments.
- PatchCraft VST Expansion: paid exporter package that installs the standalone VST3 export templates and advertises `export.vst3`.

## Install Locations

PatchCraft scans:

- User extensions: `%APPDATA%/PatchCraft/Extensions`
- Bundled extensions: `PatchCraftStudio.exe/../Extensions`
- Built-in modules: compiled into PatchCraft, starting with `pScript Core`

Installers can either:

- Copy an extracted extension folder into the user/bundled extension folder.
- Install a `.pcexp` archive through PatchCraft Settings.

## `.pcexp` Layout

A `.pcexp` archive is a ZIP file renamed to `.pcexp`. It must contain a top-level `manifest.json`.

```text
AdvancedSynth.pcexp
  manifest.json
  Modules/
  Templates/
  Presets/
  Docs/
  Assets/
```

Folder-based development packages are also supported:

```text
AdvancedSynth.pcexp/
  manifest.json
  Modules/
  Templates/
```

## Manifest Contract

```json
{
  "format": "PatchCraft Extension",
  "formatVersion": 1,
  "id": "com.patchcraft.advanced-synth",
  "name": "Advanced Synth",
  "version": "1.0.0",
  "kind": "synth-module-pack",
  "author": "PatchCraft",
  "description": "Advanced oscillators, modulation sources, and DSP blocks.",
  "minPatchCraftVersion": "0.1.0",
  "capabilities": [
    "synth.oscillator.additive",
    "synth.oscillator.physical-model",
    "dsp.filter.morphing-bank"
  ],
  "dependencies": [
    "com.patchcraft.pscript.core>=1.0.0"
  ],
  "license": {
    "mode": "external",
    "productId": "advanced-synth",
    "endpoint": "https://license.example.com/activate"
  },
  "tags": ["synth", "oscillator", "dsp"]
}
```

## License Modes

- `built-in`: compiled into PatchCraft and always available.
- `none`: installed extension is ready immediately.
- `local-key`: validates a local key against the expansion product id.
- `external`: expects an external entitlement/licensing service and stores the entitlement state locally.

The first built-in expansion is:

```text
com.patchcraft.pscript.core
Capabilities:
  script.pscript
  script.events
  script.macros
  script.midi
  script.ui
  script.samplemap
  script.validation
```

## Capability Naming

Use stable, namespaced capability ids:

```text
export.vst3
script.runtime.javascript
script.runtime.lua
synth.oscillator.wavetable-pro
synth.oscillator.granular
dsp.eq.dynamic
dsp.fx.spectral
workflow.oneshot.batch-render
```

PatchCraft features should check capabilities rather than hard-coding expansion names.

## Paid VST Expansion

The base PatchCraft installer ships Studio plus the Player/Player FX runtime plugins. It does **not** ship the standalone VST3 export templates.

The separate `PatchCraftVstExpansionPackage` build target creates:

```text
PatchCraftVstExpansion/
  PluginTemplate/
    PatchCraft Player.vst3
    PatchCraft Player FX.vst3
  Extension/PatchCraftVstExpansion/manifest.json
  installer/PatchCraftVstExpansion-Windows.iss
  installer/PatchCraftVstExpansion-macOS-notes.md
```

The addon manifest uses:

```text
id: com.patchcraft.vst-expansion
capabilities:
  export.vst3
  white_label.vst3
  pluginclub.publish.vst3
```

Studio treats standalone VST3 export as locked unless the addon has installed `PluginTemplate/` beside the Studio executable or the development build-tree templates are present.

## Implementation Status

- `PcexpManager` validates, installs, scans, enables/disables, and license-checks `.pcexp` packages.
- Settings now has an `Extensions` tab for scanning, folder access, and install.
- `pScript Core` is built in and not treated as a removable extension.
- VST Expansion is separated from the base installer and packaged by `PatchCraftVstExpansionPackage`.
- JavaScript, Lua, and other language runtimes should be `.pcexp` add-ons that provide capabilities like `script.runtime.javascript`.
