@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0source\tools\test-evo-most.ps1" -ExhaustInfo %*
set "evo_exit=%errorlevel%"
echo.
pause
exit /b %evo_exit%
