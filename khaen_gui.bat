@echo off
:: khaen_gui.bat — Lanza la GUI del prototipo Khaen
:: Doble-click para abrir
echo.
echo  Khaen Physical Model Synthesizer - Prototipo Portable
echo  =====================================================
echo.

:: Verificar numpy
python -c "import numpy" 2>nul
if %errorlevel% neq 0 (
    echo  Instalando dependencias...
    pip install numpy scipy sounddevice
)

:: Verificar sounddevice
python -c "import sounddevice" 2>nul
if %errorlevel% neq 0 (
    echo  Instalando sounddevice...
    pip install sounddevice
)

echo  Lanzando GUI...
echo.
python "%~dp0khaen_prototype.py" --gui

if %errorlevel% neq 0 (
    echo.
    echo  La GUI fallo. Intentando modo consola + guardar WAV...
    python "%~dp0khaen_prototype.py" --save --preset 0
    echo.
    echo  Archivo guardado en la carpeta actual.
    pause
)
