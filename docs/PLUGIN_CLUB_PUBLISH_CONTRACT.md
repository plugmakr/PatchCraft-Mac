# Plugin.club Platform Integration Contract

Verified against live [Plugin.club API Docs](https://plugin.club/api-docs) and HTTP probes (2026-07-22).

## Base URL

```text
https://plugin.club/functions/v1
```

All function calls must include `/functions/v1`. Omitting it returns **405 Method Not Allowed**.

## Auth model

| Use | Auth |
| --- | --- |
| Studio seller publish / buyer library | Device Auth access token (`Authorization: Bearer <access_token>`) |
| Optional fallback | `PLUGINCLUB_API_KEY` or Settings token override |
| End-customer pack licensing | License key + hardware id via `activateLicense` / `validateLicense` (no seller token) |

## Live endpoints

| Purpose | Method | Live URL | Status |
| --- | --- | --- | --- |
| Start Device Auth | POST | `.../v1/deviceAuthStart` body `{}` | Live (200) |
| Poll Device Auth | POST | `.../v1/deviceAuthPoll` body `{"device_code":"..."}` | Live |
| Device Logout | POST | `.../v1/deviceAuthLogout` | Live (Bearer) |
| Seller Publish | POST | `.../v1/sellerImport` multipart `package` + `metadata` | Live (401 without token) |
| Activate License | POST | `.../v1/activateLicense` | Live (`license_key`, `hardware_id`, …) |
| Validate License | POST | `.../v1/validateLicense` | Live |
| Public Catalog | GET | `.../v1/romplurCatalog` | Live (200) |
| Buyer Library | GET | `.../v1/romplurBuyer/library` | Live (401 without token) |

### Documented but not live yet

These appear on the docs Content Sync / Publishing tabs but currently **404**:

- `GET /catalogAPI` (use `romplurCatalog` instead)
- `GET /marketplaceAPI/library` (use `romplurBuyer/library` instead)
- `POST /marketplaceAPI/products/:id/download`
- `POST /romplurSellerImport` (**404** — do not use)
- `POST /functions/sellerImport` without `/v1` (**405**)

## PatchCraft wiring

| Feature | Connected? | Notes |
| --- | --- | --- |
| Device Auth sign-in | Yes | Settings → Sign in to Plugin.club |
| Seller publish draft | Yes | Ship / Publish → `sellerImport` with stored access token |
| Pack license activate | Yes | Player → `activateLicense` payload (`license_key` / `hardware_id`) |
| Pack license validate | Partial | Same client; URL can be set to `validateLicense` |
| Catalog browse in-app | No | Endpoint live; no Studio/Player store UI yet |
| Buyer library sync | No | Endpoint live; needs Device Auth + UI |
| Purchase / download sync | No | Docs paths 404 or not wired |

## Publish request

```text
POST https://plugin.club/functions/v1/sellerImport
Authorization: Bearer <device_access_token>
multipart/form-data:
  package=<zip>
  metadata=<JSON string>
  artifact_kind=<string>
```

Studio normalizes configured endpoints to the live sellerImport URL, including legacy `romplurSellerImport` settings.

## License request (Player)

```text
POST https://plugin.club/functions/v1/activateLicense
Content-Type: application/json

{
  "license_key": "...",
  "hardware_id": "...",
  "machine_name": "...",
  "os_info": "..."
}
```

Legacy pack URLs pointing at `deviceAuthStart` are rewritten client-side to `activateLicense`.
