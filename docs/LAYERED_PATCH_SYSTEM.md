# PatchCraft Layered Patch System

## Purpose

Layered Patches turn PatchCraft from a single-instrument builder into a performance instrument builder. A Layered Patch is one playable preset that can contain multiple sound layers: arp, bassline, pad, drums, one-shots, granular texture, FX returns, and user-imported material.

The goal is to let a developer build an instrument where a player can introduce parts over time, morph between scenes, mute or solo layers, and perform a full musical idea from one branded Player.

## Core Concept

A Layered Patch is the source of truth for:

- Child patches or sound engines used by the performance.
- MIDI generators or imported MIDI clips feeding each layer.
- Per-layer mixer, routing, and FX chains.
- Scene states that turn layers on/off and change mix or macro values.
- Global performance controls exposed in the Player.

Example:

- Layer 1: synced arp synth, always active.
- Layer 2: bassline synth, launched by scene or mixer fade.
- Layer 3: granular vocal pad, introduced by macro.
- Layer 4: drum machine, pattern changes by scene.
- Layer 5: transition FX, triggered by pad or automation.

## Data Model

### LayeredPatch

- `id`
- `name`
- `key`
- `scale`
- `bpm`
- `layers`
- `scenes`
- `globalMacros`
- `masterMixer`
- `performanceControls`

### PatchLayer

- `id`
- `name`
- `role`: `arp`, `bass`, `pad`, `lead`, `drums`, `sample`, `granular`, `fx`
- `patchId` or `engineGraphId`
- `enabled`
- `volume`
- `pan`
- `mute`
- `solo`
- `midiSource`: keyboard, MIDI clip, arp, drum grid, external MIDI, generated MIDI
- `noteRange`
- `midiChannel`
- `launchMode`: always, key latch, scene, pad, one-shot, toggle
- `launchQuantize`: none, beat, bar, 2 bars, 4 bars
- `outputBus`
- `insertFxChain`
- `sendFxLevels`

### Scene

- `id`
- `name`
- `activeLayerIds`
- `mixerSnapshot`
- `macroSnapshot`
- `patternSelections`
- `transitionTimeMs`
- `launchQuantize`

Scenes are performance states. A user can move from Intro to Verse to Drop by switching scenes instead of manually managing every layer.

### Performance Controls

- Scene buttons.
- Layer mute/solo buttons.
- Layer faders.
- Crossfader A/B assignments.
- Intensity macro.
- Variation macro.
- Fill trigger.
- Drop/riser trigger.
- Panic/all-notes-off.

## Studio Workflow

1. Build or import individual patches.
2. Open Layer Stack.
3. Add layers and assign each layer a patch, sample map, drum kit, or synth graph.
4. Assign MIDI input per layer.
5. Set key range, channel, launch mode, and quantize.
6. Mix layers in the Layer Mixer.
7. Add scenes and capture layer/mixer/macro snapshots.
8. Add Player controls to the canvas: scene buttons, mixer, crossfader, macro controls, layer toggles.
9. Test the exact Player runtime in Brand Lab.
10. Export as Player pack, standalone VST3, or Plugin.club product.

## Player Runtime UX

The Player should expose the layered system without making the end user feel like they are editing a DAW session.

Recommended Player areas:

- Compact layer rack in the top menu or side drawer.
- Per-layer power, volume, pan, mute, solo, and output routing.
- Scene strip with Intro, Build, Drop, Breakdown, Custom.
- Optional full mixer modal for advanced users.
- Global macro controls for musical movement.
- Visual playback feedback for active MIDI clips, drums, samples, and arps.

## Routing

Every layer should have a clear audio path:

`Layer Sound Source -> Layer Inserts -> Layer Sends -> Layer Bus -> Master FX -> Output`

This allows hybrid instruments:

- Delay only on the arp oscillator.
- Different delay on the sample layer.
- Distortion only on the bass layer.
- Global reverb shared by all layers.
- Sidechain compression from drum layer into pad layer.

## MVP Implementation Slice

1. Add `LayeredPatch` project model and serialization.
2. Add Studio Layer Stack panel.
3. Add 4-layer runtime mixer with mute/solo/volume/pan.
4. Add per-layer MIDI source assignment.
5. Add scene snapshots.
6. Add Player layer rack and scene buttons.
7. Add Brand Lab preview parity.
8. Add export validation for missing child patches, routes, and controls.

## Value Proposition

This creates a new PatchCraft product type: a playable layered performance instrument. It is not just a synth preset or sample map; it is a complete branded performance environment that can combine sound design, MIDI, drums, samples, scenes, macros, and mixing into one sellable Player experience.
