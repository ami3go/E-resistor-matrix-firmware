@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" robot
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=%ERESISTOR_GATE%"
if not "%~1"=="" set "RUN_GATE=%~1"
set "FAILED=0"

echo ============================================================
echo Running all non-active Robot regression profiles for %RUN_GATE%
echo ============================================================

call "%~dp0run_robot_read_only.bat" "%RUN_GATE%"
if errorlevel 1 set "FAILED=1"

call "%~dp0run_robot_safe_output.bat" "%RUN_GATE%"
if errorlevel 1 set "FAILED=1"

if not "%ERESISTOR_SOURCE_DIR%"=="" if exist "%ERESISTOR_SOURCE_DIR%" (
    call "%~dp0run_robot_source_build.bat" "%RUN_GATE%"
    if errorlevel 1 set "FAILED=1"
) else (
    echo Source/build profile skipped because ERESISTOR_SOURCE_DIR is not configured.
)

if "%FAILED%"=="0" (
    echo All selected profiles passed.
    exit /b 0
)

echo One or more profiles failed. Review results\robot.
exit /b 1
