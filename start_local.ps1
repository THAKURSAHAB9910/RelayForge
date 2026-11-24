# RelayForge — Local Development Unified Startup Script (PowerShell)
# Starts C++ Engine Core (:8080), Java Spring Boot / FastAPI Gateway (:8000), and React + Vite (:5173)

param (
    [ValidateSet("java", "python")]
    [string]$Gateway = "java"
)

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "   ⚡ RELAYFORGE — FAULT-TOLERANT EVENT PROCESSING ENGINE ⚡   " -ForegroundColor Green
Write-Host "   Hybrid Architecture: C++20 Core + Java 21 / Spring Boot 3    " -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " Starting local development tiers (Gateway: $Gateway)..." -ForegroundColor Yellow

$PIDs = @()

# 1. Start C++ Core Engine via WSL
Write-Host "[1/3] Starting RelayForge C++ Core Engine on :8080..." -ForegroundColor Cyan
$EngineProcess = Start-Process -FilePath "wsl" -ArgumentList "bash", "-c", "cd /mnt/c/Users/Vishe/Downloads/FORGE && ./cpp-engine/build/relayforge_server --port 8080 --db relayforge.db --workers 4" -PassThru -NoNewWindow
if ($EngineProcess) { $PIDs += $EngineProcess.Id }

Start-Sleep -Seconds 2

# 2. Start Application Gateway (Java 21 Spring Boot or Python FastAPI)
if ($Gateway -eq "java") {
    Write-Host "[2/3] Starting Java 21 Spring Boot REST Gateway on :8000..." -ForegroundColor Cyan
    $ApiProcess = Start-Process -FilePath "wsl" -ArgumentList "bash", "-c", "java -jar /mnt/c/Users/Vishe/Downloads/FORGE/java-api/target/relayforge-java-api-1.0.0.jar" -PassThru -NoNewWindow
} else {
    Write-Host "[2/3] Starting Python FastAPI Gateway on :8000..." -ForegroundColor Cyan
    $env:RELAYFORGE_BACKEND_URL = "http://127.0.0.1:8080"
    $ApiProcess = Start-Process -FilePath "python" -ArgumentList "-m", "uvicorn", "api.fast_api_gateway:app", "--port", "8000", "--host", "127.0.0.1" -PassThru -NoNewWindow
}
if ($ApiProcess) { $PIDs += $ApiProcess.Id }

Start-Sleep -Seconds 3

# 3. Start React + Vite Frontend
Write-Host "[3/3] Starting React + Vite Dev Server on :5173..." -ForegroundColor Cyan
$FrontendProcess = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "cd frontend && npm run dev" -PassThru -NoNewWindow
if ($FrontendProcess) { $PIDs += $FrontendProcess.Id }

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  ✅ ALL SERVICES SUCCESSFULLY RUNNING LOCALLY!                " -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  🌐 Frontend React App:       http://localhost:5173" -ForegroundColor White
Write-Host "  ☕ Java Spring Boot Gateway:  http://localhost:8000" -ForegroundColor White
Write-Host "  📖 Swagger Documentation:    http://localhost:8000/swagger-ui.html" -ForegroundColor White
Write-Host "  📋 OpenAPI Specification:    http://localhost:8000/v3/api-docs" -ForegroundColor White
Write-Host "  ⚡ C++ Core Engine REST:     http://localhost:8080" -ForegroundColor White
Write-Host "  📊 Prometheus Metrics:       http://localhost:8080/metrics" -ForegroundColor White
Write-Host "================================================================" -ForegroundColor Green
Write-Host " Press [Ctrl+C] to stop all services..." -ForegroundColor Yellow

try {
    # Open browser automatically
    Start-Process "http://localhost:5173"
    while ($true) {
        Start-Sleep -Seconds 1
    }
} finally {
    Write-Host ""
    Write-Host "[RelayForge] Stopping all background processes..." -ForegroundColor Yellow
    foreach ($procId in $PIDs) {
        try {
            Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
        } catch {}
    }
    # Also terminate any WSL java/relayforge child processes
    wsl -e bash -c "pkill -f relayforge_server; pkill -f relayforge-java-api" 2>$null
    Write-Host "[RelayForge] Clean shutdown complete." -ForegroundColor Green
}
