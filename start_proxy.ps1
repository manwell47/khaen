# start_proxy.ps1 — Levanta LiteLLM Proxy para el proyecto Khaen VST3
# Puerto: 4000 | Estrategia: usage-based-routing
# Uso: .\start_proxy.ps1 [-Background]

param([switch]$Background)

$configPath = Join-Path $PSScriptRoot "litellm_config.yaml"
$logPath    = Join-Path $PSScriptRoot "litellm.log"

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

# Verificar que litellm está instalado
if (-not (Get-Command litellm -ErrorAction SilentlyContinue)) {
    Write-Host "⚠  LiteLLM no encontrado. Instalando..." -ForegroundColor Yellow
    pip install "litellm[proxy]" --quiet
    if ($LASTEXITCODE -ne 0) {
        Write-Error "❌ Error instalando LiteLLM. Ejecuta: pip install 'litellm[proxy]'"
        exit 1
    }
    Write-Host "✅ LiteLLM instalado correctamente." -ForegroundColor Green
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
    # Lanzar en background con UTF-8 forzado (evita UnicodeEncodeError en cp1252)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName    = $litellmExe
    $psi.Arguments   = "--config `"$configPath`" --port 4000"
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    $psi.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8"
    $psi.EnvironmentVariables["PYTHONUTF8"]       = "1"
    $proc = [System.Diagnostics.Process]::Start($psi)
    Start-Sleep 2
    if ($proc.HasExited) {
        Write-Host "❌ Proxy falló al iniciar. Ver $logPath.err" -ForegroundColor Red
    } else {
        Write-Host "🚀 Proxy activo (PID: $($proc.Id)) — http://localhost:4000" -ForegroundColor Green
        Write-Host "   Para detenerlo: Stop-Process -Id $($proc.Id)" -ForegroundColor DarkGray
        $proc.Id | Out-File -FilePath (Join-Path $PSScriptRoot ".proxy_pid") -Force
    }
} else {
    # Lanzar en foreground visible
    Write-Host "🚀 Iniciando proxy (Ctrl+C para detener)..." -ForegroundColor Green
    $env:PYTHONIOENCODING = "utf-8"
    $env:PYTHONUTF8 = "1"
    & $litellmExe --config $configPath --port 4000
}
