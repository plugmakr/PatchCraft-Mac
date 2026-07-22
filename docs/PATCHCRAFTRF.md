# PatchCraft RF (Refactor Branch)

PatchCraftRF implements the unified Studio vision:

## Spine (six tabs, Layout first)

1. **Layout** — canvas, bindings, element palette
2. **Sound** — sample mapper, keyzones, velocity
3. **Graph** — DSP signal path
4. **Perform** — Steps, Circles, drum grid, piano roll
5. **Brand** — identity + embedded live preview
6. **Ship** — export pack, launch bundle, installer (Plugin.club deferred)

## One runtime

`PackRuntimeHost` is the single Player preview engine used by:

- Brand tab preview
- **Customer Preview** overlay (toolbar / Window menu) — same audio + layout from any tab

## Advanced tools (Window → Advanced)

One-Shot Maker, Widget Builder, Animation Lab, Dashboard, and Project Browser remain available but are not primary tabs.

## Deferred on this branch

- Plugin.club publish UI (Ship focuses on local export + launch bundle)

## Ship readiness (PatchCraftRF)

- **LaunchReadiness** (`Source/Shared/LaunchReadiness.cpp`) — shared Launch Doctor blocking checks used by menu export, VST export, and factory-demo smoke tests.
- **Menu export gating** — File / toolbar **Export Pack** and **Export VST3** now match Ship tab: blocked when Launch Doctor reports errors (opens Ship on confirm).
- **Automated gates** — run from repo root:

```powershell
cmake --build build-codex --target PatchCraftReleaseChecklist --config Release --parallel 6
build-codex\bin\Release\PatchCraftAudioSmokeTests.exe
cmake --build build-codex --target PatchCraftRcBundle --config Release --parallel 6
```

- **Manual QA still required** — `docs/DAW_QA_CHECKLIST.md` (FL Studio Player/FX, drag/drop, presets, installer proof).
