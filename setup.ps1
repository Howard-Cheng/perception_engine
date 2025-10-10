# ============================================================================
# Nova Perception Engine - Automated Setup Script
# ============================================================================
# This script downloads all missing dependencies and builds the project
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File setup.ps1
#
# What it does:
#   1. Downloads Whisper models (~43MB)
#   2. Downloads Silero VAD model (~1.8MB)
#   3. Downloads OpenCV (~120MB)
#   4. Downloads ONNX Runtime (~15MB)
#   5. Initializes whisper.cpp git submodule
#   6. Builds whisper.cpp
#   7. Installs Python dependencies
#   8. Builds PerceptionEngine
#   9. Copies required files
#   10. Verifies all components
# ============================================================================

param(
    [switch]$SkipPython = $false,
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"  # Faster downloads

# Enable TLS 1.2 for downloads (required for HTTPS)
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Colors for output
function Write-Success { param($Message) Write-Host "✅ $Message" -ForegroundColor Green }
function Write-Info { param($Message) Write-Host "ℹ️  $Message" -ForegroundColor Cyan }
function Write-Warning { param($Message) Write-Host "⚠️  $Message" -ForegroundColor Yellow }
function Write-Error-Custom { param($Message) Write-Host "❌ $Message" -ForegroundColor Red }
function Write-Step { param($Message) Write-Host "`n━━━ $Message ━━━" -ForegroundColor Magenta }

# ============================================================================
# Step 0: Check Prerequisites
# ============================================================================

Write-Step "Checking Prerequisites"

# Check if running from correct directory
if (-not (Test-Path "windows_code\CMakeLists.txt")) {
    Write-Error-Custom "Please run this script from the root of the PE repository"
    Write-Info "Current directory: $(Get-Location)"
    exit 1
}

# Check CMake
try {
    $cmakeVersion = & cmake --version 2>&1 | Select-Object -First 1
    Write-Success "CMake found: $cmakeVersion"
} catch {
    Write-Error-Custom "CMake not found! Please install CMake 3.20+ from https://cmake.org/download/"
    exit 1
}

# Check Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    if ($vsPath) {
        Write-Success "Visual Studio found: $vsPath"
    } else {
        Write-Error-Custom "Visual Studio 2019/2022 not found! Please install Visual Studio with C++ workload"
        exit 1
    }
} else {
    Write-Warning "Could not verify Visual Studio installation"
}

# Check Python
if (-not $SkipPython) {
    try {
        $pythonVersion = & python --version 2>&1
        Write-Success "Python found: $pythonVersion"
    } catch {
        Write-Error-Custom "Python not found! Please install Python 3.8+ from https://python.org"
        exit 1
    }
}

# ============================================================================
# Step 1: Download Whisper Model
# ============================================================================

Write-Step "Downloading Whisper Model"

$whisperModelDir = "models\whisper"
$whisperModelPath = "$whisperModelDir\ggml-tiny.en-q8_0.bin"

if (Test-Path $whisperModelPath) {
    Write-Success "Whisper model already exists: $whisperModelPath"
} else {
    Write-Info "Downloading Whisper tiny.en Q8_0 model (~43MB)..."
    New-Item -ItemType Directory -Force -Path $whisperModelDir | Out-Null

    $whisperUrl = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en-q8_0.bin"

    try {
        Invoke-WebRequest -Uri $whisperUrl -OutFile $whisperModelPath -UseBasicParsing
        Write-Success "Downloaded Whisper model: $whisperModelPath"
    } catch {
        Write-Error-Custom "Failed to download Whisper model: $_"
        Write-Info "Please download manually from: $whisperUrl"
        exit 1
    }
}

# ============================================================================
# Step 2: Download Silero VAD Model
# ============================================================================

Write-Step "Downloading Silero VAD Model"

$vadModelDir = "models\vad"
$vadModelPath = "$vadModelDir\silero_vad.onnx"

if (Test-Path $vadModelPath) {
    Write-Success "Silero VAD model already exists: $vadModelPath"
} else {
    Write-Info "Downloading Silero VAD model (~1.8MB)..."
    New-Item -ItemType Directory -Force -Path $vadModelDir | Out-Null

    $vadUrl = "https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx"

    try {
        Invoke-WebRequest -Uri $vadUrl -OutFile $vadModelPath -UseBasicParsing
        Write-Success "Downloaded Silero VAD model: $vadModelPath"
    } catch {
        Write-Error-Custom "Failed to download Silero VAD model: $_"
        Write-Info "Please download manually from: $vadUrl"
        exit 1
    }
}

# ============================================================================
# Step 3: Download OpenCV
# ============================================================================

Write-Step "Downloading OpenCV"

$opencvDir = "windows_code\third-party\opencv"
$opencvDll = "$opencvDir\opencv\build\x64\vc16\bin\opencv_world4100.dll"

if (Test-Path $opencvDll) {
    Write-Success "OpenCV already exists"
} else {
    Write-Info "Downloading OpenCV 4.10.0 (~120MB, this may take a few minutes)..."
    New-Item -ItemType Directory -Force -Path $opencvDir | Out-Null

    $opencvUrl = "https://github.com/opencv/opencv/releases/download/4.10.0/opencv-4.10.0-windows.exe"
    $opencvExe = "$opencvDir\opencv-4.10.0-windows.exe"

    try {
        # Download OpenCV self-extracting archive
        Invoke-WebRequest -Uri $opencvUrl -OutFile $opencvExe -UseBasicParsing
        Write-Success "Downloaded OpenCV installer"

        # Extract OpenCV (it's a self-extracting 7z archive)
        Write-Info "Extracting OpenCV (this may take 2-3 minutes)..."

        # OpenCV .exe is a self-extracting archive, extract to temp then move
        $tempExtract = "$opencvDir\temp_extract"
        Start-Process -FilePath $opencvExe -ArgumentList "-o$tempExtract", "-y" -Wait -NoNewWindow

        # Move extracted content to correct location
        if (Test-Path "$tempExtract\opencv") {
            Move-Item -Path "$tempExtract\opencv" -Destination $opencvDir -Force
            Remove-Item $tempExtract -Recurse -Force -ErrorAction SilentlyContinue
        }

        # Clean up installer
        Remove-Item $opencvExe -Force

        # Verify extraction
        if (Test-Path $opencvDll) {
            Write-Success "OpenCV extracted successfully"
        } else {
            throw "OpenCV DLL not found after extraction"
        }
    } catch {
        Write-Error-Custom "Failed to download/extract OpenCV: $_"
        Write-Info "Please download manually from: $opencvUrl"
        Write-Info "Extract to: $opencvDir"
        exit 1
    }
}

# ============================================================================
# Step 4: Download ONNX Runtime
# ============================================================================

Write-Step "Downloading ONNX Runtime"

$onnxDir = "windows_code\third-party\onnxruntime"
$onnxDll = "$onnxDir\lib\onnxruntime.dll"

if (Test-Path $onnxDll) {
    Write-Success "ONNX Runtime already exists"
} else {
    Write-Info "Downloading ONNX Runtime 1.16.3 (~15MB)..."
    New-Item -ItemType Directory -Force -Path $onnxDir | Out-Null

    $onnxUrl = "https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-win-x64-1.16.3.zip"
    $onnxZip = "$onnxDir\onnxruntime.zip"

    try {
        # Download ONNX Runtime
        Invoke-WebRequest -Uri $onnxUrl -OutFile $onnxZip -UseBasicParsing
        Write-Success "Downloaded ONNX Runtime"

        # Extract ONNX Runtime
        Write-Info "Extracting ONNX Runtime..."
        Expand-Archive -Path $onnxZip -DestinationPath "$onnxDir\temp" -Force

        # Move files to correct location (remove version-specific folder)
        $extractedDir = Get-ChildItem -Path "$onnxDir\temp" -Directory | Select-Object -First 1
        if ($extractedDir) {
            Copy-Item -Path "$($extractedDir.FullName)\*" -Destination $onnxDir -Recurse -Force
        }

        # Clean up
        Remove-Item "$onnxDir\temp" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item $onnxZip -Force

        # Verify extraction
        if (Test-Path $onnxDll) {
            Write-Success "ONNX Runtime extracted successfully"
        } else {
            throw "ONNX Runtime DLL not found after extraction"
        }
    } catch {
        Write-Error-Custom "Failed to download/extract ONNX Runtime: $_"
        Write-Info "Please download manually from: $onnxUrl"
        Write-Info "Extract to: $onnxDir"
        exit 1
    }
}

# ============================================================================
# Step 5: Initialize whisper.cpp (Git Submodule)
# ============================================================================

Write-Step "Initializing whisper.cpp"

if (Test-Path "windows_code\third-party\whisper.cpp\include\whisper.h") {
    Write-Success "whisper.cpp already initialized"
} else {
    Write-Info "Initializing whisper.cpp git submodule..."
    try {
        # Initialize all git submodules
        git submodule update --init --recursive

        # Verify whisper.cpp was initialized
        if (Test-Path "windows_code\third-party\whisper.cpp\include\whisper.h") {
            Write-Success "whisper.cpp submodule initialized"
        } else {
            throw "whisper.cpp files not found after submodule init"
        }
    } catch {
        Write-Error-Custom "Failed to initialize whisper.cpp submodule: $_"
        Write-Info "Please run manually: git submodule update --init --recursive"
        exit 1
    }
}

# ============================================================================
# Step 6: Build whisper.cpp
# ============================================================================

Write-Step "Building whisper.cpp"

$whisperBuildDir = "windows_code\third-party\whisper.cpp\build"

if (Test-Path "$whisperBuildDir\Release\whisper.lib") {
    Write-Success "whisper.cpp already built"
} else {
    Write-Info "Building whisper.cpp (this may take 5-10 minutes)..."

    Push-Location "windows_code\third-party\whisper.cpp"

    try {
        # Configure CMake
        cmake -B build -G "Visual Studio 17 2022" -A x64 -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF

        # Build Release
        cmake --build build --config Release --target whisper

        Write-Success "whisper.cpp built successfully"
    } catch {
        Write-Error-Custom "Failed to build whisper.cpp: $_"
        Pop-Location
        exit 1
    }

    Pop-Location
}

# Copy whisper.lib to expected location
$whisperLibSource = "windows_code\third-party\whisper.cpp\build\Release\whisper.lib"
$whisperLibDest = "windows_code\third-party\whisper.cpp\build\Release\whisper.lib"

if (Test-Path $whisperLibSource) {
    Write-Success "whisper.lib found at: $whisperLibSource"
} else {
    Write-Error-Custom "whisper.lib not found after build!"
    exit 1
}

# ============================================================================
# Step 7: Install Python Dependencies
# ============================================================================

if (-not $SkipPython) {
    Write-Step "Installing Python Dependencies"

    try {
        python -m pip install --upgrade pip
        python -m pip install -r requirements_windows.txt
        Write-Success "Python dependencies installed"
    } catch {
        Write-Warning "Failed to install Python dependencies: $_"
        Write-Info "You can install manually: pip install -r requirements_windows.txt"
    }
} else {
    Write-Info "Skipping Python dependencies (--SkipPython)"
}

# ============================================================================
# Step 8: Build PerceptionEngine
# ============================================================================

if (-not $SkipBuild) {
    Write-Step "Building PerceptionEngine"

    Push-Location "windows_code"

    try {
        # Configure CMake
        Write-Info "Configuring CMake..."
        cmake -B build -G "Visual Studio 17 2022" -A x64

        # Build Release
        Write-Info "Building PerceptionEngine (this may take 5-10 minutes)..."
        cmake --build build --config Release --target PerceptionEngine

        Write-Success "PerceptionEngine built successfully!"
    } catch {
        Write-Error-Custom "Failed to build PerceptionEngine: $_"
        Pop-Location
        exit 1
    }

    Pop-Location
} else {
    Write-Info "Skipping build (--SkipBuild)"
}

# ============================================================================
# Step 9: Copy dashboard.html
# ============================================================================

Write-Step "Copying dashboard.html"

$dashboardSource = "windows_code\dashboard.html"
$dashboardDest = "windows_code\build\bin\Release\dashboard.html"

if (Test-Path $dashboardSource) {
    New-Item -ItemType Directory -Force -Path "windows_code\build\bin\Release" | Out-Null
    Copy-Item $dashboardSource $dashboardDest -Force
    Write-Success "dashboard.html copied to build directory"
} else {
    Write-Warning "dashboard.html not found at: $dashboardSource"
}

# ============================================================================
# Step 10: Verify Installation
# ============================================================================

Write-Step "Verifying Installation"

$exePath = "windows_code\build\bin\Release\PerceptionEngine.exe"
$dashboardPath = "windows_code\build\bin\Release\dashboard.html"

$allGood = $true

# Verify executable
if (Test-Path $exePath) {
    Write-Success "PerceptionEngine.exe found"
} else {
    Write-Error-Custom "PerceptionEngine.exe NOT found!"
    $allGood = $false
}

# Verify dashboard
if (Test-Path $dashboardPath) {
    Write-Success "dashboard.html found"
} else {
    Write-Warning "dashboard.html NOT found"
}

# Verify AI models
if (Test-Path $whisperModelPath) {
    $whisperSize = (Get-Item $whisperModelPath).Length / 1MB
    Write-Success "Whisper model found ($([math]::Round($whisperSize,1))MB)"
} else {
    Write-Error-Custom "Whisper model NOT found!"
    $allGood = $false
}

if (Test-Path $vadModelPath) {
    $vadSize = (Get-Item $vadModelPath).Length / 1MB
    Write-Success "Silero VAD model found ($([math]::Round($vadSize,1))MB)"
} else {
    Write-Error-Custom "Silero VAD model NOT found!"
    $allGood = $false
}

# Verify third-party libraries
if (Test-Path $opencvDll) {
    Write-Success "OpenCV library found"
} else {
    Write-Error-Custom "OpenCV NOT found!"
    $allGood = $false
}

if (Test-Path $onnxDll) {
    Write-Success "ONNX Runtime found"
} else {
    Write-Error-Custom "ONNX Runtime NOT found!"
    $allGood = $false
}

# Verify whisper.lib
if (Test-Path "windows_code\third-party\whisper.cpp\build\Release\whisper.lib") {
    Write-Success "whisper.cpp library found"
} else {
    Write-Error-Custom "whisper.lib NOT found!"
    $allGood = $false
}

# ============================================================================
# Final Summary
# ============================================================================

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta

if ($allGood) {
    Write-Success "Setup Complete!"
    Write-Host "`n📦 All dependencies installed and project built successfully!`n"

    Write-Info "Next steps:"
    Write-Host "  1. Start the C++ server:"
    Write-Host "     cd windows_code\build\bin\Release"
    Write-Host "     .\PerceptionEngine.exe"
    Write-Host ""
    Write-Host "  2. (Optional) Start Python camera client in another terminal:"
    Write-Host "     cd windows_code"
    Write-Host "     python win_camera_fastvlm_pytorch.py"
    Write-Host ""
    Write-Host "  3. Open dashboard in browser:"
    Write-Host "     http://localhost:8777/dashboard"
    Write-Host ""
} else {
    Write-Error-Custom "Setup completed with errors!"
    Write-Info "Please check the error messages above and resolve issues"
    exit 1
}

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Magenta
