# ============================================================================
# PostgreSQL 18.1 Startup Script - Development Version
# Version: 1.0
# Description: Start PostgreSQL server
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
Write-Host "     PostgreSQL $PG_VERSION Startup" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Check if PostgreSQL binary exists
# ============================================================================
if (-not (Test-Path $PG_DIR)) {
    Write-Host "? PostgreSQL binary not found: $PG_DIR" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please run deployment first:" -ForegroundColor Yellow
    Write-Host "  .\deploy_postgresql_binary.ps1" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if (-not (Test-Path $PG_CTL)) {
    Write-Host "? pg_ctl not found: $PG_CTL" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please re-run deployment:" -ForegroundColor Yellow
    Write-Host "  .\deploy_postgresql_binary.ps1" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# ============================================================================
# Check if data directory exists
# ============================================================================
if (-not (Test-Path $DATA_DIR)) {
    Write-Host "? Data directory not found: $DATA_DIR" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please run deployment first to initialize database:" -ForegroundColor Yellow
    Write-Host "  .\deploy_postgresql_binary.ps1" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# ============================================================================
# Check if PostgreSQL is already running
# ============================================================================
Write-Host "Checking PostgreSQL status..." -ForegroundColor Yellow

# Check using pg_ctl status
try {
    $statusOutput = & $PG_CTL status -D $DATA_DIR 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "? PostgreSQL is already running" -ForegroundColor Green
        Write-Host ""
        Write-Host $statusOutput
        Write-Host ""
        Write-Host "Connection Info:" -ForegroundColor Cyan
        Write-Host "  Host: 127.0.0.1" -ForegroundColor White
        Write-Host "  Port: 5432" -ForegroundColor White
        Write-Host "  User: postgres" -ForegroundColor White
        Write-Host "  Database: postgres" -ForegroundColor White
        Write-Host ""
        Write-Host "Connect using:" -ForegroundColor Yellow
        Write-Host "  .\$PG_FOLDER_NAME\bin\psql.exe -h 127.0.0.1 -U postgres" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "To restart:" -ForegroundColor Yellow
        Write-Host "  .\stop_postgresql.ps1" -ForegroundColor Cyan
        Write-Host "  .\start_postgresql.ps1" -ForegroundColor Cyan
        Write-Host ""
        Read-Host "Press Enter to exit"
        exit 0
    }
} catch {
    # Not running, continue with startup
}

# Check port 5432
try {
    $portCheck = Get-NetTCPConnection -LocalPort 5432 -State Listen -ErrorAction SilentlyContinue
    if ($portCheck) {
        Write-Host "??  Port 5432 is in use by another process" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Process using port 5432:" -ForegroundColor Yellow
        $portCheck | Format-Table -Property OwningProcess, State
        
        $processId = $portCheck[0].OwningProcess
        if ($processId) {
            $process = Get-Process -Id $processId -ErrorAction SilentlyContinue
            if ($process) {
                Write-Host "  Process: $($process.ProcessName) (PID: $processId)" -ForegroundColor White
            }
        }
        
        Write-Host ""
        $continue = Read-Host "Continue anyway? This may fail. (y/n)"
        if ($continue -ne "y") {
            exit 0
        }
    }
} catch {
    # Port check failed, but continue
}

# ============================================================================
# Start PostgreSQL
# ============================================================================
Write-Host "Starting PostgreSQL..." -ForegroundColor Yellow
Write-Host "  Version: $PG_VERSION" -ForegroundColor White
Write-Host "  Data Directory: $DATA_DIR" -ForegroundColor White
Write-Host "  Binary Location: $PG_BIN" -ForegroundColor White
Write-Host ""
Write-Host "Note: First startup may take 10-20 seconds" -ForegroundColor Gray
Write-Host ""

try {
    # Create log directory if it doesn't exist
    $logDir = Join-Path $DATA_DIR "log"
    if (-not (Test-Path $logDir)) {
        Write-Host "Creating log directory..." -ForegroundColor Yellow
        New-Item -Path $logDir -ItemType Directory -Force | Out-Null
        Write-Host "  ? Log directory created" -ForegroundColor Green
        Write-Host ""
    }
    
    # Start PostgreSQL using pg_ctl
    $logFile = Join-Path $logDir "postgresql.log"
    
    Write-Host "Running: pg_ctl start -D $DATA_DIR" -ForegroundColor Gray
    Write-Host "  Log file: $logFile" -ForegroundColor Gray
    Write-Host ""
    
    & $PG_CTL start -D $DATA_DIR -l $logFile
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "? Failed to start PostgreSQL (exit code: $LASTEXITCODE)" -ForegroundColor Red
        Write-Host ""
        
        # Try to show log content if it exists
        if (Test-Path $logFile) {
            Write-Host "Last 20 lines of log:" -ForegroundColor Yellow
            Get-Content $logFile -Tail 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
            Write-Host ""
        }
        
        Write-Host "Check logs:" -ForegroundColor Yellow
        Write-Host "  Get-Content `"$logFile`" -Tail 50" -ForegroundColor Cyan
        Write-Host ""
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host ""
    Write-Host "? PostgreSQL start command issued" -ForegroundColor Green
    Write-Host ""
    Write-Host "Waiting for server to start..." -ForegroundColor Yellow
    
    # Wait for startup with timeout
    $maxWait = 30  # 30 seconds
    $waited = 0
    $started = $false
    
    while ($waited -lt $maxWait) {
        Start-Sleep -Seconds 2
        $waited += 2
        
        Write-Host "  . Waiting... ($waited seconds)" -ForegroundColor Gray
        
        # Check if port is listening
        $portCheck = Get-NetTCPConnection -LocalPort 5432 -State Listen -ErrorAction SilentlyContinue
        if ($portCheck) {
            Write-Host "  ? Port 5432 is listening" -ForegroundColor Green
            
            # Try to connect using psql
            $psqlExe = Join-Path $PG_BIN "psql.exe"
            $testQuery = "SELECT version();"
            
            try {
                $result = & $psqlExe -h 127.0.0.1 -U postgres -t -A -c $testQuery 2>&1
                if ($LASTEXITCODE -eq 0) {
                    $started = $true
                    break
                }
            } catch {
                # Still starting up
            }
        }
    }
    
    Write-Host ""
    
    if ($started) {
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "     PostgreSQL Started Successfully!" -ForegroundColor Green
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host ""
        
        # Get version info
        try {
            $psqlExe = Join-Path $PG_BIN "psql.exe"
            $versionQuery = "SELECT version();"
            $version = & $psqlExe -h 127.0.0.1 -U postgres -t -A -c $versionQuery 2>&1
            
            Write-Host "Server Information:" -ForegroundColor Cyan
            Write-Host "  $version" -ForegroundColor White
            Write-Host ""
        } catch {
            # Version retrieval failed, but service is running
        }
        
        Write-Host "Connection Details:" -ForegroundColor Cyan
        Write-Host "  Host: 127.0.0.1 (localhost)" -ForegroundColor White
        Write-Host "  Port: 5432" -ForegroundColor White
        Write-Host "  User: postgres" -ForegroundColor White
        Write-Host "  Password: (none - trust authentication)" -ForegroundColor White
        Write-Host "  Default Database: postgres" -ForegroundColor White
        Write-Host ""
        
        Write-Host "Connect using psql:" -ForegroundColor Yellow
        Write-Host "  .\$PG_FOLDER_NAME\bin\psql.exe -h 127.0.0.1 -U postgres" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "Test in PowerShell:" -ForegroundColor Yellow
        Write-Host "  .\$PG_FOLDER_NAME\bin\psql.exe -h 127.0.0.1 -U postgres -c `"SELECT version();`"" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "Create application database:" -ForegroundColor Yellow
        Write-Host "  .\$PG_FOLDER_NAME\bin\psql.exe -h 127.0.0.1 -U postgres -c `"CREATE DATABASE perception_engine;`"" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "C++ Configuration (config.ini):" -ForegroundColor Yellow
        Write-Host "  [Database]" -ForegroundColor Cyan
        Write-Host "  db_type=postgresql" -ForegroundColor Cyan
        Write-Host "  db_host=127.0.0.1" -ForegroundColor Cyan
        Write-Host "  db_port=5432" -ForegroundColor Cyan
        Write-Host "  db_name=perception_engine" -ForegroundColor Cyan
        Write-Host "  db_user=postgres" -ForegroundColor Cyan
        Write-Host "  db_password=" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "Management Commands:" -ForegroundColor Yellow
        Write-Host "  Stop: .\stop_postgresql.ps1" -ForegroundColor Cyan
        Write-Host "  Restart: .\restart_postgresql.ps1" -ForegroundColor Cyan
        Write-Host "  Status: .\$PG_FOLDER_NAME\bin\pg_ctl.exe status -D pg_data" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "View Logs:" -ForegroundColor Yellow
        Write-Host "  Get-Content $logFile -Tail 50" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "??  IMPORTANT:" -ForegroundColor Yellow
        Write-Host "  PostgreSQL is running as a background process" -ForegroundColor White
        Write-Host "  Use stop_postgresql.ps1 to stop it properly" -ForegroundColor White
        Write-Host ""
        
    } else {
        Write-Host "??  PostgreSQL process started but not responding yet" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "This usually means it's still initializing." -ForegroundColor White
        Write-Host ""
        Write-Host "Please:" -ForegroundColor Yellow
        Write-Host "  1. Wait another 30 seconds" -ForegroundColor White
        Write-Host "  2. Check status: .\$PG_FOLDER_NAME\bin\pg_ctl.exe status -D pg_data" -ForegroundColor Cyan
        Write-Host "  3. Check logs:" -ForegroundColor White
        Write-Host "     Get-Content $logFile -Tail 50" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "If problems persist:" -ForegroundColor Yellow
        Write-Host "  1. Stop: .\stop_postgresql.ps1" -ForegroundColor Cyan
        Write-Host "  2. Check logs for errors" -ForegroundColor White
        Write-Host "  3. Try starting again" -ForegroundColor White
        Write-Host ""
    }
    
} catch {
    Write-Host "? Failed to start PostgreSQL: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Check data directory exists: Test-Path $DATA_DIR" -ForegroundColor Cyan
    Write-Host "  2. Check port 5432: Get-NetTCPConnection -LocalPort 5432" -ForegroundColor Cyan
    Write-Host "  3. View logs: Get-Content $DATA_DIR\log\*.log" -ForegroundColor Cyan
    Write-Host "  4. Re-initialize: .\deploy_postgresql_binary.ps1" -ForegroundColor Cyan
    Write-Host ""
}

Read-Host "Press Enter to exit"
