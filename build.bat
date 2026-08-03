@echo off
:: build.bat — Launch the Khaen build script from CMD
:: Double-click or run from cmd: build.bat
echo.
echo  Khaen VST3 - Starting build from PowerShell...
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
echo.
pause
