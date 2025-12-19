# ============================================================================
# PostgreSQL 18.1 Stop Script - Development Version
# Version: 1.0
# Description: Stop PostgreSQL server
# ============================================================================

$ErrorActionPreference = "Continue"
$PG_VERSION = "18.1-2"
$PG_FOLDER_NAME = "postgresql-$PG_VERSION"
$CURRENT_DIR = $PSScriptRoot
$PG_DIR = Join-Path $CURRENT_DIR $PG_FOLDER_NAME
$DATA_DIR = Join-Path $CURRENT_DIR "pg_data"
$PG_BIN = Join-Path $PG_DIR "bin"
$PG_CTL = Join-Path $PG_BIN "pg_ctl.exe"

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     PostgreSQL $PG_VERSION Stop" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Check if pg_ctl exists
# ============================================================================
if (-not (Test-Path $PG_CTL)) {
    Write-Host "? pg_ctl not found: $PG_CTL" -ForegroundColor Red
    Write-Host ""
    Write-Host "PostgreSQL may not be installed at expected location." -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if (-not (Test-Path $DATA_DIR)) {
    Write-Host "? Data directory not found: $DATA_DIR" -ForegroundColor Red
    Write-Host ""
    Write-Host "PostgreSQL may not have been initialized." -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# ============================================================================
# Check PostgreSQL status
# ============================================================================
Write-Host "Checking PostgreSQL status..." -ForegroundColor Yellow

try {
    $statusOutput = & $PG_CTL status -D $DATA_DIR 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "??  PostgreSQL is not running" -ForegroundColor Yellow
        Write-Host ""
        Write-Host $statusOutput
        Write-Host ""
        Read-Host "Press Enter to exit"
        exit 0
    }
    
    Write-Host "? PostgreSQL is running" -ForegroundColor Green
    Write-Host ""
    Write-Host $statusOutput
    Write-Host ""
    
} catch {
    Write-Host "??  Could not determine PostgreSQL status" -ForegroundColor Yellow
    Write-Host ""
}

# ============================================================================
# Stop PostgreSQL
# ============================================================================
Write-Host "Stopping PostgreSQL..." -ForegroundColor Yellow

try {
    # Stop using pg_ctl with smart mode (wait for clients to disconnect)
    Write-Host "Running: pg_ctl stop -D $DATA_DIR -m smart" -ForegroundColor Gray
    Write-Host ""
    
    & $PG_CTL stop -D $DATA_DIR -m smart
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "??  pg_ctl stop returned exit code: $LASTEXITCODE" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Trying immediate shutdown..." -ForegroundColor Yellow
        
        & $PG_CTL stop -D $DATA_DIR -m immediate
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "? Failed to stop PostgreSQL" -ForegroundColor Red
            Write-Host ""
            Write-Host "You may need to manually kill the postgres process:" -ForegroundColor Yellow
            Write-Host "  Get-Process postgres | Stop-Process -Force" -ForegroundColor Cyan
            Write-Host ""
            Read-Host "Press Enter to exit"
            exit 1
        }
    }
    
    Write-Host ""
    Write-Host "Waiting for server to stop..." -ForegroundColor Yellow
    
    # Wait for shutdown
    $maxWait = 30  # 30 seconds
    $waited = 0
    $stopped = $false
    
    while ($waited -lt $maxWait) {
        Start-Sleep -Seconds 2
        $waited += 2
        
        Write-Host "  . Waiting... ($waited seconds)" -ForegroundColor Gray
        
        # Check if port is still in use
        $portCheck = Get-NetTCPConnection -LocalPort 5432 -State Listen -ErrorAction SilentlyContinue
        if (-not $portCheck) {
            $stopped = $true
            break
        }
    }
    
    Write-Host ""
    
    if ($stopped) {
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "     PostgreSQL Stopped Successfully!" -ForegroundColor Green
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host ""
        
        Write-Host "? Port 5432 is now available" -ForegroundColor Green
        Write-Host ""
        
        Write-Host "To start PostgreSQL again:" -ForegroundColor Yellow
        Write-Host "  .\start_postgresql.ps1" -ForegroundColor Cyan
        Write-Host ""
        
    } else {
        Write-Host "??  PostgreSQL stop command issued but port still in use" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "The server may still be shutting down." -ForegroundColor White
        Write-Host ""
        Write-Host "Check status in a few seconds:" -ForegroundColor Yellow
        Write-Host "  .\$PG_FOLDER_NAME\bin\pg_ctl.exe status -D pg_data" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Check port:" -ForegroundColor Yellow
        Write-Host "  Get-NetTCPConnection -LocalPort 5432" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Force stop if needed:" -ForegroundColor Yellow
        Write-Host "  Get-Process postgres | Stop-Process -Force" -ForegroundColor Cyan
        Write-Host ""
    }
    
} catch {
    Write-Host "? Failed to stop PostgreSQL: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Try manual cleanup:" -ForegroundColor Yellow
    Write-Host "  1. Find postgres processes:" -ForegroundColor White
    Write-Host "     Get-Process postgres" -ForegroundColor Cyan
    Write-Host "  2. Kill processes:" -ForegroundColor White
    Write-Host "     Get-Process postgres | Stop-Process -Force" -ForegroundColor Cyan
    Write-Host "  3. Check port:" -ForegroundColor White
    Write-Host "     Get-NetTCPConnection -LocalPort 5432" -ForegroundColor Cyan
    Write-Host ""
}

Read-Host "Press Enter to exit"
