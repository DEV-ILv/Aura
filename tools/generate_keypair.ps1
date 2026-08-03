<#
.SYNOPSIS
    Generates an ECDSA P-256 (secp256r1) keypair for AURA firmware signing.

.DESCRIPTION
    Windows-native equivalent of generate_keypair.py. Uses .NET ECDsa (CNG).

    Outputs (into <out-dir>, default ./keys):
      - private.d    : raw 32-byte private scalar D as lowercase hex
      - private.pem  : PKCS8 PEM (for tools/firmware_signer.py / openssl)
      - public.h     : C header with the DER SubjectPublicKeyInfo for firmware_keys.h

    KEEP private.d and private.pem SECURE. NEVER commit them.
    The keys/ directory is git-ignored.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File generate_keypair.ps1
    powershell -ExecutionPolicy Bypass -File generate_keypair.ps1 -OutDir .\keys
#>
param(
    [string]$OutDir = (Join-Path $PSScriptRoot "..\keys")
)

$ErrorActionPreference = "Stop"

function New-DerOid {
    param([string]$OidDotted)
    $parts = $OidDotted.Split('.')
    $bytes = [System.Collections.Generic.List[byte]]::new()
    $first = [int]$parts[0] * 40 + [int]$parts[1]
    $bytes.Add([byte]$first)
    for ($i = 2; $i -lt $parts.Count; $i++) {
        $val = [uint64]$parts[$i]
        $stack = [System.Collections.Generic.List[byte]]::new()
        $stack.Add([byte]($val -band 0x7F))
        $val = $val -shr 7
        while ($val -gt 0) {
            $stack.Insert(0, [byte](($val -band 0x7F) -bor 0x80))
            $val = $val -shr 7
        }
        foreach ($b in $stack) { $bytes.Add($b) }
    }
    return ,$bytes.ToArray()
}

function New-DerTag {
    param([byte]$Tag, [byte[]]$Content)
    $len = $Content.Length
    $out = [System.Collections.Generic.List[byte]]::new()
    $out.Add($Tag)
    if ($len -lt 128) {
        $out.Add([byte]$len)
    } else {
        $lenBytes = [System.Collections.Generic.List[byte]]::new()
        $tmp = $len
        while ($tmp -gt 0) {
            $lenBytes.Insert(0, [byte]($tmp -band 0xFF))
            $tmp = $tmp -shr 8
        }
        $out.Add([byte](0x80 -bor $lenBytes.Count))
        foreach ($b in $lenBytes) { $out.Add($b) }
    }
    foreach ($b in $Content) { $out.Add($b) }
    return ,$out.ToArray()
}

function New-DerInteger {
    param([byte[]]$Value)
    # Minimal two's-complement INTEGER encoding
    $start = 0
    while ($start -lt $Value.Length - 1 -and $Value[$start] -eq 0) { $start++ }
    $trimmed = $Value[$start..($Value.Length - 1)]
    if (($trimmed[0] -band 0x80) -ne 0) {
        $withPad = New-Object byte[] ($trimmed.Length + 1)
        $withPad[0] = 0x00
        [Array]::Copy($trimmed, 0, $withPad, 1, $trimmed.Length)
        $trimmed = $withPad
    }
    return (New-DerTag -Tag 0x02 -Content $trimmed)
}

function New-Spki {
    param([byte[]]$X, [byte[]]$Y)
    $algBody = [System.Collections.Generic.List[byte]]::new()
    $oidEc = New-DerOid "1.2.840.10045.2.1"
    $oidP256 = New-DerOid "1.2.840.10045.3.1.7"
    foreach ($b in (New-DerTag -Tag 0x06 -Content $oidEc)) { $algBody.Add($b) }
    foreach ($b in (New-DerTag -Tag 0x06 -Content $oidP256)) { $algBody.Add($b) }
    $algId = New-DerTag -Tag 0x30 -Content $algBody.ToArray()

    $bitBody = [System.Collections.Generic.List[byte]]::new()
    $bitBody.Add(0x00)  # 0 unused bits
    $bitBody.Add(0x04)  # uncompressed point
    foreach ($b in $X) { $bitBody.Add($b) }
    foreach ($b in $Y) { $bitBody.Add($b) }
    $bitString = New-DerTag -Tag 0x03 -Content $bitBody.ToArray()

    $body = [System.Collections.Generic.List[byte]]::new()
    foreach ($b in $algId) { $body.Add($b) }
    foreach ($b in $bitString) { $body.Add($b) }
    return (New-DerTag -Tag 0x30 -Content $body.ToArray())
}

function New-Pkcs8PrivateKey {
    param([byte[]]$D, [byte[]]$PublicSpki)
    $oidP256 = New-DerOid "1.2.840.10045.3.1.7"

    $ecBody = [System.Collections.Generic.List[byte]]::new()
    foreach ($b in (New-DerInteger -Value ([byte[]](0x01)))) { $ecBody.Add($b) }
    foreach ($b in (New-DerTag -Tag 0x04 -Content $D)) { $ecBody.Add($b) }
    # [0] EXPLICIT prime256v1 OID
    $oidWrap = New-DerTag -Tag 0x06 -Content $oidP256
    foreach ($b in (New-DerTag -Tag 0xA0 -Content $oidWrap)) { $ecBody.Add($b) }
    $ecPrivateKey = New-DerTag -Tag 0x30 -Content $ecBody.ToArray()

    $algBody = [System.Collections.Generic.List[byte]]::new()
    $oidEc = New-DerOid "1.2.840.10045.2.1"
    foreach ($b in (New-DerTag -Tag 0x06 -Content $oidEc)) { $algBody.Add($b) }
    foreach ($b in (New-DerTag -Tag 0x06 -Content $oidP256)) { $algBody.Add($b) }
    $algId = New-DerTag -Tag 0x30 -Content $algBody.ToArray()

    $body = [System.Collections.Generic.List[byte]]::new()
    foreach ($b in (New-DerInteger -Value ([byte[]](0x00)))) { $body.Add($b) }
    foreach ($b in $algId) { $body.Add($b) }
    foreach ($b in (New-DerTag -Tag 0x04 -Content $ecPrivateKey)) { $body.Add($b) }
    return (New-DerTag -Tag 0x30 -Content $body.ToArray())
}

function ConvertTo-Pem {
    param([string]$Label, [byte[]]$Der)
    $b64 = [Convert]::ToBase64String($Der, [Base64FormattingOptions]::InsertLineBreaks)
    return "-----BEGIN $Label-----`r`n$b64`r`n-----END $Label-----`r`n"
}

function ConvertTo-HexCArray {
    param([byte[]]$Bytes)
    $hex = ($Bytes | ForEach-Object { "0x{0:x2}" -f $_ }) -join ", "
    return $hex
}

# ---- main ----
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$oid = [System.Security.Cryptography.Oid]::new("1.2.840.10045.3.1.7")
$curve = [System.Security.Cryptography.ECCurve]::CreateFromOid($oid)
$ecdsa = [System.Security.Cryptography.ECDsa]::Create()
$ecdsa.GenerateKey($curve)

$priv = $ecdsa.ExportParameters($true)
$pub = $ecdsa.ExportParameters($false)

if ($priv.D.Length -ne 32 -or $pub.Q.X.Length -ne 32 -or $pub.Q.Y.Length -ne 32) {
    throw "Unexpected key sizes (expected P-256 = 32-byte components)"
}

$spki = New-Spki -X $pub.Q.X -Y $pub.Q.Y
$pkcs8 = New-Pkcs8PrivateKey -D $priv.D -PublicSpki $spki

# raw private scalar (used by firmware_signer.ps1)
$dHex = -join ($priv.D | ForEach-Object { $_.ToString("x2") })
Set-Content -Path (Join-Path $OutDir "private.d") -Value $dHex -NoNewline -Encoding Ascii

# public point X||Y (used by firmware_signer.ps1 to import the key on .NET
# Framework, which requires the public point even for private-key imports)
$xHex = -join ($pub.Q.X | ForEach-Object { $_.ToString("x2") })
$yHex = -join ($pub.Q.Y | ForEach-Object { $_.ToString("x2") })
Set-Content -Path (Join-Path $OutDir "publicpoint.hex") -Value ($xHex + $yHex) -NoNewline -Encoding Ascii

# PKCS8 PEM (used by firmware_signer.py / openssl)
$pem = ConvertTo-Pem -Label "PRIVATE KEY" -Der $pkcs8
Set-Content -Path (Join-Path $OutDir "private.pem") -Value $pem -NoNewline -Encoding Ascii

# C header
$header = @"
// Auto-generated by tools/generate_keypair.ps1 -- DO NOT EDIT MANUALLY
#ifndef AURA_FIRMWARE_KEYS_H
#define AURA_FIRMWARE_KEYS_H

#include <stdint.h>
#include <stddef.h>

// ECDSA P-256 public key in DER SubjectPublicKeyInfo format (length: $($spki.Length) bytes)
// Generated from the matching private key (kept OUT of version control).
static constexpr uint8_t kFirmwareSigningKey[] = { $(ConvertTo-HexCArray $spki) };
static constexpr size_t kFirmwareSigningKeyLen = $($spki.Length);

// Signature: DER-encoded ECDSA (r || s) produced by firmware_signer.py/.ps1.
static constexpr size_t kSignatureLen = 64;

#endif // AURA_FIRMWARE_KEYS_H
"@
Set-Content -Path (Join-Path $OutDir "public.h") -Value $header -Encoding Ascii

$ecdsa.Dispose()

Write-Host "Private key : $(Join-Path $OutDir 'private.d') / $(Join-Path $OutDir 'private.pem')  (KEEP SECURE)"
Write-Host "Public key  : $(Join-Path $OutDir 'public.h')"
