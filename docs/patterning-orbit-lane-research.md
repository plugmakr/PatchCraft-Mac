# Patterning-Inspired Orbit Lane Research

Research date: 2026-05-24

This is a product-direction note for PatchCraft's circular Arp Lane element. It is inspired by the interaction model of Olympia Noise Co.'s Patterning apps, but should remain PatchCraft-native in naming, visuals, code, and workflow.

## Researched Behavior

- Patterning 2 and 3 center the workflow on a circular drum-machine interface.
- The key musical feature is independent per-track loop length, which enables polymeter and polyrhythm.
- Patterning 3 adds AUv3, macOS support, parametric swing, fill patterns, parameter modulation, an FX track, multiple timelines, MIDI export, multichannel audio/MIDI, and deeper sampler controls.
- Patterning-style lanes need per-lane target selection, direction, rotation, pulse density, probability, ratcheting, sample/slice targeting, and MIDI export.

## PatchCraft Mapping

- PatchCraft element: `ArpLane`
- JSON type: `arpLane`
- Runtime engine: MIDI Playground phrase bank
- Product-facing naming direction: Orbit Lane / CircleSEQ Lane

## Implemented First Pass

- Arp Lane now serializes sample target, base note, sample slots, direction, rotation, Euclidean pulses, probability, and ratchets.
- Inspector adds controls for the lane target and circular performance behavior.
- Build Ring writes Patterning-style lane data into the MIDI Playground bank.
- CircleSEQ demo lanes now demonstrate notes, pitched samples, drums, one-shots, and loop slices with different direction/pulse settings.
- Sample/drum/loop lanes draw as concentric multi-ring Orbit lanes so slot assignment is visible at a glance.
- Fill pulses and fill probability add a second performance layer for Patterning-style variations.

## Next Build Targets

- Multi-ring editor view: edit all Orbit lanes at once, not one selected Arp Lane at a time.
- Per-step automation rings for pitch, filter, pan, FX send, slice, and probability.
- Fill layer per lane with momentary/latched playback.
- Timeline and pattern-launch grid for complete song sections.
- Sampler kit browser and kit swapping tied directly to Orbit Lane tracks.
- Drag-and-drop MIDI/audio export from a lane or full pattern.
- Hardware MIDI learn for lane selection, fills, mutes, solos, and pattern launch.

Sources:
- https://www.olympianoiseco.com/apps/patterning-3/
- https://apps.apple.com/us/app/patterning-3-drum-machine/id1631913793
- https://www.olympianoiseco.com/manuals/Patterning_2.pdf
