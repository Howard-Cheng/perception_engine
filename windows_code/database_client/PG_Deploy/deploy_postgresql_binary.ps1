# ============================================================================
# PostgreSQL 18.1 Binary Deployment Script - Development Version
# Version: 1.0
# Description: Deploy PostgreSQL from binary distribution
# ============================================================================

$ErrorActionPreference = "Continue"
$PG_VERSION = "18.1-2"
$PG_FOLDER_NAME = "postgresql-$PG_VERSION"
$CURRENT_DIR = $PSScriptRoot
$PG_DIR = Join-Path $CURRENT_DIR $PG_FOLDER_NAME
$DATA_DIR = Join-Path $CURRENT_DIR "pg_data"
$PG_BIN = Join-Path $PG_DIR "bin"

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     PostgreSQL $PG_VERSION Binary Deployment" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Check if PostgreSQL binary exists
# ============================================================================
if (-not (Test-Path $PG_DIR)) {
    Write-Host "? PostgreSQL binary folder not found: $PG_DIR" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please ensure you have downloaded the PostgreSQL binary:" -ForegroundColor Yellow
    Write-Host "  Expected folder: $PG_FOLDER_NAME" -ForegroundColor Cyan
    Write-Host "  Current directory: $CURRENT_DIR" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Download from:" -ForegroundColor Yellow
    Write-Host "  https://www.enterprisedb.com/download-postgresql-binaries" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "? Found PostgreSQL binary: $PG_DIR" -ForegroundColor Green
Write-Host ""

# Check essential binaries
$essentialBins = @("initdb.exe", "postgres.exe", "pg_ctl.exe", "psql.exe")
foreach ($bin in $essentialBins) {
    $binPath = Join-Path $PG_BIN $bin
    if (-not (Test-Path $binPath)) {
        Write-Host "? Missing essential binary: $bin" -ForegroundColor Red
        exit 1
    }
}

Write-Host "? All essential binaries found" -ForegroundColor Green
Write-Host ""

# ============================================================================
# Check if PostgreSQL is already running
# ============================================================================
Write-Host "Checking if PostgreSQL is already running..." -ForegroundColor Yellow

try {
    $portCheck = Get-NetTCPConnection -LocalPort 5432 -State Listen -ErrorAction SilentlyContinue
    if ($portCheck) {
        Write-Host "??  Port 5432 is already in use" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Please stop existing PostgreSQL instance first:" -ForegroundColor Yellow
        Write-Host "  .\stop_postgresql.ps1" -ForegroundColor Cyan
        Write-Host "  OR" -ForegroundColor White
        Write-Host "  Stop-Service postgresql-x64-*" -ForegroundColor Cyan
        Write-Host ""
        $continue = Read-Host "Continue anyway? (y/n)"
        if ($continue -ne "y") {
            exit 0
        }
    }
} catch {
    # Port check failed, but continue
}

# ============================================================================
# Initialize Database Cluster
# ============================================================================
if (Test-Path $DATA_DIR) {
    Write-Host "??  Data directory already exists: $DATA_DIR" -ForegroundColor Yellow
    Write-Host ""
    $reinit = Read-Host "Reinitialize? This will DELETE all existing data! (yes/no)"
    if ($reinit -eq "yes") {
        Write-Host "Removing existing data directory..." -ForegroundColor Yellow
        Remove-Item $DATA_DIR -Recurse -Force
        Write-Host "? Old data removed" -ForegroundColor Green
    } else {
        Write-Host "Keeping existing data directory" -ForegroundColor Green
        Write-Host ""
        Write-Host "Skipping initdb step..." -ForegroundColor Yellow
        Write-Host ""
        
        # Check if configuration needs update
        $configFile = Join-Path $DATA_DIR "postgresql.conf"
        if (Test-Path $configFile) {
            Write-Host "Checking configuration..." -ForegroundColor Yellow
            $config = Get-Content $configFile -Raw
            if ($config -notmatch "listen_addresses = '127.0.0.1'") {
                Write-Host "Updating configuration for localhost binding..." -ForegroundColor Yellow
                # Update configuration
                $config = $config -replace "#listen_addresses = 'localhost'", "listen_addresses = '127.0.0.1'"
                $config = $config -replace "listen_addresses = '\*'", "listen_addresses = '127.0.0.1'"
                $config | Set-Content $configFile -Encoding UTF8
                Write-Host "? Configuration updated" -ForegroundColor Green
            } else {
                Write-Host "? Configuration is correct" -ForegroundColor Green
            }
        }
        
        Write-Host ""
        Write-Host "To start PostgreSQL, run:" -ForegroundColor Yellow
        Write-Host "  .\start_postgresql.ps1" -ForegroundColor Cyan
        Write-Host ""
        Read-Host "Press Enter to exit"
        exit 0
    }
}

Write-Host "Initializing database cluster..." -ForegroundColor Yellow
Write-Host "  Data directory: $DATA_DIR" -ForegroundColor White
Write-Host "  Encoding: UTF8" -ForegroundColor White
Write-Host "  Locale: en_US.UTF-8" -ForegroundColor White
Write-Host ""

try {
    $initdbExe = Join-Path $PG_BIN "initdb.exe"
    
    # Run initdb
    $initdbArgs = @(
        "-D", $DATA_DIR,
        "-U", "postgres",
        "-E", "UTF8",
        "--locale=en_US.UTF-8",
        "--auth=trust"
    )
    
    Write-Host "Running: initdb $($initdbArgs -join ' ')" -ForegroundColor Gray
    Write-Host ""
    
    & $initdbExe $initdbArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "? initdb failed with exit code: $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
    
    Write-Host ""
    Write-Host "? Database cluster initialized successfully" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host "? Failed to initialize database: $_" -ForegroundColor Red
    exit 1
}

# ============================================================================
# Configure PostgreSQL for Development
# ============================================================================
Write-Host "Configuring PostgreSQL for development..." -ForegroundColor Yellow

$configFile = Join-Path $DATA_DIR "postgresql.conf"
$hbaFile = Join-Path $DATA_DIR "pg_hba.conf"

# Create log directory
$logDir = Join-Path $DATA_DIR "log"
if (-not (Test-Path $logDir)) {
    Write-Host "  Creating log directory..." -ForegroundColor White
    try {
        New-Item -Path $logDir -ItemType Directory -Force | Out-Null
        Write-Host "    ? Log directory created: $logDir" -ForegroundColor Green
    } catch {
        Write-Host "    ??  Failed to create log directory: $_" -ForegroundColor Yellow
    }
    Write-Host ""
}

# Update postgresql.conf
Write-Host "  Updating postgresql.conf..." -ForegroundColor White

try {
    # Read configuration file line by line
    $configLines = Get-Content $configFile
    $newConfig = @()
    
    foreach ($line in $configLines) {
        $newLine = $line
        
        # Update listen_addresses
        if ($line -match "^#?listen_addresses\s*=") {
            $newLine = "listen_addresses = '127.0.0.1'"
        }
        # Update port
        elseif ($line -match "^#?port\s*=\s*5432") {
            $newLine = "port = 5432"
        }
        # Update max_connections
        elseif ($line -match "^#?max_connections\s*=") {
            $newLine = "max_connections = 200"
        }
        # Update logging_collector
        elseif ($line -match "^#?logging_collector\s*=") {
            $newLine = "logging_collector = on"
        }
        # Update log_directory
        elseif ($line -match "^#?log_directory\s*=") {
            $newLine = "log_directory = 'log'"
        }
        # Update log_filename
        elseif ($line -match "^#?log_filename\s*=") {
            $newLine = "log_filename = 'postgresql-%Y-%m-%d.log'"
        }
        
        $newConfig += $newLine
    }
    
    # Save configuration with UTF-8 NO BOM
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllLines($configFile, $newConfig, $utf8NoBom)
    
    Write-Host "    ? listen_addresses = 127.0.0.1" -ForegroundColor Green
    Write-Host "    ? port = 5432" -ForegroundColor Green
    Write-Host "    ? max_connections = 200" -ForegroundColor Green
    Write-Host "    ? logging_collector = on" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host "    ??  Failed to update postgresql.conf: $_" -ForegroundColor Yellow
    Write-Host "    Will try to continue with default configuration" -ForegroundColor Yellow
}

# Update pg_hba.conf for trust authentication
Write-Host "  Updating pg_hba.conf..." -ForegroundColor White

try {
    $hbaConfig = @"
# TYPE  DATABASE        USER            ADDRESS                 METHOD

# "local" is for Unix domain socket connections only
local   all             all                                     trust

# IPv4 local connections:
host    all             all             127.0.0.1/32            trust

# IPv6 local connections:
host    all             all             ::1/128                 trust

# Allow replication connections from localhost
local   replication     all                                     trust
host    replication     all             127.0.0.1/32            trust
host    replication     all             ::1/128                 trust
"@
    
    # Save with UTF-8 NO BOM
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    $hbaLines = $hbaConfig -split "`r?`n"
    [System.IO.File]::WriteAllLines($hbaFile, $hbaLines, $utf8NoBom)
    
    Write-Host "    ? Trust authentication enabled for localhost" -ForegroundColor Green
    Write-Host ""
    
} catch {
    Write-Host "    ??  Failed to update pg_hba.conf: $_" -ForegroundColor Yellow
}

# ============================================================================
# Deployment Complete
# ============================================================================
Write-Host "================================================================" -ForegroundColor Green
Write-Host "     PostgreSQL Deployment Complete!" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""

Write-Host "Configuration Summary:" -ForegroundColor Cyan
Write-Host "  Version: PostgreSQL $PG_VERSION" -ForegroundColor White
Write-Host "  Binary Location: $PG_DIR" -ForegroundColor White
Write-Host "  Data Directory: $DATA_DIR" -ForegroundColor White
Write-Host "  Listen Address: 127.0.0.1 (localhost only)" -ForegroundColor White
Write-Host "  Port: 5432" -ForegroundColor White
Write-Host "  Authentication: Trust (no password required)" -ForegroundColor White
Write-Host "  Default User: postgres" -ForegroundColor White
Write-Host ""

Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Start PostgreSQL:" -ForegroundColor White
Write-Host "     .\start_postgresql.ps1" -ForegroundColor Cyan
Write-Host ""
Write-Host "  2. Connect using psql:" -ForegroundColor White
Write-Host "     .\$PG_FOLDER_NAME\bin\psql.exe -U postgres" -ForegroundColor Cyan
Write-Host ""
Write-Host "  3. Test connection:" -ForegroundColor White
Write-Host "     .\test_postgresql.ps1" -ForegroundColor Cyan
Write-Host ""

Write-Host "C++ Application Configuration (config.ini):" -ForegroundColor Yellow
Write-Host "  [Database]" -ForegroundColor Cyan
Write-Host "  db_type=postgresql" -ForegroundColor Cyan
Write-Host "  db_host=127.0.0.1" -ForegroundColor Cyan
Write-Host "  db_port=5432" -ForegroundColor Cyan
Write-Host "  db_name=perception_engine" -ForegroundColor Cyan
Write-Host "  db_user=postgres" -ForegroundColor Cyan
Write-Host "  db_password=" -ForegroundColor Cyan
Write-Host ""

Write-Host "??  IMPORTANT NOTES:" -ForegroundColor Yellow
Write-Host "  - This is a DEVELOPMENT configuration" -ForegroundColor White
Write-Host "  - Security is DISABLED (trust authentication)" -ForegroundColor White
Write-Host "  - Binds to localhost ONLY (127.0.0.1)" -ForegroundColor White
Write-Host "  - NOT suitable for production use" -ForegroundColor White
Write-Host ""

Read-Host "Press Enter to exit"
