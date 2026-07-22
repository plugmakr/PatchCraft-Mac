# PatchCraft V1 — Codex Video Production Bible

**Audience:** Codex agent (Remotion + live PatchCraft capture + voiceover → MP4)  
**Goal:** Ship a coherent tutorial series for V1 launch — no AI Studio, no Mureka, no paid-expansion deep dives unless noted.  
**Canonical workflow:** Sound → Graph → Perform → Layout → Presets → Brand → Ship  
**Last updated:** 2026-06-19

---

## Codex execution prompt (paste at start of each run)

```
You are producing PatchCraft tutorial MP4s for AudiCode.

Rules:
1. Use Remotion for intro, chapter bumpers, lower-thirds, and outro only.
2. Capture live PatchCraft Studio walkthroughs at 1920×1080 — do not simulate UI in Remotion.
3. Launch Studio from the RC build unless told otherwise:
   M:\AudiCode\PCraft\build-codex\dist\PatchCraftStudio-RC\PatchCraftStudio.exe
4. Use CURRENT toolbar labels: Sound | Graph | Perform | Layout | Brand | Ship
   Perform in-page toggles: Steps | Circles
5. Voiceover: calm, expert, concise. Male or female neutral American English. ~140 wpm.
6. Export: H.264 MP4, 1920×1080, 30fps, AAC 192kbps stereo.
7. Output path: docs/tutorials/patchcraft/videos/<episode-id>.mp4
8. Do NOT show AI Studio surfaces (disabled in V1 build).
9. After export, write a 3-line chapter summary to docs/tutorials/patchcraft/videos/<episode-id>.summary.txt

Read the episode block below. Follow scene order exactly. Pause 1.5s after each major click.
```

---

## Production system

### Folder layout (create if missing)

```
docs/tutorials/patchcraft/videos/
  remotion/                 # Remotion project (shared)
    src/
      Intro.tsx
      ChapterBumper.tsx
      LowerThird.tsx
      Outro.tsx
      Root.tsx
    public/
      patchcraft-logo.png   # from Library or FactoryDemos thumbnail
  raw/                      # Screen captures per episode (Codex saves here)
  audio/                    # Voiceover WAV per episode
  exports/                  # Final MP4 deliverables
```

### Remotion composition specs

| Composition | Duration | Purpose |
|-------------|----------|---------|
| `Intro` | 5s | Logo + “PatchCraft Studio” + episode title |
| `ChapterBumper` | 3s | “Sound” / “Graph” / etc. tab name on brand gradient |
| `LowerThird` | overlay | `{chapter}` + `{tip}` — fade 0.4s |
| `Outro` | 8s | “AudiCode · patchcraft.com” + next episode card |

**Brand colors (match Player demos):**  
- Background `#07090f` · Accent `#58b7ff` · Teal `#62f7d2` · Panel `#101722`

### Screen capture specs

- **Resolution:** 1920×1080 (Studio canvas default 1280×800 — zoom canvas to fit, keep toolbar visible)
- **FPS:** 30
- **Cursor:** visible, slow deliberate moves
- **Audio:** mute system audio during capture; voiceover added in post
- **Project file:** save demo project to `FactoryDemos/` copy or `Documents/PatchCraft/Tutorials/<episode>.patchcraft` before recording

### Voiceover pipeline

1. Generate narration from **Voiceover script** blocks below (TTS or recorded)
2. Align to timeline markers `[MM:SS]`
3. Duck −6dB under Remotion intro/outro music (optional subtle bed, −18dB)

### Final assembly timeline (every episode)

```
[0:00-0:05]  Remotion Intro
[0:05-??:??] Live Studio capture (chapters cut with Remotion bumpers between major sections)
[??:??-end]  Remotion Outro
```

---

## Series map (V1 launch order)

| ID | File | Title | Target length | Priority |
|----|------|-------|---------------|----------|
| `00-trailer` | 00-trailer.mp4 | What is PatchCraft? | 3:00 | P0 |
| `masterclass` | masterclass.mp4 | Zero to Shipped Pack | 22:00 | P0 |
| `01-overview` | 01-overview.mp4 | Studio, Player & the Pack | 8:00 | P0 |
| `02-getting-started` | 02-getting-started.mp4 | Install, Audio & Navigation | 7:00 | P0 |
| `03-sound` | 03-sound.mp4 | Sound Workspace & Sample Mapper | 10:00 | P0 |
| `04-graph` | 04-graph.mp4 | Graph & Signal Path | 10:00 | P0 |
| `05-perform` | 05-perform.mp4 | Perform Overview (Steps + Circles) | 8:00 | P0 |
| `11-patterns` | 11-patterns.mp4 | Perform · Steps Deep Dive | 12:00 | P1 |
| `12-circles` | 12-circles.mp4 | Perform · Circles Deep Dive | 10:00 | P1 |
| `06-layout` | 06-layout.mp4 | Layout, Canvas & Bindings | 10:00 | P0 |
| `07-brand` | 07-brand.mp4 | Brand Lab & Player Preview | 8:00 | P1 |
| `08-presets` | 08-presets.mp4 | Presets & Expansions | 8:00 | P1 |
| `09-ship` | 09-ship.mp4 | Ship, Export & Plugin.club | 10:00 | P0 |

**P0 = record before V1 launch.** P1 = ship within first week post-launch.

---

## Demo projects (reuse across episodes)

| Demo | Path | Use for |
|------|------|---------|
| Aurora Arp Synth | `FactoryDemos/AuroraArpSynth.patchcraft` | Synth + Perform Steps |
| Romplur Drum Machine | `FactoryDemos/RomplurDrumMachine.patchcraft` | Sound + drum grid |
| EchoCraft | `FactoryDemos/EchoCraft.patchcraft` | Graph FX + Brand |
| BeatFoundry | `FactoryDemos/BeatFoundry.patchcraft` | Ship / export showcase |

**Masterclass:** start blank → build minimal synth pack → export (do not use factory demo until recap).

---

# Episode breakdowns

---

## `00-trailer` — What is PatchCraft? (3:00)

### Remotion only + b-roll captures (no full tutorial)

**Voiceover script:**

> [0:05] PatchCraft is an instrument builder — not a DAW, not a sample editor alone.  
> [0:12] You design the sound, the performance layer, the player UI, and the presets — then ship a pack your customers load in PatchCraft Player.  
> [0:22] One project. One source of truth. From blank canvas to VST-ready product.  
> [0:28] Sound. Graph. Perform. Layout. Brand. Ship.  
> [0:35] This is PatchCraft Studio — let's build something playable.

**Capture shots (5–8s each):** Workflow dashboard · Sound tab · Perform Steps grid · Layout canvas · Brand preview · Ship export dialog · Player in DAW (if available).

**Export:** `videos/exports/00-trailer.mp4`

---

## `masterclass` — Zero to Shipped Pack (22:00)

### Chapters

1. Intro + mental model (1:30)
2. Sound — synth starter (3:00)
3. Graph — blocks & routing (3:00)
4. Perform — 8-step arp pattern (3:30)
5. Layout — knobs + bindings (4:00)
6. Presets — save 3 patches (2:00)
7. Brand — title bar + colors (2:00)
8. Ship — export `.patchcraft` + open in Player (3:00)

### Live walkthrough (step-by-step)

| Time | Action | UI target |
|------|--------|-----------|
| 0:05 | Launch Studio, Workflow page visible | Dashboard |
| 0:30 | Create → Synth starter | Canvas toolbar **Create** |
| 1:00 | Click **Sound** tab — confirm engine | Toolbar |
| 1:30 | Click **Graph** — show source/filter/fx blocks | Graph editor |
| 4:30 | Click **Perform** → **Steps** — Make Pattern | Perform |
| 5:00 | Paint 8 steps, click **Make Pattern** | Perform |
| 8:00 | Click **Layout** — Designer Mode on | Layout |
| 8:30 | Add 2 knobs from palette, bind to filter + volume | Canvas |
| 12:00 | Bottom strip **CONTROL BINDINGS** — wire one control | Layout bottom |
| 14:00 | Presets panel — Save Patch × 3 names | Presets |
| 16:00 | **Brand** tab — set display name, accent color | Brand Lab |
| 18:00 | **Ship** tab — Export pack | Launch Center |
| 20:00 | Open exported pack in **PatchCraft Player** standalone | PlayerPlugins folder |
| 21:00 | Play MIDI, switch preset | Player |

**Voiceover script (full):**

> [0:05] In this masterclass we'll build a complete playable instrument and export it — in one sitting.  
> [0:15] PatchCraft separates *authoring* in Studio from *playing* in Player. Everything we save lives in a patchcraft pack.  
> [0:30] We start with Create, Synth. Studio gives us a starter graph and layout.  
> [1:00] Sound is where samples and drum maps live. For synths, the graph is the sound.  
> [1:30] Graph is your signal path — source, filter, effects. This is real routing, not decoration.  
> [4:30] Perform adds an optional pattern layer — steps, drums, or circles. We'll paint a simple arp and click Make Pattern to write it into the pack.  
> [8:00] Layout is the customer-facing UI. Place knobs, bind them to parameters in CONTROL BINDINGS.  
> [14:00] Presets capture playable states — give buyers starting points.  
> [16:00] Brand Lab polishes Player chrome — name, colors, title bar.  
> [18:00] Ship exports the pack. Drop it in Player — same sound, same UI, same presets.  
> [21:00] That's the full loop. Sound, Graph, Perform, Layout, Brand, Ship.

**Remotion bumpers:** Insert `ChapterBumper` at 1:00, 4:30, 8:00, 14:00, 16:00, 18:00.

**Export:** `videos/exports/masterclass.mp4`

---

## `01-overview` — Studio, Player & the Pack (8:00)

### Voiceover script

> [0:05] PatchCraft has two apps: Studio for builders, Player for musicians.  
> [0:15] A patchcraft pack bundles sound, parameters, layout, patterns, presets, and branding.  
> [0:25] Studio tabs map to the build order: Sound, Graph, Perform, Layout, Brand, Ship.  
> [0:40] Player reads that pack verbatim — what you ship is what they hear.  
> [1:00] Let's open a factory demo and see the same pack in Studio and Player.

### Capture

1. Show Workflow **Always Keep One Source of Truth** card (30s)
2. Load `AuroraArpSynth.patchcraft` from project browser (45s)
3. Walk toolbar left → right naming each tab (60s)
4. Open Player standalone with same pack (90s)
5. Split recap diagram — Remotion animated `.patchcraft` layer cake (30s)

**Export:** `videos/exports/01-overview.mp4`

---

## `02-getting-started` — Install, Audio & Navigation (7:00)

### Capture

1. RC bundle folder structure: `PatchCraftStudio.exe`, `FactoryDemos`, `Library`, `PlayerPlugins` (60s)
2. First launch → audio device settings (90s)
3. Workflow guided buttons 1–5 (Sound → Ship) (90s)
4. Canvas zoom, snap, Designer vs Player Mode toggle (60s)
5. Save project, reload (45s)

**Voiceover highlights:** Where demos live, how to pick audio/MIDI, Designer Mode for editing vs preview.

**Export:** `videos/exports/02-getting-started.mp4`

---

## `03-sound` — Sound Workspace & Sample Mapper (10:00)

### Demo: `RomplurDrumMachine.patchcraft`

| Time | Action |
|------|--------|
| 0:30 | Open **Sound** tab |
| 1:00 | Sample Mapper sub-tabs: Mapper / Keyzones / Velocity |
| 2:30 | Drag WAV onto zone, set root key |
| 4:00 | Select All + Auto Trim |
| 5:30 | Drum pad grid preview in Test or Brand |
| 7:00 | One Shot Maker mention (30s only — no deep dive) |

**Voiceover script:**

> [0:05] Sound is first in the workflow because everything else reads from the playable patch.  
> [0:20] Sample Mapper handles zones, velocity layers, and drum maps.  
> [0:45] Auto Trim works on selected zones — select all if you want the whole kit processed.  
> [1:30] When you're building drums, the graph and Perform grid share the same cell data — we'll connect that in Perform.

**Export:** `videos/exports/03-sound.mp4`

---

## `04-graph` — Graph & Signal Path (10:00)

### Demo: `EchoCraft.patchcraft` or synth starter

| Time | Action |
|------|--------|
| 0:30 | **Graph** tab — global node editor |
| 1:00 | Pan source → filter → fx → out |
| 3:00 | Add block from palette (arp or midi playground) |
| 5:00 | Validation badge — fix a warning |
| 7:00 | Link from Workflow **Graph** button (was DSP Builder) |

**Voiceover script:**

> [0:05] Graph is the sound engine — not the layout canvas.  
> [0:20] Blocks hold parameters. Perform and presets read those values.  
> [0:40] If Launch Center reports graph errors, fix them here — not on Layout.

**Export:** `videos/exports/04-graph.mp4`

---

## `05-perform` — Perform Overview (8:00)

### Capture

1. **Perform** tab, header **PERFORM**
2. Toggle **Steps** | **Circles** (same slots, different editors)
3. **Make Pattern** vs **Update Player Controls** — say the distinction clearly (90s)
4. Quick paint in Steps, switch to Circles, same slot still active (60s)
5. Brand preview with pattern playing (45s)

**Voiceover script (critical line):**

> [2:00] Make Pattern writes data into the graph — steps, cells, phrases.  
> [2:15] Update Player Controls only changes the layout widgets — it does not change your pattern.  
> [2:30] Steps is piano roll and grids. Circles is orbit lanes and rings. Same five phrase slots.

**Export:** `videos/exports/05-perform.mp4`

---

## `11-patterns` — Perform · Steps Deep Dive (12:00)

**Source doc:** `11-patterns.html`

### Capture checklist (from docs)

- [ ] Open Perform → Steps
- [ ] Musical preset drop (chord phrase)
- [ ] Drum machine mode + grid paint
- [ ] Operators menu (transpose, invert)
- [ ] Phrase slots mpBank1–5
- [ ] Make Pattern → template
- [ ] Update Player Controls → generated knobs
- [ ] Export MIDI clip
- [ ] Test in Player

**Voiceover:** Follow doc sections `#workflow`, `#editors`, `#templates` — read callouts verbatim where marked.

**Export:** `videos/exports/11-patterns.mp4`

---

## `12-circles` — Perform · Circles Deep Dive (10:00)

**Source doc:** `12-circles.html`

**Note for Codex:** UI now shows **Perform** + **Circles** toggle, not separate Circles toolbar tab. Update narration accordingly.

### Capture

- Circles toggle active
- Lane list + ring editor
- Target / Sound dropdowns (registry params)
- Make Circle Pattern + Update Player Controls
- Add Arp Lane on Layout → jump back to Perform Circles

**Export:** `videos/exports/12-circles.mp4`

---

## `06-layout` — Layout, Canvas & Bindings (10:00)

### Capture

1. **Layout** tab, Designer Mode
2. Element palette — knob, slider, pad grid, tab panel
3. Inspector — parameter binding
4. **CONTROL BINDINGS** bottom strip
5. Right-click → Add Arp Lane / Drum grid
6. Save Patch / Save Patch As

**Voiceover script:**

> [0:05] Layout is the customer UI — every widget must bind to a real parameter.  
> [0:30] CONTROL BINDINGS is where you wire controls to sound, motion, and output.  
> [1:00] Generated Perform controls are prefixed — Update Player Controls replaces them, not stacks on top.

**Export:** `videos/exports/06-layout.mp4`

---

## `07-brand` — Brand Lab & Player Preview (8:00)

### Capture

- Brand tab — title bar themes, accent, logo slot
- Live Player preview pane
- Toggle Player Mode on canvas
- library artwork + modal preview

**Export:** `videos/exports/07-brand.mp4`

---

## `08-presets` — Presets & Expansions (8:00)

### Capture

- Presets browser, tags, expansion packs
- Save full patch vs section preset
- Expansions page — card layout for Player library

**Export:** `videos/exports/08-presets.mp4`

---

## `09-ship` — Ship, Export & Plugin.club (10:00)

### Capture

1. **Ship** tab (Launch Center)
2. Health checklist — green/pass items
3. Export `.patchcraft` pack
4. Export Player VST3 product (not standalone branded — mention VST Expansion addon one sentence)
5. Plugin.club publish dialog — draft only, blur API key
6. Launch bundle markdown files

**Voiceover script:**

> [0:05] Ship is where you validate, export, and publish.  
> [0:20] Fix graph errors in Graph. Fix binding errors on Layout.  
> [0:40] Base PatchCraft ships Player packs and Player VST3 hosting. Standalone branded VST3 export is the paid VST Expansion.  
> [1:00] Plugin.club handles licensing drafts — product ID and activation URL must match your Launch settings.

**Export:** `videos/exports/09-ship.mp4`

---

## Remotion starter `Root.tsx` (Codex: create project)

```tsx
import { Composition } from "remotion";
import { Intro } from "./Intro";
import { Outro } from "./Outro";
import { ChapterBumper } from "./ChapterBumper";

export const RemotionRoot = () => (
  <>
    <Composition
      id="Intro"
      component={Intro}
      durationInFrames={150}
      fps={30}
      width={1920}
      height={1080}
      defaultProps={{ title: "PatchCraft Studio", subtitle: "Episode title here" }}
    />
    <Composition
      id="ChapterBumper"
      component={ChapterBumper}
      durationInFrames={90}
      fps={30}
      width={1920}
      height={1080}
      defaultProps={{ chapter: "Sound" }}
    />
    <Composition
      id="Outro"
      component={Outro}
      durationInFrames={240}
      fps={30}
      width={1920}
      height={1080}
      defaultProps={{ nextEpisode: "Graph & Signal Path" }}
    />
  </>
);
```

**Render commands:**

```bash
cd docs/tutorials/patchcraft/videos/remotion
npm install
npx remotion render Intro ../exports/intro-segment.mp4 --props='{"title":"PatchCraft","subtitle":"Perform · Steps"}'
```

**Final mux (Codex ffmpeg):**

```bash
ffmpeg -i intro.mp4 -i capture.mp4 -i outro.mp4 -i narration.wav \
  -filter_complex "[0:v][0:a][1:v][1:a][2:v][2:a]concat=n=3:v=1:a=1[v][a];[a][3:a]amix=inputs=2:duration=first" \
  -map "[v]" -map "[a]" -c:v libx264 -crf 18 -c:a aac -b:a 192k final.mp4
```

---

## Quality checklist (every episode)

- [ ] Toolbar labels match V1 refactor (Perform not Patterns/Circles tabs)
- [ ] No AI Studio menus visible
- [ ] Audio clicks audible in preview where relevant
- [ ] No personal paths, API keys, or Plugin.club secrets on screen
- [ ] Captions optional — burn lower-thirds for tab names at first use
- [ ] `summary.txt` written with: one-line goal, key clicks, next episode ID
- [ ] Thumbnail: Remotion still at `[0:03]` intro frame → `videos/thumbs/<id>.jpg`

---

## Codex batch schedule (suggested)

| Day | Episodes |
|-----|----------|
| 1 | `00-trailer`, `01-overview`, `02-getting-started` |
| 2 | `03-sound`, `04-graph` |
| 3 | `05-perform`, `masterclass` (long) |
| 4 | `06-layout`, `07-brand`, `09-ship` |
| 5 | `11-patterns`, `12-circles`, `08-presets` |

---

## Post-ship updates (do NOT record for V1)

- AI Studio / DeepSeek / Mureka backends
- Mac-specific install (separate pass when Mac RC ready)
- VST Expansion standalone export walkthrough (paid addon video)

---

## Reference links

- Written tutorials: `docs/tutorials/patchcraft/index.html`
- Patterns doc: `11-patterns.html` · Circles doc: `12-circles.html`
- Ship gates: `docs/SHIP_READY.md`
- DAW QA: `docs/DAW_QA_CHECKLIST.md`
- RC build: `build-codex/dist/PatchCraftStudio-RC/PatchCraftStudio.exe`
