@echo off
:: Khaen_Standalone.bat — Launch the Standalone (.exe) version of Khaen
:: Double-click to open

echo.
echo  Khaen Physical Model Synthesizer - Standalone
echo  =============================================
echo.

set EXE_PATH="%~dp0build\Khaen_artefacts\Release\Standalone\Khaen.exe"

if exist %EXE_PATH% (
    echo Launching Khaen Standalone...
    start "" %EXE_PATH%
) else (
    echo [ERROR] Standalone executable not found.
    echo Please compile the project first by running build.bat or .\build.ps1
    pause
)
