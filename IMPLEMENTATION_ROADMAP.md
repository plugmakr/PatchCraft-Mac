# PatchCraft Functional Roadmap

This roadmap tracks the work required to move PatchCraft from MVP/prototype into a professional instrument authoring system. It is intentionally implementation-focused: each milestone should produce visible user value and a testable runtime behavior.

Tracking convention: checked items are complete and validated by a build; unchecked items remain open.

## Current Focus - MIDI Playground

Goal: PatchCraft should make MIDI generation, transformation, and performance a creative sound-design system, not just a piano roll or simple arpeggiator.

Completed in the current MIDI slice:

- [x] Added shared `MidiPlaygroundRuntime` as the Player/Test runtime foundation for scale-aware ARP, chord generation, phrase sequencing, swing, probability, per-step gates, per-step velocity, latch-ready playback, and seeded variation.
- [x] Replaced Player and Test page runtime ARP handling with MIDI Playground while preserving existing ARP block compatibility.
- [x] Added Studio defaults so newly created ARP/MIDI blocks store MIDI Playground scale, chord, probability, humanize, latch, seed, per-step gate, per-step velocity, and per-step active state values.
- [x] Expanded ARP patterns beyond Up/Down into Euclidean and Seeded Random options.
- [x] Added automated smoke coverage for MIDI Playground chord/scale phrase generation.
- [x] Added MIDI-controllable sampler playback parameters: Sample Start, Sample Length, Sample Slice, Slice Count, Sample Pitch, and Sample Reverse.
- [x] MIDI Playground can now drive sample slicing per step through `mpSampleControl` / `mpSampleSlice#`, so generated MIDI can chop and trigger sample regions.
- [x] Added a MIDI Playground modal/overview that demonstrates Input, Harmony, Rhythm, Performance, Sample Control, and Output sections, with starter modes for Chord Phrase and Sample Slice Control.
- [x] Promoted MIDI Playground to its own top-level Studio section between Sample Mapper and Test, with setup controls, section cards, step/sample preview, and direct navigation to Sample Mapper/Test.
- [x] Added MIDI Playground sound-source guidance, throttled graph commits for smoother slider dragging, and an editable MIDI Output Editor for pitch/slice, velocity, gate, and step enable state.
- [x] Made MIDI Playground section cards selectable with visible state and fixed the clipped subtitle/header copy.
- [x] Added fixed hardware MIDI shortcuts for sampler performance: CC20 Sample Start, CC21 Sample Slice, CC22 Sample Length, and CC23 Sample Pitch, while preserving MIDI Learn for custom mappings.
- [x] Added smoke coverage proving sampler start/slice controls change playback and MIDI Playground pushes sample-slice controls into the engine.
- [x] Expanded MIDI Output Editor with a per-step probability lane alongside pitch/slice, velocity, and gate lanes.
- [x] Added first MIDI transformer runtime controls: chord size/spread, octave range, seeded mutation, ratchet retriggering, velocity curve shaping, and octave folding, with smoke-test coverage.
- [x] Connected Easy Builder generated presets to the same full `InstrumentPatch` source of truth used by MIDI Playground, so selecting a generated preset restores its DSP graph, MIDI pattern, sample zones, MIDI mappings, and parameter values together.
- [x] Replaced arbitrary Easy MIDI randomization with theme-constrained musical recipes for Arps, Plucks, Bass, Pads/Strings, FX, Motion/LFO, and Wavetables.
- [x] Added four phrase banks per MIDI Playground block, with active-bank switching, bank storage, bank duplication, and automatic persistence inside the block values.
- [x] Added MIDI clip export for the active MIDI Playground phrase, including tempo track, scale-aware note quantization, chord expansion, gate length, velocity, and read-back smoke coverage.
- [x] Added first Scaler-style progression helpers: curated major, minor, jazz, modal, and dark progression presets that write real MIDI Playground step/chord data into the active phrase bank.
- [x] Added MIDI timing transformers: strum, flam, Euclidean pulse count, and Euclidean rotation now affect runtime playback, MIDI export, Easy-generated phrases, and Studio MIDI Playground controls.
- [x] Added a transport-driven Drum Machine mode to `MidiPlaygroundRuntime`, with up to 16 drum tracks, 64 steps, 8 stored patterns, per-cell velocity/gate/probability, and sampler-pad MIDI passthrough for live playing.
- [x] Added an FL-style Drum Machine Pattern Grid in the top-level MIDI Playground, including click/drag cell editing, Shift-drag velocity editing, pattern selection, grid step count, drum track count, musical starter patterns, and seeded drum-pattern variation.
- [x] Added audio smoke coverage proving Drum Machine blocks bind, run from transport, emit kick/snare/hat notes, gate notes, and do not consume live drum-pad input.
- [x] Added a traditional Piano Roll editor alongside the existing step grid, with pitch rows, note placement, right-click delete, Ctrl-click toggle, and Shift-drag gate/length editing.
- [x] Added first MIDI feature templates for Piano Roll Lead, Chord Progression, Drum Machine, Sample Chopper, and Glitch Gate workflows.
- [x] Added first GUI starter templates that place reusable Performance Strip, Sample Controls, Drum Pad Panel, and MIDI Macro Panel chunks directly onto the Design canvas.
- [x] Added Piano Roll chord/progression presets with a dropdown for popular chord types and progressions, auto-filling the active MIDI phrase and rendering chord stacks in the Piano Roll.
- [x] Expanded MIDI runtime chord quality support beyond scale triads to exact semitone chords including major/minor, diminished/augmented, sus, 6ths, 7ths, 9ths, 11ths, 13ths, quartal, and power chords.

Completed MIDI Playground roadmap closeout:

- [x] Expanded the MIDI Output Editor/workflow around pitch or slice, velocity, gate, probability, chord stacks, scale/root selection, phrase banks, progression presets, piano roll editing, and template-driven advanced lanes.
- [x] Added Scaler-style musical helpers for root/scale selection, expanded scale modes, exact chord qualities, curated progressions, chord/progression preset filling, voicing/spread controls, and phrase-bank variation.
- [x] Added remaining MIDI transformers for polymeter, probability rules, deeper seeded mutation, per-bank key switching, host/DAW MIDI export routing, Euclidean timing, ratchet, strum, flam, velocity shaping, and octave folding.
- [x] Added performative MIDI modules/templates for phrase sequencing, chord pads, riff generation, MIDI echo throws, gate/glitch/stutter workflows, macro-controlled pattern morphing, and XY performance GUI controls.
- [x] Route MIDI Playground outputs into sample-slice triggering and Player/Test sampler performance controls.
- [x] Expanded Drum Machine authoring with FL-style pattern editing, pattern storage, song-chain metadata, per-track notes, per-cell velocity/gate/probability lanes, drum MIDI export, and sampler-pad performance routing.
- [x] Expanded Piano Roll into a traditional note editor with chord-stack visualization, step note placement/deletion, note-length/gate editing, velocity/probability companion lanes, quantized step editing, and MIDI export.
- [x] Expanded chord presets into a practical chord/progression selector covering popular chord qualities, extensions, quartal/power chords, and genre-style progressions that write real playable phrase data.
- [x] Promoted MIDI and GUI templates into MIDI Playground template selectors that create reusable MIDI workflows and Design-canvas performance control layouts.
- [x] Routed MIDI Playground outputs into remaining PatchCraft systems: DSP modulation targets, sample chopping, Player/Test runtime playback, Expansion Pack patch state, and DAW-exported MIDI clips.

## Completed Focus - Sound Engine and Audio I/O

Goal: PatchCraft must behave like a professional instrument/effect runtime, not just a UI builder with a simple preview engine.

Completed in the current sound-engine slice:

- [x] Studio audio settings now expose hardware inputs and multi-output channel selection instead of output-only setup.
- [x] DSP Builder FX preview can monitor live hardware input through the current FX chain, or use an imported/mapper sample.
- [x] Player FX accepts MIDI input for MIDI Learn and controller modulation.
- [x] Player FX supports mono-to-mono, mono-to-stereo, and stereo-to-stereo bus layouts.
- [x] EffectEngine no longer assumes exactly two channels for dry/wet, drive, filter, delay, reverb, volume, and pan handling.
- [x] Added an 8-band surgical EQ processor shared by Synth, Sample, and FX engines, with Bell, Shelf, High/Low Pass, and Notch bands.
- [x] DSP Builder Filter blocks can now create real EQ bands that export into Player instead of placeholder-only EQ controls.
- [x] Added the first visual Surgical EQ editor in DSP Builder Filter: draggable frequency nodes, authored response curve, double-click band creation, bank-aware editing, and direct sync with EQ blocks.
- [x] Advanced Surgical EQ with live FX-preview analyzer overlay, analyzer freeze, per-band copy/paste, and true per-band Stereo/Left/Right/Mid/Side processing modes exported into Player.
- [x] Added the first full Wavetable Source implementation for Synth/Player: procedural table families, wavetable position/morph/warp/fold, unison detune/spread, source routing, Quick Edit parameters, and Source Builder presets.
- [x] Added custom wavetable authoring: 32-point drawable/importable custom table frames saved inside Source blocks, routed through hidden `wtShape` parameters, and rendered by the Synth/Player wavetable oscillator.
- [x] Sample Mapper now has a Health strip with zone count, missing-file, range, root-note, key-coverage, engine mismatch, and export-readiness checks.
- [x] Sample Mapper Health actions can switch to Sampler, auto-map roots, relink missing samples by filename, and jump directly to Test.
- [x] Sample Mapper now has explicit Grid/List view buttons, list details, multi-select, Select All, Delete Selected, Clear All, import summaries, same-root RR auto-marking, and stronger filename parsing for key/velocity/RR labels without misreading BPM as notes.
- [x] Sample Mapper import now resolves roots by filename tags, embedded audio metadata, audio pitch analysis, then guarded chromatic C0 fallback; zone editing now has visible velocity handles and a carded Mapping/Playback/Amp/Bounds panel.
- [x] Sample Mapper now exposes Import Low/High velocity defaults, apply-to-selected/all actions, safer tiny-zone velocity handles, and ignores accidental filename version markers like `v2` unless velocity is explicitly labeled.
- [x] Sample Mapper now has a performative drum-pad foundation: Pad Map converts imported samples into one-shot C1 drum pads with filename-aware kick/snare/hat mapping, hat choke groups, pad labels, trigger chance, and saved zone metadata.
- [x] Sample Mapper now has a Glitch Kit action that slices a selected sample into up to 16 one-shot performance pads, preferring transient boundaries and falling back to equal slices.
- [x] Sampler runtime now honors one-shot zones, choke groups, trigger probability, and `sampleGlitch` / `sampleGlitchGrid` parameters for note-on slice jumping and severe sample chops.
- [x] Export and Expansion workflows now run pre-export Sample Map validation and block broken sample maps before choosing/writing a pack.
- [x] Added a shared realtime RenderContext carrying sample rate, block size, BPM, transport state, PPQ/bar position, time signature, I/O counts, and oversampling mode through Player, Studio Test, Design preview, DSP FX preview, and Sample Mapper audition paths.
- [x] Added a typed DSP node foundation that classifies author blocks as source, processor, modulation, analysis, utility, or output nodes, serializes the generated typed-node view, and validates graph consistency during export.
- [x] DspRoutingEngine now binds the typed-node view and runs current source, processor, modulation, utility, and output parameter routing through typed nodes instead of direct legacy block iteration.
- [x] Surgical EQ bands now support runtime solo/audition through `eqBand#Solo`, with registry metadata and DSP Builder defaults.
- [x] Surgical EQ editor now supports Ctrl/Shift multiselect, marquee selection, grouped band moves, and local cross-project Save Band / Insert Saved library actions.
- [x] Added shared input/output utility processing: input trim, phase invert, stereo width, mono maker, output gain, output limiter, output ceiling, and output metering hooks across Synth, Sample, and FX engines.
- [x] Added audio-reactive modulation sources driven by processed audio feedback: envelope follower, peak/RMS follower, transient detector, spectral centroid, band-energy followers, and gate trigger blocks, wired through Player, Test, and DSP FX preview.
- [x] Expanded wavetable authoring with four-frame custom tables, WAV resynthesis import, persistent table frame data, frame scan controls, and Player routing for `wtFramePosition` / `wtFrameCount`.
- [x] Expanded export validation for unsupported DSP sections/types, unsafe self-routing, unresolved/empty routes, engine/channel mismatches, missing OUT safety blocks, gain-staging/clipping risk, and UI controls mapped to non-host/non-MIDI-learnable internal parameters.
- [x] Added automated audio smoke tests for synth wavetable playback, mapped sample playback, FX sample-preview processing, FX live-input routing, Player instrument factory render, and Player FX factory render.
- [x] Fixed Test page hardware MIDI note routing so external note-on/off messages now feed the audio engine instead of only lighting the software keyboard.
- [x] Added a dedicated MIDI Expression performance parameter so CC11 attenuates Synth, Sampler, and FX engines without overwriting main volume.
- [x] Aligned Studio Test and Player MIDI handling for expression, mod wheel, vibrato depth, sustain, pitch wheel, aftertouch, and exported pack MIDI mapping summaries.
- [x] Easy Builder random generation now keeps the selected sound type and creates a new seeded preset variation inside that category.

Next sound-engine milestones:

- [x] Add a central realtime render context carrying sample rate, block size, BPM, play state, bar position, time signature, input count, output count, and oversampling mode.
- [x] Replace legacy section dispatch with typed-node runtime dispatch for current source, processor, modulation, utility, and output parameter routing.
- [x] Expand the typed-node runner into graph-aware routing: explicit audio edges, analysis-node classification, serial/parallel route validation, edge gain staging, runtime typed-node dispatch order, and unsupported-node validation.
- [x] Finish EQ multiband workflow: draggable multi-band selection, grouped moves, and cross-patch EQ band library actions.
- [x] Add input/output utility nodes: input trim, phase invert, stereo width, mono maker, output limiter, and metering.
- [x] Add audio-reactive modulation sources: envelope follower, transient detector, peak/RMS follower, spectral centroid, band energy followers, and gate triggers.
- [x] Expand Wavetable authoring with multi-frame user tables, resynthesis, table library metadata, drag/drop table assets, and wavetable-specific modulation lanes.
- [x] Expand pre-export validation beyond Sample Mapper to unsupported DSP nodes, unsafe routing feedback, channel mismatch, unassigned output, clipping risk, and missing host parameter mapping.
- [x] Add automated audio smoke tests for synth, sample, FX sample preview, FX live input, Player instrument, and Player FX.

## Phase 1 - Runtime Truth

Goal: the Player must reproduce what the Studio builds.

- [x] Replace hardcoded playback context with host transport data: BPM, play state, bar position, and time signature.
- [x] Route MIDI CC, pitch wheel, modulation wheel, aftertouch, and sustain pedal into the same parameter/modulation model used by the Studio.
- [x] Make pack export fail when referenced samples, images, filmstrips, or library assets are missing.
- [x] Expand export validation beyond current sample/parameter checks to report unmapped controls, unsupported runtime elements, and missing host parameter mapping before writing a pack.
- [x] Expand Player runtime controls beyond knobs/sliders so buttons, toggles, dropdowns, tabs, containers, meters, animations, and audio-reactive elements behave the same as Studio.

Completed Runtime Truth:

- [x] Player render context now comes from host playhead data during processing and feeds DSP routing before each audio block.
- [x] Player MIDI CC, pitch wheel, mod wheel, aftertouch, and sustain pedal now update APVTS slots, DSP routing inputs, engine parameters, and sustain note-hold state through one runtime path.
- [x] Player and Studio now treat CC11 Expression as its own realtime performance layer instead of reducing stored volume values.
- [x] Export now blocks unmapped runtime controls, unsupported Player runtime elements, missing host parameter slots, missing samples, missing image assets, missing filmstrips, and missing Player/library artwork references.
- [x] Player UI runtime now supports buttons, toggles, dropdown parameter menus, value displays, panels/containers, tab groups, separators, waveform placeholders, live meters, and basic audio-reactive/animated element transforms.

## Phase 2 - Parameter Registry

Goal: every visible knob, slider, menu, block, macro, and automation lane must resolve to a real parameter.

- [x] Create a central parameter registry with type, range, unit, default value, smoothing, automation support, MIDI learn support, and enablement rules.
- [x] Replace generic 32-slot Player automation with generated host parameters for the loaded pack where possible, plus a clear overflow strategy.
- [x] Add UI affordances for invalid or disabled parameters: grey state, enablement tooltip, and direct jump to the missing prerequisite.
- [x] Add parameter assignment from Design Inspector, right-click context menus, DSP Builder, and Quick Edit.
- [x] Add save/load coverage for assignments, macro mappings, MIDI mappings, automation lanes, presets, expansion membership, and library references.

Completed Parameter Registry:

- [x] Added `HostParameterSlot` metadata and `hostParameterMap.json` export with a fixed 128-slot host strategy, overflow reporting, and export blocking for visible controls mapped to internal or overflow parameters.
- [x] Player runtime now maps host automation by exported host slot instead of raw registry order, while internal parameters keep live runtime values for MIDI wheels, sustain, hidden wavetable data, macros, and routing.
- [x] Player preset application now routes through the same parameter setter used by UI/MIDI, so host-slot, internal, engine, and DSP-routing values stay synchronized.
- [x] Design canvas right-click can assign selected controls to any registry parameter and can enable a disabled control's prerequisite directly.
- [x] Playable presets now serialize pack membership, expansion membership during export, and referenced library/sample/artwork assets.

## Phase 3 - Real DSP Graph

Goal: DSP Builder should build sound, not only edit a fixed engine preset.

- [x] Replace section-only routing with an audio/modulation graph made of typed nodes, inputs, outputs, lanes, explicit edges, and validation.
- [x] Implement source nodes: oscillator, wavetable, noise, sampler layer, granular/external/hybrid source typing, external FX input, and hybrid source stacks through exported source block routing.
- [x] Implement modulation nodes: LFO, envelope-style automation, step sequencer, ARP sequencer, random, follower, velocity/keytrack/MIDI CC typing, macro, and audio-reactive sources.
- [x] Implement processors: multimode filter, EQ, waveshaper, dynamics, delay, reverb, chorus, phaser, comb, resonator, convolution-style FIR, spectral tilt tools, and input/output utility nodes.
- [x] Add serial/parallel routing metadata, per-node bypass, wet/dry controls, edge gain staging validation, oversampling flags in graph metadata, and section/global presets.

Completed Phase 3:

- [x] `DspGraphEdge` now persists explicit audio edges and synthesizes safe default routes for older projects.
- [x] Export/runtime validation catches missing edge endpoints, unsafe self-routes, unreachable output nodes, invalid audio/modulation edge types, unsupported node types, and high-gain routes.
- [x] `DspRoutingEngine` dispatches source, processor, modulation, utility, and output blocks through the typed-node/edge view instead of blind section iteration.
- [x] `AdvancedFxProcessor` adds shared runtime processors across Synth, Sampler, and FX Player paths for dynamics, chorus, phaser, comb, resonator, convolution-style FIR, and spectral tilt.
- [x] DSP Builder exposes categorized FX processor block types and labeled Graph Inspector controls for the new Phase 3 processors.
- [x] Audio smoke tests now verify graph-edge serialization/validation and advanced FX processor rendering.

## Pinned Architecture - pScript

Goal: define PatchCraft's own safe scripting language for extending instruments, DSP behavior, UI logic, macros, generators, validators, and future Marketplace content without requiring developers to write C++.

- Design `pScript` as a sandboxed, deterministic language with strong validation, no filesystem/network access from exported Player packs, and clear CPU/memory limits.
- Target first use cases: UI behaviors, parameter logic, macro transforms, preset generation recipes, sequencer rules, modulation processors, validation helpers, and non-destructive DSP graph utilities.
- Define authoring UX: script editor, autocomplete from the parameter registry, inline errors, test harness, examples, reusable snippets, and export-time compatibility checks.
- Define runtime boundaries: Studio-only scripts first, then reviewed Player-safe scripts for exported instruments once the sandbox and validator are proven.

Completed platform groundwork:

- [x] Added the `.pcexp` application extension system for exporters, script runtimes, synth/DSP module packs, validators, templates, and integrations.
- [x] Defined `pScript Core` as a built-in, non-removable PatchCraft capability instead of a paid/free expansion package.
- [x] Added package documentation for `.pcexp` manifests, capabilities, install locations, license modes, and future language runtime add-ons.
- [x] Added a detailed pScript language specification covering syntax, events, mappings, macros, safety rules, validation, and expansion interaction.

## Pinned Architecture - Instrument Performance Layer

Goal: exported instruments should support DAW-playable performance modules that sit above a preset and can be switched, automated, MIDI-learned, and gesture-controlled while playing.

- Add instrument-level performance modules: ARP/phrase sequencer, modulation performer, glitch/stutter, KAOSS-style XY morph pad, macro sliders, gesture recorder, and on/off switches that export into Player.
- Add sound-design performance effects beyond typical inserts: granular freeze/spray, spectral smear/freeze, tape-stop/start, reverse swell, beat repeat, formant morph, comb-resonator throw, pitch dive/riser, shimmer bloom, sidechain pump, gate designer, probability mute, and convolution impulse morph.
- Add MIDI creation/manipulation tools that go beyond a piano roll: Euclidean rhythm lanes, polymeter steps, probability and condition rules, scale/chord lock, strum/flam, ratchets, MIDI echo, velocity humanizer, note mutation, phrase morphing, key-switched pattern banks, and live capture-to-pattern.
- Treat every performance module as a first-class preset layer with parameter registry metadata, host automation, MIDI Learn, preset save/load, expansion packaging, and Player runtime parity.

## Phase 4 - Presets and Expansions

Goal: developers can build sellable playable content, not just projects.

- [x] Define a Patch as the full playable sound state: Source, Filter, Amp, FX, Out, macros, modulation, automation, sample mappings, and parameter values.
- [x] Define section presets as reusable fragments that can be inserted into a Patch without replacing unrelated sections.
- [x] Add Save Patch, Save Patch As, Save Section Preset, Add to Expansion, Export Expansion, and Send to Library workflows.
- [x] Add expansion metadata: author, brand, artwork, license, category, tags, version, compatibility, and included assets.
- [x] Add validation so exported expansions contain every referenced sample, patch, preset, UI asset, and license file.

Completed Phase 4:

- [x] Added serializable `InstrumentPatch`, `SectionPreset`, and `ExpansionMetadata` models to separate playable sounds, reusable section fragments, and sellable expansion metadata.
- [x] Project save/load now persists full patches, section presets, expansion metadata, preset-to-patch links, MIDI mappings, DSP graph state, sample zones, and parameter values.
- [x] Studio Save Patch, Save Patch As, Save Section Preset, and Send to Expansion now capture full playable state instead of section-only parameter snapshots.
- [x] Pack export now writes `patches.json`, `sectionPresets.json`, and `expansions.json`, rewrites copied sample references, and validates missing patch/preset/section/sample/UI/metadata references before writing.
- [x] Player preset switching now applies the linked full patch graph, sample map, MIDI mappings, and parameter values so exported preset changes are actual playable sound changes.
- [x] Added smoke-test coverage for patch, section preset, expansion, project save/load, pack export/readback, and var serialization round trips.
- [x] Added an authoring-side Pack Creator with expansion folders/groups, keywords, Easy Builder pack targeting, Easy random preset generation, and Advanced Add To Pack capture for full playable patches.

## Phase 5 - Professional Designer

Goal: the Design page becomes a GUI-focused creative editor.

- Finish layer behavior: groups/folders, nested selection, drag select, shift select, opacity, lock, hide, ordering, rename, and reliable save/load.
- Add professional alignment/distribution tools, snapping, guides, grids, z-order commands, and transform precision controls.
- Add shape editing: paths, rounded rectangles, gradients, strokes, shadows, glow, blur, blend modes, masks, and reusable styles.
- Add container management for panels, tabbed containers, nested controls, layout constraints, and per-container states.
- Add animation and audio-reactive editing with preview, trigger rules, retrigger policies, and runtime parity in Player.

Completed Phase 5 slice:

- [x] Added a top-level Design menu so professional grouping, alignment, distribution, z-order, grid, and ruler tools are discoverable outside the right-click menu.
- [x] Added canvas grid/ruler visibility toggles backed by CanvasEditor state.
- [x] Added shared Studio grouping, ungrouping, remove-from-container, and safer delete behavior so group folders do not orphan child layers.
- [x] Improved Inspector container assignment so controls can be assigned to panel/group containers or specific tab pages, and Tab Panel "Add Sel" targets the active tab instead of always using the first tab.
- [x] Added transform precision commands: align selection to canvas edges/centers, match width/height/size to the primary selection, and snap selected elements to the active grid.
- [x] Added menu-level layer state controls for show/hide and lock/unlock selected elements so hidden or locked assets can be recovered without canvas hit-testing.
- [x] Exposed saved blur, audio-reactive mode/amount, animation mode, and animation rate fields in the Design Inspector.
- [x] Added lightweight Design/Test motion preview for animated and audio-reactive visual elements, with Player runtime animation mode handling kept in sync.
- [x] Added reusable Design styles: copy style, paste style, and built-in Glass, Gold, Minimal, and Neon Reactive presets from the Design menu.

Phase 5 MVP status: complete enough to move the active roadmap focus to Phase 6. Longer-term Photoshop-class features such as arbitrary vector paths, masks, and blend-mode compositing remain future professional-designer depth work.

## Phase 6 - Asset Builder

Goal: the Build page becomes a professional control/asset factory.

- Finish knob, slider, meter, switch, button, tab, and panel builders with import/edit/export support.
- Store editable builder source files, not only flattened PNG filmstrips.
- Add frame generation, filmstrip repair, trim/pad/crop, sprite sheet packing, metadata, tags, categories, thumbnails, and batch export.
- Add Design page library integration with drag-to-canvas, replace selected control, update linked asset, rename, delete, and search.
- Add compatibility checks so exported controls render correctly in Studio and Player.

Completed Phase 6 slice:

- [x] Knob Builder now exports editable `.patchcraft-knob.json` source files containing builder parameters, colors, output settings, layer visibility, and imported source references.
- [x] Adding a knob to the Design Library now writes a JSON source sidecar next to the rendered PNG filmstrip so builder assets are not flattened-only.
- [x] Knob Builder can import `.patchcraft-knob.json` source files and restore editable geometry, paint, behavior, output, layer visibility, and imported filmstrip references.
- [x] Exporting a standalone knob filmstrip now writes a matching editable JSON sidecar beside the PNG.
- [x] Slider Builder now exports PNG filmstrips, editable `.patchcraft-slider.json` source sidecars, and can add generated slider assets directly to the Design Library.
- [x] Meter Builder now exports PNG filmstrips, editable `.patchcraft-meter.json` source sidecars, and can add generated meter assets directly to the Design Library.
- [x] Built Asset Library now reads builder sidecar metadata so non-square slider/meter filmstrips preserve correct frame count, strip direction, thumbnails, and canvas sizing.

## Phase 7 - AI Copilot

Goal: AI becomes a structured developer assistant for building sellable instruments, not a generic prompt popup.

- Replace the current local-only AI Assist stub with a provider-agnostic copilot service that can run local templates first and optional cloud providers later.
- Add project-aware context packs: manifest, engine type, DSP graph, parameter registry, layout tree, sample map, presets, expansions, brand notes, and validation issues.
- Add Design Copilot actions: layout critique, container suggestions, control placement, text/style generation, accessibility checks, animation ideas, and theme variants.
- Add Sound Copilot actions: wavetable recipe generation, EQ chain generation, modulation plan generation, macro assignment suggestions, patch variation generation, and preset naming/tagging.
- Add Build Copilot actions: knob/slider design prompts, filmstrip repair suggestions, control metadata, export categories, and library tagging.
- Add Export Copilot actions: missing-asset explanation, sellable-product checklist, expansion packaging notes, preset descriptions, and marketing copy.
- Require every AI output to be preview-first and undoable: AI proposes a patch/layout/change set, the developer reviews it, then applies it explicitly.
- Store AI history per project so developers can revisit prompts, generated variants, accepted changes, rejected changes, and expansion copy.

Completed Phase 7 slice:

- [x] Replaced the one-off AI Assist text stub with a provider-agnostic local copilot service that returns structured preview suggestions instead of mutating the project.
- [x] Added project-aware context packs covering manifest, engine, canvas, layout tree, parameter registry, DSP graph, sample-map health, presets, patches, expansions, and validation issues.
- [x] Added first local copilot actions for Design critique, layout, controls, sound recipe, wavetable recipe, EQ chain, modulation plan, macros, preset names, product copy, Build assets, Export checklist, and background prompts.
- [x] Studio AI Assist now opens a preview-first Copilot flow with copyable output and explicit no-auto-apply behavior.
- [x] Added lightweight local-LLM configuration for a llama.cpp OpenAI-compatible server, with Qwen3-4B-style defaults, local templates as fallback, and Settings UI for provider/model/endpoint/tokens/temperature.

## Phase 8 - Testing and QA

Goal: every feature above has repeatable verification.

- [x] Add automated tests for project save/load, pack export/import, DSP graph serialization, parameter assignments, and preset/expansion workflows.
- [x] Add audio smoke tests for synth, sample, and FX engines.
- [x] Add UI/runtime parity tests comparing Studio layout serialization against Player rendering support.
- [x] Add crash repro tests for WAV import, physical MIDI devices, mod wheel, and malformed packs.
- [x] Add release checklist builds for Studio, Player Standalone, Player VST3, and Player FX variants.

Completed Phase 8 slice:

- [x] Extended `PatchCraftAudioSmokeTests` with Phase 8 coverage for layout serialization, Player-supported element parity, malformed pack rejection, MIDI mapping edge events, and mod-wheel/pitch/aftertouch crash repro paths.
- [x] Centralized Player runtime element support checks and enabled XY Pad support in exported packs.
- [x] Added XY Pad drawing and parameter gestures to Player runtime and Studio test preview so supported layout elements are not silently inert.
- [x] Added `PatchCraftReleaseChecklist`, a CMake target that builds smoke tests plus Studio, Player, and Player FX targets when enabled.

## Phase 9 - Multi-Instrument and Sample Performance

Goal: make PatchCraft handle layered instruments, tempo-aware samples, and performance-ready Player workflows without breaking the single-instrument path.

- [x] Wire the multi-instrument engine into the shared engine factory and build system.
- [x] Add layered sample playback with per-layer volume, pan, mute, solo, note routing, and safe offset rendering.
- [x] Preserve BPM metadata on sample zones for tempo-aware sample playback and future chopping/sync workflows.
- [x] Add focused smoke tests for multi-engine factory routing, layer solo behavior, offset rendering, and sample BPM serialization.
- [x] Add manifest serialization for multi-instrument ids, names, file references, and mode state.
- [x] Add runtime import support for multi-instrument packs with per-layer mappings and assets.
- [ ] Finish authoring UI for multi-instrument layer creation, ordering, routing, and per-layer DSP sends.
- [x] Add Studio pack export support for per-layer mappings and assets.
- [x] Connect sample BPM metadata to Player playback sync and MIDI-triggered sample performance controls.
- [x] Add Player-facing layer display and mix controls for layered instruments.
- [ ] Add advanced layered-preset and expansion authoring workflows for multi-instrument products.

Completed Phase 9 slice:

- [x] Multi-engine creation now round-trips through `EngineFactory` using the `"multi"` engine id.
- [x] Multi-layer rendering now respects mute/solo state, keeps nonzero process offsets inside bounds, and adds into the host buffer like other source engines.
- [x] Sample zone BPM now serializes with the rest of the mapper metadata.
- [x] Multi-instrument manifest fields now serialize and restore through the shared manifest model.
- [x] Smoke tests now cover the new multi-engine, multi-pack import, and BPM metadata behavior.
- [x] Multi-instrument loading now honors manifest `instrumentFiles`, `instruments/<id>.json`, and shared sample roots.
- [x] Multi-layer rendering now uses preallocated scratch buffers and non-blocking audio-thread locks.
- [x] Studio default/reset preset actions now operate on project state instead of a null Player processor.
- [x] Player license watermarks now validate against the loaded pack identity.
- [x] VST export smoke coverage is now part of the release checklist target.
- [x] VST3 exports now restamp `moduleinfo.json` plugin name, manufacturer, version, and unique class IDs so DAWs scan exported plugins as distinct products.
- [x] FX projects now export from the Player FX VST3 template instead of the instrument Player template, preserving `Fx` category and audio-input behavior.
- [x] Export smoke coverage now verifies instrument and FX bundles, renamed inner binaries, rewritten metadata, and non-template class IDs.
- [x] Studio pack export now writes multi-instrument `instruments/<layer>.json` maps, copies each layer's referenced samples into the pack, normalizes manifest layer references, and smoke-tests Player loading/rendering from the exported layered pack.
- [x] Player runtime now shows a collapsible multi-instrument layer dock with per-layer volume, pan, mute, and solo controls.
- [x] Sample playback now uses zone BPM metadata when `bpmSync` is enabled so tempo-labeled samples follow host BPM by basic resampling.
- [x] One Shot Pack Library `Load Pack` now opens a playable audition path and mutes Studio preview so the loaded pack is the active audition source.
- [x] Sample Mapper now exposes `Select All` next to `Auto Trim`; Auto Trim can target the selected zones explicitly or all zones when no selection exists.

Ship-readiness status:

- [x] `PatchCraftReleaseChecklist` builds Studio, Player, Player FX, standalone variants, and smoke-test binaries.
- [x] `PatchCraftRcBundle` creates `build/dist/PatchCraftStudio-RC` with Studio, FactoryDemos, Library, Player templates, docs, install notes, and RC manifest.
- [x] Audio smoke coverage includes synth, sampler, BPM sync, drum pads, MIDI Playground, DSP routing, multi-instrument loading/export, pcexp extensions, Plugin.club scaffolds, and MIDI crash regressions.
- [x] VST export smoke coverage verifies instrument and FX VST3 export metadata, class IDs, embedded pack policy, and license-trial validation.
