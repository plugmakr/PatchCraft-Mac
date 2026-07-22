# DSP Simple Stack

PatchCraft’s goal: **simple instruments in minutes, complex ones without a CS degree.**

This document is the audit of what actually runs today, what was over-modeled, and the product model going forward.

---

## The finding (audit summary)

We audited all **13 factory demos** and the runtime code path.

### What shipped demos actually use

| Pattern | Count | Example |
|---------|-------|---------|
| **3-block stack** (source → shape → fx) | 10 demos | DreamKeysSampler, EchoCraft, ModularMotionFX |
| **+ Motion block** (arp, drums, circles, harmony) | 4 demos | CircleSeqFlagship, RomplurDrumMachine, HarmonyComposer |
| **Explicit graph edges** | 2 demos | AuroraFlagship, ArpStepSequencer |
| **Macro routes in graph** | 1 demo | AuroraFlagship (3 macros) |
| **Mod matrix / automation lanes** | 0 demos | Empty in all factory packs |

Typical factory `dspGraph.json`:

```
source  (sample | oscillator | liveInput)
shape   (filter + ADSR values)
fx      (fxChain: delay, reverb, drive)
```

No edges, macros, or modulation arrays — yet everything sounds correct.

### What new projects used to get (the mismatch)

`DspGraph::resetForEngine()` previously created **8–10 blocks**, explicit edges, LFO, macro_motion, mod routes, and automation — while factory demos used **3 blocks**.

**That’s fixed:** new projects now use the same **Simple Stack** as factory demos. The old template lives in `resetForEngineExpanded()` for advanced Graph editing.

### What actually runs each audio block

Audio is **not** re-patched through modular cables at sample rate.

1. **Voices** — `SampleSynthEngine` / `SynthEngine` generate audio.
2. **EffectEngine** — fixed chain: filter → EQ → delay → reverb → utility.
3. **DspRoutingEngine** — each block syncs its `values` map onto **parameters** before the engine renders. LFOs, macros, and motion blocks modulate parameters — they do not reroute audio buffers.
4. **Typed graph edges** — affect **apply order** and validation, not a modular audio graph.

So complexity in the *data model* was ahead of complexity in the *audio path*. That’s fine for roadmap, confusing for authors.

---

## The Simple Stack (v1 product model)

Think in **five layers**. Authors only need the first three for a working instrument.

```
┌─────────────┐
│   SOURCE    │  Osc / samples / input — what makes sound
├─────────────┤
│    TONE     │  Filter + envelope — shape the note
├─────────────┤
│    SPACE    │  Delay + reverb + drive — depth and glue
├─────────────┤
│   MOTION    │  Optional: arp, sequencer, drum machine, pScript
├─────────────┤
│   OUTPUT    │  Level, limiter (auto in simple mode)
└─────────────┘
```

### Simple path (default)

1. Pick engine (Sample / Synth / FX).
2. Map knobs to parameters (`filterCutoff`, `delayMix`, `volume`, …).
3. Drop samples if needed.
4. Preview → Export.

**No Graph tab required.** The three default blocks (`source`, `shape`, `fx`) already mirror the factory demos.

### Powerful path (still simple UX)

| Need | Tool | Not required |
|------|------|--------------|
| “Cutoff opens on velocity” | **pScript** | Mod matrix |
| “One knob controls three things” | **Macro knob** + Inspector macro routing | Typed edges |
| “Arp / drums / circles” | **Motion block** (canvas module or Perform tab) | Separate FX plugins |
| “LFO on filter” | Add LFO block in **Graph** or pScript timer | Full modular patch |

### Advanced path (defer in UI)

- Full **Control Node Editor** module catalog
- Explicit **typed edges / ports**
- **Automation lanes** in graph JSON
- Premium expansion modules until they have a one-click “Add to stack” entry

---

## What to use when (author cheat sheet)

| I want… | Do this |
|---------|---------|
| Basic pad / keys / drums | 3–6 knobs on Source + Tone + Space params |
| Motion / rhythm | Add Perform surface or Motion block |
| Control relationships | pScript (`when knob "filterCutoff" moves: set delayMix to …`) |
| Share behaviour | Drop `.pscript` on a knob → export pack |
| Deep modular patching | Graph tab → **Expand graph** (`resetForEngineExpanded`) |

---

## Code map

| File | Role |
|------|------|
| `Source/Shared/SoundStack.cpp` | **Simple** and **Expanded** default graphs |
| `Source/Shared/DspRoutingEngine.cpp` | Parameter routing from blocks (macros, LFO, motion) |
| `Source/Shared/EffectEngine.cpp` | Actual FX audio chain |
| `Source/Studio/ControlNodeEditor.cpp` | Advanced graph UI (power users) |
| `docs/PSCRIPT_USER_GUIDE.md` | Behaviour layer without DSP graph complexity |

### API

```cpp
graph.resetForEngine ("synth");           // Simple 3-block stack (default)
graph.resetForEngineExpanded ("synth");   // Legacy multi-block + LFO + macros
```

---

## Roadmap (simple + powerful)

### Phase 1 — Done in code

- [x] Align `resetForEngine` with factory 3-block pattern
- [x] Preserve expanded graph for tests and advanced users
- [x] Document the model (this file)

### Phase 2 — Studio UX (next)

- [x] Rename Graph tab label → **Sound Stack** with “Advanced graph” toggle
- [x] Workflow “Connect Sound” tutorial references Source / Tone / Space only
- [x] Hide mod matrix + automation in Inspector until Motion block exists
- [x] One-click **Add Motion** (arp / drum / circle) inserts 4th block, not 20 canvas modules

### Phase 3 — Power without clutter

- [ ] Macro wizard: pick source knob → pick 2–3 targets (writes macros + optional pScript)
- [ ] Graph validation warnings only when user opens Advanced graph
- [ ] Factory demo loader documents which layer each demo uses

---

## Bottom line

**We did not over-complicate the audio engine.** The EffectEngine chain is straightforward.

**We did over-complicate the default authoring story** by giving new projects an expanded graph while shipping demos on a 3-block stack. That’s now aligned.

**Power comes from stacking layers**, not from exposing every typed node on day one:

- **Simple** = parameters + Sound Stack + layout  
- **Powerful** = + Motion + pScript + macros  
- **Advanced** = full Graph editor when you need it
