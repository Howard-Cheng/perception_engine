# TestASR - Microsoft Speech Foundation Core Test Application

## Overview

TestASR is a test application for the Microsoft Speech Foundation Core (MSFTCore) ASR library. It demonstrates real-time speech recognition, translation, and summarization capabilities.

## Features

- **Real-time Speech Recognition**: Convert speech to text in real-time
- **Multi-language Support**: Support for Chinese (zh-CN) and English (en-US)
- **Translation**: Optional real-time translation between languages
- **Summarization**: AI-powered summarization of recorded speech
- **Recording Control**: Start, stop, pause, and resume audio recording
- **Callback System**: Event-driven architecture for ASR results and state changes

## Project Structure

```
test_asr/
├── CMakeLists.txt          # CMake build configuration
├── src/
│   └── TestASRMain.cpp     # Main application source code
└── README.md               # This file
```

## Dependencies

- **MSFTCore Library**: Located at `../third-party/asr/`
  - Include: `MSFTCore.h`
  - Library: `MSFTCore.lib`
  - DLL: `MSFTCore.dll`

## Building

### Prerequisites

- CMake 3.15 or higher
- MSVC compiler (Visual Studio 2019 or later)
- MSFTCore library installed in `../third-party/asr/`

### Build Steps

1. **Configure CMake** (from `windows_code/buildnew/` directory):
   ```bash
   cmake .. -DBUILD_TEST_ASR=ON
   ```

2. **Build the project**:
   ```bash
   cmake --build . --config Release
   ```

3. **Run the executable**:
   ```bash
   bin/Release/TestASR.exe
   ```

### Build Options

- `BUILD_TEST_ASR`: Enable/disable TestASR build (default: ON)
  ```bash
  cmake .. -DBUILD_TEST_ASR=OFF  # Disable TestASR
  ```

## Usage

### Running the Application

```bash
cd bin/Release
TestASR.exe
```

### Available Commands

Once the application starts, you can use the following commands:

| Command  | Description                                    |
|----------|------------------------------------------------|
| `start`  | Start audio recording                          |
| `stop`   | Stop audio recording and generate summary      |
| `pause`  | Pause audio recording                          |
| `resume` | Resume audio recording                         |
| `help`   | Display help message                           |
| `quit`   | Exit the program                               |
| `exit`   | Exit the program                               |

### Example Session

```
Initializing ASR system...
ASR Initialized!

========================================
   TestASR - ASR Library Test Program
========================================
Available commands:
  start   - Start audio recording
  stop    - Stop audio recording and get summary
  pause   - Pause audio recording
  resume  - Resume audio recording
  help    - Display this help message
  quit    - Exit the program
  exit    - Exit the program
========================================

> start
Starting recording...
[State]: Listening...

> [ASR Result]: 你好，这是一个测试

> stop
Stopping recording...
[Summary Progress]: 50%
[Summary Progress]: 100%
[Summary Result]: This is a test of the ASR system.

> quit
Exiting...
Program terminated.
```

## Configuration

The application can be configured by modifying the source code:

### Language Settings

```cpp
// In TestASRMain.cpp
QAASR::InitializeASR(L"zh-CN");  // Chinese (China)
// or
QAASR::InitializeASR(L"en-US");  // English (US)
```

### Translation Settings

```cpp
// Enable translation from Chinese to English
QATranslation::EnableTranslation(true, L"zh-CN", L"en-US");
```

### Summarization Prompt

```cpp
const wchar_t* prompt = L"Your custom summarization prompt here...";
QTSummary::EnableSummary(prompt, content);
```

## API Reference

### Core Functions (QAASR namespace)

- `InitializeASR(const wchar_t* language)`: Initialize ASR with specified language
- `StartRecord()`: Start audio recording
- `StopRecord()`: Stop audio recording
- `PauseRecord()`: Pause audio recording
- `ResumeRecord()`: Resume audio recording
- `SetASRCallback(ASR_Callback callback)`: Set callback for ASR results
- `SetSpeechStateCallback(SpeechState_Callback callback)`: Set callback for speech state changes

### Translation Functions (QATranslation namespace)

- `EnableTranslation(bool state, const wchar_t* sourceLanguage, const wchar_t* targetLanguage)`: Enable/disable translation

### Summarization Functions (QTSummary namespace)

- `EnableSummary(const wchar_t* prompt, const wchar_t* content)`: Enable summarization with custom prompt
- `StartSummary(const wchar_t* sessionId)`: Start summarization process
- `SummaryOnResult(SummarizedCallback callback)`: Set callback for summary results
- `SummaryProgress(progressCallback callback)`: Set callback for progress updates

## Troubleshooting

### MSFTCore.dll not found

Make sure `MSFTCore.dll` is in the same directory as `TestASR.exe`. The CMake build script automatically copies it during the build process.

### No audio input

Check your system's default audio input device in Windows settings.

### Compilation errors

Ensure you have:
- Visual Studio 2019 or later installed
- MSFTCore library files in the correct location
- UNICODE and _UNICODE defined (set automatically by CMakeLists.txt)

## License

This test application is part of the PerceptionEngine project. Refer to the main project license for details.

## Reference Implementation

This implementation is based on the reference code in:
`D:\PerceiptionEngine_Howard\perception_engine\windows_code\third-party\asr\TestASRTranscipt.cpp`

## Support

For issues or questions related to:
- TestASR application: Create an issue in the PerceptionEngine repository
- MSFTCore library: Contact Microsoft Speech Foundation team
