# PatchCraft Installer System

This folder contains source-controlled installer tooling for products created with PatchCraft.

The system has two shipping modes:

- `StudioStandalone`: installs a standalone app build, currently used for PatchCraft Studio beta/testing builds.
- `PlayerInstrument`: installs a white-label Player instrument/plugin package for a customer-facing product.

Windows installer generation is handled by:

```powershell
.\InstallerSystem\Windows\Build-PatchCraftInstaller.ps1
```

The script stages payload files, writes an Inno Setup script, writes an installer manifest, and optionally runs Inno Setup if `ISCC.exe` is installed. It also writes:

- `white-label-product.json`: product, payload, install, license, and support metadata.
- `license-activation.json`: activation request template for Plugin.club/AudiLock.
- `artifact-manifest.json`: SHA256 hashes for every staged file.

## Player Instrument Example

```powershell
.\InstallerSystem\Windows\Build-PatchCraftInstaller.ps1 `
  -Mode PlayerInstrument `
  -ProductName "EchoCraft" `
  -Publisher "Your Company" `
  -Version "1.0.0" `
  -Vst3Bundle "C:\Users\seth\OneDrive\Documents\PatchCraft\VST3 Exports\EchoCraft.vst3" `
  -PackFolder "M:\AudiCode\PCraft\FactoryDemos\EchoCraft.patchcraft" `
  -LicenseRequired `
  -RequireLicenseOnFirstRun `
  -LicenseProductId "echocraft-001" `
  -LicenseServerUrl "https://plugin.club/functions/v1/activateLicense" `
  -TrialDays 7 `
  -SupportUrl "https://plugin.club/support" `
  -Compile
```

Use `-Vst3Scope PerUser` for beta testers who do not have admin access. Use
`-Vst3Scope System` only for a signed/elevated public installer that writes to
`{commoncf64}\VST3`.

## Studio Standalone Example

```powershell
.\InstallerSystem\Windows\Build-PatchCraftInstaller.ps1 `
  -Mode StudioStandalone `
  -ProductName "PatchCraft Studio Beta" `
  -Publisher "AudiCode" `
  -Version "1.0.0-beta" `
  -StandaloneExe "M:\AudiCode\PCraft\build-codex\bin\Release\PatchCraftStudio.exe" `
  -Compile
```

## Required QA

- Install on a clean Windows user profile or VM.
- Confirm uninstall does not delete user presets, imported samples, or recorded content.
- For Player instruments, rescan in at least one DAW and verify the plugin name, presets, MIDI input, audio output, and bundled pack.
- For Studio standalone, confirm the app launches, templates load, sample import works, and VST3 export can write to the user Documents export folder.
