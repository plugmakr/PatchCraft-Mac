# AudiLock Licensing Plan

Date: 2026-05-21

## Decision

AudiLock will become the licensing and entitlement source of truth for PatchCraft products, Plugin.club, and future marketplaces.

For the launch build, PatchCraft uses Plugin.club directly:

- Seller publish: `https://plugin.club/functions/sellerImport`
- Runtime/device activation: `https://plugin.club/functions/deviceAuth`

## Contract To Preserve

PatchCraft already exports license metadata in pack manifests, generated launch materials, and Plugin.club publish payloads. AudiLock should accept or translate the same fields:

- `product_id`
- `license_required`
- `license_type`
- `trial_days`
- `offline_grace_days`
- `bind_to_machine`
- `public_key`
- `server_url`
- `license_config`

Keeping this shape stable lets Studio switch from Plugin.club to AudiLock by changing endpoints instead of rewriting exported products.

## Launch Behavior

- Settings shows Plugin.club publish and Plugin.club licensing as the current release path.
- Brand Lab embeds the product ID, license URL, public key metadata, trial length, offline grace, and machine-binding policy into exported products.
- Launch Center includes the same license settings in customer handoff files, Plugin.club metadata, and release manifests.
- If a license endpoint or product ID is missing, Studio should warn before publish/export and exported Players should fail gracefully.

## AudiLock Cutover Requirements

Before replacing Plugin.club licensing, AudiLock needs:

- A device activation endpoint compatible with the current `deviceAuth` request shape.
- A seller/product API that can map Plugin.club product IDs to AudiLock product IDs or use the same ID.
- Public key or key-id metadata for Player-side verification.
- Trial, offline grace, activation limit, revoke, reset, and machine-binding support.
- A migration path for Plugin.club products already published during launch.

## Do Not Do For Launch

- Do not block release on AudiLock.
- Do not enable the hidden AI Studio expansion by default.
- Do not bundle VST Exporter templates in the base installer.
- Do not change the Plugin.club publish payload unless Plugin.club backend requires it.
