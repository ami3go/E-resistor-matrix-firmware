@echo off
call "%~dp0scripts\setup_windows_environment.bat" python
exit /b %errorlevel%
