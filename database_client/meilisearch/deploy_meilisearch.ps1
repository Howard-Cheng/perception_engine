# ================================================================
# Perception Engine - MeiliSearch Automated Deployment Script
# ================================================================
# 
# Features:
# 1. Download MeiliSearch binary
# 2. Start MeiliSearch server
# 3. Verify connection
# 4. Configure for Perception Engine
#
# Usage:
#   .\deploy_meilisearch.ps1
#
# ================================================================

param(
    [string]$Version = "v1.6.0",
    [int]$Port = 7700,
    [string]$MasterKey = "perception_engine_master_key_2025",
    [string]$DataPath = "./meili_data",
    [string]$Environment = "development",
    [switch]$AsService = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Perception Engine - MeiliSearch Deployment" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ============================================
# 1. Check if MeiliSearch already exists
# ============================================
Write-Host "[1/6] Checking for existing MeiliSearch..." -ForegroundColor Yellow

$meiliExe = "meilisearch.exe"
$meiliExists = Test-Path $meiliExe

if ($meiliExists) {
    Write-Host "  ! MeiliSearch binary found" -ForegroundColor Yellow
    $version = & .\$meiliExe --version 2>&1 | Select-String "meilisearch" | Out-String
    Write-Host "  Current version: $version" -ForegroundColor Gray
    
    Write-Host ""
    Write-Host "  Options:" -ForegroundColor Yellow
    Write-Host "  1. Use existing binary" -ForegroundColor White
    Write-Host "  2. Download latest version" -ForegroundColor White
    Write-Host ""
    $choice = Read-Host "  Please choose (1/2)"
    
    if ($choice -eq "2") {
        Remove-Item $meiliExe -Force
        $meiliExists = $false
        Write-Host "  ? Removed old binary" -ForegroundColor Green
    } else {
        Write-Host "  ? Using existing binary" -ForegroundColor Green
    }
}

Write-Host ""

# ============================================
# 2. Download MeiliSearch if needed
# ============================================
if (-not $meiliExists) {
    Write-Host "[2/6] Downloading MeiliSearch..." -ForegroundColor Yellow
    
    $downloadUrl = "https://github.com/meilisearch/meilisearch/releases/latest/download/meilisearch-windows-amd64.exe"
    
    Write-Host "  ¡ú Downloading from GitHub..." -ForegroundColor Cyan
    Write-Host "  URL: $downloadUrl" -ForegroundColor Gray
    
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $downloadUrl -OutFile $meiliExe
        Write-Host "  ? Download complete" -ForegroundColor Green
        
        # Verify download
        if ((Get-Item $meiliExe).Length -lt 1MB) {
            Write-Host "  ? Download may have failed (file too small)" -ForegroundColor Red
            exit 1
        }
        
        $fileSize = [math]::Round((Get-Item $meiliExe).Length / 1MB, 2)
        Write-Host "  File size: $fileSize MB" -ForegroundColor Gray
        
    } catch {
        Write-Host "  ? Download failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host ""
        Write-Host "  Please download manually from:" -ForegroundColor Yellow
        Write-Host "  https://github.com/meilisearch/meilisearch/releases" -ForegroundColor Cyan
        exit 1
    }
} else {
    Write-Host "[2/6] Using existing MeiliSearch binary" -ForegroundColor Yellow
    Write-Host "  ? Binary found" -ForegroundColor Green
}

Write-Host ""

# ============================================
# 3. Create data directory
# ============================================
Write-Host "[3/6] Creating data directory..." -ForegroundColor Yellow

if (-not (Test-Path $DataPath)) {
    New-Item -ItemType Directory -Path $DataPath -Force | Out-Null
    Write-Host "  ? Created: $DataPath" -ForegroundColor Green
} else {
    Write-Host "  ? Directory exists: $DataPath" -ForegroundColor Green
}

Write-Host ""

# ============================================
# 4. Stop existing MeiliSearch process
# ============================================
Write-Host "[4/6] Checking for running MeiliSearch..." -ForegroundColor Yellow

$existingProcess = Get-Process -Name "meilisearch" -ErrorAction SilentlyContinue

if ($existingProcess) {
    Write-Host "  ! MeiliSearch is already running (PID: $($existingProcess.Id))" -ForegroundColor Yellow
    Write-Host "  ¡ú Stopping existing process..." -ForegroundColor Cyan
    
    Stop-Process -Name "meilisearch" -Force
    Start-Sleep -Seconds 2
    
    Write-Host "  ? Process stopped" -ForegroundColor Green
} else {
    Write-Host "  ? No running process found" -ForegroundColor Green
}

Write-Host ""

# ============================================
# 5. Start MeiliSearch
# ============================================
Write-Host "[5/6] Starting MeiliSearch server..." -ForegroundColor Yellow

$meiliArgs = @(
    "--db-path", $DataPath,
    "--env", $Environment,
    "--http-addr", "127.0.0.1:$Port"
)

if ($MasterKey) {
    $meiliArgs += "--master-key"
    $meiliArgs += $MasterKey
}

Write-Host "  Configuration:" -ForegroundColor Gray
Write-Host "    Port:        $Port" -ForegroundColor Gray
Write-Host "    Environment: $Environment" -ForegroundColor Gray
Write-Host "    Data Path:   $DataPath" -ForegroundColor Gray
Write-Host "    Master Key:  $(if ($MasterKey) { '***' + $MasterKey.Substring([Math]::Max(0, $MasterKey.Length - 4)) } else { 'None (insecure)' })" -ForegroundColor Gray
Write-Host ""

if ($AsService) {
    Write-Host "  ¡ú Starting as background service..." -ForegroundColor Cyan
    Write-Host "  Note: This requires 'nssm' to be installed" -ForegroundColor Yellow
    Write-Host "  Install nssm: choco install nssm" -ForegroundColor Gray
    # TODO: Implement service installation
    Write-Host "  ! Service mode not yet implemented, starting as process..." -ForegroundColor Yellow
}

Write-Host "  ¡ú Starting MeiliSearch process..." -ForegroundColor Cyan

try {
    # Start process in background
    $process = Start-Process -FilePath ".\$meiliExe" -ArgumentList $meiliArgs -PassThru -WindowStyle Hidden
    
    if ($process) {
        Write-Host "  ? MeiliSearch started (PID: $($process.Id))" -ForegroundColor Green
    } else {
        Write-Host "  ? Failed to start MeiliSearch" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "  ? Error starting MeiliSearch: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""

# ============================================
# 6. Wait for MeiliSearch to be ready
# ============================================
Write-Host "[6/6] Waiting for MeiliSearch to be ready..." -ForegroundColor Yellow

$maxRetries = 30
$retryCount = 0
$isReady = $false
$healthUrl = "http://localhost:$Port/health"

while (-not $isReady -and $retryCount -lt $maxRetries) {
    try {
        $response = Invoke-RestMethod -Uri $healthUrl -Method Get -TimeoutSec 2 -ErrorAction SilentlyContinue
        if ($response.status -eq "available") {
            $isReady = $true
            Write-Host "  ? MeiliSearch is ready!" -ForegroundColor Green
        }
    } catch {
        $retryCount++
        Write-Host "  ¡ú Waiting... ($retryCount/$maxRetries)" -ForegroundColor Gray
        Start-Sleep -Seconds 1
    }
}

if (-not $isReady) {
    Write-Host "  ? MeiliSearch startup timeout!" -ForegroundColor Red
    Write-Host "  Please check the process manually" -ForegroundColor Yellow
    exit 1
}

# Get version info
try {
    $versionUrl = "http://localhost:$Port/version"
    $versionInfo = Invoke-RestMethod -Uri $versionUrl -Method Get
    Write-Host "  Version: $($versionInfo.pkgVersion)" -ForegroundColor Gray
    Write-Host "  Commit:  $($versionInfo.commitSha.Substring(0, 8))" -ForegroundColor Gray
} catch {
    # Ignore version check errors
}

Write-Host ""

# ============================================
# 7. Display deployment information
# ============================================
Write-Host "========================================" -ForegroundColor Green
Write-Host "  MeiliSearch Deployment Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Access URL:     http://localhost:$Port" -ForegroundColor Cyan
Write-Host "  Health Check:   http://localhost:$Port/health" -ForegroundColor Cyan
Write-Host "  Data Path:      $DataPath" -ForegroundColor Cyan
Write-Host "  Environment:    $Environment" -ForegroundColor Cyan
if ($MasterKey) {
    Write-Host "  Master Key:     $MasterKey" -ForegroundColor Cyan
}
Write-Host ""
Write-Host "  Common Commands:" -ForegroundColor Yellow
Write-Host "    Health check: curl http://localhost:$Port/health" -ForegroundColor White
Write-Host "    View stats:   curl http://localhost:$Port/stats" -ForegroundColor White
Write-Host "    Stop server:  Stop-Process -Name meilisearch" -ForegroundColor White
Write-Host ""
Write-Host "  Dashboard:" -ForegroundColor Yellow
Write-Host "    Open browser: http://localhost:$Port" -ForegroundColor White
Write-Host ""
Write-Host "  Integration:" -ForegroundColor Yellow
Write-Host "    C++ code: auto client = DatabaseClientFactory::createMeiliSearch(" -ForegroundColor White
Write-Host "                  ""http://localhost:$Port"", ""$MasterKey"");" -ForegroundColor White
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Test connection
Write-Host "Testing connection..." -ForegroundColor Yellow
try {
    $healthCheck = Invoke-RestMethod -Uri $healthUrl -Method Get
    Write-Host "? Health check passed: $($healthCheck.status)" -ForegroundColor Green
    
    # Try to get stats
    if ($MasterKey) {
        $headers = @{
            "Authorization" = "Bearer $MasterKey"
        }
        $statsUrl = "http://localhost:$Port/stats"
        $stats = Invoke-RestMethod -Uri $statsUrl -Method Get -Headers $headers
        Write-Host "? Stats retrieved: $($stats.databaseSize) bytes in database" -ForegroundColor Green
    }
} catch {
    Write-Host "? Connection test failed, but server may still be starting" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "MeiliSearch is ready for Perception Engine!" -ForegroundColor Green
Write-Host ""

# Optional: Open dashboard in browser
$openBrowser = Read-Host "Open MeiliSearch dashboard in browser? (Y/N)"
if ($openBrowser -eq "Y" -or $openBrowser -eq "y") {
    Start-Process "http://localhost:$Port"
}
