#!/usr/bin/env python3
"""
Firmware signing tool for AURA OTA updates.
Signs firmware binaries with an ECDSA P-256 private key.
Produces a hex-encoded DER signature for the OTA server response.

Usage:
    python firmware_signer.py --key private.pem --firmware firmware.bin
    python firmware_signer.py --key private.pem --firmware firmware.bin --output signature.hex

The server should include the hex signature in its check response as:
    {"signature": "<hex>"}

Device-side verification uses mbedtls_pk_verify() with the embedded
ECDSA P-256 public key from firmware_keys.h.
"""

import argparse
import hashlib
import sys
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import load_pem_private_key


def parse_args():
    parser = argparse.ArgumentParser(description="AURA firmware signing tool")
    parser.add_argument("--key", required=True, help="Path to ECDSA P-256 PEM private key")
    parser.add_argument("--firmware", required=True, help="Path to firmware binary to sign")
    parser.add_argument("--output", "-o", help="Output file for hex signature (default: stdout)")
    return parser.parse_args()


def main():
    args = parse_args()

    with open(args.key, "rb") as f:
        private_key = load_pem_private_key(f.read(), password=None)

    if not isinstance(private_key, ec.EllipticCurvePrivateKey):
        sys.stderr.write("Error: Key must be ECDSA (EllipticCurvePrivateKey)\n")
        sys.exit(1)

    if not isinstance(private_key.curve, ec.SECP256R1):
        sys.stderr.write("Error: Key must use secp256r1 (P-256) curve\n")
        sys.exit(1)

    with open(args.firmware, "rb") as f:
        firmware_data = f.read()

    firmware_hash = hashlib.sha256(firmware_data).digest()

    # sign() returns a DER-encoded ASN.1 signature (r, s integers in SEQUENCE)
    signature = private_key.sign(firmware_hash, ec.ECDSA(hashes.SHA256()))
    hex_sig = signature.hex()

    if args.output:
        with open(args.output, "w") as f:
            f.write(hex_sig + "\n")
        print(f"Signature written to {args.output}", file=sys.stderr)
    else:
        print(hex_sig)


if __name__ == "__main__":
    main()
