# build.ps1 — Khaen VST3 / AU / Standalone Build Script
# Configures and builds the plugin suite in Release mode
# Usage: .\build.ps1 [-Debug] [-Clean]

param(
    [switch]$Debug,
    [switch]$Clean
)

$buildType = if ($Debug) { "Debug" } else { "Release" }
$buildDir  = Join-Path $PSScriptRoot "build"

Write-Host ""
Write-Host "[KHAEN] Multi-Format Synthesizer Build System" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor DarkGray
Write-Host "   Target Config : $buildType" -ForegroundColor Gray
Write-Host ""

# ── Check prerequisites ────────────────────────────────────────────────────────
$missingDeps = @()

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $missingDeps += "CMake 3.22+ (https://cmake.org/download/)"
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    $missingDeps += "Git (https://git-scm.com)"
}

# Check MSVC on Windows
if ($IsWindows -or ($env:OS -like "*Windows*")) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $missingDeps += "Visual Studio 2022 with C++ Desktop workload"
    }
}

if ($missingDeps.Count -gt 0) {
    Write-Host "[X] Missing dependencies:" -ForegroundColor Red
    foreach ($d in $missingDeps) { Write-Host "   • $d" -ForegroundColor Yellow }
    Write-Host ""
    Write-Host "Please install them and run this script again." -ForegroundColor Gray
    exit 1
}

Write-Host "[OK] All prerequisites detected." -ForegroundColor Green

# ── Clean if requested ─────────────────────────────────────────────────────────
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "[CLEAN] Purging build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# ── CMake Configure ────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[CMAKE] Configuring with CMake..." -ForegroundColor Cyan
& cmake -B $buildDir `
        -DCMAKE_BUILD_TYPE=$buildType `
        -G "Visual Studio 17 2022" `
        -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] CMake configuration failed!" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Configuration successful." -ForegroundColor Green

# ── Build ──────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[BUILD] Compiling Khaen (VST3, Standalone, DSP tests)..." -ForegroundColor Cyan
$cores = [System.Environment]::ProcessorCount
& cmake --build $buildDir --config $buildType -- /m:$cores

if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] Build failed! Check output above." -ForegroundColor Red
    exit 1
}

# ── Verify output ──────────────────────────────────────────────────────────────
$vst3Path       = Join-Path $buildDir "Khaen_artefacts\$buildType\VST3\Khaen.vst3"
$standalonePath = Join-Path $buildDir "Khaen_artefacts\$buildType\Standalone\Khaen.exe"

Write-Host ""
Write-Host "[OK] BUILD COMPLETED SUCCESSFULLY!" -ForegroundColor Green
if (Test-Path $vst3Path) {
    Write-Host "   [VST3]       : $vst3Path" -ForegroundColor Cyan
    Write-Host "   [INSTALL]    : Copy to 'C:\Program Files\Common Files\VST3\'" -ForegroundColor Gray
}
if (Test-Path $standalonePath) {
    Write-Host "   [STANDALONE] : $standalonePath" -ForegroundColor Cyan
    Write-Host "   [RUN]        : Run directly or execute .\Khaen_Standalone.bat" -ForegroundColor Gray
}
Write-Host ""
