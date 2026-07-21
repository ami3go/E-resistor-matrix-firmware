[CmdletBinding()]
param(
    [string]$HostAddress = "192.168.0.55",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputPath = Join-Path $PSScriptRoot "calibration-backup-$stamp.txt"
}

$uri = "http://$HostAddress/api/calibration/download_all"
Write-Host "Downloading calibration bundle from $uri ..."
$response = Invoke-WebRequest -UseBasicParsing -Uri $uri -TimeoutSec 15
$text = [string]$response.Content

for ($channel = 1; $channel -le 8; $channel++) {
    if ($text -notmatch "(?m)^#BEGIN CH$channel\b" -or $text -notmatch "(?m)^#END CH$channel\s*$") {
        throw "Downloaded bundle is missing CH$channel markers."
    }
}

$unsaved = [regex]::Matches($text, '(?m)^#BEGIN CH([1-8])\b[^\r\n]*\bsaved=0\b') |
    ForEach-Object { $_.Groups[1].Value }
if ($unsaved.Count -gt 0) {
    Write-Warning "Channels without persistent calibration: $($unsaved -join ', '). The bundle still contains active nominal/runtime values."
}

$fullPath = [IO.Path]::GetFullPath($OutputPath)
[IO.File]::WriteAllText($fullPath, $text, [Text.UTF8Encoding]::new($false))
$hash = (Get-FileHash -Algorithm SHA256 -Path $fullPath).Hash.ToLowerInvariant()
Write-Host "Saved: $fullPath"
Write-Host "SHA-256: $hash"
