@echo off
:: Khaen_Standalone.bat — Lanza la versión Standalone (.exe) del plugin Khaen
:: Doble-click para abrir

echo.
echo  Khaen Physical Model Synthesizer - Standalone
echo  =============================================
echo.

set EXE_PATH="%~dp0build\Khaen_artefacts\Release\Standalone\Khaen.exe"

if exist %EXE_PATH% (
    echo Iniciando Khaen...
    start "" %EXE_PATH%
) else (
    echo [ERROR] No se encuentra el archivo Standalone.
    echo Asegurate de compilar el proyecto primero ejecutando build.bat
    pause
)
