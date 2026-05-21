# User-Only Ship Tasks

Date: 2026-05-21

These items require your accounts, hardware, DAW environment, signing credentials, legal decisions, or production infrastructure. They cannot be fully completed from the local code checkout.

## Licensing And Commerce

- Create the real Plugin.club seller products that will be used for launch.
- Confirm the Plugin.club seller API key is valid for `https://plugin.club/functions/sellerImport`.
- Confirm Plugin.club runtime activation works through `https://plugin.club/functions/deviceAuth`.
- Decide the final AudiLock endpoint names, product ID strategy, public key format, activation limits, revoke/reset rules, trial rules, and migration behavior.
- Provide the AudiLock production URL and public verification key when AudiLock is ready to replace Plugin.club as the source of truth.
- Decide final pricing, refund policy, upgrade policy, VST Exporter addon policy, and support policy.

## DAW And Hardware Proof

- Test Studio, Player, and Player FX in FL Studio with your actual install paths and plugin scan settings.
- Test exported Player products in at least one clean DAW project and one existing real project.
- Verify hardware MIDI note-on, mod wheel, pitch wheel, expression, sustain, pads, and mapped CC controls using your physical keyboard/controllers.
- Verify host drag/drop into Player and Player FX for samples and MIDI files; this is host-specific and cannot be proven by headless tests.
- Confirm Player window resizing, tab switching, labels, keyboard layout, runtime import, mixer state, drum grid playback, and sample playback in DAW.

## Release Operations

- Supply the production code-signing certificate and signing password/token.
- Run installer signing and SmartScreen reputation checks on a clean Windows machine.
- Confirm protected locations such as `C:\Program Files\Common Files\VST3` are handled by installers, not by running Studio elevated.
- Run a clean-machine install/uninstall test with a non-developer Windows account.
- Approve final EULA, privacy policy, license terms, installer text, support links, and marketing copy.
- Upload final installers, demo media, documentation, and sales pages to the production distribution channels.

## Final Acceptance

- Publish a real Plugin.club draft and verify title, artwork, metadata, pricing, tags, compatibility, license config, and download payload.
- Purchase or redeem a test license as a buyer and activate the exported Player product from a clean machine.
- Confirm customer support contact, manual link, store link, and update path all point to production destinations.
