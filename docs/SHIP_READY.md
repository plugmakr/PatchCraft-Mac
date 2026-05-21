# PatchCraft Ship-Ready Handoff

Date: 2026-05-21

## Release Candidate

- RC bundle: `build-codex/dist/PatchCraftStudio-RC`
- Studio executable: `build-codex/dist/PatchCraftStudio-RC/PatchCraftStudio.exe`
- Player VST3 runtime: `build-codex/dist/PatchCraftStudio-RC/PlayerPlugins/PatchCraft Player.vst3`
- Player FX VST3 runtime: `build-codex/dist/PatchCraftStudio-RC/PlayerPlugins/PatchCraft Player FX.vst3`
- Paid VST Expansion package: `build-codex/dist/PatchCraftVstExpansion`
- DAW QA checklist: `docs/DAW_QA_CHECKLIST.md`
- Factory demos: `build-codex/dist/PatchCraftStudio-RC/FactoryDemos`
- Creator libraries: `build-codex/dist/PatchCraftStudio-RC/Library`
- AI Studio expansion plan: `docs/AI_STUDIO_EXPANSION_PLAN.md`

## Validated Commands

Run from the repository root:

```powershell
cmake -S . -B build-codex -DPATCHCRAFT_ENABLE_AI_STUDIO=OFF
cmake --build build-codex --target PatchCraftReleaseChecklist --config Release --parallel 6
build-codex\bin\Release\PatchCraftAudioSmokeTests.exe
build-codex\bin\Release\PatchCraftVstExportSmokeTest.exe
cmake --build build-codex --target PatchCraftRcBundle --config Release --parallel 6
cmake --build build-codex --target PatchCraftVstExpansionPackage --config Release --parallel 6
```

## Current Ship Scope

- Studio can author and export PatchCraft packs, one-shot packs, Player VST3 products, Player FX VST3 products, and Plugin.club publish payloads.
- Standalone branded VST3 export is intentionally separated into the paid PatchCraft VST Expansion package.
- AI Studio is intentionally hidden from launch builds with `PATCHCRAFT_ENABLE_AI_STUDIO=OFF`; the first paid addon for launch is the VST3 Exporter.
- Launch licensing uses Plugin.club endpoints. AudiLock is the planned licensing source of truth and should preserve the same product ID, activation URL, public key, trial, offline grace, and bind-to-machine fields when it replaces Plugin.club.
- Player can load embedded/exported packs, render custom layouts, switch tabbed containers, play MIDI, expose MIDI Learn, and host multi-instrument packs.
- Player includes commercial runtime workspaces for Rack, Mixer, Snapshots/Favorites, Sound DNA, user imports, control center, and branded support/about flows.
- Multi-instrument Player UI now includes a collapsible layer dock with volume, pan, mute, and solo controls.
- Sample Mapper includes explicit `Select All` next to `Auto Trim`; Auto Trim can process selected zones or every zone if nothing is selected.
- One Shot Pack Library `Load Pack` opens an in-app audition modal and mutes Studio preview so the pack is the active audition source.
- Sample playback uses zone BPM metadata when `bpmSync` is enabled, using basic tempo-ratio resampling against host BPM.

## Manual QA Still Required

These cannot be fully proven by headless smoke tests:

- Open Studio, select the real audio/MIDI devices, and confirm hardware keyboard note-on, mod wheel, expression, sustain, and pad triggers.
- Drag WAV/AIFF/FLAC samples and MID/MIDI files onto the standalone Player and verify import, audition, pad/keyboard mapping, and session reset behavior.
- Drag WAV/AIFF/FLAC samples and MID/MIDI files onto Player VST3 and Player FX VST3 inside FL Studio; verify the DAW forwards drag/drop to the plugin editor and imported content plays immediately.
- Export at least one sample instrument, one synth instrument, one drum machine, one FX plugin, and one multi-instrument pack.
- Load PatchCraft Player and Player FX in FL Studio and verify labels, tab switching, MIDI Learn, drum grid cells, pad playback, preset loading overlay, mixer faders, mute/solo, and LED meter response.
- Install the separate VST Expansion package, then export/load one standalone branded VST3 product in FL Studio.
- Publish a Plugin.club draft using `https://plugin.club/functions/sellerImport`, license through `https://plugin.club/functions/deviceAuth`, and verify the returned draft/edit URL.
- Confirm the final base installer preserves `FactoryDemos`, `Library`, and `PlayerPlugins` beside the Studio executable.
- Confirm the paid VST Expansion installer adds `PluginTemplate` beside the Studio executable without changing the base install.
- Confirm Launch Bundle includes `seller-launch-playbook.md`, `buyer-quick-start.md`, and `marketplace-asset-checklist.md`.

## Ship Readiness Gates

### P0 - Must Pass Before Release

- Player VST3 and Player FX VST3 load in FL Studio without layout drift, missing text, tab-switch failures, or silent output.
- Preset switching displays an obvious loading state and never leaves the Player in a partially loaded sound state.
- Runtime sample/MIDI import works through the Player import panel and drag/drop in standalone and DAW plugin contexts.
- Mixer controls affect the correct instrument/layer paths: volume, pan, mute, solo, LED metering, and global output.
- User snapshots save, recall, favorite, delete, and restore with the DAW session.
- Sound DNA accurately shows active blocks, modulation, macros, automations, samples, and live exposed parameter values.
- Exported sample, synth, drum, FX, and multi-instrument products reload exactly as authored in Studio and Brand Lab.
- Plugin.club publish creates a valid seller draft with artwork, metadata, licensing fields, package archive, and edit URL.
- Licensing setup is clear in Launch/Settings for the current Plugin.club backend and future AudiLock handoff; exported Players fail gracefully when licensing is misconfigured.
- RC bundle/installer installs Studio, factory demos, creator libraries, and Player runtime plugins into predictable locations.
- VST Expansion installer installs standalone export templates separately.

### P1 - Strongly Recommended Before Public Sale

- Run the repeatable FL Studio checklist in `docs/DAW_QA_CHECKLIST.md` and capture screenshots for Player, Player FX, sample drag/drop, tab switching, MIDI Learn, and multi-instrument routing.
- Add a demo-pack audit pass: every shipped demo needs unique sound, functional tabs, no overlapping text, and no missing parameter warnings.
- Add a dedicated runtime-import demo test: sample import, MIDI import, imported pads, imported keyboard zones, playback display, and clear/reset.
- Add a final UI pass for modals, long file names, dropdowns, property accordions, and small-display scaling.
- Add a signed installer path for writing optional VST3 components to protected system folders without requiring Studio to run elevated.

## Known Post-RC Work

- Multi-instrument authoring still needs a deeper Studio workflow for layer creation, ordering, per-layer DSP sends, and layered preset/expansion authoring.
- BPM sync is currently basic resampling, not phase-vocoder/time-stretch quality.
- A production installer/signing pipeline is generated as an Inno Setup starter script, but final code signing and notarization remain release operations.
