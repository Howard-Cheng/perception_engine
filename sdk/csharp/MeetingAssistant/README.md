# Meeting Assistant - Proactive Meeting Detection

Standalone C# service that monitors for meeting apps and notifies users when meetings are detected.

## Architecture

**Separation of Concerns:**
- **PerceptionEngine.exe** - Data collection layer (no changes)
- **MeetingAssistant.exe** - Proactive feature layer (this project)

## Features

- ✅ Detects 15+ meeting apps (Teams, Zoom, Webex, Google Meet, etc.)
- ✅ Monitors microphone audio sessions via native Windows APIs
- ✅ Shows Windows toast notification when meeting detected
- ✅ User can click "Start" to trigger Pay Attention SDK
- ✅ In-memory state tracking (no persistence)
- ✅ Mock SDK integration (logs to file until real SDK available)

## Build Instructions

### Step 1: Build MicrophoneMonitor.dll

```powershell
cd windows_code
build_microphone_monitor_dll.bat
```

This creates `build\bin\Release\MicrophoneMonitor.dll` from existing C++ code.

### Step 2: Build MeetingAssistant.exe

```powershell
cd sdk\csharp\MeetingAssistant
dotnet build -c Release
```

Output: `bin\Release\net6.0-windows10.0.19041.0\MeetingAssistant.exe`

## Run Instructions

```powershell
cd sdk\csharp\MeetingAssistant\bin\Release\net6.0-windows10.0.19041.0
.\MeetingAssistant.exe
```

**Note:** You do NOT need to run PerceptionEngine.exe for meeting detection to work.

## Demo Flow

1. **Start MeetingAssistant**
   ```
   [MeetingAssistant] Running... (polling every 2 seconds)
   [MeetingAssistant] No meeting detected (state: Idle)
   ```

2. **Join a Teams/Zoom meeting**
   ```
   [MeetingAssistant] Meeting detected: ms-teams.exe (PID: 19976)
   [State] Idle → Detected
   [Notification] Shown for ms-teams.exe
   ```

3. **Toast notification appears:**
   ```
   ┌──────────────────────────────────┐
   │ Meeting Detected!                │
   │ Want Qira to pay attention to    │
   │ your ms-teams.exe meeting?       │
   │                                  │
   │  [Start]  [Dismiss]              │
   └──────────────────────────────────┘
   ```

4. **Click "Start"**
   ```
   [USER ACTION] Clicked 'Start'
   [PayAttentionBridge] Would call Pay Attention SDK:
     - App: ms-teams.exe
     - Process ID: 19976
     - Action: Start Meeting Transcription

   [State] Detected → PayingAttention
   ```

5. **Check log file**
   ```
   meeting_assistant.log created with mock SDK call details
   ```

## Components

### MicrophoneMonitorNative.cs
- P/Invoke wrapper for MicrophoneMonitor.dll
- Reuses existing C++ meeting detection logic
- Detects meeting apps using microphone

### MeetingStateMachine.cs
- Tracks state: Idle → Detected → PayingAttention → Idle
- In-memory only (no persistence)
- Handles state transitions based on detection

### NotificationService.cs
- Windows toast notifications
- Action buttons: [Start] [Dismiss]
- Handles user clicks

### PayAttentionBridge.cs
- **Mock integration** - logs to file
- Replace with real SDK when available
- Shows format for actual SDK call

### Program.cs
- Main service loop
- Polls microphone every 2 seconds
- Coordinates all components

## Supported Meeting Apps

- Microsoft Teams (ms-teams.exe, Teams.exe)
- Zoom (Zoom.exe, ZoomWebHost.exe)
- Google Meet (chrome.exe, msedge.exe, firefox.exe)
- Webex (Webex.exe, CiscoCollabHost.exe)
- Discord, Skype, Slack, and more...

## Integration with Pay Attention SDK

When SDK is available, replace `PayAttentionBridge.cs`:

```csharp
// Example (adjust based on actual SDK)
using PayAttentionSDK;

public static async Task StartMeetingTranscription(MeetingInfo meeting)
{
    var client = new PayAttentionClient();
    await client.StartTranscriptionAsync(new MeetingSession {
        AppName = meeting.AppName,
        ProcessId = meeting.ProcessId,
        StartTime = meeting.DetectedAt
    });
}
```

## Troubleshooting

**"Failed to create MicrophoneMonitor instance"**
- Ensure `MicrophoneMonitor.dll` is in same directory as executable
- Check DLL was built for x64 architecture
- Run from "x64 Native Tools Command Prompt"

**Notifications not showing**
- Check Windows notification settings
- Ensure notifications are enabled for the app
- Try running as administrator

**No meeting detected**
- Join an active meeting (Teams/Zoom)
- Ensure microphone is being used by the meeting app
- Check Task Manager → Audio sessions

## Files Created

```
sdk/csharp/MeetingAssistant/
├── MeetingAssistant.csproj          # Project file
├── Program.cs                        # Main service loop
├── MicrophoneMonitorNative.cs        # P/Invoke wrapper for DLL
├── MeetingStateMachine.cs            # State tracking
├── NotificationService.cs            # Toast notifications
├── PayAttentionBridge.cs             # Mock SDK integration
└── README.md                         # This file

windows_code/
├── MicrophoneMonitorDLL.h            # C-compatible wrapper header
├── MicrophoneMonitorDLL.cpp          # C-compatible wrapper impl
└── build_microphone_monitor_dll.bat  # DLL build script
```

## Next Steps

1. Test with actual meetings (Teams, Zoom)
2. Get Pay Attention SDK from adjacent team
3. Replace `PayAttentionBridge.cs` with real SDK calls
4. Add production notification UI (themed design)
5. Add meeting end detection → stop transcription

## License

Internal use only - Perception Engine project
