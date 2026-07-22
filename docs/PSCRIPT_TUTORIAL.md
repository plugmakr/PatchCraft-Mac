# learning pScript: The Ultimate Guide

pScript is PatchCraft's built-in, lightweight, and safe scripting language. It is designed specifically for musicians and sound designers to easily add dynamic, interactive, and musical behaviors to PatchCraft instruments—all without needing to learn complex programming languages like C++ or Javascript.

Whether you want to build a velocity-responsive filter cutoff, create complex custom macros, build timed LFO-style randomizers, or map the MIDI mod wheel to multiple FX, this guide will teach you everything from scratch.

---

## Table of Contents
1. [Getting Started: Your First Script](#getting-started-your-first-script)
2. [The Two Ways to Script: Global vs. Attached](#the-two-ways-to-script-global-vs-attached)
3. [Language Syntax & Core Concepts](#language-syntax--core-concepts)
4. [Event Handlers Reference](#event-handlers-reference)
5. [Complete Parameter & Target Map](#complete-parameter--target-map)
6. [Interactive Code Recipes](#interactive-code-recipes)
7. [Best Practices & Troubleshooting](#best-practices--troubleshooting)

---

## Getting Started: Your First Script

Let's write a simple script that sets the filter cutoff to 1200 Hz whenever a new preset is loaded.

1. Launch **PatchCraft Studio** and open your project.
2. Navigate to the **pScript** tab (located in the left sidebar or the canvas top toolbar).
3. Type the following code:
   ```pscript
   when preset loads:
       set filterCutoff to 1200 Hz
   ```
4. Click **Compile** (or check the **Live** box to auto-compile).
5. Look at the console at the bottom—it should print `Compilation successful!`.
6. Play a note or click **Preview** to hear it!

---

## The Two Ways to Script: Global vs. Attached

pScript supports two different workflows depending on how you like to organize your project:

### 1. Global Editor
Code typed directly into the **pScript Tab Editor** is global to the entire instrument. This is the ideal place for global events like:
- Initializing parameters when presets load.
- MIDI note triggers (`when note starts:` and `when note ends:`).
- Timer-based background modulation (`when timer 250 ms:`).

### 2. Attached Scripts (Drag & Drop)
You can tie behavior directly to individual knobs, sliders, or buttons by creating a standalone text file with the extension `.pscript` and dropping it directly onto a control in the Canvas.

* **Step 1:** Create a text file called `macro.pscript` containing:
  ```pscript
  when knob moves:
      let amount = value mapped 0.0..1.0 -> 0.0..1.0
      set reverbMix to amount * 0.4
      set delayMix to amount * 0.6
  ```
* **Step 2:** Make sure the target control on your canvas is assigned to a real parameter in the Inspector (e.g., `filterCutoff` or `volume`).
* **Step 3:** Drag and drop `macro.pscript` from your file explorer onto that control.
* **Step 4:** PatchCraft will automatically copy the file to your project's `scripts/` folder and rewrite the first line to target the specific control:
  ```pscript
  # Bound to knob parameter filterCutoff
  when knob "filterCutoff" moves:
      let amount = value mapped 0.0..1.0 -> 0.0..1.0
      set reverbMix to amount * 0.4
      set delayMix to amount * 0.6
  ```

When you export your finished instrument pack, all global and attached scripts are merged automatically into a single optimized runtime file (`pscript.txt`) that runs inside the Player.

---

## Language Syntax & Core Concepts

pScript uses a clean, indentation-based syntax similar to Python. Block scopes are defined by 4-space indentation.

### 1. Variables
Use the `let` keyword to declare temporary, local variables inside an event block:
```pscript
let depth = 0.5
let driveAmount = depth * 1.5
```

### 2. Setting Parameters
Use the `set` keyword to change a parameter's value:
```pscript
set volume to -6 dB
set filterCutoff to 4200 Hz
```

### 3. Printing Logs
Use the `print` keyword to output values to the Studio console for debugging:
```pscript
when knob "filterCutoff" moves:
    print value
```

### 4. Mapped Ranges
You can map event inputs (like `velocity`, `modwheel`, or a knob's `value`) from their raw range to target musical values:
```pscript
# Map MIDI Velocity (0-127) to Filter Cutoff (800 Hz - 9000 Hz)
set filterCutoff to velocity mapped 0..127 -> 800 Hz..9000 Hz

# Map Knob Value (0.0-1.0) to Stereo Width (50% - 200%)
set stereoWidth to value mapped 0.0..1.0 -> 50%..200%
```

### 5. Conditional Logic (`if / else`)
Run code selectively based on conditions:
```pscript
when note starts:
    if velocity > 100:
        set lofiBits to 8
    else:
        set lofiBits to 16
```

### 6. Counted Loops (`repeat`)
Repeat a block of code a specific number of times:
```pscript
repeat 4:
    # Code runs four times
```

### 7. Parameter Smoothing (`smooth`)
Avoid clicks and pops during rapid value changes by adding a smoothing ramp:
```pscript
when modwheel moves:
    set filterCutoff to modwheel mapped 0..127 -> 400 Hz..8000 Hz
    smooth 35 ms
```

### 8. Random Ranges (`randomize`)
Introduce subtle organic variation by picking random values:
```pscript
when note starts:
    randomize filterCutoff between 800 Hz and 1500 Hz
```

### 9. Toggle Effects (`turn on/off`)
Enable or disable specific DSP effect blocks dynamically:
```pscript
turn on effect "Delay"
turn off effect "Phaser"
```

---

## Event Handlers Reference

Every pScript must begin with an event handler block. The following handlers are available:

| Event Handler | Trigger Timing | Available Context Variables |
| :--- | :--- | :--- |
| `when preset loads:` | Triggered once when the instrument loads or the preset changes. | None |
| `when note starts:` | Triggered whenever a MIDI Note On message is received. | `velocity` (0.0 - 127.0), `note` (MIDI number 0 - 127) |
| `when note ends:` | Triggered whenever a MIDI Note Off message is received. | `note` (MIDI number 0 - 127) |
| `when modwheel moves:` | Triggered when the MIDI Mod Wheel (CC 1) is moved. | `modwheel` (0.0 - 127.0) |
| `when knob "id" moves:` | Triggered when a UI knob/slider with parameter ID `id` is moved. | `value` (0.0 - 1.0) |
| `when timer XXX ms:` | Triggered periodically at the specified millisecond interval. | None |

---

## Complete Parameter & Target Map

When setting parameters or target variables in pScript, you can use the formal ID or a simplified friendly name. The compiler resolves these names automatically:

### Filters & Space
* **Filter Cutoff:** `filterCutoff`, `cutoff` (Values: `Hz`, e.g., `1200 Hz`)
* **Filter Resonance:** `filterResonance`, `resonance` (Values: `0.0..1.0` or `%`)
* **Reverb Mix:** `reverbMix`, `reverb` (Values: `0.0..1.0` or `%`)

### Delay
* **Feedback:** `delayFeedback`, `feedback` (Values: `0.0..1.0` or `%`)
* **Mix:** `delayMix`, `delay` (Values: `0.0..1.0` or `%`)
* **Time:** `delayTime`, `time` (Values: `ms`, e.g., `250 ms`)

### Dynamics (Compressor / Limiter)
* **Threshold:** `dynThresholdDb`, `threshold`, `dynThreshold` (Values: `dB`, e.g., `-18 dB`)
* **Ratio:** `dynRatio`, `ratio` (Values: `1.0..20.0`)
* **Attack:** `dynAttackMs`, `dynAttack` (Values: `ms`, e.g., `15 ms`)
* **Release:** `dynReleaseMs`, `dynRelease` (Values: `ms`, e.g., `200 ms`)
* **Makeup Gain:** `dynMakeupDb`, `makeup`, `dynMakeup` (Values: `dB`)
* **Mix:** `dynMix`, `dynamics` (Values: `0.0..1.0` or `%`)

### Chorus & Phaser
* **Chorus Rate / Depth / FB / Mix:** `chorusRate`, `chorusDepth`, `chorusFeedback`, `chorusMix`
* **Phaser Rate / Depth / FB / Mix:** `phaserRate`, `phaserDepth`, `phaserFeedback`, `phaserMix`

### Creative & Degradation FX
* **Comb Filter Freq / FB / Mix:** `combFreq`, `combFeedback`, `combMix`
* **Resonator Freq / Q / Mix:** `resonatorFreq`, `resonatorQ`, `resonatorMix`
* **Convolution Size / Mix:** `convolutionSize`, `convolutionMix`
* **Spectral Tilt / Mix:** `spectralTilt`, `spectralMix`
* **Tape Drive / Tone / Flutter / Mix:** `tapeDrive`, `tapeTone`, `tapeFlutter`, `tapeMix`
* **Vinyl Age / Dust / Warp / Mix:** `vinylAge`, `vinylDust`, `vinylWarp`, `vinylMix`
* **Lo-Fi Bits / Rate / Mix:** `lofiBits`, `lofiRate`, `lofiMix`
* **Vocal Formant / Body / Mix:** `vocalFormant`, `vocalBody`, `vocalMix`

### Master & Utility
* **Stereo Width:** `stereoWidth`, `width` (Values: `0.0..2.0` or `%`)
* **Mono Maker:** `monoMaker` (Values: `Hz`, e.g., `120 Hz`)
* **Volume:** `volume`, `vol` (Values: `dB` or `0.0..1.0`)
* **Pan:** `pan` (Values: `-1.0..1.0`)
* **Project BPM:** `projectBpm`, `bpm` (Read-only)

---

## Interactive Code Recipes

### 1. Organic Key Vel-to-Cutoff Bloom
Makes notes played harder sound brighter and more open:
```pscript
when note starts:
    let cutoffTarget = velocity mapped 0..127 -> 800 Hz..7500 Hz
    set filterCutoff to cutoffTarget
    smooth 20 ms
```

### 2. Vinyl Tape-Stop Pitch Wobble
Randomizes pitch and drift on a timer to create an unstable vintage vibe:
```pscript
when timer 120 ms:
    # Randomize fine pitch drift
    randomize pan between -0.15 and 0.15
    randomize tapeFlutter between 5% and 18%
```

### 3. Mod-Wheel Performance Swell
Connects your keyboard's Mod Wheel to open up the delay, reverb, and tape saturation simultaneously:
```pscript
when modwheel moves:
    let mod = modwheel mapped 0..127 -> 0.0..1.0
    set reverbMix to mod * 0.45
    set delayMix to mod * 0.65
    set tapeDrive to mod mapped 0.0..1.0 -> 0.0..12.0 dB
    smooth 40 ms
```

### 4. Gated Pad / Sidechain Effect
Uses a timer to create a host-synced sidechain volume ducking pump:
```pscript
when timer 250 ms:
    set volume to -24 dB
    smooth 10 ms
    
    # Wait and restore volume
    set volume to 0 dB
    smooth 150 ms
```

---

## Best Practices & Troubleshooting

* **Indentation Matters:** pScript uses exactly 4 spaces (or a tab) to define blocks. Mixing indentation levels will cause compile errors.
* **Control vs. Parameter ID:** Always use the raw parameter ID (e.g., `filterCutoff`) rather than the cosmetic label you wrote on the knob.
* **Unit Safety:** You can write values with their musical units directly (e.g. `Hz`, `ms`, `%`, `dB`), and the compiler will convert them to safe DSP values automatically.
* **Clean up attached scripts:** If you rename a canvas element or detach a parameter, remember to select **Detach pScript File** from the Inspector to avoid warnings on compile.
