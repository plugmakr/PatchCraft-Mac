# PatchCraft DAW QA Checklist

Use this checklist after building `PatchCraftStudio-Setup.exe` and, when needed, `PatchCraftVstExpansion-Setup.exe`.

## Install Proof

- Install PatchCraft Studio with `PatchCraftStudio-Setup.exe`.
- Confirm `PatchCraftStudio.exe` launches from the installed folder.
- Confirm PatchCraft Player and PatchCraft Player FX appear in the DAW plugin scan.
- Confirm the base install does **not** include `PluginTemplate/`.
- Install `PatchCraftVstExpansion-Setup.exe`.
- Confirm `PluginTemplate/PatchCraft Player.vst3` and `PluginTemplate/PatchCraft Player FX.vst3` are added beside Studio.

## Player Instrument Proof

- Load PatchCraft Player as an instrument in FL Studio.
- Open the Pack browser and load at least one synth demo, one sample demo, one drum demo, and one multi-layer demo.
- Confirm the Player window is fixed to the authored canvas size and does not stretch/misalign when the host window changes.
- Confirm text labels, tab titles, preset name, toolbar buttons, knobs, sliders, meters, and keyboard stay aligned.
- Click every instrument tab/container and confirm the visible controls change.
- Play software keyboard notes.
- Play hardware MIDI notes.
- Move mod wheel, expression, sustain, pitch wheel, and mapped MIDI CC controls.
- Change presets and confirm the loading overlay appears and then clears.
- Move every visible mapped control while holding a note and confirm audio responds without silence/dropout.

## Player FX Proof

- Load PatchCraft Player FX on a mixer insert.
- Load an FX/demo pack.
- Confirm incoming audio passes when bypassed and processes when enabled.
- Switch all tabs/containers.
- Move all mapped FX controls and confirm audible changes.
- Confirm no generic Player pack crossover appears inside standalone exported FX products.

## Runtime Import Proof

- Open the Player import panel.
- Drag a WAV file onto the Player.
- Drag a folder containing WAV/AIFF/FLAC files onto the Player.
- Drag a MID/MIDI file onto the Player.
- Confirm the import panel opens automatically on drag enter.
- Confirm imported samples can be auditioned from the panel keyboard.
- Confirm imported samples respond to hardware MIDI.
- Confirm imported MIDI can be applied and played by the Player transport or DAW transport.
- Save the DAW project, reload it, and confirm runtime imports are recalled.
- Use Clear Imports and confirm only the runtime overlay is removed, not the protected factory pack.

## Drum Machine Proof

- Load the drum machine demo.
- Press each pad and confirm the pad highlights and triggers the correct sound.
- Start DAW transport and confirm drum grid playback follows host timing.
- Use the Player PLAY button with DAW stopped and confirm pattern playback starts.
- Toggle cells on/off.
- Ctrl-click cells for divisions/ratchets and confirm playback changes.
- Switch drum banks/patterns and confirm each pattern has independent cell data.
- Test mute, solo, volume, pan, and mixer LED response.

## Branded Product Export Proof

- In Studio, open Launch and create a Launch Bundle.
- Confirm `payload/Packs/<product>.patchcraft` exists.
- Confirm `payload/PlayerPlugins` exists.
- Compile or inspect `installer/windows-inno-setup.iss`.
- Install the generated customer installer on a clean user account or VM.
- Confirm the branded pack loads in PatchCraft Player without requiring Studio.
- Confirm licensing/support/manual/store links show the configured Brand Lab values.

## VST Expansion Proof

- Before installing VST Expansion, confirm Studio explains that standalone VST3 export is locked.
- Install VST Expansion.
- Export one synth standalone VST3 and one FX standalone VST3.
- Rescan FL Studio.
- Confirm each exported plugin has its own name, manufacturer, version, artwork, embedded pack, and no generic Pack loader unless intentionally enabled.
- Confirm exported standalone plugins do not collide with PatchCraft Player or each other.

## Plugin.club Proof

- Configure `https://plugin.club/functions/sellerImport` and seller API key in Settings.
- Configure `https://plugin.club/functions/deviceAuth` as the launch License URL. AudiLock should later replace this backend without changing product IDs or pack metadata shape.
- Publish a PatchCraft instrument pack draft.
- Publish a one-shot/sample pack draft.
- If VST Expansion is installed, publish a standalone VST3 draft.
- Confirm Studio shows success, draft id, archive path, payload path, and an Open Draft option.
- Open the draft URL and verify title, artwork, metadata, price, tags, compatibility, and license config.
- Activate one protected Player product and confirm the license state, trial/offline grace, and support links show correctly.

## Fail Criteria

Stop the release if any of these occur:

- Silent output after loading a pack.
- Hardware MIDI lights the UI but does not trigger audio.
- Player window resizing causes layout drift.
- Tabs do not switch the visible controls.
- Runtime samples/MIDI cannot be imported in standalone or DAW plugin mode.
- Drum grid has no playback path.
- Mixer faders move but do not affect audio or layer state.
- Exported standalone VST3 shares identity with another PatchCraft plugin.
- Plugin.club upload returns an HTML page, 405, 5xx, or no draft URL.
