# start_proxy.ps1 — Launch LiteLLM Proxy for the Khaen VST3 project
# Port: 4000 | Strategy: usage-based-routing
# Usage: .\start_proxy.ps1 [-Background]

param([switch]$Background)

$configPath = Join-Path $PSScriptRoot "litellm_config.yaml"
$examplePath = Join-Path $PSScriptRoot "litellm_config.example.yaml"
$logPath    = Join-Path $PSScriptRoot "litellm.log"

if (-not (Test-Path $configPath) -and (Test-Path $examplePath)) {
    Write-Host "[!] 'litellm_config.yaml' not found. Creating from template..." -ForegroundColor Yellow
    Copy-Item $examplePath $configPath
}

# ── Ensure pip-installed scripts are on PATH ─────────────────────────────────
$pyScripts = "$env:APPDATA\Python\Python314\Scripts"
if ((Test-Path $pyScripts) -and ($env:PATH -notlike "*$pyScripts*")) {
    $env:PATH += ";$pyScripts"
    Write-Host "📌 Added Python scripts to PATH: $pyScripts" -ForegroundColor DarkGray
}
# Also try py launcher path discovery
if (-not (Get-Command litellm -ErrorAction SilentlyContinue)) {
    $pyPaths = (python -c "import sysconfig; print(sysconfig.get_path('scripts'))" 2>$null)
    if ($pyPaths -and (Test-Path $pyPaths)) {
        $env:PATH += ";$pyPaths"
    }
}

Write-Host "🎋 Khaen VST3 — LiteLLM Proxy" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor DarkGray

# Check if litellm is installed
if (-not (Get-Command litellm -ErrorAction SilentlyContinue)) {
    Write-Host "⚠  LiteLLM not found. Installing..." -ForegroundColor Yellow
    pip install "litellm[proxy]" --quiet
    if ($LASTEXITCODE -ne 0) {
        Write-Error "❌ Error installing LiteLLM. Run: pip install 'litellm[proxy]'"
        exit 1
    }
    Write-Host "✅ LiteLLM installed successfully." -ForegroundColor Green
}

Write-Host "📄 Config: $configPath" -ForegroundColor Gray
Write-Host "📝 Log:    $logPath"    -ForegroundColor Gray
Write-Host "🌐 URL:    http://localhost:4000" -ForegroundColor Green
Write-Host ""

# Resolved litellm.exe full path (avoids PATH issues on Windows)
$litellmExe = "$env:APPDATA\Python\Python314\Scripts\litellm.exe"
if (-not (Test-Path $litellmExe)) {
    $litellmExe = (Get-Command litellm -ErrorAction SilentlyContinue)?.Source
}
if (-not $litellmExe -or -not (Test-Path $litellmExe)) {
    Write-Error "Cannot locate litellm.exe. Run: pip install 'litellm[proxy]'"
    exit 1
}

if ($Background) {
    # Launch in background with UTF-8 forced
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName    = $litellmExe
    $psi.Arguments   = "--config `"$configPath`" --port 4000"
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8

    $proc = [System.Diagnostics.Process]::Start($psi)
    $proc.Id | Out-File (Join-Path $PSScriptRoot ".proxy_pid")
    Write-Host "🚀 LiteLLM Proxy running in background (PID: $($proc.Id))" -ForegroundColor Green
} else {
    & $litellmExe --config $configPath --port 4000
}
