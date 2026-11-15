@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0source\tools\uninstall-dashboard.ps1" %*
set "evo_exit=%errorlevel%"
echo.
pause
exit /b %evo_exit%
