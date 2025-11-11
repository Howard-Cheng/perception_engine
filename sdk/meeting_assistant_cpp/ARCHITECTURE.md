# Architecture Documentation

## System Overview

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦                  MeetingAssistant.exe                       ©¦
©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©¦
©¦  ©¦              Main Application Loop                    ©¦  ©¦
©¦  ©¦  - Polls every 2 seconds                             ©¦  ©¦
©¦  ©¦  - Detects meeting state changes                     ©¦  ©¦
©¦  ©¦  - Coordinates all components                        ©¦  ©¦
©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¦
©¦               ©¦            ©¦            ©¦                    ©¦
©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤¨‹©¤©¤©´  ©°©¤©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©´  ©°©¤¨‹©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´   ©¦
©¦  ©¦ MeetingState  ©¦  ©¦Notification ©¦  ©¦ PayAttention    ©¦   ©¦
©¦  ©¦   Machine     ©¦  ©¦  Service    ©¦  ©¦    Bridge       ©¦   ©¦
©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¼  ©¸©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¼  ©¸©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼   ©¦
©¦               ©¦            ©¦            ©¦                    ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                ©¦            ©¦            ©¦
                ©¦            ©¦            ¨‹
                ©¦            ©¦     ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                ©¦            ©¦     ©¦  MSFTCore.dll    ©¦
                ©¦            ©¦     ©¦  (Speech SDK)    ©¦
                ©¦            ©¦     ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                ©¦            ©¦
                ©¦            ¨‹
                ©¦     ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                ©¦     ©¦ Windows Runtime  ©¦
                ©¦     ©¦ (Toast Notif.)   ©¦
                ©¦     ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                ©¦
                ¨‹
  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
  ©¦   MicrophoneMonitor.dll          ©¦
  ©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©¦
  ©¦  ©¦  MicrophoneMonitor Class   ©¦  ©¦
  ©¦  ©¦  - Enumerate audio sessions©¦  ©¦
  ©¦  ©¦  - Detect meeting apps     ©¦  ©¦
  ©¦  ©¦  - Monitor mic/speaker     ©¦  ©¦
  ©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¦
  ©¦               ©¦                   ©¦
  ©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©¦
  ©¦  ©¦    Windows WASAPI          ©¦  ©¦
  ©¦  ©¦  - IAudioSessionManager2   ©¦  ©¦
  ©¦  ©¦  - IMMDeviceEnumerator     ©¦  ©¦
  ©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¦
  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

## Module Descriptions

### 1. MicrophoneMonitor.dll

**Responsibility**: Low-level audio device monitoring

**Key Classes**:
- `MicrophoneMonitor`: Main monitoring class
- `Logger`: Thread-safe logging utility

**Key Functions**:
- `GetActiveMicrophoneSessions()`: List apps using microphone
- `GetActiveSpeakerSessions()`: List apps using speakers
- `IsMeetingAppUsingMicrophone()`: Check for meeting apps
- `IsMeetingApp()`: Identify known meeting applications

**Dependencies**:
- Windows WASAPI (mmdeviceapi.h, audiopolicy.h)
- Process Status API (psapi.lib)
- COM (ole32.lib)

**Export Interface** (C-compatible):
```cpp
MicrophoneMonitorHandle MicrophoneMonitor_Create();
void MicrophoneMonitor_Destroy(MicrophoneMonitorHandle);
int MicrophoneMonitor_IsMeetingAppUsingMicrophone(MicrophoneMonitorHandle);
int MicrophoneMonitor_GetMeetingAppName(MicrophoneMonitorHandle, char*, int);
unsigned long MicrophoneMonitor_GetMeetingAppPID(MicrophoneMonitorHandle);
```

### 2. MeetingAssistant.exe

**Responsibility**: User-facing application logic

**Key Components**:

#### A. MeetingStateMachine
- **Purpose**: Track meeting lifecycle
- **States**:
  - `Idle`: No meeting detected
  - `Detected`: Meeting found, notification shown
  - `WaitingForUser`: Awaiting user action
  - `PayingAttention`: Recording/transcribing
  - `Dismissed`: User dismissed notification
- **Key Methods**:
  - `Update()`: Process detection results
  - `OnUserConfirmed()`: Handle "Start" button
  - `OnUserDismissed()`: Handle "Dismiss" button

#### B. NotificationService
- **Purpose**: Windows Toast notifications
- **Technology**: Windows Runtime (WinRT) API
- **Features**:
  - Interactive buttons
  - Notification activation handling
  - Meeting detected notifications
  - Meeting summary notifications
- **Key Methods**:
  - `ShowMeetingDetectedNotification()`
  - `ShowMeetingSummaryNotification()`
  - `OnNotificationActivated()`

#### C. PayAttentionBridge
- **Purpose**: Integration with MSFTCore SDK
- **Features**:
  - Speech recognition (ASR) callbacks
  - Real-time transcription
  - Meeting content accumulation
  - AI-powered summarization
- **Key Methods**:
  - `Initialize()`: Setup SDK callbacks
  - `StartMeetingTranscription()`: Begin recording
  - `StopMeetingTranscription()`: End recording
  - `StartMeetingSummarization()`: Generate summary

#### D. Main Loop
- **Purpose**: Application orchestration
- **Frequency**: 2-second polling
- **Responsibilities**:
  - Query MicrophoneMonitor for meetings
  - Update state machine
  - Trigger notifications
  - Handle user actions
  - Coordinate SDK calls

## Data Flow

### Meeting Detection Flow

```
1. Main Loop (every 2s)
   ©¦
   ©À©¤¡ú MicrophoneMonitor.dll::IsMeetingAppUsingMicrophone()
   ©¦   ©¦
   ©¦   ©À©¤¡ú EnumerateAudioSessions()
   ©¦   ©¦   ©¸©¤¡ú Windows WASAPI
   ©¦   ©¦
   ©¦   ©¸©¤¡ú IsMeetingApp(processName)
   ©¦       ©¸©¤¡ú Returns: true/false
   ©¦
   ©À©¤¡ú MeetingStateMachine::Update(detected, info)
   ©¦   ©¦
   ©¦   ©¸©¤¡ú Returns: stateChanged
   ©¦
   ©¸©¤¡ú If stateChanged && state == Detected:
       ©¸©¤¡ú NotificationService::ShowMeetingDetectedNotification()
           ©¸©¤¡ú Windows Runtime Toast API
```

### User Action Flow

```
User clicks "Start" button
   ©¦
   ©À©¤¡ú Windows Runtime activates notification
   ©¦
   ©À©¤¡ú NotificationService::OnNotificationActivated()
   ©¦   ©¸©¤¡ú Parses arguments
   ©¦
   ©À©¤¡ú OnUserClickedStart() callback
   ©¦   ©¦
   ©¦   ©À©¤¡ú PayAttentionBridge::StartMeetingTranscription()
   ©¦   ©¦   ©¦
   ©¦   ©¦   ©À©¤¡ú MSFTCore.dll::StartRecord()
   ©¦   ©¦   ©¦
   ©¦   ©¦   ©¸©¤¡ú Logs to file
   ©¦   ©¦
   ©¦   ©¸©¤¡ú MeetingStateMachine::OnUserConfirmed()
   ©¦       ©¸©¤¡ú State: Detected ¡ú PayingAttention
   ©¦
   ©¸©¤¡ú ASR callbacks start flowing
       ©¸©¤¡ú PayAttentionBridge::OnASRResult()
           ©¸©¤¡ú Accumulate meeting content
```

### Meeting End Flow

```
Meeting ends (microphone released)
   ©¦
   ©À©¤¡ú Main Loop detects: meetingDetected == false
   ©¦
   ©À©¤¡ú MeetingStateMachine::Update()
   ©¦   ©¸©¤¡ú State: PayingAttention ¡ú Idle
   ©¦
   ©À©¤¡ú PayAttentionBridge::StopRecord()
   ©¦   ©¸©¤¡ú MSFTCore.dll::StopRecord()
   ©¦
   ©¸©¤¡ú NotificationService::ShowMeetingSummaryNotification()
       ©¦
       ©¸©¤¡ú User clicks "Start"
           ©¸©¤¡ú PayAttentionBridge::StartMeetingSummarization()
               ©¦
               ©À©¤¡ú MSFTCore.dll::EnableSummary()
               ©À©¤¡ú MSFTCore.dll::StartSummary()
               ©¸©¤¡ú Display summary result
```

## Threading Model

### Main Thread
- Application loop
- State machine updates
- UI events (notification activation)

### MicrophoneMonitor (Internal)
- COM apartment threads (WASAPI)
- Audio session enumeration

### MSFTCore SDK (External)
- ASR callback thread
- Speech state callback thread

**Thread Safety**:
- All callbacks use console output (not thread-safe in general, but acceptable for debugging)
- Logger class uses mutexes for thread-safe file writing
- Static variables in PayAttentionBridge accessed from callbacks (potential race condition - consider adding mutex)

## Memory Management

### MicrophoneMonitor.dll
- **Pattern**: RAII (Resource Acquisition Is Initialization)
- **Cleanup**: Destructor releases COM interfaces
- **Handles**: Opaque pointer for C API

### MeetingAssistant.exe
- **Pattern**: Smart pointers (`std::unique_ptr`, `std::optional`)
- **Cleanup**: Automatic via RAII
- **Lifetime**: Objects live for application lifetime

### COM Objects
- **Management**: Manual `Release()` calls
- **Ownership**: Clear single-owner model
- **Cleanup**: In destructors

## Error Handling

### MicrophoneMonitor.dll
- **Strategy**: Defensive programming
- **Errors**: Logged via Logger class
- **Fallback**: Returns empty vectors/false on error
- **No Exceptions**: C-compatible interface

### MeetingAssistant.exe
- **Strategy**: Exception-based
- **Errors**: Logged to console + file
- **Recovery**: Continue running on non-fatal errors
- **User Notification**: Console messages

## Performance Characteristics

### Polling Overhead
- **Frequency**: Every 2 seconds
- **Work per poll**:
  - Enumerate audio sessions: ~10ms
  - Update state machine: <1ms
  - Total: ~10-20ms per poll
- **CPU Impact**: Negligible (<0.1%)

### Audio Detection
- **Method**: Session enumeration (not audio analysis)
- **Latency**: Near-instant (no buffering)
- **Accuracy**: Depends on app session reporting

### Notification Display
- **Latency**: ~100-500ms (Windows Runtime)
- **User Experience**: Smooth, non-blocking

## Security Considerations

### Privacy
- **Audio Content**: NOT captured by MicrophoneMonitor
- **Metadata Only**: Process names, PIDs, session states
- **MSFTCore**: Handles actual audio (external component)

### Permissions
- **No Special Permissions**: Standard user can run
- **COM Initialization**: Required for WASAPI
- **Process Enumeration**: Built-in Windows API

### Code Execution
- **DLL Security**: Loads from application directory first
- **No Network**: Offline operation (unless MSFTCore uses network)

## Extensibility Points

### Adding New Meeting Apps
Edit `MicrophoneMonitor::IsMeetingApp()`:
```cpp
static const std::vector<std::string> meetingApps = {
    "Zoom.exe",
    "Teams.exe",
    "YourNewApp.exe",  // Add here
    // ...
};
```

### Custom Notifications
Extend `NotificationService`:
```cpp
void ShowCustomNotification(const std::string& title, 
                            const std::string& message);
```

### Alternative State Machines
Implement `IMeetingStateMachine` interface:
```cpp
class CustomStateMachine : public IMeetingStateMachine {
    // Custom logic
};
```

### Logging Backends
Extend `Logger` class:
```cpp
class RemoteLogger : public Logger {
    // Send logs to remote server
};
```

## Build System Architecture

### CMake Targets

```
Project: MeetingAssistant
©À©¤©¤ Target: MicrophoneMonitor (SHARED)
©¦   ©À©¤©¤ Type: Dynamic Library (.dll)
©¦   ©À©¤©¤ Sources: src/MicrophoneMonitor/*.cpp
©¦   ©À©¤©¤ Headers: include/MicrophoneMonitor/*.h
©¦   ©À©¤©¤ Defines: BUILDING_DLL
©¦   ©¸©¤©¤ Links: ole32.lib, psapi.lib
©¦
©¸©¤©¤ Target: MeetingAssistant (EXECUTABLE)
    ©À©¤©¤ Type: Console Application (.exe)
    ©À©¤©¤ Sources: src/MeetingAssistant/*.cpp, src/main.cpp
    ©À©¤©¤ Headers: include/MeetingAssistant/*.h
    ©À©¤©¤ Depends: MicrophoneMonitor (auto-linked)
    ©¸©¤©¤ Links: windowsapp.lib
```

### Include Path Resolution

```
MeetingAssistant.exe compiling:
  - Include search paths:
    1. include/MeetingAssistant/
    2. include/MicrophoneMonitor/
  
  - #include "MeetingAssistant/PayAttentionBridge.h"
    ¡ú Resolves to: include/MeetingAssistant/PayAttentionBridge.h
  
  - #include "MicrophoneMonitor/MicrophoneMonitorDLL.h"
    ¡ú Resolves to: include/MicrophoneMonitor/MicrophoneMonitorDLL.h
```

### Link-time Behavior

```
Linking MeetingAssistant.exe:
  1. Links MicrophoneMonitor.lib (import library)
  2. Runtime: Loads MicrophoneMonitor.dll
  3. Runtime: Loads MSFTCore.dll (delay-load)
  4. Runtime: Loads Windows Runtime (system)
```

## Deployment Architecture

### File Layout

```
DeploymentPackage/
©À©¤©¤ MeetingAssistant.exe          # Main application
©À©¤©¤ MicrophoneMonitor.dll         # Audio monitoring
©À©¤©¤ MSFTCore.dll                  # Speech SDK (external)
©À©¤©¤ (MSFTCore dependencies)       # SDK required DLLs
©¸©¤©¤ meeting_assistant.log         # Log file (created at runtime)
```

### Installation
- **No Installation Required**: Portable application
- **Dependencies**: Windows 10/11 x64
- **User Permissions**: Standard user sufficient

### Updates
- **MicrophoneMonitor.dll**: Can update independently
- **MSFTCore.dll**: Managed by SDK team
- **MeetingAssistant.exe**: Replace to update main logic

---

## Summary

This architecture provides:
- ? **Modularity**: Clear separation of concerns
- ? **Maintainability**: Standard directory structure
- ? **Extensibility**: Easy to add features
- ? **Performance**: Efficient polling-based design
- ? **Reliability**: Defensive error handling
- ? **Privacy**: Metadata-only detection

The system is designed for long-running operation with minimal resource usage and clear user notifications.
