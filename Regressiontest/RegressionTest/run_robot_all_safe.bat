@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G4"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%ERESISTOR_GATE%"=="%RUN_GATE%" echo NOTE: package default gate is %RUN_GATE%; bench_config.local.bat contains %ERESISTOR_GATE%.
set "FAILED=0"

echo ============================================================
echo Running all non-active Robot regression profiles for %RUN_GATE%
echo ============================================================

call "%~dp0run_robot_read_only.bat" "%RUN_GATE%"
if errorlevel 1 set "FAILED=1"

call "%~dp0run_robot_safe_output.bat" "%RUN_GATE%"
if errorlevel 1 set "FAILED=1"

if "%FAILED%"=="0" (
    echo All selected profiles passed.
    exit /b 0
)

echo One or more profiles failed. Review results\robot.
exit /b 1
