# PatchCraft Studio - Current Ship Handoff

**Date:** May 24, 2026
**Status:** Release-candidate bundle verified, Windows and macOS repositories synced and pushed.

## Current Release Candidate

- RC bundle: `build-codex/dist/PatchCraftStudio-RC`
- Studio app: `build-codex/dist/PatchCraftStudio-RC/PatchCraftStudio.exe`
- Player VST3: `build-codex/dist/PatchCraftStudio-RC/PlayerPlugins/PatchCraft Player.vst3`
- Player FX VST3: `build-codex/dist/PatchCraftStudio-RC/PlayerPlugins/PatchCraft Player FX.vst3`
- Studio installer: `build-codex/dist/PatchCraftStudio-RC/installer/Output/PatchCraftStudio-Setup.exe`
- VST Exporter installer: `build-codex/dist/PatchCraftVstExpansion/installer/Output/PatchCraftVstExpansion-Setup.exe`
- User VST3 install path used for testing: `C:\Users\seth\AppData\Local\Programs\Common\VST3`

## Current Validation

- `cmake -S . -B build-codex -DPATCHCRAFT_ENABLE_AI_STUDIO=OFF`
- `cmake --build build-codex --target PatchCraftReleaseChecklist --config Release --parallel 6`
- `build-codex\bin\Release\PatchCraftAudioSmokeTests.exe`
- `build-codex\bin\Release\PatchCraftVstExportSmokeTest.exe`
- `cmake --build build-codex --target PatchCraftRcBundle --config Release --parallel 6`
- `cmake --build build-codex --target PatchCraftVstExpansionPackage --config Release --parallel 6`
- `ISCC.exe build-codex\dist\PatchCraftStudio-RC\installer\PatchCraftStudio-Windows.iss`
- `ISCC.exe build-codex\dist\PatchCraftVstExpansion\installer\PatchCraftVstExpansion-Windows.iss`

## Current Ship Notes

- Multi-instrument exported packs load in Player and now show a compact layer dock with volume, pan, mute, and solo.
- One Shot Pack Library `Load Pack` now opens a playable audition flow and mutes Studio preview first.
- Sample Mapper has explicit `Select All` beside `Auto Trim`; Auto Trim processes the selected zones or all zones if nothing is selected.
- Zone BPM metadata now affects sample playback when `bpmSync` is enabled, using basic tempo-ratio resampling.
- Launch licensing uses Plugin.club now; AudiLock is documented as the future source of truth in `docs/AUDILOCK_LICENSING_PLAN.md`.
- Player now includes Rack, Mixer, Snapshots/Favorites, Sound DNA, runtime import, and expanded commercial launch materials; see `docs/PLAYER_COMMERCIAL_SYSTEM.md`.
- See `docs/SHIP_READY.md` for the release handoff and remaining manual QA.

---
# PatchCraft Studio - Development Handoff

**Date:** May 3, 2026
**Branch:** main
**Status:** Build Successful

---

## Summary of Changes

This push contains fixes for the Test page audio, DSP page UI sizing, and MIDI keyboard input handling.

---

## Files Modified

### 1. Source/Studio/DspPage.h
- **Added label declarations for ARP section:** `rateLabel`, `gateLabel`, `swingLabel`, `octavesLabel`
- **Added label declarations for ENV section:** `attackLabel`, `decayLabel`, `sustainLabel`, `releaseLabel`

### 2. Source/Studio/DspPage.cpp
- **Reduced knob sizes:** Changed padding from `reduced(2)` to `reduced(8)` in OSC, LFO, ENV, ARP, and FX sections
- **Reduced ComboBox sizes:**
  - OSC waveform: 80 ΓåÆ 60
  - LFO waveform: 80 ΓåÆ 60
  - ARP mode: 120 ΓåÆ 80
  - ARP pattern: 80 ΓåÆ 60
  - FX filter type: 80 ΓåÆ 60
- **Reduced control heights:**
  - Tab bar: 32 ΓåÆ 24
  - Section titles: 24 ΓåÆ 16
  - Margins: 8px ΓåÆ 4px (or less)
- **Slider text boxes:** 40├ù15 ΓåÆ 32├ù12
- **Label font:** Increased to 12.0f for better visibility
- **Added label positioning:** Labels now appear above sliders in all sections

### 3. Source/Studio/BottomPanel.h
- **Added `testEngine` member:** `std::unique_ptr<IInstrumentEngine>` for audio rendering

### 4. Source/Studio/BottomPanel.cpp
- **Fixed audio engine creation:** Defaults to "synth" engine type when none specified
- **Fixed sample loading:** Now only loads samples for `engineType == "sample"` (not synth)
- **Added MIDI keyboard callbacks:** Connected `studioRenderer->onNoteOn/onNoteOff` to `testEngine->noteOn/noteOff`
- **Added audio callback implementation:** `audioDeviceIOCallbackWithContext`, `audioDeviceAboutToStart`, `audioDeviceStopped`

### 5. Source/Studio/StudioInstrumentRenderer.h
- **Added functional header:** For `std::function` support
- **Added MIDI callbacks:**
  - `std::function<void(int note, float velocity)> onNoteOn`
  - `std::function<void(int note)> onNoteOff`
- **Added mouse handlers:** `mouseDrag`, `mouseUp`
- **Added keyboard hit detection:** `hitKeyboardNote()` method

### 6. Source/Studio/StudioInstrumentRenderer.cpp
- **Implemented keyboard mouse handling:** Click/drag on keyboard sends MIDI notes
- **Note range:** 48-76 (C3 to C5 approximately, 28 white keys)
- **Black/white key detection:** Proper hit testing with black keys on top
- **Real-time note triggering:** Supports glissando (dragging across keys)

---

## Known Issues

1. **IDE Lint Errors:** IDE shows false errors about JUCE headers not found - these are IDE parsing issues, not actual compilation errors. The build succeeds.

2. **Dropdown Arrow Character:** Unicode character `Γû╝` (down arrow) in `drawDropdown()` may not render on all code pages (warning C4566).

3. **Test Page Audio:** Requires audio device to be selected in Settings dialog.

---

## Architecture Notes

### Audio Flow
```
User clicks keyboard ΓåÆ StudioInstrumentRenderer ΓåÆ onNoteOn callback
  Γåô
BottomPanel receives note ΓåÆ testEngine->noteOn(note, velocity)
  Γåô
Audio callback (audioDeviceIOCallbackWithContext) ΓåÆ testEngine->process(buffer)
  Γåô
Audio device output
```

### Engine Types
- **"synth"** - No samples needed, generates sound via oscillators
- **"sample"** - Loads samples from project sample map
- **"effect"** - Audio processing engine covered by FX smoke tests and Player FX export/load checks

### UI Scaling Philosophy
- All controls use `reduced(padding)` to create visual breathing room
- Smaller padding = larger controls, larger padding = smaller controls
- Consistent 8px padding used across all DSP sections for uniform knob sizing

---

## Next Steps for Full Custom Branding

To achieve fully custom instrument designs:

1. **PatchCraftLookAndFeel.h:** Make color palette configurable
   - Current: Hardcoded constants (kBgDarkest, kAccent, etc.)
   - Goal: Load from project manifest

2. **StudioInstrumentRenderer:** Add custom background support
   - Current: Uses `manifest.backgroundImage`
   - Goal: Support multiple backgrounds, custom drawing hooks

3. **Font System:** Allow custom fonts per project
   - Current: System default fonts
   - Goal: Load custom TTF/OTF fonts

4. **Control Skins:** Support image-based knobs/sliders
   - Current: Vector-drawn controls
   - Goal: Filmstrip image support for custom knobs

---

## Testing Checklist

- [ ] Open PatchCraft Studio
- [ ] Select audio device in Settings
- [ ] Click Test tab - verify audio device activates
- [ ] Click virtual keyboard - verify sound output
- [ ] Click DSP tab - verify smaller knobs and visible labels
- [ ] Check OSC, LFO, ENV, ARP, FX, MACRO, MOD sections
- [ ] Verify dropdown boxes are smaller

---

## Build Command

```bash
cmake --build M:\AudiCode\PCraft\build --config Release --target PatchCraftStudio
```

Exit code 0 = Success

---

## Contact

For questions about this handoff, refer to the code comments in the modified files or check the git commit history.
