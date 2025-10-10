# 🚀 Nova Perception Engine - Setup Guide

**For your colleague:** This guide explains how to set up the Nova Perception Engine from a fresh git clone.

---

## ⚡ Quick Start (Automated Setup)

**The easiest way to get started:**

```powershell
# Option 1: Double-click setup.bat (Windows Explorer)
# OR
# Option 2: Run from command line
.\setup.bat
```

This will:
1. ✅ Download Whisper model (~43MB)
2. ✅ Download Silero VAD model (~1.8MB)
3. ✅ Verify third-party libraries (OpenCV, ONNX Runtime)
4. ✅ Build whisper.cpp
5. ✅ Install Python dependencies
6. ✅ Build PerceptionEngine.exe
7. ✅ Copy dashboard.html

**Time:** 10-15 minutes (depending on internet speed and CPU)

---

## 📋 What's Missing from Git?

Due to file size limits, the following are **NOT** committed to Git and will be downloaded by `setup.bat`:

### **1. AI Models** (~45MB total)
- ❌ `models/whisper/ggml-tiny.en-q8_0.bin` (43MB)
- ❌ `models/vad/silero_vad.onnx` (1.8MB)

### **2. Build Artifacts**
- ❌ `windows_code/build/` - Entire build directory
- ❌ `PerceptionEngine.exe` - Final executable
- ❌ All `.dll`, `.lib`, `.obj` files

### **3. Third-Party Builds**
- ❌ `windows_code/third-party/whisper.cpp/build/` - whisper.cpp compiled library

**Note:** OpenCV and ONNX Runtime binaries ARE included in the repository (in `third-party/`), so you don't need to download them separately.

---

## 🛠️ Manual Setup (If Automated Setup Fails)

### **Prerequisites**

1. **Windows 10/11** (x64)
2. **Visual Studio 2022** with "Desktop development with C++"
   - Download: https://visualstudio.microsoft.com/
3. **CMake 3.20+**
   - Download: https://cmake.org/download/
4. **Python 3.8+**
   - Download: https://python.org
5. **Git** (for cloning)
   - Download: https://git-scm.com/

### **Step-by-Step Manual Installation**

#### **Step 1: Clone Repository**
```powershell
git clone <your-repo-url> PE
cd PE
```

#### **Step 2: Download Whisper Model**
```powershell
# Create models directory
mkdir models\whisper

# Download Whisper tiny.en model (~43MB)
# Visit: https://huggingface.co/ggerganov/whisper.cpp/tree/main
# Download: ggml-tiny.en-q8_0.bin
# Save to: models\whisper\ggml-tiny.en-q8_0.bin

# Or use PowerShell to download:
Invoke-WebRequest -Uri "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en-q8_0.bin" -OutFile "models\whisper\ggml-tiny.en-q8_0.bin"
```

#### **Step 3: Download Silero VAD Model**
```powershell
# Create VAD models directory
mkdir models\vad

# Download Silero VAD model (~1.8MB)
Invoke-WebRequest -Uri "https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx" -OutFile "models\vad\silero_vad.onnx"
```

#### **Step 4: Verify Third-Party Libraries**

**Check OpenCV:**
```powershell
dir windows_code\third-party\opencv\opencv\build\x64\vc16\lib\opencv_world4100.lib
```
If missing, download OpenCV 4.10.0 from https://opencv.org/releases/ and extract to `windows_code/third-party/opencv/`

**Check ONNX Runtime:**
```powershell
dir windows_code\third-party\onnxruntime\lib\onnxruntime.dll
```
If missing, download from https://github.com/microsoft/onnxruntime/releases and extract to `windows_code/third-party/onnxruntime/`

**Check whisper.cpp:**
```powershell
dir windows_code\third-party\whisper.cpp\include\whisper.h
```
If missing:
```powershell
cd windows_code\third-party
git submodule update --init --recursive
cd ..\..
```

#### **Step 5: Build whisper.cpp**
```powershell
cd windows_code\third-party\whisper.cpp

# Configure CMake
cmake -B build -G "Visual Studio 17 2022" -A x64 -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF

# Build Release
cmake --build build --config Release --target whisper

cd ..\..\..
```

#### **Step 6: Install Python Dependencies**
```powershell
python -m pip install --upgrade pip
pip install -r requirements_windows.txt
```

#### **Step 7: Build PerceptionEngine**
```powershell
cd windows_code

# Configure CMake
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release
cmake --build build --config Release --target PerceptionEngine

cd ..
```

#### **Step 8: Copy Dashboard**
```powershell
copy windows_code\dashboard.html windows_code\build\bin\Release\dashboard.html
```

---

## ▶️ Running the System

### **Method 1: Quick Start (Recommended)**
```powershell
# Navigate to windows_code directory
cd windows_code

# Start both C++ server and Python camera client
.\start_perception_engine.bat
```

### **Method 2: Manual Start**

**Terminal 1 - C++ Server:**
```powershell
cd windows_code\build\bin\Release
.\PerceptionEngine.exe
```

**Terminal 2 - Python Camera Client (Optional):**
```powershell
cd windows_code
python win_camera_fastvlm_pytorch.py
```

### **Open Dashboard:**
```
http://localhost:8777/dashboard
```

---

## ✅ Verify Installation

Run these commands to verify everything is set up correctly:

```powershell
# Check if Whisper model exists
dir models\whisper\ggml-tiny.en-q8_0.bin

# Check if Silero VAD model exists
dir models\vad\silero_vad.onnx

# Check if PerceptionEngine.exe exists
dir windows_code\build\bin\Release\PerceptionEngine.exe

# Check if dashboard exists
dir windows_code\build\bin\Release\dashboard.html
```

All files should exist! ✅

---

## 🐛 Troubleshooting

### **Problem: "CMake not found"**
**Solution:** Install CMake from https://cmake.org/download/ and add to PATH

### **Problem: "Visual Studio not found"**
**Solution:** Install Visual Studio 2022 with "Desktop development with C++" workload

### **Problem: "whisper.lib not found"**
**Solution:** Build whisper.cpp first (see Step 5 above)

### **Problem: "OpenCV not found"**
**Solution:** OpenCV should be in `windows_code/third-party/opencv/`. If missing, download from https://opencv.org/releases/

### **Problem: "Models not found"**
**Solution:** Run `setup.bat` to automatically download models, or download manually (see Step 2-3 above)

### **Problem: "Python dependencies failed to install"**
**Solution:**
```powershell
# Try installing with verbose output
pip install -r requirements_windows.txt --verbose

# Or install individually:
pip install torch transformers opencv-python requests numpy pillow
```

### **Problem: Build fails with "LNK2001: unresolved external symbol"**
**Solution:** Ensure whisper.cpp is built before building PerceptionEngine
```powershell
cd windows_code\third-party\whisper.cpp
cmake --build build --config Release --target whisper
cd ..\..\..
cmake --build windows_code\build --config Release --target PerceptionEngine
```

---

## 📚 Additional Resources

- **README.md** - User guide for running the application
- **CLAUDE.md** - Complete technical documentation
- **CMakeLists.txt** - Build configuration (for developers)

---

## 💡 Tips for Your Colleague

1. **Use the automated setup:** `setup.bat` handles everything for you
2. **Check prerequisites first:** Make sure Visual Studio, CMake, and Python are installed
3. **Be patient:** First build takes 10-15 minutes (whisper.cpp + PerceptionEngine)
4. **Check the logs:** If something fails, read the error messages carefully
5. **OpenCV is already included:** No need to download it separately

---

## 🎯 Expected File Structure After Setup

```
PE/
├── models/
│   ├── whisper/
│   │   └── ggml-tiny.en-q8_0.bin          ✅ Downloaded by setup.bat
│   └── vad/
│       └── silero_vad.onnx                ✅ Downloaded by setup.bat
├── windows_code/
│   ├── build/
│   │   └── bin/Release/
│   │       ├── PerceptionEngine.exe       ✅ Built by setup.bat
│   │       ├── dashboard.html             ✅ Copied by setup.bat
│   │       ├── onnxruntime.dll            ✅ Copied during build
│   │       ├── whisper.dll                ✅ Copied during build
│   │       └── opencv_world4100.dll       ✅ Copied during build
│   ├── third-party/
│   │   ├── opencv/                        ✅ Already in git
│   │   ├── onnxruntime/                   ✅ Already in git
│   │   └── whisper.cpp/
│   │       └── build/Release/
│   │           └── whisper.lib            ✅ Built by setup.bat
│   └── ...
├── setup.bat                              👈 Run this!
├── setup.ps1                              (PowerShell backend)
├── SETUP_GUIDE.md                         👈 This file
├── README.md
└── CLAUDE.md
```

---

**Questions?** Check the troubleshooting section above or refer to CLAUDE.md for detailed technical documentation.

**Happy coding! 🚀**
