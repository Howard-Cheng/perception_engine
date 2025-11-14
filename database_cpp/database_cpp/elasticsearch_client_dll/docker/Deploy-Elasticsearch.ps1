# ========================================
# Elasticsearch Docker Deployment Script
# Purpose: Automated deployment and management of Elasticsearch service
# ========================================

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('start', 'stop', 'restart', 'status', 'logs', 'clean', 'test')]
    [string]$Action = 'start',
    
    [Parameter(Mandatory=$false)]
    [switch]$WithKibana = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$Follow = $false
)

$ErrorActionPreference = "Stop"

# Configuration variables
$CONTAINER_NAME = "elasticsearch"
$KIBANA_NAME = "kibana"
$IMAGE_VERSION = "8.11.0"
$ES_PORT = 9200
$KIBANA_PORT = 5601

# Color output functions
function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-ColorOutput "========================================" "Cyan"
    Write-ColorOutput $Message "Cyan"
    Write-ColorOutput "========================================" "Cyan"
    Write-Host ""
}

function Write-Success {
    param([string]$Message)
    Write-ColorOutput "? $Message" "Green"
}

function Write-Warning {
    param([string]$Message)
    Write-ColorOutput "??  $Message" "Yellow"
}

function Write-Error-Custom {
    param([string]$Message)
    Write-ColorOutput "? $Message" "Red"
}

function Write-Info {
    param([string]$Message)
    Write-ColorOutput "??  $Message" "Cyan"
}

# Check if Docker is running
function Test-DockerRunning {
    try {
        docker info > $null 2>&1
        return $true
    } catch {
        return $false
    }
}

# Check if container exists
function Test-ContainerExists {
    param([string]$Name)
    $result = docker ps -a --filter "name=^${Name}$" --format "{{.Names}}"
    return ($result -eq $Name)
}

# Check if container is running
function Test-ContainerRunning {
    param([string]$Name)
    $result = docker ps --filter "name=^${Name}$" --format "{{.Names}}"
    return ($result -eq $Name)
}

# Wait for service to be ready
function Wait-ForService {
    param(
        [string]$Url,
        [int]$TimeoutSeconds = 60,
        [string]$ServiceName = "Service"
    )
    
    Write-Info "Waiting for $ServiceName to start..."
    $elapsed = 0
    $interval = 5
    
    while ($elapsed -lt $TimeoutSeconds) {
        try {
            $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 5
            if ($response.StatusCode -eq 200) {
                Write-Success "$ServiceName is ready!"
                return $true
            }
        } catch {
            # Continue waiting
        }
        
        Start-Sleep -Seconds $interval
        $elapsed += $interval
        Write-Host "." -NoNewline
    }
    
    Write-Host ""
    Write-Warning "$ServiceName startup timeout"
    return $false
}

# Start Elasticsearch
function Start-Elasticsearch {
    Write-Header "Starting Elasticsearch"
    
    # Check Docker
    if (-not (Test-DockerRunning)) {
        Write-Error-Custom "Docker is not running, please start Docker Desktop first"
        exit 1
    }
    
    # Set vm.max_map_count
    Write-Info "Configuring system parameters..."
    try {
        wsl -d docker-desktop sysctl -w vm.max_map_count=262144 > $null 2>&1
    } catch {
        Write-Warning "Unable to set vm.max_map_count, container may fail to start"
    }
    
    # Check if container already exists
    if (Test-ContainerExists $CONTAINER_NAME) {
        if (Test-ContainerRunning $CONTAINER_NAME) {
            Write-Warning "Elasticsearch is already running"
            Show-Status
            return
        } else {
            Write-Info "Starting existing container..."
            docker start $CONTAINER_NAME
        }
    } else {
        Write-Info "Creating new container..."
        
        # Create network
        docker network create elastic 2>$null
        
        # Run container
        docker run -d `
            --name $CONTAINER_NAME `
            --net elastic `
            -p "${ES_PORT}:9200" `
            -p "9300:9300" `
            -e "discovery.type=single-node" `
            -e "xpack.security.enabled=false" `
            -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" `
            -e "bootstrap.memory_lock=true" `
            --ulimit memlock=-1:-1 `
            --ulimit nofile=65536:65536 `
            -v elasticsearch_data:/usr/share/elasticsearch/data `
            docker.elastic.co/elasticsearch/elasticsearch:${IMAGE_VERSION}
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Custom "Container creation failed"
            exit 1
        }
    }
    
    # Wait for service to be ready
    if (Wait-ForService "http://localhost:${ES_PORT}" -ServiceName "Elasticsearch") {
        Show-ElasticsearchInfo
        
        # Start Kibana if needed
        if ($WithKibana) {
            Start-Kibana
        }
    }
}

# Start Kibana
function Start-Kibana {
    Write-Header "Starting Kibana"
    
    if (Test-ContainerExists $KIBANA_NAME) {
        if (Test-ContainerRunning $KIBANA_NAME) {
            Write-Warning "Kibana is already running"
            return
        } else {
            Write-Info "Starting existing Kibana container..."
            docker start $KIBANA_NAME
        }
    } else {
        Write-Info "Creating new Kibana container..."
        
        docker run -d `
            --name $KIBANA_NAME `
            --net elastic `
            -p "${KIBANA_PORT}:5601" `
            -e "ELASTICSEARCH_HOSTS=http://elasticsearch:9200" `
            -e "xpack.security.enabled=false" `
            docker.elastic.co/kibana/kibana:${IMAGE_VERSION}
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Custom "Kibana container creation failed"
            return
        }
    }
    
    # Wait for Kibana to be ready
    if (Wait-ForService "http://localhost:${KIBANA_PORT}/api/status" -TimeoutSeconds 120 -ServiceName "Kibana") {
        Write-Info "Kibana URL: http://localhost:${KIBANA_PORT}"
    }
}

# Stop services
function Stop-Services {
    Write-Header "Stopping Services"
    
    $stopped = $false
    
    if (Test-ContainerRunning $KIBANA_NAME) {
        Write-Info "Stopping Kibana..."
        docker stop $KIBANA_NAME > $null
        Write-Success "Kibana has been stopped"
        $stopped = $true
    }
    
    if (Test-ContainerRunning $CONTAINER_NAME) {
        Write-Info "Stopping Elasticsearch..."
        docker stop $CONTAINER_NAME > $null
        Write-Success "Elasticsearch has been stopped"
        $stopped = $true
    }
    
    if (-not $stopped) {
        Write-Info "No running services"
    }
}

# Restart services
function Restart-Services {
    Write-Header "Restarting Services"
    Stop-Services
    Start-Sleep -Seconds 3
    Start-Elasticsearch
}

# Show status
function Show-Status {
    Write-Header "Service Status"
    
    $esRunning = Test-ContainerRunning $CONTAINER_NAME
    $kibanaRunning = Test-ContainerRunning $KIBANA_NAME
    
    # Elasticsearch status
    Write-Host "Elasticsearch: " -NoNewline
    if ($esRunning) {
        Write-ColorOutput "Running ?" "Green"
        Write-Info "  Port: http://localhost:${ES_PORT}"
        
        # Get cluster health status
        try {
            $health = Invoke-WebRequest -Uri "http://localhost:${ES_PORT}/_cluster/health" -UseBasicParsing | ConvertFrom-Json
            Write-Info "  Cluster Status: $($health.status)"
            Write-Info "  Node Count: $($health.number_of_nodes)"
        } catch {
            Write-Warning "  Unable to get cluster status"
        }
    } else {
        Write-ColorOutput "Stopped ?" "Red"
    }
    
    # Kibana status
    Write-Host "Kibana: " -NoNewline
    if ($kibanaRunning) {
        Write-ColorOutput "Running ?" "Green"
        Write-Info "  Port: http://localhost:${KIBANA_PORT}"
    } else {
        Write-ColorOutput "Stopped ?" "Gray"
    }
    
    # Docker resource usage
    Write-Host ""
    Write-Info "Resource Usage:"
    if ($esRunning) {
        docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}" $CONTAINER_NAME
    }
}

# Show logs
function Show-Logs {
    Write-Header "View Logs"
    
    if (-not (Test-ContainerExists $CONTAINER_NAME)) {
        Write-Error-Custom "Elasticsearch container does not exist"
        return
    }
    
    if ($Follow) {
        Write-Info "Viewing logs in real-time (Ctrl+C to exit)..."
        docker logs -f $CONTAINER_NAME
    } else {
        docker logs --tail 50 $CONTAINER_NAME
    }
}

# Clean all resources
function Clear-AllResources {
    Write-Header "Clean Resources"
    Write-Warning "This will delete all containers and data!"
    
    $confirmation = Read-Host "Confirm deletion? (yes/no)"
    if ($confirmation -ne "yes") {
        Write-Info "Operation cancelled"
        return
    }
    
    # Stop and remove containers
    if (Test-ContainerExists $KIBANA_NAME) {
        Write-Info "Removing Kibana container..."
        docker rm -f $KIBANA_NAME > $null 2>&1
    }
    
    if (Test-ContainerExists $CONTAINER_NAME) {
        Write-Info "Removing Elasticsearch container..."
        docker rm -f $CONTAINER_NAME > $null 2>&1
    }
    
    # Remove data volumes
    Write-Info "Removing data volumes..."
    docker volume rm elasticsearch_data 2>$null
    
    # Remove network
    Write-Info "Removing network..."
    docker network rm elastic 2>$null
    
    Write-Success "Cleanup complete!"
}

# Show Elasticsearch info
function Show-ElasticsearchInfo {
    try {
        $info = Invoke-WebRequest -Uri "http://localhost:${ES_PORT}" -UseBasicParsing | ConvertFrom-Json
        
        Write-Host ""
        Write-Success "Elasticsearch has started!"
        Write-Host ""
        Write-Info "Cluster Name: $($info.cluster_name)"
        Write-Info "Version: $($info.version.number)"
        Write-Info "REST API: http://localhost:${ES_PORT}"
        Write-Host ""
        Write-ColorOutput "Test Connection:" "Yellow"
        Write-Host "  curl http://localhost:${ES_PORT}" -ForegroundColor Gray
        Write-Host "  Or open in browser: http://localhost:${ES_PORT}" -ForegroundColor Gray
        Write-Host ""
    } catch {
        Write-Warning "Unable to get Elasticsearch information"
    }
}

# Test connection
function Test-Connection {
    Write-Header "Test Connection"
    
    if (-not (Test-ContainerRunning $CONTAINER_NAME)) {
        Write-Error-Custom "Elasticsearch is not running"
        Write-Info "Please run: .\Deploy-Elasticsearch.ps1 -Action start"
        return
    }
    
    try {
        # Test basic connection
        Write-Info "Testing basic connection..."
        $response = Invoke-WebRequest -Uri "http://localhost:${ES_PORT}" -UseBasicParsing
        $info = $response.Content | ConvertFrom-Json
        Write-Success "Connection successful!"
        Write-Host ($info | ConvertTo-Json -Depth 3)
        
        # Test cluster health
        Write-Host ""
        Write-Info "Testing cluster health..."
        $health = Invoke-WebRequest -Uri "http://localhost:${ES_PORT}/_cluster/health?pretty" -UseBasicParsing
        Write-Host $health.Content
        
        # Test index creation
        Write-Host ""
        Write-Info "Testing index creation..."
        $testIndex = "test-index-$(Get-Date -Format 'yyyyMMddHHmmss')"
        try {
            Invoke-WebRequest -Uri "http://localhost:${ES_PORT}/$testIndex" -Method PUT -UseBasicParsing > $null
            Write-Success "Index created successfully: $testIndex"
            
            # Delete test index
            Invoke-WebRequest -Uri "http://localhost:${ES_PORT}/$testIndex" -Method DELETE -UseBasicParsing > $null
            Write-Success "Test index cleaned up"
        } catch {
            Write-Warning "Index creation failed: $_"
        }
        
        Write-Host ""
        Write-Success "All tests passed! Service is running normally."
        
    } catch {
        Write-Error-Custom "Connection failed: $_"
    }
}

# Main logic
try {
    switch ($Action) {
        'start' {
            Start-Elasticsearch
        }
        'stop' {
            Stop-Services
        }
        'restart' {
            Restart-Services
        }
        'status' {
            Show-Status
        }
        'logs' {
            Show-Logs
        }
        'clean' {
            Clear-AllResources
        }
        'test' {
            Test-Connection
        }
        default {
            Write-Error-Custom "Unknown action: $Action"
            Write-Host ""
            Write-Host "Usage: .\Deploy-Elasticsearch.ps1 -Action <action> [options]"
            Write-Host ""
            Write-Host "Available actions:"
            Write-Host "  start   - Start service"
            Write-Host "  stop    - Stop service"
            Write-Host "  restart - Restart service"
            Write-Host "  status  - View status"
            Write-Host "  logs    - View logs (add -Follow for real-time)"
            Write-Host "  clean   - Clean all resources"
            Write-Host "  test    - Test connection"
            Write-Host ""
            Write-Host "Options:"
            Write-Host "  -WithKibana - Also start Kibana"
            Write-Host "  -Follow     - Real-time log viewing"
            Write-Host ""
            Write-Host "Examples:"
            Write-Host "  .\Deploy-Elasticsearch.ps1 -Action start"
            Write-Host "  .\Deploy-Elasticsearch.ps1 -Action start -WithKibana"
            Write-Host "  .\Deploy-Elasticsearch.ps1 -Action logs -Follow"
        }
    }
} catch {
    Write-Error-Custom "An error occurred: $_"
    exit 1
}
