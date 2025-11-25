# ============================================================================
# Elasticsearch 9.2.1 Automated Deployment Script - Development Version
# Version: 2.0
# Description: Deploy ES 9.2.1 with localhost binding and security disabled
# ============================================================================

$ErrorActionPreference = "Continue"
$ES_VERSION = "9.2.1"
$ES_FOLDER_NAME = "elasticsearch-$ES_VERSION"
$ES_DOWNLOAD_URL = "https://artifacts.elastic.co/downloads/elasticsearch/$ES_FOLDER_NAME-windows-x86_64.zip"
$JAVA_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java17"
$CURRENT_DIR = $PSScriptRoot

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "     Elasticsearch $ES_VERSION Deployment (Development Mode)" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  - Version: $ES_VERSION" -ForegroundColor White
Write-Host "  - Binding: localhost only (127.0.0.1)" -ForegroundColor White
Write-Host "  - Protocol: HTTP (no SSL)" -ForegroundColor White
Write-Host "  - Security: Disabled" -ForegroundColor White
Write-Host "  - Mode: Development/Testing" -ForegroundColor White
Write-Host ""

# ============================================================================
# Step 1: Detect Java Environment
# ============================================================================
Write-Host "[1/5] Detecting Java environment..." -ForegroundColor Yellow

$javaFound = $false
$javaVersion = $null
$javaMajorVersion = 0

# Method 1: Try PATH
try {
    $output = cmd /c "java -version 2>&1"
    if ($output) {
        $versionLine = $output | Select-Object -First 1
        if ($versionLine -match '(\d+)\.(\d+)\.(\d+)') {
            $javaMajorVersion = [int]$Matches[1]
            if ($javaMajorVersion -eq 1) {
                $javaMajorVersion = [int]$Matches[2]
            }
            $javaVersion = "$javaMajorVersion.$($Matches[2]).$($Matches[3])"
            $javaFound = $true
        } elseif ($versionLine -match '(\d+)\.(\d+)') {
            $javaMajorVersion = [int]$Matches[1]
            $javaVersion = "$javaMajorVersion.$($Matches[2])"
            $javaFound = $true
        }
    }
} catch {
    # Continue to next method
}

# Method 2: Check JAVA_HOME
if (-not $javaFound -and $env:JAVA_HOME) {
    $javaExe = Join-Path $env:JAVA_HOME "bin\java.exe"
    if (Test-Path $javaExe) {
        try {
            $output = cmd /c "`"$javaExe`" -version 2>&1"
            if ($output) {
                $versionLine = $output | Select-Object -First 1
                if ($versionLine -match '(\d+)\.(\d+)') {
                    $javaMajorVersion = [int]$Matches[1]
                    $javaVersion = "$javaMajorVersion.$($Matches[2])"
                    $javaFound = $true
                }
            }
        } catch {
            # Continue
        }
    }
}

# Evaluate Java version
if ($javaFound) {
    if ($javaMajorVersion -ge 17) {
        Write-Host "  ? Java $javaMajorVersion detected (version $javaVersion)" -ForegroundColor Green
    } else {
        Write-Host "  ? Java version too old: $javaMajorVersion (requires Java 17+)" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please install Java 17 or higher from:" -ForegroundColor Yellow
        Write-Host $JAVA_DOWNLOAD_URL -ForegroundColor Cyan
        Read-Host "Press Enter to exit"
        exit 1
    }
} else {
    Write-Host "  ? Java not detected" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Java 17+ from: $JAVA_DOWNLOAD_URL" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""

# ============================================================================
# Step 2: Check Elasticsearch Directory
# ============================================================================
Write-Host "[2/5] Checking Elasticsearch installation directory..." -ForegroundColor Yellow

$ES_DIR = Join-Path $CURRENT_DIR $ES_FOLDER_NAME

if (Test-Path $ES_DIR) {
    Write-Host "  ? Existing installation detected: $ES_DIR" -ForegroundColor Yellow
    Write-Host ""
    $response = Read-Host "Remove and re-download? (y/N)"
    if ($response -eq 'y' -or $response -eq 'Y') {
        Write-Host "  Removing existing installation..." -ForegroundColor Yellow
        Remove-Item -Path $ES_DIR -Recurse -Force
        Write-Host "  ? Removed" -ForegroundColor Green
    } else {
        Write-Host "  Using existing installation" -ForegroundColor Yellow
        Write-Host ""
        # Skip to configuration
        $skipDownload = $true
    }
}

Write-Host ""

# ============================================================================
# Step 3: Download Elasticsearch
# ============================================================================
if (-not $skipDownload) {
    Write-Host "[3/5] Downloading Elasticsearch $ES_VERSION..." -ForegroundColor Yellow
    Write-Host "  URL: $ES_DOWNLOAD_URL" -ForegroundColor Gray

    $ZIP_FILE = Join-Path $CURRENT_DIR "$ES_FOLDER_NAME.zip"

    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $ES_DOWNLOAD_URL -OutFile $ZIP_FILE -UseBasicParsing
        $ProgressPreference = 'Continue'
        
        Write-Host "  ? Download complete: $('{0:N2}' -f ((Get-Item $ZIP_FILE).Length / 1MB)) MB" -ForegroundColor Green
    } catch {
        Write-Host "  ? Download failed: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please download manually from:" -ForegroundColor Yellow
        Write-Host "  $ES_DOWNLOAD_URL" -ForegroundColor Cyan
        Read-Host "Press Enter to exit"
        exit 1
    }

    Write-Host ""

    # ============================================================================
    # Step 4: Extract Elasticsearch
    # ============================================================================
    Write-Host "[4/5] Extracting Elasticsearch..." -ForegroundColor Yellow

    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($ZIP_FILE, $CURRENT_DIR)
        
        Write-Host "  ? Extraction complete" -ForegroundColor Green
        
        Remove-Item -Path $ZIP_FILE -Force
        Write-Host "  ? Cleaned up temporary files" -ForegroundColor Green
        
    } catch {
        Write-Host "  ? Extraction failed: $_" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }

    Write-Host ""
} else {
    Write-Host "[3/5] Download skipped (using existing files)" -ForegroundColor Yellow
    Write-Host "[4/5] Extraction skipped (using existing files)" -ForegroundColor Yellow
    Write-Host ""
}

# ============================================================================
# Step 5: Configure Elasticsearch for Development
# ============================================================================
Write-Host "[5/5] Configuring Elasticsearch for development..." -ForegroundColor Yellow

$CONFIG_FILE = Join-Path $ES_DIR "config\elasticsearch.yml"

if (-not (Test-Path $CONFIG_FILE)) {
    Write-Host "  ? Configuration file not found: $CONFIG_FILE" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Backup original config
$BACKUP_FILE = $CONFIG_FILE + ".original"
if (-not (Test-Path $BACKUP_FILE)) {
    Copy-Item $CONFIG_FILE $BACKUP_FILE
    Write-Host "  ? Backup created: elasticsearch.yml.original" -ForegroundColor Green
}

# Create development configuration
$devConfig = @"
# ============================================================================
# Elasticsearch $ES_VERSION - Development Configuration
# Auto-generated by deploy_elasticsearch_dev.ps1
# ============================================================================

# Cluster settings
cluster.name: elasticsearch-dev
node.name: node-1

# Network settings - LOCALHOST ONLY
network.host: 127.0.0.1
http.port: 9200

# Discovery settings (single node)
discovery.type: single-node

# Security - DISABLED for development
xpack.security.enabled: false
xpack.security.enrollment.enabled: false
xpack.security.http.ssl.enabled: false
xpack.security.transport.ssl.enabled: false

# Performance settings
bootstrap.memory_lock: false

# Logging
logger.level: info

"@

try {
    Set-Content -Path $CONFIG_FILE -Value $devConfig -Force
    Write-Host "  ? Development configuration applied" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Configuration details:" -ForegroundColor Cyan
    Write-Host "    - Cluster: elasticsearch-dev" -ForegroundColor White
    Write-Host "    - Node: node-1" -ForegroundColor White
    Write-Host "    - Network: 127.0.0.1 (localhost only)" -ForegroundColor White
    Write-Host "    - Port: 9200" -ForegroundColor White
    Write-Host "    - Security: Disabled" -ForegroundColor White
    Write-Host "    - Protocol: HTTP" -ForegroundColor White
    Write-Host ""
} catch {
    Write-Host "  ? Failed to write configuration: $_" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""

# ============================================================================
# Deployment Complete
# ============================================================================
Write-Host "================================================================" -ForegroundColor Green
Write-Host "     Deployment Complete!" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Installation:" -ForegroundColor Yellow
Write-Host "  Location: $ES_DIR" -ForegroundColor Cyan
Write-Host "  Version: $ES_VERSION" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  URL: http://localhost:9200" -ForegroundColor Cyan
Write-Host "  Binding: 127.0.0.1 (localhost only)" -ForegroundColor Cyan
Write-Host "  Authentication: None (security disabled)" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Start Elasticsearch:" -ForegroundColor White
Write-Host "     .\start_elasticsearch.ps1" -ForegroundColor Cyan
Write-Host ""
Write-Host "  2. Test connection:" -ForegroundColor White
Write-Host "     Invoke-WebRequest http://localhost:9200" -ForegroundColor Cyan
Write-Host ""
Write-Host "  3. Update your config.ini:" -ForegroundColor White
Write-Host "     elasticsearch_url=http://localhost:9200" -ForegroundColor Cyan
Write-Host ""
Write-Host "WARNING: This is a development configuration!" -ForegroundColor Yellow
Write-Host "  - Security is disabled" -ForegroundColor Red
Write-Host "  - Only accessible from localhost" -ForegroundColor White
Write-Host "  - Do NOT use in production" -ForegroundColor Red
Write-Host ""

Read-Host "Press Enter to exit"
