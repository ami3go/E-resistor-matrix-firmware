[CmdletBinding()]
param(
    [int]$WaitSeconds = 15,
    [string]$Uf2Path = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Uf2Path)) {
    $buildRoot = Join-Path $PSScriptRoot ".build\arduino-cli\waveshare_rp2040_zero"
    $uf2 = Get-ChildItem -Path $buildRoot -Filter "*.uf2" -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $uf2) {
        throw "No UF2 found under $buildRoot. Run .\build_firmware.bat first."
    }
} else {
    $resolved = Resolve-Path -LiteralPath $Uf2Path -ErrorAction Stop
    $uf2 = Get-Item -LiteralPath $resolved.Path
    if ($uf2.Extension -ne ".uf2") {
        throw "The selected file is not a UF2 image: $($uf2.FullName)"
    }
}

$volume = Get-Volume | Where-Object FileSystemLabel -eq "RPI-RP2" | Select-Object -First 1
if (-not $volume -or -not $volume.DriveLetter) {
    throw "RPI-RP2 BOOTSEL volume was not found. Hold BOOTSEL while connecting USB."
}

$beforePorts = @{}
Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue | ForEach-Object {
    $beforePorts[[string]$_.PNPDeviceID] = [string]$_.DeviceID
}

$destination = "$($volume.DriveLetter):\"
Write-Host "UF2: $($uf2.FullName)"
Write-Host "BOOTSEL destination: $destination"
Copy-Item -LiteralPath $uf2.FullName -Destination $destination -Force
Write-Host "UF2 copied. Waiting for normal firmware USB CDC enumeration..."

$deadline = (Get-Date).AddSeconds([Math]::Max(1, $WaitSeconds))
do {
    Start-Sleep -Milliseconds 500
    $ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue)
    $rp2040Ports = @($ports | Where-Object {
        ([string]$_.PNPDeviceID) -match 'VID_2E8A' -or
        ((-not $beforePorts.ContainsKey([string]$_.PNPDeviceID)) -and $_.DeviceID)
    })
    if ($rp2040Ports.Count -gt 0) {
        Write-Host "Firmware USB CDC port detected:"
        $rp2040Ports | Select-Object DeviceID, Name, PNPDeviceID | Format-Table -AutoSize
        exit 0
    }
} while ((Get-Date) -lt $deadline)

Write-Warning "No new RP2040 USB CDC port was detected before timeout."
Write-Host "Current serial ports:"
Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
    Select-Object DeviceID, Name, PNPDeviceID |
    Format-Table -AutoSize
Write-Warning "Verify that RPI-RP2 disappeared, BOOTSEL is released, and the production UF2 was selected."
exit 2
