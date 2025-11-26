# Complete Dependency Fix and Setup

param(
    [switch]$ForceReinstall
)

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Complete Dependency Fix" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Find vcpkg
Write-Host "[Step 1] Locating vcpkg..." -ForegroundColor Yellow

$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot -or -not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    $searchPaths = @("C:\vcpkg", "D:\vcpkg", "C:\tools\vcpkg")
    foreach ($path in $searchPaths) {
        if (Test-Path "$path\vcpkg.exe") {
            $vcpkgRoot = $path
            break
        }
    }
}

if (-not $vcpkgRoot) {
    Write-Host "  ? vcpkg not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install vcpkg:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg" -ForegroundColor Gray
    Write-Host "  cd C:\vcpkg" -ForegroundColor Gray
    Write-Host "  .\bootstrap-vcpkg.bat" -ForegroundColor Gray
    exit 1
}

Write-Host "  ? vcpkg found: $vcpkgRoot" -ForegroundColor Green
$env:VCPKG_ROOT = $vcpkgRoot

# Check and update vcpkg
Write-Host ""
Write-Host "[Step 2] Updating vcpkg..." -ForegroundColor Yellow
Push-Location $vcpkgRoot
git pull origin master 2>&1 | Out-Null
.\bootstrap-vcpkg.bat -disableMetrics 2>&1 | Out-Null
Pop-Location
Write-Host "  ? vcpkg updated" -ForegroundColor Green

# Install/reinstall dependencies
Write-Host ""
Write-Host "[Step 3] Installing dependencies..." -ForegroundColor Yellow

$packages = @(
    "curl:x64-windows",
    "nlohmann-json:x64-windows"
)

foreach ($pkg in $packages) {
    Write-Host ""
    Write-Host "  Processing: $pkg" -ForegroundColor Cyan
    
    if ($ForceReinstall) {
        Write-Host "    Removing existing installation..." -ForegroundColor Gray
        & "$vcpkgRoot\vcpkg.exe" remove $pkg --recurse 2>$null
    }
    
    # Check if already installed
    $installed = & "$vcpkgRoot\vcpkg.exe" list | Select-String $pkg
    
    if ($installed -and -not $ForceReinstall) {
        Write-Host "    ? Already installed: $installed" -ForegroundColor Green
    } else {
        Write-Host "    Installing..." -ForegroundColor Gray
        & "$vcpkgRoot\vcpkg.exe" install $pkg
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "    ? Failed to install $pkg" -ForegroundColor Red
            exit 1
        }
        
        Write-Host "    ? Installed successfully" -ForegroundColor Green
    }
}

# Verify installation
Write-Host ""
Write-Host "[Step 4] Verifying installations..." -ForegroundColor Yellow

$allInstalled = $true

foreach ($pkg in $packages) {
    $pkgName = $pkg -replace ":x64-windows", ""
    $installed = & "$vcpkgRoot\vcpkg.exe" list | Select-String $pkg
    
    if ($installed) {
        Write-Host "  ? $pkg" -ForegroundColor Green
        
        # Check for CMake config
        $configPath = Join-Path $vcpkgRoot "installed\x64-windows\share\$pkgName"
        if (Test-Path $configPath) {
            Write-Host "     Config: $configPath" -ForegroundColor Gray
        } else {
            # Try alternative name
            $altName = $pkgName -replace "-", "_"
            $configPath = Join-Path $vcpkgRoot "installed\x64-windows\share\$altName"
            if (Test-Path $configPath) {
                Write-Host "     Config: $configPath" -ForegroundColor Gray
            }
        }
    } else {
        Write-Host "  ? $pkg NOT INSTALLED" -ForegroundColor Red
        $allInstalled = $false
    }
}

if (-not $allInstalled) {
    Write-Host ""
    Write-Host "? Some packages failed to install" -ForegroundColor Red
    exit 1
}

# Integration
Write-Host ""
Write-Host "[Step 5] Running vcpkg integration..." -ForegroundColor Yellow
& "$vcpkgRoot\vcpkg.exe" integrate install
Write-Host "  ? Integration complete" -ForegroundColor Green

# Show installed packages
Write-Host ""
Write-Host "[Step 6] Installed packages summary:" -ForegroundColor Yellow
Write-Host ""
& "$vcpkgRoot\vcpkg.exe" list | Select-String "curl|nlohmann"

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Dependencies Ready!" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "vcpkg root: $vcpkgRoot" -ForegroundColor Cyan
Write-Host "Toolchain: $vcpkgRoot\scripts\buildsystems\vcpkg.cmake" -ForegroundColor Cyan
Write-Host ""
Write-Host "Environment variable set:" -ForegroundColor Yellow
Write-Host "  `$env:VCPKG_ROOT = `"$vcpkgRoot`"" -ForegroundColor Gray
Write-Host ""
Write-Host "Next step:" -ForegroundColor Yellow
Write-Host "  cd D:\PerceiptionEngine_Howard\perception_engine\database_cpp\database_cpp\elasticsearch_client_dll" -ForegroundColor Gray
Write-Host "  .\setup_oneclick.ps1" -ForegroundColor Gray
Write-Host ""

# Offer to run setup
$response = Read-Host "Run setup_oneclick.ps1 now? (y/n)"
if ($response -eq "y") {
    $projectRoot = "D:\PerceiptionEngine_Howard\perception_engine\database_cpp\database_cpp\elasticsearch_client_dll"
    if (Test-Path "$projectRoot\setup_oneclick.ps1") {
        Set-Location $projectRoot
        Write-Host ""
        Write-Host "Running setup_oneclick.ps1..." -ForegroundColor Cyan
        & ".\setup_oneclick.ps1"
    } else {
        Write-Host "setup_oneclick.ps1 not found in $projectRoot" -ForegroundColor Red
    }
}
