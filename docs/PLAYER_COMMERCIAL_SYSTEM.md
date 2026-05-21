# PatchCraft Commercial Player System

Date: 2026-05-21

## Goal

The exported Player should feel like a premium commercial instrument shell that developers can sell to artists, labels, producers, composers, and white-label clients.

PatchCraft Studio authors the sound, layout, branding, installer metadata, sales page, Plugin.club draft metadata, and buyer handoff materials. The Player is the customer-facing product.

## Player Features

### Premium Top Shell

- `F` opens pack/file actions.
- `LIB` opens the branded library browser when external loading is allowed.
- `VIEW` controls visible Player workspaces.
- `TOOL` opens performance, rack, routing, MIDI, snapshots, Sound DNA, and import workflows.
- `PLAY/STOP` auditions internal drum/MIDI patterns when the host transport is stopped.
- `SND` opens the runtime Sound Control Center.
- `RACK` opens multi-instrument layer management.
- `CTRL` opens product info, routing, mixer, and MIDI controls.
- `SNAP` opens user snapshots and favorites.
- `DNA` opens the readable signal-chain formula.

### Instrument Rack

The Rack turns the Player into a Kontakt-style multi-instrument workspace without copying Kontakt.

- Per-layer on/off, mute, solo.
- MIDI channel splits.
- Output route selection.
- Tuning/transposition.
- Per-layer volume and pan.
- Global and per-layer mixer rows.
- Host-session persistence for rack state.

### Snapshots And Favorites

End users can capture useful sound states without modifying protected factory content.

- Save current parameter state as a user snapshot.
- Recall a selected snapshot.
- Mark snapshots as favorites.
- Delete snapshots.
- Snapshots are stored in the host/plugin state so a DAW project reloads the buyer's edits.

### Sound DNA

Sound DNA is the buyer/developer clarity layer. It shows what is actually making the sound.

- Active block counts by module: Source, Filter, Amp, Mod, FX, Out.
- Total blocks, modulation routes, macros, automations, sample zones.
- Signal formula list with active blocks and sections.
- Live parameter values for the most important exposed controls.

This is a PatchCraft differentiator: buyers can understand the instrument instead of guessing what a knob does.

## Sales And Handoff System

Launch Center now generates a fuller commercial delivery package:

- `sales-page.md`
- `sales-page.html`
- `product-copy.md`
- `readiness-report.md`
- `runtime-test-plan.md`
- `installer-checklist.md`
- `client-delivery.md`
- `customer-package-wizard.md`
- `seller-launch-playbook.md`
- `buyer-quick-start.md`
- `marketplace-asset-checklist.md`
- `pluginclub-metadata-preview.json`
- `release-manifest.json`
- `installer/white-label-product.json`
- `installer/windows-inno-setup.iss`
- `installer/macos-pkgbuild-notes.md`
- `installer/activation-flow.md`

## Commercial Workflow

1. Build the instrument in Studio.
2. Brand and test it in Brand Lab.
3. Open Launch Center and run Launch Doctor.
4. Create Launch Bundle.
5. Use the generated Seller Launch Playbook to prepare demos, screenshots, and store copy.
6. Use the Buyer Quick Start as the customer-facing install guide.
7. Publish a Plugin.club draft for launch.
8. Use AudiLock later as the licensing source of truth without changing the payload shape.

## Current Boundaries

- AI Studio remains hidden for launch.
- Standalone branded VST3 export remains part of the paid VST Exporter expansion.
- Plugin.club is the launch licensing/publish backend until AudiLock replaces it.
