# PatchCraft AI Studio Expansion Plan

Date: 2026-05-20

## Launch Decision

PatchCraft launches with the paid VST3 Exporter first. AI Studio is not part of the launch build and is hidden by default behind `PATCHCRAFT_ENABLE_AI_STUDIO=OFF`.

The AI code path is not deleted. It is held as a future paid expansion so the launch product stays focused on the working Studio, Player, Player FX, and VST3 Exporter flow.

## Expansion Lineup

### 1. PatchCraft VST3 Exporter

- Launch paid addon.
- Unlocks standalone branded VST3 export from PatchCraft Studio.
- Ships separately from the base Studio/Player installer.
- Installs the `PluginTemplate` payload required by standalone VST3 export.

### 2. PatchCraft AI Studio

- Future expansion.
- Adds assisted preset design, pack planning, sample/one-shot ideation, image/art direction, sales copy generation, and Plugin.club draft preparation.
- Does not auto-publish products. All publishing remains manual-review-first.

## Pricing Model

### Managed AI Studio

- Price: `$25/mo`.
- Includes access to PatchCraft-managed AI routing.
- Best for users who do not want to configure keys or provider accounts.

### Credit Top-Up

- Price: `$15 for 50 credits`.
- Top-ups supplement the managed subscription for heavier generation workflows.
- Credits are consumed only by provider-backed work, not local UI planning or validation.

### BYOK AI Studio

- Price: `$75 one-time`.
- User brings their own keys.
- PatchCraft provides the workflow UI, queueing, validation, local storage, and Plugin.club preparation tooling.
- Provider billing is paid directly by the user.

## Suggested Credit Costs

- AI pack/preset brief: `1 credit`.
- DeepSeek sound-design plan: `1 credit`.
- Full one-shot kit plan: `3 credits`.
- Sales copy, tags, and listing metadata: `1 credit`.
- OpenAI image generation for artwork/background concepts: `5 credits`.
- Mureka sample/stem generation: `5-10 credits`, depending on duration and provider cost.
- Plugin.club publish package preparation: `1 credit`.
- Background agent run that opens plugins, prepares render plans, exports files, and queues manual review: `10-20 credits`, depending on scope.

## Product Guardrails

- AI Studio never publishes directly without user approval.
- Generated packs go into a review queue before Plugin.club submission.
- Provider keys are stored locally for BYOK mode.
- Managed mode should route through a PatchCraft backend so provider keys are never exposed.
- DeepSeek is best used for planning, structured metadata, sound-design direction, preset naming, workflow automation, and agent orchestration.
- Mureka or another music/audio provider is better suited for actual generated audio.
- OpenAI Images is better suited for artwork, backgrounds, product visuals, and UI inspiration assets.

## Build Flag

Launch builds use:

```powershell
cmake -S . -B build-codex -DPATCHCRAFT_ENABLE_AI_STUDIO=OFF
```

Future AI Studio builds can use:

```powershell
cmake -S . -B build-codex-ai -DPATCHCRAFT_ENABLE_AI_STUDIO=ON
```

