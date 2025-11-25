# ============================================================================
# Elasticsearch 9.2.1 Connection Test
# Description: Verify ES is accessible on localhost
# ============================================================================

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Elasticsearch Connection Test" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Test 1: Port Check
Write-Host "[Test 1] Checking port 9200..." -ForegroundColor Yellow

try {
    $portCheck = Get-NetTCPConnection -LocalPort 9200 -State Listen -ErrorAction SilentlyContinue
    if ($portCheck) {
        Write-Host "  ? Port 9200 is listening" -ForegroundColor Green
        Write-Host "    Local Address: $($portCheck.LocalAddress)" -ForegroundColor White
        Write-Host "    Process ID: $($portCheck.OwningProcess)" -ForegroundColor White
    } else {
        Write-Host "  ? Port 9200 is not listening" -ForegroundColor Red
        Write-Host "    Elasticsearch may not be running" -ForegroundColor Yellow
    }
} catch {
    Write-Host "  ? Port 9200 is not listening" -ForegroundColor Red
}

Write-Host ""

# Test 2: HTTP Connection Test
Write-Host "[Test 2] Testing HTTP connection to http://localhost:9200..." -ForegroundColor Yellow

try {
    $response = Invoke-WebRequest -Uri "http://localhost:9200" -UseBasicParsing -TimeoutSec 5
    Write-Host "  ? SUCCESS! Elasticsearch is responding" -ForegroundColor Green
    Write-Host "    Status Code: $($response.StatusCode)" -ForegroundColor White
    Write-Host ""
    
    # Parse JSON response
    $json = $response.Content | ConvertFrom-Json
    Write-Host "  Elasticsearch Info:" -ForegroundColor Cyan
    Write-Host "    Cluster: $($json.cluster_name)" -ForegroundColor White
    Write-Host "    Node: $($json.name)" -ForegroundColor White
    Write-Host "    Version: $($json.version.number)" -ForegroundColor White
    Write-Host "    Tagline: $($json.tagline)" -ForegroundColor White
    Write-Host ""
    
    $esWorking = $true
    
} catch {
    Write-Host "  ? FAILED! Cannot connect" -ForegroundColor Red
    Write-Host "    Error: $_" -ForegroundColor Yellow
    Write-Host ""
    $esWorking = $false
}

# Test 3: Cluster Health
if ($esWorking) {
    Write-Host "[Test 3] Checking cluster health..." -ForegroundColor Yellow
    
    try {
        $healthResponse = Invoke-WebRequest -Uri "http://localhost:9200/_cluster/health" -UseBasicParsing -TimeoutSec 5
        $health = $healthResponse.Content | ConvertFrom-Json
        
        $healthColor = switch ($health.status) {
            "green" { "Green" }
            "yellow" { "Yellow" }
            "red" { "Red" }
            default { "White" }
        }
        
        Write-Host "  ? Cluster Health: $($health.status.ToUpper())" -ForegroundColor $healthColor
        Write-Host "    Nodes: $($health.number_of_nodes)" -ForegroundColor White
        Write-Host "    Data Nodes: $($health.number_of_data_nodes)" -ForegroundColor White
        Write-Host "    Active Shards: $($health.active_shards)" -ForegroundColor White
        Write-Host ""
        
    } catch {
        Write-Host "  ? Could not get cluster health" -ForegroundColor Yellow
        Write-Host ""
    }
}

# Test 4: Create and Query Test Document
if ($esWorking) {
    Write-Host "[Test 4] Testing document create/search..." -ForegroundColor Yellow
    
    try {
        # Create test document
        $testData = @{
            "timestamp" = (Get-Date).ToString("o")
            "message" = "Test from ES 9.2.1"
            "test_id" = (Get-Random -Maximum 1000)
        } | ConvertTo-Json
        
        $createResponse = Invoke-WebRequest -Method POST `
            -Uri "http://localhost:9200/test_index/_doc" `
            -ContentType "application/json" `
            -Body $testData `
            -UseBasicParsing `
            -TimeoutSec 5
        
        Write-Host "  ? Test document created" -ForegroundColor Green
        
        # Wait for indexing
        Start-Sleep -Seconds 1
        
        # Query the document
        $searchResponse = Invoke-WebRequest -Method POST `
            -Uri "http://localhost:9200/test_index/_search" `
            -ContentType "application/json" `
            -Body '{"query":{"match_all":{}}}' `
            -UseBasicParsing `
            -TimeoutSec 5
        
        $searchResult = $searchResponse.Content | ConvertFrom-Json
        Write-Host "  ? Test query successful: found $($searchResult.hits.total.value) documents" -ForegroundColor Green
        
        # Clean up
        Invoke-WebRequest -Method DELETE `
            -Uri "http://localhost:9200/test_index" `
            -UseBasicParsing `
            -TimeoutSec 5 | Out-Null
        
        Write-Host "  ? Test index cleaned up" -ForegroundColor Green
        Write-Host ""
        
    } catch {
        Write-Host "  ? Create/search test failed: $_" -ForegroundColor Yellow
        Write-Host ""
    }
}

# Summary
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Test Summary" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

if ($esWorking) {
    Write-Host "? ELASTICSEARCH IS WORKING!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Configuration verified:" -ForegroundColor Yellow
    Write-Host "  - Accessible on: http://localhost:9200" -ForegroundColor Cyan
    Write-Host "  - No authentication required" -ForegroundColor White
    Write-Host "  - Ready for development use" -ForegroundColor White
    Write-Host ""
    Write-Host "For your C++ application (config.ini):" -ForegroundColor Yellow
    Write-Host "  elasticsearch_url=http://localhost:9200" -ForegroundColor Cyan
    Write-Host "  elasticsearch_index=perception_context" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Start your PerceptionEngine:" -ForegroundColor Yellow
    Write-Host "  cd D:\PerceiptionEngine_Howard\perception_engine\windows_code\buildnew" -ForegroundColor Cyan
    Write-Host "  .\PerceptionEngine.exe --console" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "? ELASTICSEARCH IS NOT RESPONDING" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Start Elasticsearch: .\start_elasticsearch_dev.ps1" -ForegroundColor Cyan
    Write-Host "  2. Check logs: Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 50" -ForegroundColor Cyan
    Write-Host "  3. Verify Java: java -version (need 17+)" -ForegroundColor Cyan
    Write-Host ""
}

Read-Host "Press Enter to exit"
