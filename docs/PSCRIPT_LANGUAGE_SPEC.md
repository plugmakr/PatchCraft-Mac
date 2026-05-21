# pScript Language Specification

pScript is PatchCraft's built-in scripting language. It is not a paid expansion. Paid/free add-ons can add additional language runtimes such as JavaScript or Lua, but pScript is the native, deeply integrated language for PatchCraft instruments.

## Design Goals

- Human-readable first: musicians and sound designers should understand scripts without being programmers.
- Deterministic: the same input should produce the same output unless controlled randomness is explicitly requested.
- Safe in exported instruments: no filesystem access, no network access, no blocking calls, and no unbounded loops.
- Musical by default: time, notes, velocity, BPM sync, layers, macros, and parameters are first-class concepts.
- Validated before export: scripts must compile to PatchCraft's internal automation graph/bytecode before they can ship in Player instruments.

## Runtime Boundary

pScript v1 controls:

- MIDI events and note routing.
- Parameter changes.
- Macro mappings.
- UI behavior.
- Sample-map transforms.
- Preset generation recipes.
- Validation helpers.
- Performance logic such as gates, stutters, key switches, and mod-wheel behavior.

pScript v1 does not allow:

- File I/O.
- Network I/O.
- OS commands.
- Dynamic code loading.
- Infinite loops.
- Audio-thread allocation.
- Arbitrary C++/plugin access.

## Style

pScript is indentation-based and uses musical phrases.

```pscript
script "Velocity Bloom"

when note starts:
    play layer "Sample Attack"
    set layer "Synth Body".volume to velocity mapped 0..127 -> 0.2..1.0
    set filter.cutoff to velocity mapped 0..127 -> 400 Hz..12000 Hz

when modwheel moves:
    set macro "Motion" to modwheel
```

## Core Concepts

### Events

```pscript
when preset loads:
when note starts:
when note ends:
when knob "Cutoff" moves:
when modwheel moves:
when transport starts:
when beat 1/4:
when pad "Glitch" is held:
```

### Parameters

```pscript
set filter.cutoff to 1200 Hz
set delay.feedback to 45%
set layer "Sample".volume to -6 dB
set macro "Chaos" to 80%
```

### Mappings

```pscript
set filter.cutoff to velocity mapped 0..127 -> 300 Hz..12000 Hz
set wavetable.position to modwheel mapped 0..127 -> 0%..100%
set delay.mix to knob "Throw" mapped 0%..100% -> 0%..60%
```

### Conditions

```pscript
when note starts:
    if velocity > 110:
        play layer "Hard Attack"
    else:
        play layer "Soft Attack"
```

### Randomness

Random must be bounded and optionally seeded.

```pscript
when note starts:
    randomize sample.start between 0% and 20%
    randomize filter.cutoff between 800 Hz and 1400 Hz per note
```

### Smoothing

Every fast parameter motion should support smoothing.

```pscript
when modwheel moves:
    set filter.cutoff to modwheel mapped 0..127 -> 500 Hz..9000 Hz
    smooth 35 ms
```

### Macros

```pscript
macro "Chaos":
    control knob "Chaos"
    change sample.pitch from -12 st to 12 st
    change delay.feedback from 10% to 85%
    change wavetable.position from 0% to 100%
    smooth 40 ms
```

### Performance Effects

```pscript
when pad "Stutter" is held:
    turn on effect "Repeater"
    set repeater.rate to 1/16 sync
    set repeater.mix to 70%

when pad "Stutter" is released:
    turn off effect "Repeater"
```

## Validation Rules

The compiler must reject scripts that:

- Reference missing layers, macros, controls, samples, effects, or parameters.
- Assign values outside the declared parameter range.
- Use unavailable capabilities.
- Use unsupported runtime features for Player export.
- Create feedback loops in the graph.
- Exceed CPU, event, or memory limits.

## Expansion Interaction

pScript Core is always available. Language-specific expansions add optional runtimes:

```text
JavaScript Runtime.pcexp -> script.runtime.javascript
Lua Runtime.pcexp        -> script.runtime.lua
Python Recipe Tools      -> script.runtime.python-studio-only
```

Non-pScript runtimes must still compile to PatchCraft's safe internal graph before export. If they cannot compile safely, they remain Studio-only tools.

## Recommended Compiler Pipeline

```text
pScript text
  -> lexer/parser
  -> semantic validator
  -> capability checker
  -> PatchCraft automation graph / bytecode
  -> Studio test runtime
  -> Player-safe export validator
```

## First Built-In Templates

### Hybrid Sample + Synth

```pscript
script "Hybrid Attack Body"

when note starts:
    play layer "Sample Attack"
    play layer "Synth Body"
    set layer "Sample Attack".volume to velocity mapped 1..127 -> -18 dB..0 dB
    set layer "Synth Body".filter.cutoff to modwheel mapped 0..127 -> 400 Hz..9000 Hz
```

### Trap Hat Ratchet

```pscript
when pad "Hat Repeat" is held:
    repeat note every 1/32 sync for 1 beat:
        play sample "Closed Hat"
        randomize velocity between 65 and 115
```

### One-Knob Throw

```pscript
macro "Throw":
    control knob "Throw"
    change delay.mix from 0% to 55%
    change reverb.size from 20% to 90%
    change filter.cutoff from 18000 Hz to 4200 Hz
    smooth 50 ms
```
