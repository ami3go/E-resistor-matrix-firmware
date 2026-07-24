@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G5"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%RUN_GATE%"=="G5" (
    echo ERROR: This profile is defined for Gate G5 only.
    exit /b 3
)

set "G4_BASELINE=%~2"
if "%G4_BASELINE%"=="" set "G4_BASELINE=%ERESISTOR_BASELINE%"
if "%G4_BASELINE%"=="" set "G4_BASELINE=%REGRESSION_ROOT%\baselines\G4_v0.7.2_board_503359277A981F9F\results.json"
if not exist "%G4_BASELINE%" (
    echo ERROR: Gate 4 baseline not found:
    echo   %G4_BASELINE%
    exit /b 3
)

echo ============================================================
echo GATE 5 HTTP AND SCPI SERVICE REGRESSION
echo Baseline: %G4_BASELINE%
echo Page requests: 1000
echo /state latency limit: Gate 4 p95 + 5 percent
echo ============================================================

cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile gate5_service ^
  --gate G5 ^
  --output "%ERESISTOR_OUTPUT_DIR%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
  --stress-iterations 1000 ^
  --heap-drift-limit "%ERESISTOR_HEAP_DRIFT_LIMIT%" ^
  --latency-regression-percent 5.0 ^
  --baseline "%G4_BASELINE%" ^
  --gate-manifest "%ERESISTOR_GATE_MANIFEST%"
exit /b %errorlevel%
