param([string]$Venv = ".venv")
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
python -m venv (Join-Path $Root $Venv)
$Python = Join-Path $Root "$Venv\Scripts\python.exe"
& $Python -m pip install --upgrade pip
& $Python -m pip install -r (Join-Path $Root "requirements-robot.txt")
& $Python -m pip install -e "$Root"
Write-Host "Activate with: $Venv\Scripts\Activate.ps1"
