# Start MCP Inspector for C# Perception Engine Server
# This script starts the MCP Inspector to test and debug the C# MCP Server

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MCP Inspector Launcher" -ForegroundColor Cyan
Write-Host "C# Perception Engine Server" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Check Perception Engine
Write-Host "[1/4] Checking Perception Engine..." -ForegroundColor Yellow
$engineRunning = $false
try {
    # Try to connect with proper error handling
    $response = Invoke-RestMethod -Uri "http://localhost:8777/context" -Method Get -TimeoutSec 5 -ErrorAction Stop
    Write-Host "  ✓ Perception Engine is running" -ForegroundColor Green
    
    # Show some info
    if ($response.activeApp) {
        Write-Host "  Active App: $($response.activeApp)" -ForegroundColor Gray
    }
    if ($null -ne $response.cpuUsage) {
        Write-Host "  CPU Usage: $($response.cpuUsage)%" -ForegroundColor Gray
    }
    $engineRunning = $true
} catch {
    Write-Host "  ⚠ Unable to connect to Perception Engine" -ForegroundColor Yellow
    Write-Host "  URL: http://localhost:8777/context" -ForegroundColor Gray
    
    # Check if port is listening
    $listening = Get-NetTCPConnection -LocalPort 8777 -State Listen -ErrorAction SilentlyContinue
    if ($listening) {
        Write-Host "  ℹ Port 8777 is listening, but HTTP request failed" -ForegroundColor Yellow
        Write-Host "  This is OK - continuing..." -ForegroundColor Green
        $engineRunning = $true
    } else {
        Write-Host "  ✗ Port 8777 is NOT listening" -ForegroundColor Red
        Write-Host ""
        Write-Host "  To start Perception Engine:" -ForegroundColor Yellow
        Write-Host "  cd d:\PerceiptionEngine_Howard\perception_engine\windows_code" -ForegroundColor White
        Write-Host "  .\start_perception_engine.bat" -ForegroundColor White
        Write-Host ""
        $continue = Read-Host "Continue anyway? (y/n)"
        if ($continue -ne "y" -and $continue -ne "Y") {
            exit 1
        }
    }
}

Write-Host ""

# Step 2: Check Node.js
Write-Host "[2/4] Checking Node.js..." -ForegroundColor Yellow
try {
    $nodeVersion = node --version 2>$null
    if ($null -eq $nodeVersion) {
        throw "Node.js not found"
    }
    Write-Host "  ✓ Node.js version: $nodeVersion" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Node.js not found" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please install Node.js:" -ForegroundColor Yellow
    Write-Host "  https://nodejs.org/" -ForegroundColor White
    Write-Host ""
    pause
    exit 1
}

Write-Host ""

# Step 3: Check .NET SDK
Write-Host "[3/4] Checking .NET SDK..." -ForegroundColor Yellow
try {
    $dotnetVersion = dotnet --version 2>$null
    if ($null -eq $dotnetVersion) {
        throw ".NET SDK not found"
    }
    Write-Host "  ✓ .NET SDK version: $dotnetVersion" -ForegroundColor Green
} catch {
    Write-Host "  ✗ .NET SDK not found" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Please install .NET 8.0 SDK:" -ForegroundColor Yellow
    Write-Host "  https://dotnet.microsoft.com/download/dotnet/8.0" -ForegroundColor White
    Write-Host ""
    pause
    exit 1
}

Write-Host ""

# Step 4: Build project
Write-Host "[4/4] Building C# project..." -ForegroundColor Yellow
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

$buildOutput = dotnet build --configuration Release 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ Build failed" -ForegroundColor Red
    Write-Host $buildOutput
    Write-Host ""
    pause
    exit 1
}
Write-Host "  ✓ Build successful" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Starting MCP Inspector..." -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Inspector will open in your browser at:" -ForegroundColor Yellow
Write-Host "  http://localhost:6277 " -ForegroundColor White
Write-Host ""
Write-Host "What to do next:" -ForegroundColor Cyan
Write-Host "  1. Browser will open automatically" -ForegroundColor White
Write-Host "  2. Click on 'perception-engine-context' server" -ForegroundColor White
Write-Host "  3. Click on 'Tools' tab" -ForegroundColor White
Write-Host "  4. Click on 'get_perception_context' tool" -ForegroundColor White
Write-Host "  5. Click 'Call Tool' button" -ForegroundColor White
Write-Host "  6. View the formatted context result" -ForegroundColor White
Write-Host ""
Write-Host "Press Ctrl+C to stop the Inspector" -ForegroundColor Gray
Write-Host ""

# Start Inspector
npx @modelcontextprotocol/inspector dotnet run --configuration Release
