# ============================================================================
# Elasticsearch 9.2.1 Stop Script
# Description: Stop Elasticsearch and clean up
# ============================================================================

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Stop Elasticsearch" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

$ES_VERSION = "9.2.1"
$ES_DIR = "elasticsearch-$ES_VERSION"

# Step 1: Find and stop Elasticsearch processes
Write-Host "[Step 1] Finding Elasticsearch processes..." -ForegroundColor Yellow
Write-Host ""

$foundProcesses = $false

# Method 1: Check port 9200
try {
    $port9200 = Get-NetTCPConnection -LocalPort 9200 -ErrorAction SilentlyContinue
    if ($port9200) {
        $foundProcesses = $true
        Write-Host "  ? Port 9200 is in use by PID: $($port9200.OwningProcess)" -ForegroundColor Yellow
        
        $portProcess = Get-Process -Id $port9200.OwningProcess -ErrorAction SilentlyContinue
        if ($portProcess) {
            Write-Host "    Process: $($portProcess.ProcessName)" -ForegroundColor Gray
            
            # Stop the process
            Write-Host "    Stopping process..." -ForegroundColor Yellow
            try {
                Stop-Process -Id $portProcess.Id -Force -ErrorAction Stop
                Write-Host "    ? Stopped" -ForegroundColor Green
            } catch {
                Write-Host "    ? Could not stop: $_" -ForegroundColor Yellow
            }
        }
    }
} catch {
    # Silent
}

# Method 2: By process name (Java processes running elasticsearch)
$javaProcesses = Get-Process -Name "java" -ErrorAction SilentlyContinue | Where-Object {
    $_.WorkingSet64 -gt 100MB  # ES typically uses >100MB
}

if ($javaProcesses) {
    $foundProcesses = $true
    Write-Host "  Found Java processes (potential ES):" -ForegroundColor Yellow
    foreach ($proc in $javaProcesses) {
        Write-Host "    PID: $($proc.Id), Memory: $('{0:N0}' -f ($proc.WorkingSet64/1MB)) MB" -ForegroundColor Gray
        
        $response = Read-Host "    Stop this process? (y/N)"
        if ($response -eq 'y' -or $response -eq 'Y') {
            try {
                Stop-Process -Id $proc.Id -Force -ErrorAction Stop
                Write-Host "    ? Stopped" -ForegroundColor Green
            } catch {
                Write-Host "    ? Could not stop: $_" -ForegroundColor Yellow
            }
        }
    }
}

if (-not $foundProcesses) {
    Write-Host "  ? No Elasticsearch processes found" -ForegroundColor Green
}

Write-Host ""

# Step 2: Clean lock files
Write-Host "[Step 2] Cleaning lock files..." -ForegroundColor Yellow

$lockFile = Join-Path $PSScriptRoot "$ES_DIR\data\node.lock"
$lockFilesFound = $false

if (Test-Path $lockFile) {
    $lockFilesFound = $true
    Write-Host "  Found lock file: node.lock" -ForegroundColor Gray
    try {
        Remove-Item $lockFile -Force -ErrorAction Stop
        Write-Host "  ? Removed" -ForegroundColor Green
    } catch {
        Write-Host "  ? Could not remove: $_" -ForegroundColor Yellow
    }
}

if (-not $lockFilesFound) {
    Write-Host "  ? No lock files found" -ForegroundColor Green
}

Write-Host ""

# Step 3: Verify port is free
Write-Host "[Step 3] Verifying port 9200..." -ForegroundColor Yellow

Start-Sleep -Seconds 2

try {
    $port9200 = Get-NetTCPConnection -LocalPort 9200 -ErrorAction SilentlyContinue
    if ($port9200) {
        Write-Host "  ? Port 9200 is still in use!" -ForegroundColor Yellow
        Write-Host "    PID: $($port9200.OwningProcess), State: $($port9200.State)" -ForegroundColor Red
    } else {
        Write-Host "  ? Port 9200 is free" -ForegroundColor Green
    }
} catch {
    Write-Host "  ? Port 9200 is free" -ForegroundColor Green
}

Write-Host ""

# Summary
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Summary" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

if (-not $foundProcesses) {
    Write-Host "  ? Elasticsearch was not running" -ForegroundColor Green
} else {
    Write-Host "  ? Elasticsearch stopped" -ForegroundColor Green
}

Write-Host ""
Write-Host "To start Elasticsearch again:" -ForegroundColor Yellow
Write-Host "  .\start_elasticsearch_dev.ps1" -ForegroundColor Cyan
Write-Host ""

Read-Host "Press Enter to exit"
