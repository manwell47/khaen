# build.ps1 — Khaen VST3 Build Script
# Configura y compila el plugin en Release
# Uso: .\build.ps1 [-Debug] [-Clean]

param(
    [switch]$Debug,
    [switch]$Clean
)

$buildType = if ($Debug) { "Debug" } else { "Release" }
$buildDir  = Join-Path $PSScriptRoot "build"

Write-Host ""
Write-Host "[KHAEN] VST3 Build System" -ForegroundColor Cyan
Write-Host "===========================" -ForegroundColor DarkGray
Write-Host "   Type: $buildType" -ForegroundColor Gray
Write-Host ""

# ── Check prerequisites ────────────────────────────────────────────────────────
$missingDeps = @()

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $missingDeps += "CMake (https://cmake.org/download/)"
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    $missingDeps += "Git (https://git-scm.com)"
}

# Check MSVC (vswhere)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    $missingDeps += "Visual Studio 2022 with C++ Desktop workload"
}

if ($missingDeps.Count -gt 0) {
    Write-Host "[X] Missing dependencies:" -ForegroundColor Red
    foreach ($d in $missingDeps) { Write-Host "   • $d" -ForegroundColor Yellow }
    Write-Host ""
    Write-Host "Install them and run this script again." -ForegroundColor Gray
    exit 1
}

Write-Host "[OK] All prerequisites found." -ForegroundColor Green

# ── Clean if requested ─────────────────────────────────────────────────────────
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "[CLEAN] Cleaning build directory..." -ForegroundColor Yellow
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
Write-Host "[OK] Configuration complete." -ForegroundColor Green

# ── Build ──────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[BUILD] Building Khaen..." -ForegroundColor Cyan
$cores = [System.Environment]::ProcessorCount
& cmake --build $buildDir --config $buildType -- /m:$cores

if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] Build failed! Check output above." -ForegroundColor Red
    exit 1
}

# ── Verify output ──────────────────────────────────────────────────────────────
$vst3Path = Join-Path $buildDir "Khaen_artefacts\$buildType\VST3\Khaen.vst3"
if (Test-Path $vst3Path) {
    Write-Host ""
    Write-Host "[OK] BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "   VST3: $vst3Path" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "[INSTALL] To install: Copy Khaen.vst3 to:" -ForegroundColor Gray
    Write-Host "   Windows: C:\Program Files\Common Files\VST3" -ForegroundColor White
} else {
    Write-Host "[!] Build completed but VST3 not found at expected path." -ForegroundColor Yellow
    Write-Host "   Check: $buildDir" -ForegroundColor Gray
}
