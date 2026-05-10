# PatchCraft

PatchCraft is a two-part platform for designing and selling sample
instruments:

- **PatchCraft Studio** — JUCE C++ standalone desktop app for visually
  designing instrument GUIs, mapping samples, building custom knobs and
  exporting `.patchcraft` instrument packs.
- **PatchCraft Player** — JUCE C++ VST3 instrument plugin that loads
  `.patchcraft` packs at runtime, renders the custom UI, and plays the
  mapped samples. The same plugin binary can host any pack — creators do
  not recompile the plugin per instrument.

This repository is the MVP scaffold: full UI for the Studio, working
sample mapping, knob filmstrip exporter, JSON-based pack format,
runtime UI rendering, and a polyphonic sampler engine shared by both
the Studio preview and the Player plugin.

## Repository layout

```
PCraft/
  CMakeLists.txt
  README.md
  Source/
    Shared/      # core data + audio engine (PatchCraftCore)
    Studio/     # standalone authoring app
    Player/     # VST3 instrument plugin
  Templates/
    DefaultInstrument/   # JSON template a new project starts from
  Examples/
    CinematicPad.patchcraft/  # ready-to-load example pack
```

## Prerequisites

- **CMake 3.22+**
- **A JUCE source tree** (8.x recommended). By default the build looks
  for `../JUCE` next to this folder. You can override with
  `-DPATCHCRAFT_JUCE_PATH=/path/to/JUCE`.
- A C++17 compiler — MSVC (Visual Studio 2022), Xcode, or recent
  GCC/Clang.

## Build

```sh
# From the PCraft folder
cmake -B build -DPATCHCRAFT_JUCE_PATH=../JUCE
cmake --build build --config Release
```

By default both the Studio app and the Player VST3 are built. Disable
either with `-DPATCHCRAFT_BUILD_STUDIO=OFF` or `-DPATCHCRAFT_BUILD_PLAYER=OFF`.

Build outputs:

- Studio standalone: `build/PatchCraftStudio_artefacts/Release/PatchCraft Studio.exe`
- Player VST3:        `build/PatchCraftPlayer_artefacts/Release/VST3/PatchCraft Player.vst3`

## Studio quick tour

The Studio window opens at 1600×1000 with a layout that mirrors the
reference design:

- **Top toolbar** — `New`, `Open`, `Save`, `Import Samples`, `Import BG`,
  `AI Assist`, then the accent `Preview` and `Export Pack` buttons.
- **Left sidebar** — `Elements` (Knob, Slider, Button, Toggle, Dropdown,
  Label, Value Display, Meter, Waveform, Keyboard, Panel, Image, XY Pad)
  and `Layers` for z-order manipulation.
- **Centre canvas** — rulers, grid, draggable / resizable layout
  elements rendered with the same drawing routines used at runtime.
- **Right inspector** — Type, ID, Position, Size, Parameter, Label,
  Value Format, Style, Knob Style, Min, Max, Default, Step, Value Type,
  Smoothing + Duplicate / Delete / Forward / Backward.
- **Bottom workspace** — four side-by-side sections:
  - Sample Mapper (with `Sample Mapper` / `Keyzones` / `Velocity` tabs).
  - Parameters list.
  - Knob / Slider / Meter Builder (tabbed). The Knob Builder has a
    live preview and exports a vertical PNG filmstrip.
  - Preset browser with default-marker star.
- **Status bar** — CPU, voice count, sample rate, project name, last saved.

### Creating an instrument

1. `New` resets to the default Cinematic Evolve Pad scaffold.
2. `Import Samples` brings WAVs into the sample map. Drag zone edges in
   `Sample Mapper` (or edit the table fields in `Keyzones`).
3. Drop knobs/sliders onto the canvas from the **Elements** sidebar,
   then bind them to parameters in the **Inspector**.
4. Use the **Knob Builder** to design a knob style and click
   `Export Knob...` to write a filmstrip PNG.
5. `Save` writes a `.patchcraftproject` folder.
6. `Export Pack` writes a runtime `.patchcraft` folder (manifest +
   layout + mappings + parameters + presets + assets + samples).

## Player VST3 quick tour

1. Open your DAW and load **PatchCraft Player** as an instrument.
2. The empty splash shows the PATCHCRAFT logo and a
   `Load PatchCraft Instrument` button.
3. Pick any `.patchcraft` folder (e.g. `Examples/CinematicPad.patchcraft`).
4. The plugin renders the custom UI from `layout.json`, binds knobs to
   APVTS parameters, and plays the mapped samples from MIDI.
5. Click the `Pack` button to swap or unload the instrument without
   removing the plugin from your project.

## Pack format (v1)

A `.patchcraft` pack is a folder. MVP builds use plain folders for
debuggability. Each pack contains:

```
MyInstrument.patchcraft/
  manifest.json     # name, creator, category, default preset, etc.
  layout.json       # canvas size + layout elements
  mappings.json     # sample zones (root, low/high note, velocity, gain, pan, loop)
  parameters.json   # parameter definitions (id, range, default, unit)
  presets.json      # preset list with parameter values
  assets/
    background.png
    knobs/  sliders/  meters/  images/
  samples/
    *.wav
```

See `Templates/DefaultInstrument/` and `Examples/CinematicPad.patchcraft/`
for canonical examples.

## MVP limitations

The MVP focuses on the core authoring + playback pipeline. Not yet
implemented (architecture notes / TODOs left throughout the source):

- encrypted / compressed packs
- license validation
- creator marketplace
- white-label / branded Player builder
- cloud preset library
- AI image generation API
- AI auto-mapping / auto-tagging
- AAX format
- a full Kontakt-level modulation/scripting system

These features are intentionally out of scope for v0.1 — the priorities
were (1) the visual builder feels right, (2) packs round-trip cleanly,
(3) the Player loads any pack without recompiling, and (4) the audio
engine is RT-safe.

## License

Internal — see header files for per-source license terms.
