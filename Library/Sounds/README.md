# PatchCraft Factory Sound Library

PatchCraft scans this folder at launch and exposes supported audio files in the Library panel under `Sounds`.

Recommended shipping structure:

- `One Shots` - tonal one shots, impacts, stabs, plucks, and ear candy.
- `Loops` - tempo-labeled loops such as `Name_120bpm_Cmin.wav`.
- `Drums` - kicks, snares, hats, percussion, fills, and drum loops.
- `Bass` - bass one shots, bass loops, subs, and transition basses.
- `FX` - risers, downlifters, throws, reverses, noise, and cinematic effects.
- `Vocals` - vocal chops, phrases, textures, and processed adlibs.
- `Textures` - pads, drones, granular beds, atmospheres, and field layers.

Supported formats: WAV, AIFF, AIF, and FLAC.

Files placed here are staged into the Studio runtime by CMake. User-installed or separately installed factory libraries can also live in:

- `%APPDATA%\PatchCraft\AssetLibrary\sounds`
- `Documents\PatchCraft\Sounds`

Dragging or double-clicking a sound in the Library imports it into the Sample Mapper and switches the project to the Sampler engine.
