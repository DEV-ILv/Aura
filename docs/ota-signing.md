# OTA Firmware Signing

AURA authenticates firmware images before applying them over-the-air. Images
are signed with an **ECDSA P-256 (secp256r1)** private key; the device embeds
only the matching public key and verifies each image before flashing.

This document covers key generation, embedding the public key, signing,
verification, key rotation, and a CI signing workflow.

---

## Overview

| Item            | Value                                            |
| --------------- | ------------------------------------------------ |
| Algorithm       | ECDSA P-256 (secp256r1)                          |
| Digest          | SHA-256 of the firmware image                    |
| Signature       | DER-encoded ECDSA `(r ‖ s)`                      |
| Device key      | `firmware_keys.h` → `kFirmwareSigningKey` (DER SPKI, public only) |
| Server response | `{"signature": "<hex>"}` (HTTP OTA check)        |
| Web upload      | `X-Signature` header **or** `signature` form field |

`OtaManager::verifyFirmwareSignature()` is **fail-closed**: a firmware image
without a valid signature is rejected.

---

## 1. Generate an OTA keypair

### Windows (PowerShell / .NET)

```powershell
powershell -ExecutionPolicy Bypass -File tools/generate_keypair.ps1
# optional custom output dir:
powershell -ExecutionPolicy Bypass -File tools/generate_keypair.ps1 -OutDir .\keys
```

### macOS / Linux (Python)

```bash
python tools/generate_keypair.py
```

Both tools produce the same four files in `keys/`:

| File               | Contents                                            | Commit? |
| ------------------ | --------------------------------------------------- | ------- |
| `private.d`        | Raw 32-byte private scalar `D` (lowercase hex)      | **NO**  |
| `private.pem`      | PKCS#8 PEM private key (openssl / signer.py)        | **NO**  |
| `public.h`         | C header with the DER SubjectPublicKeyInfo          | Yes (optional) |
| `publicpoint.hex`  | Public point `X ‖ Y` (128 hex chars)                | **NO**  |

> **Never commit `private.d`, `private.pem`, or `publicpoint.hex`.** The
> `keys/` directory is git-ignored.

---

## 2. Embed the public key

The device needs the public key in **DER SubjectPublicKeyInfo** format.

1. Run the keypair generator (above).
2. Open `keys/public.h`. It contains an auto-generated
   `kFirmwareSigningKey[]` byte array.
3. Copy that array into `firmware_keys.h`, replacing the placeholder:

   ```cpp
   static constexpr uint8_t kFirmwareSigningKey[] = { 0x30, 0x59, ... };
   static constexpr size_t kFirmwareSigningKeyLen = sizeof(kFirmwareSigningKey);
   ```

4. Recompile and flash the device. `OtaManager` will now verify images against
   this key.

> Only the public key is embedded; the private key never leaves your signing
> environment.

---

## 3. Private key storage

- Store `private.d` / `private.pem` in a **secure, offline** location (HSM,
  encrypted vault, or a machine you control) with restricted access.
- Do not store them in the repository, in the device filesystem, or in any
  build artifact.
- Back them up: losing the private key means you can no longer sign firmware
  and must rotate the key on every device.
- Revoke/rotate immediately if the key is suspected of being compromised.

---

## 4. Firmware signing

Sign a compiled `.bin` image (the firmware build output, e.g.
`firmware.bin`):

### Windows (PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File tools/firmware_signer.ps1 `
    -Key .\keys\private.d -Firmware .\build\firmware.bin -Output sig.hex
```

### macOS / Linux (Python)

```bash
python tools/firmware_signer.py \
    --key keys/private.pem \
    --firmware build/firmware.bin \
    --output sig.hex
```

`sig.hex` is the lowercase-hex **DER** signature.

---

## 5. Verification process

### On the device

- **HTTP OTA check path** (`OtaManager`): the OTA server returns the signature
  in its check response as `{"signature": "<hex>"}`. The device hashes the
  downloaded image, decodes the hex signature to DER, and verifies it with
  `mbedtls_pk_verify`. Invalid or missing signatures are rejected
  (fail-closed).
- **Web portal upload path** (`WebPortal::verifyWebOtaSignature`): send the
  signature in the `X-Signature` header **or** the `signature` multipart form
  field. Missing signature → warning log; invalid signature → `400` + audit
  event, update aborted.

### Offline (sanity check)

```powershell
# Windows: recompute SHA-256 of the image and print the stored signature
Get-FileHash .\build\firmware.bin -Algorithm SHA256
Get-Content .\sig.hex
```

The signature must have been produced over the SHA-256 of the exact same
bytes that are flashed.

---

## 6. Key rotation

Rotate keys when the private key may have been compromised, or periodically
for defense in depth.

1. Generate a **new** keypair (never reuse the old private key).
2. Embed the **new public key** in `firmware_keys.h` and flash a new baseline
   image (signed with the old key).
3. From then on, sign all images with the **new** private key.
4. Update any CI signing secrets.
5. Optionally keep the old public key for a short transition window so already
   deployed devices can still be updated.

There is currently no built-in multiple-key support in the device; a rotation
requires a full flash with the updated `firmware_keys.h`.

---

## 7. CI signing workflow

Example GitHub Actions workflow (Linux runners):

```yaml
name: sign-firmware
on:
  workflow_dispatch:
jobs:
  sign:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      # Compile the firmware first (arduino-cli) and produce firmware.bin
      - name: Sign firmware
        env:
          # Store the private key as an Actions secret, never in the repo.
          SIGNING_KEY: ${{ secrets.AURA_SIGNING_KEY }}
        run: |
          echo "$SIGNING_KEY" > keys/private.pem
          python tools/firmware_signer.py \
            --key keys/private.pem \
            --firmware build/firmware.bin \
            --output sig.hex
          rm keys/private.pem
      - uses: actions/upload-artifact@v4
        with:
          name: signed-firmware
          path: |
            build/firmware.bin
            sig.hex
```

Key points:

- The private key is stored as an **encrypted Actions secret**
  (`AURA_SIGNING_KEY`), never in the repository.
- Delete the key file immediately after signing.
- Only trusted, review-approved workflows should be allowed to run the signing
  job.
