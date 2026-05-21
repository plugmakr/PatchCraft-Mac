# Standalone One-Shot + Loop Pack App Handoff

This document is intentionally separate from the PatchCraft roadmap. The product described here is a standalone commercial app focused only on creating, editing, packaging, and distributing one-shot and loop packs.

## Product Intent

Build a focused creator tool for sound designers, producers, and sample-pack sellers. The app should make it easy to capture sounds from plugins or audio imports, edit them cleanly, organize them into commercial packs, add artwork/metadata/licensing, and publish or export sellable bundles.

Working product name options:

- PackForge
- OneShot Foundry
- SamplePack Studio
- LoopForge

## Core Promise

The app should feel like SampleRobot, Loopcloud prep tools, a clean sampler editor, and a seller dashboard combined into one focused workflow:

1. Capture or import sounds.
2. Clean and edit the audio.
3. Build one-shot or loop packs.
4. Add commercial metadata, artwork, tags, and license files.
5. Export or publish the finished pack.

## Primary Workflows

### One-Shot Capture

- Load a VST3/AU instrument.
- Open the plugin editor in a docked or floating window.
- Select a preset inside the plugin.
- Play the plugin live from MIDI hardware or the software keyboard.
- Choose a capture template such as `C0-C6 Chromatic`, octave-only, root-only, or drum pads.
- Render WAV files with options for velocity, note length, tail, normalize, trim leading silence, fade-in, and fade-out.
- Preview every rendered file before packaging.

### Loop Capture

- Load a VST3/AU instrument or import existing loops.
- Set BPM, bars, time signature, key, scale, and loop type.
- Render exact-length loops with optional tail capture and seamless-loop crossfade.
- Detect tempo/key when importing audio.
- Validate loop length against BPM and bars.
- Export loops with consistent musical names and metadata.

### Audio Editing

- Waveform view with zoom, playhead, loop region, start/end handles, and fade handles.
- Batch trim leading/trailing silence.
- Batch normalize, gain-match, fade, reverse, convert bit depth/sample rate.
- Zero-crossing snap for clean one-shots.
- Loop crossfade preview.
- Per-file tags: key, BPM, root note, category, mood, instrument, style.

### Pack Builder

- Create pack metadata: name, creator, brand, category, version, description, keywords.
- Add artwork: cover image, banner, thumbnails, optional product screenshots.
- Organize by folders/groups: Drums, Bass, Keys, FX, Loops, MIDI, Bonus.
- Define naming schemes:
  - `Pack_025_C#0.wav`
  - `001_C#0_Pack.wav`
  - `Pack_BPM_Key_Category_001.wav`
- Generate a `pack.json` manifest for marketplaces and future automation.
- Add license text, readme, install notes, and attribution.

### Distribution

- Export folder bundle.
- Export ZIP bundle.
- Export marketplace-ready bundle.
- Publish directly to a seller API such as Plugin.club when credentials are configured.
- Optional download/license metadata for protected pack delivery.

## MVP Scope

The first standalone build should include:

- VST3 plugin loading with floating editor window.
- Live MIDI audition through hardware and software keyboard.
- One-shot render templates.
- Loop render templates.
- Waveform editor with trim/fade/normalize.
- Pack metadata editor.
- Artwork selector and copier.
- Folder and ZIP export.
- Pack manifest JSON.
- Local pack library.

## Advanced Scope

Add after the MVP is stable:

- AU support on macOS.
- Batch plugin preset capture.
- Auto-generate pack names/descriptions/tags.
- Auto-key and BPM detection.
- Loudness matching and peak/RMS targets.
- Loop seam analyzer.
- Drum one-shot classifier.
- Direct marketplace publishing.
- License-key integration.
- User templates for naming, folder structures, metadata, and artwork sizes.

## Technical Direction

Use the same proven framework concepts as the PatchCraft One Shot Maker, but keep this app separate:

- JUCE desktop app.
- Plugin hosting module.
- Shared audio render engine concepts.
- Independent app identity, settings, project format, and export format.
- No dependency on PatchCraft instrument projects, DSP Builder, Player, or VST export.

Suggested project file:

- `.packforgeproject` folder containing source plugin references, render templates, rendered audio, artwork, pack metadata, and export history.

Suggested export format:

- `PackName/`
- `PackName/Audio/One Shots/`
- `PackName/Audio/Loops/`
- `PackName/Artwork/`
- `PackName/Metadata/pack.json`
- `PackName/README.txt`
- `PackName/LICENSE.txt`

## Commercial Requirements

- Every exported pack should be reproducible from a saved project.
- Every file should carry clear metadata.
- Every filename should be predictable and sortable.
- The app should prevent silent export failures.
- The app should warn about missing artwork, missing license, clipped files, bad loop lengths, empty renders, and duplicate names.
- The user should be able to create a sellable pack without leaving the app.

## UX Principles

- Focused, not overloaded.
- Always show where the user is in the pack-building flow.
- Keep capture, edit, package, and publish as separate stages.
- Make expert options available without forcing them on beginners.
- Every export should have a clear health check before delivery.

## Open Decisions

- Final product name.
- Whether Plugin.club is the first publishing target.
- Whether cloud AI is included at launch or added later.
- Whether AU support ships in v1 or follows VST3.
- Whether pack licensing is local-only metadata first or integrated with an external platform at launch.
