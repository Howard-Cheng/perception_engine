# ============================================================================
# Elasticsearch 9.2.1 Startup Script - Development Version
# Version: 2.0
# Description: Start Elasticsearch with proper detection
# ============================================================================

$ErrorActionPreference = "Continue"
$ES_VERSION = "9.2.1"
$ES_FOLDER_NAME = "elasticsearch-$ES_VERSION"
$CURRENT_DIR = $PSScriptRoot
$ES_DIR = Join-Path $CURRENT_DIR $ES_FOLDER_NAME
$ES_EXEC = Join-Path $ES_DIR "bin\elasticsearch.bat"

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Elasticsearch $ES_VERSION Startup" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Check if Elasticsearch is installed
# ============================================================================
if (-not (Test-Path $ES_DIR)) {
    Write-Host "? Elasticsearch not found: $ES_DIR" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please run deployment first:" -ForegroundColor Yellow
    Write-Host "  .\deploy_elasticsearch_dev.ps1" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if (-not (Test-Path $ES_EXEC)) {
    Write-Host "? Elasticsearch executable not found: $ES_EXEC" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please re-run deployment:" -ForegroundColor Yellow
    Write-Host "  .\deploy_elasticsearch_dev.ps1" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# ============================================================================
# Check if already running
# ============================================================================
Write-Host "Checking Elasticsearch status..." -ForegroundColor Yellow

# Check port 9200
try {
    $portCheck = Get-NetTCPConnection -LocalPort 9200 -State Listen -ErrorAction SilentlyContinue
    if ($portCheck) {
        Write-Host "? Elasticsearch is already running on port 9200" -ForegroundColor Green
        Write-Host ""
        
        # Test HTTP connection
        try {
            $response = Invoke-WebRequest -Uri "http://localhost:9200" -UseBasicParsing -TimeoutSec 3
            $json = $response.Content | ConvertFrom-Json
            Write-Host "Cluster Info:" -ForegroundColor Cyan
            Write-Host "  Name: $($json.cluster_name)" -ForegroundColor White
            Write-Host "  Version: $($json.version.number)" -ForegroundColor White
            Write-Host "  Node: $($json.name)" -ForegroundColor White
            Write-Host ""
            Write-Host "Access URL: http://localhost:9200" -ForegroundColor Cyan
            Write-Host ""
        } catch {
            Write-Host "? Port 9200 is in use but not responding to HTTP" -ForegroundColor Yellow
        }
        
        Write-Host "To restart:" -ForegroundColor Yellow
        Write-Host "  .\stop_elasticsearch.ps1" -ForegroundColor Cyan
        Write-Host "  .\start_elasticsearch_dev.ps1" -ForegroundColor Cyan
        Write-Host ""
        Read-Host "Press Enter to exit"
        exit 0
    }
} catch {
    # Port not in use, continue with startup
}

# ============================================================================
# Start Elasticsearch
# ============================================================================
Write-Host "Starting Elasticsearch..." -ForegroundColor Yellow
Write-Host "  Version: $ES_VERSION" -ForegroundColor White
Write-Host "  Location: $ES_DIR" -ForegroundColor White
Write-Host ""
Write-Host "Note: First startup may take 60-90 seconds" -ForegroundColor Gray
Write-Host "Keep this window open to see the startup process" -ForegroundColor Gray
Write-Host ""

try {
    # Start ES in a new window
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $psi.Arguments = "/k `"cd /d `"$ES_DIR\bin`" && elasticsearch.bat`""
    $psi.UseShellExecute = $true
    $psi.CreateNoWindow = $false
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Normal
    
    $process = [System.Diagnostics.Process]::Start($psi)
    
    Write-Host "? Elasticsearch startup initiated (PID: $($process.Id))" -ForegroundColor Green
    Write-Host ""
    Write-Host "Waiting for service to start..." -ForegroundColor Yellow
    
    # Wait for startup with progress
    $maxWait = 90  # 90 seconds
    $waited = 0
    $started = $false
    
    while ($waited -lt $maxWait) {
        Start-Sleep -Seconds 3
        $waited += 3
        
        Write-Host "  . Waiting... ($waited seconds)" -ForegroundColor Gray
        
        # Check if port is listening
        $portCheck = Get-NetTCPConnection -LocalPort 9200 -State Listen -ErrorAction SilentlyContinue
        if ($portCheck) {
            Write-Host "  ? Port 9200 is listening" -ForegroundColor Green
            
            # Try HTTP connection
            try {
                $response = Invoke-WebRequest -Uri "http://localhost:9200" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop
                $started = $true
                break
            } catch {
                # Still starting up
            }
        }
    }
    
    Write-Host ""
    
    if ($started) {
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "     Elasticsearch Started Successfully!" -ForegroundColor Green
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host ""
        
        # Get cluster info
        try {
            $response = Invoke-WebRequest -Uri "http://localhost:9200" -UseBasicParsing
            $json = $response.Content | ConvertFrom-Json
            
            Write-Host "Cluster Information:" -ForegroundColor Cyan
            Write-Host "  Cluster Name: $($json.cluster_name)" -ForegroundColor White
            Write-Host "  Node Name: $($json.name)" -ForegroundColor White
            Write-Host "  Version: $($json.version.number)" -ForegroundColor White
            Write-Host "  Tagline: $($json.tagline)" -ForegroundColor White
            Write-Host ""
        } catch {
            # Info retrieval failed, but service is running
        }
        
        Write-Host "Access URL:" -ForegroundColor Yellow
        Write-Host "  http://localhost:9200" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "API Endpoints:" -ForegroundColor Yellow
        Write-Host "  Cluster Health: http://localhost:9200/_cluster/health" -ForegroundColor Cyan
        Write-Host "  Cluster Stats: http://localhost:9200/_cluster/stats" -ForegroundColor Cyan
        Write-Host "  List Indices: http://localhost:9200/_cat/indices?v" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Test in PowerShell:" -ForegroundColor Yellow
        Write-Host "  Invoke-WebRequest http://localhost:9200" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "C++ Configuration (config.ini):" -ForegroundColor Yellow
        Write-Host "  elasticsearch_url=http://localhost:9200" -ForegroundColor Cyan
        Write-Host "  elasticsearch_index=perception_context" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "To stop Elasticsearch:" -ForegroundColor Yellow
        Write-Host "  .\stop_elasticsearch.ps1" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "IMPORTANT: Keep the Elasticsearch window open!" -ForegroundColor Yellow
        Write-Host "Closing it will stop the service." -ForegroundColor Yellow
        Write-Host ""
        
    } else {
        Write-Host "? Elasticsearch process started but not responding yet" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "This usually means it's still initializing." -ForegroundColor White
        Write-Host ""
        Write-Host "Please:" -ForegroundColor Yellow
        Write-Host "  1. Wait another 60 seconds" -ForegroundColor White
        Write-Host "  2. Check the Elasticsearch console window for status" -ForegroundColor White
        Write-Host "  3. Look for 'started' message in the logs" -ForegroundColor White
        Write-Host "  4. Try: Invoke-WebRequest http://localhost:9200" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Check logs:" -ForegroundColor Yellow
        Write-Host "  Get-Content $ES_DIR\logs\elasticsearch.log -Tail 50" -ForegroundColor Cyan
        Write-Host ""
    }
    
} catch {
    Write-Host "? Failed to start Elasticsearch: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Check Java version: java -version (need 17+)" -ForegroundColor White
    Write-Host "  2. Check port 9200: Get-NetTCPConnection -LocalPort 9200" -ForegroundColor White
    Write-Host "  3. View logs: Get-Content $ES_DIR\logs\*.log" -ForegroundColor White
    Write-Host ""
}

Read-Host "Press Enter to exit"
