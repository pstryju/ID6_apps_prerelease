@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0source\tools\Test-Evo-Most.ps1" -EngineInfo %*
set "evo_exit=%errorlevel%"
echo.
pause
exit /b %evo_exit%
