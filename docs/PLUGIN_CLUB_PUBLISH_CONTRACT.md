# Plugin.club Direct Publish Contract

PatchCraft Studio now prepares and uploads Plugin.club seller drafts for these artifact types:

- `patchcraft_instrument_pack`
- `standalone_vst3_plugin`
- `one_shot_pack`
- `loop_pack` once the standalone loop workflow exists
- `generic_archive`

The Studio client sends all publish requests to a Plugin.club-compatible platform seller import endpoint.

## AudiLock Relationship

AudiLock is planned as the canonical licensing and entitlement source of truth that will power Plugin.club and future stores.
For the launch build, PatchCraft still uses Plugin.club endpoints directly:

- Publish drafts: `https://plugin.club/functions/sellerImport`
- Runtime/device licensing: `https://plugin.club/functions/deviceAuth`

Do not change the Studio payload shape for AudiLock. AudiLock should accept or translate the same `license_config`, `product_id`, trial, offline grace, bind-to-machine, and public-key metadata that Studio already embeds and sends to Plugin.club.

## Endpoint

Default configured base:

```text
https://plugin.club/functions
```

Studio appends:

```text
/sellerImport
```

The final default endpoint is:

```text
https://plugin.club/functions/sellerImport
```

Important: Base44 backend functions require the `/functions/` prefix. A request to `https://plugin.club/sellerImport` hits the frontend router and will fail with `405 Method Not Allowed`.

Studio also accepts these settings and normalizes them:

- `https://plugin.club`
- `https://www.plugin.club`
- `https://plugin.club/functions`
- `https://plugin.club/functions/sellerImport`
- Legacy settings ending in `/romplurSellerImport` are migrated client-side to `/sellerImport`

## Request

Method:

```text
POST
```

Headers:

```text
Accept: application/json
Authorization: Bearer <seller_api_key>
```

Body:

```text
multipart/form-data
```

Fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `package` | file | Yes | ZIP archive containing the product artifact. |
| `metadata` | string JSON | Yes | Product metadata JSON. |
| `artifact_kind` | string | Yes | One of the artifact kind values listed above. |

Endpoint migration note:

- Old: `https://plugin.club/functions/romplurSellerImport`
- New: `https://plugin.club/functions/sellerImport`
- Device auth follows the same universal naming pattern: `/functions/deviceAuth`
- Future AudiLock endpoints should keep this contract compatible so Studio can switch base URLs without repackaging products.

## Metadata Shape

Studio sends a JSON object similar to this:

```json
{
  "title": "Cinematic Evolve Pad",
  "source_product_id": "cinematic_evolve_pad",
  "product_type": "instrument",
  "product_subtype": "patchcraft_instrument_pack",
  "artifact_kind": "patchcraft_instrument_pack",
  "plugin_type": "sampler",
  "plugin_format": ["PatchCraft"],
  "price": 0.0,
  "short_description": "Playable PatchCraft instrument package.",
  "description": "Playable PatchCraft instrument package.",
  "creator": "Creator Name",
  "category": "Cinematic",
  "compatibility": {
    "os": ["Windows"],
    "daws": ["Ableton Live", "FL Studio", "Logic Pro", "Cubase", "Studio One", "Reaper", "Bitwig"],
    "min_requirements": "Standard ZIP extraction and compatible sampler/player."
  },
  "version": "1.0.0",
  "tags": ["PatchCraft", "patchcraft_instrument_pack", "Cinematic"],
  "status": "draft",
  "package": {
    "artifact_kind": "patchcraft_instrument_pack",
    "source_path": "local source path before upload",
    "archive_file": "uploaded-package.zip",
    "artwork_file": "cover.png"
  },
  "extra": {}
}
```

For PatchCraft instrument-pack publishes, Studio also includes:

```json
{
  "license_config": {
    "license_type": "licensed",
    "max_activations": 3,
    "require_hardware_validation": true,
    "allow_offline_validation": true,
    "trial_days": 14
  },
  "patchcraft": {
    "package_kind": "patchcraft_pack",
    "engine": "sample",
    "category": "Cinematic",
    "creator": "Creator Name",
    "archive_file": "uploaded-package.zip",
    "pack_version": "1.0.0",
    "license_required": true,
    "license_product_id": "product-id"
  }
}
```

## Artifact Package Contents

### PatchCraft Instrument Pack

The ZIP contains a `.patchcraft` runtime pack folder:

- `manifest.json`
- `patches.json`
- `presets.json`
- `expansions.json`
- `layout.json`
- `dsp.json`
- sample/audio/assets copied into the pack
- `license.json` when license metadata is enabled
- `pluginclub-metadata.json`
- `pluginclub-publish.json`

### Standalone VST3 Plugin

The ZIP contains the exported `.vst3` bundle:

- `<PluginName>.vst3/Contents/...`
- embedded PatchCraft pack under `Contents/Resources/EmbeddedPack`
- `export_info.json`
- rewritten `moduleinfo.json`
- unique patched VST3 class IDs

### One-Shot Pack

The ZIP contains the rendered one-shot folder:

- WAV files
- `oneshot_pack.json`
- optional `README.txt`
- optional `artwork/cover.png`

### Loop Pack

Reserved for the standalone one-shot/loop app:

- WAV/AIFF loops
- loop metadata JSON
- artwork/readme/license
- BPM/key/bar-length metadata

## Expected Response

Studio treats any non-HTML `2xx` response as a successful upload.

Recommended response:

```json
{
  "ok": true,
  "draft_id": "draft_123",
  "seller_product_id": "prod_123",
  "status": "draft",
  "edit_url": "https://plugin.club/seller/products/prod_123"
}
```

Recommended error response:

```json
{
  "ok": false,
  "error": "Missing package",
  "code": "missing_package"
}
```

## Server Implementation Checklist

- Verify bearer token maps to a seller account.
- Accept `package` as ZIP file upload.
- Parse `metadata` JSON.
- Validate `artifact_kind`.
- Store archive in product draft storage.
- Create or update draft by `source_product_id`.
- Attach metadata, tags, category, price, status, compatibility, artwork references, and license config.
- Return JSON, not HTML.
- Keep response body short enough for Studio error dialogs.

## Current Client Behavior

- If endpoint or API key is missing, Studio prepares the archive locally and reports paths.
- If upload fails, Studio keeps the archive, metadata, and payload locally for manual inspection.
- Studio redacts response length to a short snippet in failure dialogs.
- Studio parses `draft_id` and `edit_url`/`editUrl`/`url` from successful responses and offers to open the seller draft from the publish result dialog.
