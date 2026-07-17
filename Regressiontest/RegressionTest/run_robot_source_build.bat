@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G2"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%ERESISTOR_GATE%"=="%RUN_GATE%" echo NOTE: package default gate is %RUN_GATE%; bench_config.local.bat contains %ERESISTOR_GATE%.
if "%ERESISTOR_SOURCE_DIR%"=="" (
    echo ERROR: ERESISTOR_SOURCE_DIR is empty.
    echo Set it in robot_framework\variables\bench_config.local.bat.
    exit /b 3
)
if not exist "%ERESISTOR_SOURCE_DIR%" (
    echo ERROR: Firmware source directory does not exist:
    echo   %ERESISTOR_SOURCE_DIR%
    exit /b 3
)

echo Running source/build regression, gate %RUN_GATE% ...
cd /d "%REGRESSION_ROOT%"
if "%ERESISTOR_FQBN%"=="" (
    "%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
      --profile source_build ^
      --gate "%RUN_GATE%" ^
      --output "%ERESISTOR_OUTPUT_DIR%" ^
      --source-dir "%ERESISTOR_SOURCE_DIR%" ^
      --arduino-cli "%ERESISTOR_ARDUINO_CLI%" ^
      --baseline "%ERESISTOR_BASELINE%" ^
      --gate-manifest "%ERESISTOR_GATE_MANIFEST%"
) else (
    "%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
      --profile source_build ^
      --gate "%RUN_GATE%" ^
      --output "%ERESISTOR_OUTPUT_DIR%" ^
      --source-dir "%ERESISTOR_SOURCE_DIR%" ^
      --arduino-cli "%ERESISTOR_ARDUINO_CLI%" ^
      --fqbn "%ERESISTOR_FQBN%" ^
      --baseline "%ERESISTOR_BASELINE%" ^
      --gate-manifest "%ERESISTOR_GATE_MANIFEST%"
)
exit /b %errorlevel%
