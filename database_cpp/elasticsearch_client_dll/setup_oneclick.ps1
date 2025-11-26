# One-Click Setup for Elasticsearch Client DLL
# Automatically handles vcpkg, dependencies, and VS solution generation

param(
    [string]$VcpkgPath = "",
    [switch]$SkipBuild,
    [switch]$OpenVS
)

$ErrorActionPreference = "Stop"

function Write-Title {
    param([string]$Text)
    Write-Host ""
    Write-Host "=============================================" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "=============================================" -ForegroundColor Cyan
}

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "[$((Get-Date).ToString('HH:mm:ss'))] $Text" -ForegroundColor Yellow
}

function Write-OK {
    param([string]$Text)
    Write-Host "  ? $Text" -ForegroundColor Green
}

function Write-Fail {
    param([string]$Text)
    Write-Host "  ? $Text" -ForegroundColor Red
}

function Write-Warn {
    param([string]$Text)
    Write-Host "  ??  $Text" -ForegroundColor Yellow
}

Write-Title "Elasticsearch Client DLL - One-Click Setup"

# Define paths
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = $scriptDir
$buildDir = Join-Path $projectRoot "build"

Write-Host "Project: $projectRoot" -ForegroundColor Cyan
Write-Host "Build: $buildDir" -ForegroundColor Cyan

# Step 1: Find vcpkg
Write-Step "Step 1/6: Locating vcpkg"

$vcpkgRoot = $null
if ($VcpkgPath) {
    if (Test-Path "$VcpkgPath\vcpkg.exe") {
        $vcpkgRoot = $VcpkgPath
        Write-OK "Using specified path: $vcpkgRoot"
    } else {
        Write-Fail "Invalid vcpkg path: $VcpkgPath"
        exit 1
    }
} else {
    # Check environment
    if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
        $vcpkgRoot = $env:VCPKG_ROOT
        Write-OK "Using VCPKG_ROOT: $vcpkgRoot"
    } else {
        # Search common locations
        $searchPaths = @("C:\vcpkg", "D:\vcpkg", "C:\tools\vcpkg")
        foreach ($path in $searchPaths) {
            if (Test-Path "$path\vcpkg.exe") {
                $vcpkgRoot = $path
                Write-OK "Found at: $vcpkgRoot"
                break
            }
        }
    }
}

if (-not $vcpkgRoot) {
    Write-Fail "vcpkg not found!"
    Write-Host ""
    Write-Host "Please install vcpkg:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg" -ForegroundColor Gray
    Write-Host "  cd C:\vcpkg" -ForegroundColor Gray
    Write-Host "  .\bootstrap-vcpkg.bat" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Or specify path: -VcpkgPath 'C:\path\to\vcpkg'" -ForegroundColor Gray
    exit 1
}

$env:VCPKG_ROOT = $vcpkgRoot

# Step 2: Check dependencies
Write-Step "Step 2/6: Checking dependencies"

$packages = & "$vcpkgRoot\vcpkg.exe" list 2>$null
$curlOk = $packages | Select-String "curl:x64-windows"
$jsonOk = $packages | Select-String "nlohmann-json:x64-windows"

$needInstall = @()
if (-not $curlOk) {
    Write-Warn "CURL not installed"
    $needInstall += "curl:x64-windows"
}
if (-not $jsonOk) {
    Write-Warn "nlohmann-json not installed"
    $needInstall += "nlohmann-json:x64-windows"
}

if ($needInstall.Count -gt 0) {
    Write-Host "  Installing: $($needInstall -join ', ')" -ForegroundColor Cyan
    foreach ($pkg in $needInstall) {
        & "$vcpkgRoot\vcpkg.exe" install $pkg
        if ($LASTEXITCODE -ne 0) {
            Write-Fail "Failed to install $pkg"
            exit 1
        }
    }
    Write-OK "Dependencies installed"
} else {
    Write-OK "All dependencies present"
}

# Step 3: Prepare build directory
Write-Step "Step 3/6: Preparing build directory"

if (Test-Path $buildDir) {
    Write-Host "  Cleaning..." -ForegroundColor Gray
    Remove-Item "$buildDir\CMakeCache.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item "$buildDir\CMakeFiles" -Recurse -Force -ErrorAction SilentlyContinue
    Write-OK "Cleaned existing build"
} else {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    Write-OK "Created build directory"
}

# Step 4: Generate Visual Studio solution
Write-Step "Step 4/6: Generating Visual Studio solution"

Set-Location $buildDir

$toolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

Write-Host "  Generator: Visual Studio 17 2022" -ForegroundColor Gray
Write-Host "  Platform: x64" -ForegroundColor Gray
Write-Host "  Toolchain: $toolchain" -ForegroundColor Gray

# Use array to ensure proper argument handling
$cmakeArgs = @(
    ".."
    "-G"
    "Visual Studio 17 2022"
    "-A"
    "x64"
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    "-DBUILD_SHARED_LIBS=ON"
    "-DBUILD_TESTS=ON"
)

# Execute CMake
& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Fail "CMake generation failed"
    Write-Host ""
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "  - Visual Studio 2022 not installed" -ForegroundColor Gray
    Write-Host "  - Missing C++ development tools" -ForegroundColor Gray
    Write-Host "  - CMake version too old (need 3.15+)" -ForegroundColor Gray
    exit 1
}

Write-OK "Visual Studio solution generated"

# Find solution file
$slnFile = Get-ChildItem -Filter "*.sln" | Select-Object -First 1
if (-not $slnFile) {
    Write-Fail "Solution file not found"
    exit 1
}

Write-OK "Solution: $($slnFile.Name)"

# Step 5: Build (optional)
if (-not $SkipBuild) {
    Write-Step "Step 5/6: Building Release configuration"
    
    & cmake --build . --config Release
    
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Build failed"
        Write-Host ""
        Write-Host "You can still open the solution in Visual Studio:" -ForegroundColor Yellow
        Write-Host "  start $($slnFile.FullName)" -ForegroundColor Gray
        exit 1
    }
    
    Write-OK "Build complete"
    
    # Check outputs
    $dllPath = "bin\Release\elasticsearch_client.dll"
    if (Test-Path $dllPath) {
        $size = (Get-Item $dllPath).Length / 1KB
        Write-OK "DLL generated: $([math]::Round($size, 2)) KB"
    }
} else {
    Write-Step "Step 5/6: Skipping build (use -SkipBuild:$false to build)"
}

# Step 6: Summary
Write-Step "Step 6/6: Setup complete"

Write-Title "Success!"

Write-Host "Generated files:" -ForegroundColor Cyan
Write-Host "  Solution: $($slnFile.FullName)" -ForegroundColor White
Write-Host ""

if (-not $SkipBuild) {
    Write-Host "Built outputs:" -ForegroundColor Cyan
    Write-Host "  DLL: $buildDir\bin\Release\elasticsearch_client.dll" -ForegroundColor White
    Write-Host "  LIB: $buildDir\lib\Release\elasticsearch_client.lib" -ForegroundColor White
    Write-Host ""
}

Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Open in Visual Studio:" -ForegroundColor White
Write-Host "     start `"$($slnFile.FullName)`"" -ForegroundColor Gray
Write-Host ""
Write-Host "  2. Build from command line:" -ForegroundColor White
Write-Host "     cmake --build . --config Release" -ForegroundColor Gray
Write-Host ""
Write-Host "  3. Run tests:" -ForegroundColor White
Write-Host "     bin\Release\es_client_test.exe" -ForegroundColor Gray
Write-Host ""

# Open in VS option
if ($OpenVS) {
    Write-Host "Opening Visual Studio..." -ForegroundColor Cyan
    Start-Process $slnFile.FullName
} else {
    $response = Read-Host "Open in Visual Studio now? (y/n)"
    if ($response -eq "y") {
        Start-Process $slnFile.FullName
    }
}

Write-Host ""
Write-Host "? All done!" -ForegroundColor Green
Write-Host ""
