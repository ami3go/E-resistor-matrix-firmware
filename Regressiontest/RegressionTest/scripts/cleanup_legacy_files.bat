@echo off
setlocal EnableExtensions
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "LOGDIR=%ROOT%\results\setup"
if not exist "%LOGDIR%" mkdir "%LOGDIR%" >nul 2>&1
set "LOGFILE=%LOGDIR%\legacy_cleanup.log"
>"%LOGFILE%" echo E-Resistor RegressionTest v2.6.1 legacy cleanup

for %%F in (
  "run_python_all_safe.bat"
  "run_python_custom.bat"
  "run_python_hil_single_channel.bat"
  "run_python_read_only.bat"
  "run_python_safe_output.bat"
  "run_python_source_build.bat"
  "run_python_source_check.bat"
  "run_regression.py"
  "setup_python_environment.bat"
  "validate_python_harness.bat"
  "run_robot_source_build.bat"
  "run_robot_source_check.bat"
  "e_resistor_regression\cli.py"
  "e_resistor_regression\source_checks.py"
  "e_resistor_regression\build_check.py"
  "robot_framework\suites\source_build.robot"
  "robot_framework\suites\source_check.robot"
  "robot_framework\resources\source_build.resource"
  "robot_framework\resources\source_check.resource"
  "robot_framework\profiles\source_build.args"
  "robot_framework\profiles\source_check.args"
  "tests\test_python_bat_layout.py"
  "tests\test_no_arduino_cli.py"
) do (
  if exist "%ROOT%\%%~F" (
    del /F /Q "%ROOT%\%%~F" >nul 2>&1
    if exist "%ROOT%\%%~F" (
      >>"%LOGFILE%" echo FAILED %%~F
    ) else (
      >>"%LOGFILE%" echo REMOVED %%~F
    )
  )
)

>>"%LOGFILE%" echo COMPLETE
exit /b 0
