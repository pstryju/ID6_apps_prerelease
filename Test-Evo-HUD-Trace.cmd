@echo off
echo HUD trace is DISABLED after a target crash and Evo restart. Do not retry.
set "evo_exit=1"
echo.
pause
exit /b %evo_exit%
