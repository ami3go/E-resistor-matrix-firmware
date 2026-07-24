@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G4"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%RUN_GATE%"=="G4" (
    echo ERROR: This profile is defined for Gate G4 only.
    exit /b 3
)

set "G3_BASELINE=%~2"
if "%G3_BASELINE%"=="" set "G3_BASELINE=%ERESISTOR_BASELINE%"
if "%G3_BASELINE%"=="" set "G3_BASELINE=%REGRESSION_ROOT%\baselines\G3_v0.6.2_board_503359277A981F9F\results.json"
if not exist "%G3_BASELINE%" (
    echo ERROR: Gate 3 baseline not found:
    echo   %G3_BASELINE%
    exit /b 3
)

echo ============================================================
echo GATE 4 TWO-PHASE PROFILE REGRESSION
echo Baseline: %G3_BASELINE%
echo Cycles:   1000
echo ============================================================

cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile gate4_profile ^
  --gate G4 ^
  --output "%ERESISTOR_OUTPUT_DIR%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
  --stress-iterations 1000 ^
  --heap-drift-limit "%ERESISTOR_HEAP_DRIFT_LIMIT%" ^
  --latency-regression-percent "%ERESISTOR_LATENCY_REGRESSION_PERCENT%" ^
  --baseline "%G3_BASELINE%" ^
  --gate-manifest "%ERESISTOR_GATE_MANIFEST%" ^
  --allow-output-tests
exit /b %errorlevel%
