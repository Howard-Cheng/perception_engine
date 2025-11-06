# MeetingAssistant C++ Project

Unified C++ project for meeting detection and transcription assistant.

## Project Structure

```
meeting_assistant_cpp/
©À©¤©¤ CMakeLists.txt              # Root CMake configuration
©À©¤©¤ README.md                   # This file
©À©¤©¤ ARCHITECTURE.md             # Architecture documentation
©¦
©À©¤©¤ include/                    # Public header files
©¦   ©À©¤©¤ MicrophoneMonitor/      # MicrophoneMonitor library headers
©¦   ©¦   ©À©¤©¤ MicrophoneMonitor.h
©¦   ©¦   ©À©¤©¤ MicrophoneMonitorDLL.h
©¦   ©¦   ©¸©¤©¤ Logger.h
©¦   ©¦
©¦   ©¸©¤©¤ MeetingAssistant/       # MeetingAssistant application headers
©¦       ©À©¤©¤ PayAttentionBridge.h
©¦       ©À©¤©¤ NotificationService.h
©¦       ©¸©¤©¤ MeetingStateMachine.h
©¦
©À©¤©¤ src/                        # Implementation files
©¦   ©À©¤©¤ MicrophoneMonitor/      # MicrophoneMonitor library sources
©¦   ©¦   ©À©¤©¤ MicrophoneMonitor.cpp
©¦   ©¦   ©À©¤©¤ MicrophoneMonitor_AudioDetection.cpp
©¦   ©¦   ©À©¤©¤ MicrophoneMonitorDLL.cpp
©¦   ©¦   ©¸©¤©¤ Logger.cpp
©¦   ©¦
©¦   ©À©¤©¤ MeetingAssistant/       # MeetingAssistant application sources
©¦   ©¦   ©À©¤©¤ PayAttentionBridge.cpp
©¦   ©¦   ©À©¤©¤ NotificationService.cpp
©¦   ©¦   ©¸©¤©¤ MeetingStateMachine.cpp
©¦   ©¦
©¦   ©¸©¤©¤ main.cpp                # Application entry point
©¦
©¸©¤©¤ build/                      # Build output directory (generated)
    ©À©¤©¤ bin/                    # Executables
    ©¸©¤©¤ lib/                    # Libraries
```

## Components

### 1. MicrophoneMonitor Library (DLL)
- **Purpose**: Detect meeting applications using audio devices
- **Type**: Dynamic link library (DLL)
- **Dependencies**: Windows WASAPI
- **Output**: `MicrophoneMonitor.dll`

### 2. MeetingAssistant Application (EXE)
- **Purpose**: Main application for meeting transcription and assistance
- **Type**: Console executable
- **Dependencies**: 
  - MicrophoneMonitor.dll
  - MSFTCore.dll
  - Windows Runtime (WinRT)
- **Output**: `MeetingAssistant.exe`

## Building

### Prerequisites
- Visual Studio 2022 with C++ Desktop Development
- Windows 10 SDK (10.0 or later)
- CMake 3.20 or later

### Build Commands

```cmd
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -A x64

# Build all targets
cmake --build . --config Release

# Or build specific target
cmake --build . --config Release --target MicrophoneMonitor
cmake --build . --config Release --target MeetingAssistant
```

### Alternative: Visual Studio

Open `CMakeLists.txt` in Visual Studio 2022, which will automatically detect the CMake project.

## Dependencies

### MicrophoneMonitor.dll
- Windows WASAPI (mmdeviceapi.h, audiopolicy.h, audioclient.h)
- Psapi.lib (Process Status API)
- Ole32.lib (COM)

### MeetingAssistant.exe
- MicrophoneMonitor.dll
- MSFTCore.dll (external, for speech recognition)
- Windows Runtime (windowsapp.lib)

## Usage

After building:

1. Copy `MSFTCore.dll` and dependencies to `build/bin/Release/`
2. Run `MeetingAssistant.exe`

```cmd
cd build\bin\Release
copy D:\quantum_payattention\Quantum_PayAttention\x64\Release\*.dll .
MeetingAssistant.exe
```

## Features

- Automatic meeting detection (Teams, Zoom, Webex, etc.)
- Windows Toast notifications
- Real-time speech recognition (ASR)
- AI-powered meeting summarization
- State machine for meeting lifecycle management

## License

[Your License Here]
