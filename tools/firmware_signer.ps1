<#
.SYNOPSIS
    Signs an AURA firmware binary with the ECDSA P-256 signing key.

.DESCRIPTION
    Windows-native equivalent of tools/firmware_signer.py.

    Signs the SHA-256 of the firmware with the private key and emits a
    DER-encoded ECDSA signature as lowercase hex, matching the device-side
    mbedtls_pk_verify() expectation in OtaManager::verifyFirmwareSignature().

    The OTA server must deliver this hex in its check response as:
        {"signature": "<hex>"}

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File firmware_signer.ps1 `
        -Key .\keys\private.d -Firmware .\build\firmware.bin -Output sig.hex
#>
param(
    [Parameter(Mandatory = $true)][string]$Key,
    [Parameter(Mandatory = $true)][string]$Firmware,
    [string]$Output,
    [string]$PublicPoint
)

$ErrorActionPreference = "Stop"

function ConvertFrom-DerInteger {
    param([byte[]]$Tlv, [ref]$RemainingBytes)
    if ($Tlv[0] -ne 0x02) { throw "Not an INTEGER tag (0x$($Tlv[0].ToString('x2')))" }
    $len = $Tlv[1]
    $value = $Tlv[2..(1 + $len)]
    if ($len -lt 128) {
        $RemainingBytes.Value = $Tlv[(2 + $len)..($Tlv.Length - 1)]
        return $value
    }
    $longLen = $len -band 0x7F
    $realLen = 0
    for ($i = 0; $i -lt $longLen; $i++) { $realLen = ($realLen -shl 8) -bor $Tlv[2 + $i] }
    $value = $Tlv[(2 + $longLen)..(1 + $longLen + $realLen)]
    $RemainingBytes.Value = $Tlv[(2 + $longLen + $realLen)..($Tlv.Length - 1)]
    return $value
}

function New-DerInteger {
    param([byte[]]$Value)
    $start = 0
    while ($start -lt $Value.Length - 1 -and $Value[$start] -eq 0) { $start++ }
    $trimmed = $Value[$start..($Value.Length - 1)]
    if (($trimmed[0] -band 0x80) -ne 0) {
        $withPad = New-Object byte[] ($trimmed.Length + 1)
        $withPad[0] = 0x00
        [Array]::Copy($trimmed, 0, $withPad, 1, $trimmed.Length)
        $trimmed = $withPad
    }
    $out = New-Object byte[] ($trimmed.Length + 2)
    $out[0] = 0x02
    $out[1] = $trimmed.Length
    [Array]::Copy($trimmed, 0, $out, 2, $trimmed.Length)
    return $out
}

function New-DerSequence {
    param([byte[]]$Content)
    $out = New-Object byte[] ($Content.Length + 2)
    $out[0] = 0x30
    $out[1] = $Content.Length
    [Array]::Copy($Content, 0, $out, 2, $Content.Length)
    return $out
}

# ---- parse raw 32-byte D ----
$dHex = (Get-Content -Path $Key -Raw).Trim()
if ($dHex.Length -ne 64) { throw "Private key must be 64 hex chars (32 bytes)" }
$d = New-Object byte[] 32
for ($i = 0; $i -lt 32; $i++) {
    $d[$i] = [Convert]::ToByte($dHex.Substring($i * 2, 2), 16)
}

# ---- load companion public point (X || Y) ----
if (-not $PublicPoint) {
    $PublicPoint = Join-Path (Split-Path $Key) "publicpoint.hex"
}
$ppHex = (Get-Content -Path $PublicPoint -Raw).Trim()
if ($ppHex.Length -ne 128) { throw "Public point must be 128 hex chars (X || Y)" }
$pubBytes = New-Object byte[] 64
for ($i = 0; $i -lt 64; $i++) {
    $pubBytes[$i] = [Convert]::ToByte($ppHex.Substring($i * 2, 2), 16)
}
$pubX = $pubBytes[0..31]
$pubY = $pubBytes[32..63]

# ---- load ECDSA private key from D on P-256 ----
$oid = [System.Security.Cryptography.Oid]::new("1.2.840.10045.3.1.7")
$curve = [System.Security.Cryptography.ECCurve]::CreateFromOid($oid)
$ecdsa = [System.Security.Cryptography.ECDsa]::Create()
try {
    $ecdsa.ImportParameters((New-Object System.Security.Cryptography.ECParameters -Property @{
        Curve = $curve
        D = $d
        Q = New-Object System.Security.Cryptography.ECPoint -Property @{ X = $pubX; Y = $pubY }
    }))
} catch {
    throw "Failed to import private key: $($_.Exception.Message)"
}

# ---- hash + sign ----
$data = [System.IO.File]::ReadAllBytes((Resolve-Path $Firmware))
$sha = [System.Security.Cryptography.SHA256]::Create()
$hash = $sha.ComputeHash($data)
$rawSig = $ecdsa.SignHash($hash)   # 64-byte IEEE P1363 (r || s)
$ecdsa.Dispose()

if ($rawSig.Length -ne 64) { throw "Unexpected raw signature length $($rawSig.Length)" }

$r = $rawSig[0..31]
$s = $rawSig[32..63]

# ---- convert P1363 -> DER (ASN.1) so mbedtls_pk_verify can parse it ----
$rDer = New-DerInteger $r
$sDer = New-DerInteger $s
$body = New-Object byte[] ($rDer.Length + $sDer.Length)
[Array]::Copy($rDer, 0, $body, 0, $rDer.Length)
[Array]::Copy($sDer, 0, $body, $rDer.Length, $sDer.Length)
$der = New-DerSequence $body

$hex = -join ($der | ForEach-Object { $_.ToString("x2") })

if ($Output) {
    Set-Content -Path $Output -Value $hex -NoNewline -Encoding Ascii
    Write-Host "Signature written to $Output"
} else {
    Write-Host $hex
}
