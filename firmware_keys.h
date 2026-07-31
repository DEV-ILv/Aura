#ifndef AURA_FIRMWARE_KEYS_H
#define AURA_FIRMWARE_KEYS_H

#include <stdint.h>
#include <stddef.h>

// ======================================================================
// Firmware Signing Public Key
//
// ECDSA P-256 (secp256r1) public key in DER SubjectPublicKeyInfo format.
//
// To generate a real keypair:
//   python tools/generate_keypair.py --out-dir ./keys
// Then replace kFirmwareSigningKey[] below with keys/public.h contents.
//
// IMPORTANT: Keep private.pem SECURE — never commit it to version control.
// ======================================================================

// Default: empty key (signature verification disabled).
// Replace with a 91-byte DER SPKI block from generate_keypair.py.
static constexpr uint8_t kFirmwareSigningKey[] = { 0x00 };
static constexpr size_t kFirmwareSigningKeyLen = 0;

// ECDSA P-256 signature: 64-byte compact (r || s)
static constexpr size_t kSignatureLen = 64;

#endif // AURA_FIRMWARE_KEYS_H
