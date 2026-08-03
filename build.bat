@echo off
:: build.bat — Lanza el build script de Khaen desde CMD
:: Doble-click o ejecuta desde cmd: build.bat
echo.
echo  Khaen VST3 - Iniciando build desde PowerShell...
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
echo.
pause
