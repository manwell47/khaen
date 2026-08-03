@echo off
:: khaen_gui.bat — Launch the Python prototype GUI for Khaen
:: Double-click to open
echo.
echo  Khaen Physical Model Synthesizer - Portable Prototype
echo  ======================================================
echo.

:: Check numpy
python -c "import numpy" 2>nul
if %errorlevel% neq 0 (
    echo  Installing dependencies...
    pip install numpy scipy sounddevice
)

:: Check sounddevice
python -c "import sounddevice" 2>nul
if %errorlevel% neq 0 (
    echo  Installing sounddevice...
    pip install sounddevice
)

echo  Launching GUI...
echo.
python "%~dp0khaen_prototype.py" --gui

if %errorlevel% neq 0 (
    echo.
    echo  GUI launch failed. Falling back to console mode + saving WAV...
    python "%~dp0khaen_prototype.py" --save --preset 0
    echo.
    echo  Audio file saved in current directory.
    pause
)
