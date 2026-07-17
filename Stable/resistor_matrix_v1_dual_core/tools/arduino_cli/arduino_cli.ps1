[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Setup", "Doctor", "Build", "Flash", "BuildFlash", "Clean", "ListBoards")]
    [string]$Action,

    [string]$Port,
    [switch]$ForceSetup,
    [switch]$VerboseBuild,

    [ValidateRange(1, 60)]
    [int]$HeartbeatSeconds = 3
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Runner version is owned by this script, not by arduino_cli_config.ps1.
# Keeping it here allows newer runners to work with older local config files.
$ArduinoCliScriptVersion = "1.1.1"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$SketchDir = $ProjectRoot

. (Join-Path $ScriptDir "arduino_cli_config.ps1")
$LocalConfig = Join-Path $ScriptDir "arduino_cli.local.ps1"
if (Test-Path $LocalConfig) {
    . $LocalConfig
}

# Reassert the runner version in case an older configuration file defines it.
$ArduinoCliScriptVersion = "1.1.1"

if ([string]::IsNullOrWhiteSpace($Port)) {
    if (-not [string]::IsNullOrWhiteSpace($env:ERESISTOR_COM_PORT)) {
        $Port = $env:ERESISTOR_COM_PORT
    } else {
        $Port = $DefaultPort
    }
}

$ToolsRoot = Join-Path $ProjectRoot ".tools\arduino-cli"
$CliInstallDir = Join-Path $ToolsRoot $ArduinoCliVersion
$CliExe = Join-Path $CliInstallDir "arduino-cli.exe"
$EnvRoot = Join-Path $ProjectRoot ".arduino-cli"
$DataDir = Join-Path $EnvRoot "data"
$DownloadsDir = Join-Path $EnvRoot "downloads"
$UserDir = Join-Path $EnvRoot "user"
$CliConfig = Join-Path $EnvRoot "arduino-cli.yaml"
$BuildRoot = Join-Path $ProjectRoot ".build\arduino-cli"
$BuildDir = Join-Path $BuildRoot "waveshare_rp2040_zero"
$LogDir = Join-Path $BuildRoot "logs"
$DistDir = Join-Path $BuildRoot "dist"
$BoardOptionsText = $BoardOptions -join ","

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Convert-ToYamlPath([string]$Path) {
    return ($Path -replace "\\", "/")
}

function Format-Elapsed {
    param([Parameter(Mandatory = $true)][TimeSpan]$Elapsed)
    return "{0:00}:{1:00}:{2:00}" -f [int]$Elapsed.TotalHours, $Elapsed.Minutes, $Elapsed.Seconds
}

function Invoke-Cli {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$LogFile,
        [string]$Activity = "Arduino CLI",
        [switch]$ShowHeartbeat
    )

    if (-not (Test-Path $CliExe)) {
        throw "arduino-cli is not installed. Run setup_arduino_cli_environment.bat first."
    }

    Write-Host "arduino-cli $($Arguments -join ' ')" -ForegroundColor DarkGray
    if ([string]::IsNullOrWhiteSpace($LogFile)) {
        & $CliExe @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "arduino-cli failed with exit code $LASTEXITCODE."
        }
        return
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogFile) | Out-Null

    # Run the native tool in a background PowerShell process so output can be
    # consumed incrementally while the parent prints a heartbeat during quiet
    # compiler phases. Every received line is written immediately to both the
    # console and the timestamped log file.
    $ExitSentinel = "__ERESISTOR_CLI_EXIT_CODE__="
    $ArgumentsJson = ConvertTo-Json -InputObject $Arguments -Compress
    $Job = Start-Job -ArgumentList $CliExe, $ArgumentsJson, $ExitSentinel -ScriptBlock {
        param($Executable, $SerializedArguments, $Sentinel)

        $ErrorActionPreference = "Continue"
        $CliArguments = @(ConvertFrom-Json -InputObject $SerializedArguments)

        & $Executable @CliArguments 2>&1 |
            ForEach-Object { $_.ToString() }

        "$Sentinel$LASTEXITCODE"
    }

    $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    $Writer = New-Object System.IO.StreamWriter($LogFile, $false, $Utf8NoBom)
    $Writer.AutoFlush = $true

    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $LastHeartbeatSeconds = 0.0
    $OutputLineCount = 0
    $ExitCode = $null

    try {
        while (($Job.State -eq "NotStarted") -or ($Job.State -eq "Running") -or $Job.HasMoreData) {
            $Received = @(Receive-Job -Job $Job -ErrorAction SilentlyContinue)
            $HadOutput = $false

            foreach ($Item in $Received) {
                $Line = $Item.ToString()
                if ($Line.StartsWith($ExitSentinel, [System.StringComparison]::Ordinal)) {
                    $ExitText = $Line.Substring($ExitSentinel.Length)
                    $ParsedExitCode = 0
                    if ([int]::TryParse($ExitText, [ref]$ParsedExitCode)) {
                        $ExitCode = $ParsedExitCode
                    }
                    continue
                }

                $HadOutput = $true
                $OutputLineCount++
                $Writer.WriteLine($Line)
                Write-Host $Line
            }

            if ($ShowHeartbeat -and (-not $HadOutput)) {
                $ElapsedSeconds = $Stopwatch.Elapsed.TotalSeconds
                if (($ElapsedSeconds - $LastHeartbeatSeconds) -ge $HeartbeatSeconds) {
                    $LastHeartbeatSeconds = $ElapsedSeconds
                    $ElapsedText = Format-Elapsed -Elapsed $Stopwatch.Elapsed
                    Write-Host ("[{0}] still running | elapsed {1} | output lines {2}" -f $Activity, $ElapsedText, $OutputLineCount) -ForegroundColor Yellow
                }
            }

            if (($Job.State -eq "NotStarted") -or ($Job.State -eq "Running")) {
                Start-Sleep -Milliseconds 200
            }
        }

        Wait-Job -Job $Job | Out-Null

        # Drain any final lines queued between the last loop iteration and job completion.
        foreach ($Item in @(Receive-Job -Job $Job -ErrorAction SilentlyContinue)) {
            $Line = $Item.ToString()
            if ($Line.StartsWith($ExitSentinel, [System.StringComparison]::Ordinal)) {
                $ExitText = $Line.Substring($ExitSentinel.Length)
                $ParsedExitCode = 0
                if ([int]::TryParse($ExitText, [ref]$ParsedExitCode)) {
                    $ExitCode = $ParsedExitCode
                }
                continue
            }

            $OutputLineCount++
            $Writer.WriteLine($Line)
            Write-Host $Line
        }

        if ($Job.State -eq "Failed") {
            $Reason = $Job.ChildJobs[0].JobStateInfo.Reason
            if ($null -ne $Reason) {
                throw "arduino-cli background process failed: $($Reason.Message)"
            }
            throw "arduino-cli background process failed."
        }

        if ($null -eq $ExitCode) {
            throw "arduino-cli completed without reporting an exit code. See: $LogFile"
        }

        if ($ExitCode -ne 0) {
            throw "arduino-cli failed with exit code $ExitCode. See: $LogFile"
        }
    }
    finally {
        $Stopwatch.Stop()
        $Writer.Dispose()
        if ($null -ne $Job) {
            if (($Job.State -eq "NotStarted") -or ($Job.State -eq "Running")) {
                Stop-Job -Job $Job -ErrorAction SilentlyContinue
            }
            Remove-Job -Job $Job -ErrorAction SilentlyContinue
        }
    }

    Write-Host ("[{0}] completed in {1}; {2} output lines captured." -f $Activity, (Format-Elapsed -Elapsed $Stopwatch.Elapsed), $OutputLineCount) -ForegroundColor Green
}

function Install-ArduinoCli {
    if ((Test-Path $CliExe) -and (-not $ForceSetup)) {
        Write-Host "Arduino CLI $ArduinoCliVersion already installed: $CliExe"
        return
    }

    Write-Step "Downloading Arduino CLI $ArduinoCliVersion"
    New-Item -ItemType Directory -Force -Path $CliInstallDir | Out-Null
    $ZipPath = Join-Path $ToolsRoot "arduino-cli-$ArduinoCliVersion.zip"
    $DownloadUrl = "https://github.com/arduino/arduino-cli/releases/download/v$ArduinoCliVersion/arduino-cli_${ArduinoCliVersion}_Windows_64bit.zip"

    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath -UseBasicParsing
    Expand-Archive -Path $ZipPath -DestinationPath $CliInstallDir -Force
    Remove-Item $ZipPath -Force

    if (-not (Test-Path $CliExe)) {
        throw "Arduino CLI executable was not found after extraction: $CliExe"
    }
}

function Write-CliConfig {
    New-Item -ItemType Directory -Force -Path $EnvRoot, $DataDir, $DownloadsDir, $UserDir | Out-Null

    $Yaml = @"
board_manager:
  additional_urls:
    - $Rp2040PackageIndex
directories:
  data: "$(Convert-ToYamlPath $DataDir)"
  downloads: "$(Convert-ToYamlPath $DownloadsDir)"
  user: "$(Convert-ToYamlPath $UserDir)"
updater:
  enable_notification: false
"@
    Set-Content -Path $CliConfig -Value $Yaml -Encoding UTF8
}

function Setup-Environment {
    Install-ArduinoCli
    Write-CliConfig

    Write-Step "Arduino CLI version"
    Invoke-Cli -Arguments @("version")

    Write-Step "Updating package indexes"
    Invoke-Cli -Arguments @("core", "update-index", "--config-file", $CliConfig)
    Invoke-Cli -Arguments @("lib", "update-index", "--config-file", $CliConfig)

    Write-Step "Installing Arduino-Pico core rp2040:rp2040@$Rp2040CoreVersion"
    Invoke-Cli -Arguments @(
        "core", "install", "rp2040:rp2040@$Rp2040CoreVersion",
        "--config-file", $CliConfig,
        "--additional-urls", $Rp2040PackageIndex
    )

    Write-Step "Installing Adafruit NeoPixel $NeoPixelVersion"
    Invoke-Cli -Arguments @(
        "lib", "install", "Adafruit NeoPixel@$NeoPixelVersion",
        "--config-file", $CliConfig
    )

    Write-Step "Environment setup complete"
    Show-Doctor
}

function Assert-Environment {
    if (-not (Test-Path $CliExe)) {
        throw "Arduino CLI is missing. Run setup_arduino_cli_environment.bat."
    }
    if (-not (Test-Path $CliConfig)) {
        throw "Arduino CLI configuration is missing. Run setup_arduino_cli_environment.bat."
    }
    if (-not (Test-Path (Join-Path $SketchDir "resistor_matrix_v1_dual_core.ino"))) {
        throw "Sketch file not found in project root: $SketchDir"
    }
}

function Show-Settings {
    Write-Host "Project:        $ProjectRoot"
    Write-Host "Sketch:         $SketchDir"
    Write-Host "Script version: $ArduinoCliScriptVersion"
    Write-Host "Arduino CLI:    $CliExe"
    Write-Host "CLI version:    $ArduinoCliVersion"
    Write-Host "RP2040 core:    $Rp2040CoreVersion"
    Write-Host "NeoPixel:       $NeoPixelVersion"
    Write-Host "Board FQBN:     $BoardFqbn"
    Write-Host "Board options:  $BoardOptionsText"
    Write-Host "Upload port:    $Port"
    Write-Host "Build folder:   $BuildDir"
}

function Show-Doctor {
    Assert-Environment
    Write-Step "Configured build environment"
    Show-Settings

    Write-Step "Installed cores"
    Invoke-Cli -Arguments @("core", "list", "--config-file", $CliConfig)

    Write-Step "Installed libraries"
    Invoke-Cli -Arguments @("lib", "list", "--config-file", $CliConfig)

    Write-Step "Connected boards"
    Invoke-Cli -Arguments @("board", "list", "--config-file", $CliConfig)
}

function Build-Firmware {
    Write-Host "[1/4] Preparing clean build workspace..." -ForegroundColor Cyan
    Assert-Environment
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
    New-Item -ItemType Directory -Force -Path $BuildDir, $LogDir, $DistDir | Out-Null

    $Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $BuildLog = Join-Path $LogDir "build-$Timestamp.log"

    Write-Host "[2/4] Compiling E-Resistor firmware..." -ForegroundColor Cyan
    if ($VerboseBuild) {
        Write-Host "Verbose mode enabled: every compiler command will be streamed live." -ForegroundColor Yellow
    } else {
        Write-Host "Standard mode enabled: an elapsed-time heartbeat is printed every $HeartbeatSeconds seconds." -ForegroundColor Yellow
        Write-Host "Use build_firmware_verbose.bat for full compiler output." -ForegroundColor DarkGray
    }
    Write-Step "Build configuration"
    Show-Settings

    $Arguments = @(
        "compile",
        "--config-file", $CliConfig,
        "--fqbn", $BoardFqbn,
        "--board-options", $BoardOptionsText,
        "--build-path", $BuildDir,
        "--warnings", "all"
    )
    if ($VerboseBuild) {
        $Arguments += "--verbose"
    }
    $Arguments += $SketchDir

    Invoke-Cli -Arguments $Arguments -LogFile $BuildLog -Activity "compile" -ShowHeartbeat

    Write-Host "[3/4] Collecting build artifacts..." -ForegroundColor Cyan
    $Extensions = @("*.uf2", "*.bin", "*.elf", "*.map")
    foreach ($Pattern in $Extensions) {
        Get-ChildItem -Path $BuildDir -Filter $Pattern -File -ErrorAction SilentlyContinue |
            ForEach-Object { Copy-Item $_.FullName -Destination $DistDir -Force }
    }

    $Artifacts = Get-ChildItem -Path $DistDir -File -ErrorAction SilentlyContinue
    if (-not $Artifacts) {
        throw "Build completed but no UF2/BIN/ELF artifacts were found in $BuildDir."
    }

    $Hashes = foreach ($Artifact in $Artifacts) {
        $Hash = Get-FileHash -Path $Artifact.FullName -Algorithm SHA256
        [ordered]@{
            name = $Artifact.Name
            size_bytes = $Artifact.Length
            sha256 = $Hash.Hash.ToLowerInvariant()
        }
    }

    $Manifest = [ordered]@{
        generated_utc = (Get-Date).ToUniversalTime().ToString("o")
        script_version = $ArduinoCliScriptVersion
        verbose_build = [bool]$VerboseBuild
        heartbeat_seconds = $HeartbeatSeconds
        arduino_cli_version = $ArduinoCliVersion
        rp2040_core_version = $Rp2040CoreVersion
        neopixel_version = $NeoPixelVersion
        fqbn = $BoardFqbn
        board_options = $BoardOptions
        port = $Port
        sketch = $SketchDir
        artifacts = $Hashes
    }
    $Manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $DistDir "build_manifest.json") -Encoding UTF8

    Write-Host "[4/4] Build artifacts and manifest are ready." -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Build passed." -ForegroundColor Green
    Write-Host "Log:       $BuildLog"
    Write-Host "Artifacts: $DistDir"
}

function Flash-Firmware {
    Assert-Environment
    if (-not (Test-Path $BuildDir)) {
        throw "Build directory not found. Run build_firmware.bat first."
    }

    $Uf2 = Get-ChildItem -Path $BuildDir -Filter "*.uf2" -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $Uf2) {
        throw "No UF2 image exists in $BuildDir. Run build_firmware.bat first."
    }

    $Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $UploadLog = Join-Path $LogDir "upload-$Timestamp.log"

    Write-Step "Uploading firmware to $Port using Default (UF2)"
    Write-Host "The script will use the RP2040 1200-baud reset sequence."
    Write-Host "If automatic reset fails, hold BOOTSEL while reconnecting USB and rerun the flash command." -ForegroundColor Yellow

    Invoke-Cli -Arguments @(
        "upload",
        "--config-file", $CliConfig,
        "--port", $Port,
        "--fqbn", $BoardFqbn,
        "--board-options", $BoardOptionsText,
        "--build-path", $BuildDir,
        "--discovery-timeout", "30s",
        $SketchDir
    ) -LogFile $UploadLog -Activity "upload" -ShowHeartbeat

    Write-Host ""
    Write-Host "Upload completed." -ForegroundColor Green
    Write-Host "Log: $UploadLog"
}

function Clean-Build {
    if (Test-Path $BuildRoot) {
        Remove-Item -Recurse -Force $BuildRoot
    }
    Write-Host "Build output removed: $BuildRoot"
}

switch ($Action) {
    "Setup"      { Setup-Environment }
    "Doctor"     { Show-Doctor }
    "Build"      { Build-Firmware }
    "Flash"      { Flash-Firmware }
    "BuildFlash" { Build-Firmware; Flash-Firmware }
    "Clean"      { Clean-Build }
    "ListBoards" {
        Assert-Environment
        Invoke-Cli -Arguments @("board", "list", "--config-file", $CliConfig)
    }
}
