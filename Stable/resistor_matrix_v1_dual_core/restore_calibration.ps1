[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath,
    [string]$HostAddress = "192.168.0.55",
    [switch]$AllowSerialMismatch
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$fullPath = [IO.Path]::GetFullPath($FilePath)
if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
    throw "Calibration bundle not found: $fullPath"
}
$text = [IO.File]::ReadAllText($fullPath)
for ($channel = 1; $channel -le 8; $channel++) {
    if ($text -notmatch "(?m)^#BEGIN CH$channel\b" -or $text -notmatch "(?m)^#END CH$channel\s*$") {
        throw "Bundle is missing CH$channel markers."
    }
}

$bundleSerial = ""
if ($text -match '(?m)^# serial=(?<serial>[^\r\n]+)\s*$') {
    $bundleSerial = $Matches['serial'].Trim()
}

$state = Invoke-WebRequest -UseBasicParsing -Uri "http://$HostAddress/state" -TimeoutSec 15
$stateText = [string]$state.Content
$deviceSerial = ""
if ($stateText -match '(?m)^firmware_serial=[^-\r\n]+-(?<serial>[A-Za-z0-9]+)\s*$') {
    $deviceSerial = $Matches['serial'].Trim()
}

if (-not [string]::IsNullOrWhiteSpace($bundleSerial) -and
    -not [string]::IsNullOrWhiteSpace($deviceSerial) -and
    $bundleSerial -ne $deviceSerial) {
    $message = "Calibration serial mismatch: bundle=$bundleSerial, device=$deviceSerial"
    if (-not $AllowSerialMismatch) {
        throw "$message. Use a calibration backup from this exact board. Override only after manually proving the identity mapping with -AllowSerialMismatch."
    }
    Write-Warning "$message. Serial mismatch override is active."
}

$uri = "http://$HostAddress/calibration_import_all"
Write-Host "Restoring all eight calibration tables to $uri ..."
$response = Invoke-WebRequest -UseBasicParsing -Method Post -Uri $uri -Body @{ bundle = $text } -TimeoutSec 30
Write-Host "HTTP status: $([int]$response.StatusCode)"
Write-Host ([string]$response.Content)

# Re-download the authoritative bundle and require eight persistent files.
$verifyUri = "http://$HostAddress/api/calibration/download_all"
$verify = Invoke-WebRequest -UseBasicParsing -Uri $verifyUri -TimeoutSec 20
$verifyText = [string]$verify.Content
$missing = @()
for ($channel = 1; $channel -le 8; $channel++) {
    if ($verifyText -notmatch "(?m)^#BEGIN CH$channel\b[^\r\n]*\bsaved=1\b[^\r\n]*\bsize=([1-9][0-9]*)\b") {
        $missing += $channel
    }
}
if ($missing.Count -gt 0) {
    throw "Calibration restore verification failed. Channels without a non-empty saved file: $($missing -join ', ')"
}

$status = Invoke-WebRequest -UseBasicParsing -Uri "http://$HostAddress/api/calibration/files" -TimeoutSec 15
Write-Host "Device calibration files:"
Write-Host ([string]$status.Content)
Write-Host "Calibration restore verified: CH1-CH8 are saved and non-empty."
