[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReportZip,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$zipPath = [IO.Path]::GetFullPath($ReportZip)
if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    throw "Regression archive not found: $zipPath"
}
if ([IO.Path]::GetExtension($zipPath) -ne ".zip") {
    throw "Expected a ZIP regression archive: $zipPath"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputPath = Join-Path $PSScriptRoot "calibration-recovered-$stamp.txt"
}

$temp = Join-Path ([IO.Path]::GetTempPath()) ("e-resistor-cal-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    Expand-Archive -LiteralPath $zipPath -DestinationPath $temp -Force
    $logs = @(Get-ChildItem -Path $temp -Filter "http_transcript.log" -Recurse -File)
    if ($logs.Count -eq 0) {
        throw "No http_transcript.log was found in the regression archive."
    }

    $candidates = @()
    foreach ($log in $logs) {
        foreach ($line in Get-Content -LiteralPath $log.FullName) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            try { $record = $line | ConvertFrom-Json } catch { continue }
            if ($record.direction -ne "response") { continue }
            $url = [string]$record.metadata.url
            $payload = [string]$record.payload
            if ($url -notmatch '/api/calibration/download_all$') { continue }
            if ($payload -notmatch '(?m)^# E-Resistor calibration bundle\s*$') { continue }
            $savedCount = 0
            for ($channel = 1; $channel -le 8; $channel++) {
                if ($payload -match "(?m)^#BEGIN CH$channel\b[^\r\n]*\bsaved=1\b[^\r\n]*\bsize=([1-9][0-9]*)\b") {
                    $savedCount++
                }
            }
            $candidates += [pscustomobject]@{
                Payload = $payload
                SavedCount = $savedCount
                Timestamp = [string]$record.timestamp
                Source = $log.FullName
            }
        }
    }

    $best = $candidates |
        Sort-Object SavedCount, Timestamp -Descending |
        Select-Object -First 1
    if (-not $best) {
        throw "No calibration download response was found in the regression archive."
    }
    if ($best.SavedCount -ne 8) {
        throw "The best captured calibration bundle has only $($best.SavedCount)/8 persistent channel files. It is not safe for recovery."
    }

    $fullOutput = [IO.Path]::GetFullPath($OutputPath)
    [IO.File]::WriteAllText($fullOutput, [string]$best.Payload, [Text.UTF8Encoding]::new($false))
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $fullOutput).Hash.ToLowerInvariant()
    $serial = "UNKNOWN"
    if ([string]$best.Payload -match '(?m)^# serial=(?<serial>[^\r\n]+)\s*$') {
        $serial = $Matches['serial'].Trim()
    }
    Write-Host "Recovered calibration: $fullOutput"
    Write-Host "Bundle serial: $serial"
    Write-Host "Persistent channels: 8/8"
    Write-Host "SHA-256: $hash"
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
