# pScript User Guide

pScript lets you add musical behaviour to PatchCraft instruments: macros, MIDI reactions, timed changes, and more. This guide covers what works **today** in PatchCraft Studio and the exported Player pack.

For the long-term language vision (layers, pad events, transport sync, etc.), see [PSCRIPT_LANGUAGE_SPEC.md](PSCRIPT_LANGUAGE_SPEC.md). Those features are roadmap items unless listed below as **shipped**.

---

## Quick start

1. Open your instrument in **PatchCraft Studio**.
2. Open the **pScript** tab (left sidebar or canvas toolbar).
3. Write script in the editor, or choose **Insert template…** for a starter snippet.
4. Click **Compile**. Fix any errors shown in the console.
5. Use **Preview** and move controls / play notes to hear the behaviour.
6. **Export Pack** when ready. The pack includes `pscript.txt` so Player users get the same logic.

Example — open the filter when velocity is high:

```pscript
when note starts:
    set filterCutoff to velocity mapped 0..127 -> 800 Hz..9000 Hz
```

---

## Sharing scripts between users

You can send a standalone `.pscript` file to another PatchCraft author. They drop it on a control and it runs after export.

### Author A — create a shareable script

Save a text file with extension `.pscript` (`.psc` or `.txt` also work):

```pscript
# Macro: filter drives delay send
when knob moves:
    set delayMix to value mapped 0.0..1.0 -> 0.0..0.45
```

Use `when knob moves:` without a name, or put any placeholder name in quotes — PatchCraft **rewrites** the header when the file is attached to a control.

### Author B — attach the script

1. Open (or create) a project and **save it** once (scripts are stored in the project folder).
2. Make sure the target knob/slider/button is **assigned to a real parameter** in the Inspector.
3. **Drag the `.pscript` file** from Explorer/Finder onto that control on the canvas — or select the control and use **Attach pScript File...** from the canvas/pScript menu or Inspector.
4. The Inspector **pScript File** row shows the attached filename; use **Detach** to remove it.
5. **Preview** to verify, then **Export Pack**.

### End user (Player)

Exported packs contain:

- `pscript.txt` — merged runtime script (global editor code + all attached control scripts)
- `scripts/` — optional copies of attached `.pscript` files for reference

The Player loads `pscript.txt` when a preset loads. No extra setup is required.

---

## Knob events and parameter ids

When a mapped control moves, pScript fires:

```pscript
when knob "filterCutoff" moves:
    set reverbMix to value mapped 0.0..1.0 -> 0.0..0.35
```

**Important:** the name in quotes must be the **parameter id** (e.g. `filterCutoff`, `macro1`), not necessarily the label shown on the UI. When you use **Create pScript handler** from the canvas menu, Studio inserts the correct id for you.

In Preview and in Player, knob events use the same parameter id.

---

## Global vs per-control scripts

| Location | Purpose |
|----------|---------|
| **pScript tab editor** | Instrument-wide handlers (`when preset loads`, `when note starts`, timers, etc.) |
| **Dropped `.pscript` files** | Behaviour tied to one control; stored in `scripts/` and merged at compile/export time |

Both are combined automatically. The pScript tab shows only the **global** source; attached files appear in the project folder and in the exported pack.

---

## Shipped language features (v1)

### Events

| Event | Example |
|-------|---------|
| Preset load | `when preset loads:` |
| Note on / off | `when note starts:` / `when note ends:` |
| Mod wheel | `when modwheel moves:` |
| Control move | `when knob "filterCutoff" moves:` |
| Timer | `when timer 250 ms:` |

### Statements

- **set** — write a parameter: `set delayMix to 0.35`
- **let** — local variable: `let amount = value mapped 0.0..1.0 -> 0.0..1.0`
- **print** — debug to the pScript console (Studio Preview)
- **if / else** — conditional blocks (indentation-based)
- **repeat** — counted loop: `repeat 4:`
- **smooth** — glide a parameter toward a target over time
- **Mappings** — `value mapped 0.0..1.0 -> 0.0..0.5` or `velocity mapped 0..127 -> 800 Hz..9000 Hz`

### MIDI / note context (in note handlers)

- `note` — MIDI note number 0–127
- `velocity` — 0–127
- `modwheel` — 0–127 (in modwheel handlers)
- `value` — the moving control’s value (in knob handlers)

---

## Useful templates (Studio)

Open **pScript → Insert template…**. Prefer these when you want behaviour a single binding cannot do:

| Template | Why it’s useful |
|----------|-----------------|
| Tone Macro (3 targets) | One knob fans out to delay, reverb, and attack with different curves |
| Inverse FX Balance | Raising delay automatically pulls reverb down |
| Delay Safety Clamp | Wet delay forces feedback down so it cannot run away |
| Velocity Dynamics | Soft / medium / hard playing changes filter + FX |
| Ghost Note Mute | Very soft notes duck themselves |
| Key Track Filter | Filter follows MIDI note number |
| Velocity Envelope | Attack/release shape follows how hard you play |
| Note-Off Tail Bloom | Reverb swells on release; next note resets it |
| Living Pad Drift | Timer slowly randomizes filter for evolving pads |

---

## Not yet shipped (roadmap)

Do not rely on these in shared `.pscript` files until announced in release notes:

- `when pad "Kick" held` / pad release events
- `play layer`, layer routing
- Transport / BPM-synced phrases beyond basic timers
- Sample-map transforms, preset recipes, filesystem/network access

See [PSCRIPT_LANGUAGE_SPEC.md](PSCRIPT_LANGUAGE_SPEC.md) for the full design.

---

## Studio workflow tips

- **Compile** after edits (or enable **Live** reload while iterating).
- **Run preset on compile** — optional checkbox to fire `when preset loads` after a successful compile.
- **Create pScript handler** — right-click a mapped control → generates a knob-macro snippet with correct parameter ids.
- **Preview** — exercise knobs and MIDI; open the pScript console to see `print` output.
- **Save project** before dropping external `.pscript` files (they are copied into the project folder).

---

## Troubleshooting

| Problem | What to check |
|---------|----------------|
| Drop rejected | Control must be assigned to a parameter; project must be saved at least once. |
| Script compiles but knob does nothing | `when knob "..."` must use the **parameter id**, not the UI label. |
| Works in Preview, not in Player | Re-export the pack; confirm `pscript.txt` exists in the `.patchcraft` folder. |
| Attached file not in editor | Expected — attached scripts live in `scripts/`; the tab editor is for global code only. |

---

## File reference

| Path | Description |
|------|-------------|
| `pscript.txt` | Global script source in a `.patchcraftproject` folder |
| `scripts/*.pscript` | Per-control attached scripts |
| `<pack>/pscript.txt` | Merged runtime script shipped to Player |
| `<pack>/scripts/` | Copy of attached scripts (optional reference) |

---

## Example: shareable filter → reverb macro

**filter-to-reverb.pscript** (send to a friend):

```pscript
when knob moves:
    let send = value mapped 0.0..1.0 -> 0.0..1.0
    set reverbMix to send * 0.5
    set reverbSize to send mapped 0.0..1.0 -> 0.2..0.9
```

Friend drops it on their **Filter** knob (parameter id `filterCutoff`), exports, and the macro works in Player for anyone loading that pack.
