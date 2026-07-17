param(
    [ValidateSet("read_only", "safe_output", "hil_single_channel", "source_check")]
    [string]$Profile = "read_only",
    [ValidateSet("G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8", "G9")]
    [string]$Gate = "G0",
    [string]$HostAddress = "192.168.0.55",
    [string]$Output = ".\results\robot",
    [string]$SerialPort = "auto",
    [string]$DmmResource = "auto",
    [int]$HilChannel = 1,
    [switch]$AllowOutputTests,
    [switch]$AllowActiveOutputTests,
    [string]$FixtureConfirmation = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ArgsList = @(
    "$ScriptDir\run_robot.py",
    "--profile", $Profile,
    "--gate", $Gate,
    "--host", $HostAddress,
    "--output", $Output,
    "--serial-port", $SerialPort,
    "--dmm-resource", $DmmResource,
    "--hil-channel", $HilChannel
)
if ($AllowOutputTests) { $ArgsList += "--allow-output-tests" }
if ($AllowActiveOutputTests) { $ArgsList += "--allow-active-output-tests" }
if ($FixtureConfirmation) { $ArgsList += @("--fixture-confirmation", $FixtureConfirmation) }

& python @ArgsList
exit $LASTEXITCODE
