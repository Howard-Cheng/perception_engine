# Getting Started with Meeting Assistant

## Quick Start (3 Steps)

### Step 1: Build Native DLL (5 minutes)

Open **"x64 Native Tools Command Prompt for VS 2022"**:

```powershell
cd "C:\Users\hc001\OneDrive - Lenovo\Desktop\perception_engine\windows_code"
build_microphone_monitor_dll.bat
```

**Expected output:**
```
========================================
✓ Build SUCCESS!
========================================

DLL location: build\bin\Release\MicrophoneMonitor.dll
```

### Step 2: Build C# Application (2 minutes)

In any PowerShell or Command Prompt:

```powershell
cd "C:\Users\hc001\OneDrive - Lenovo\Desktop\perception_engine\sdk\csharp\MeetingAssistant"
dotnet build -c Release
```

**Expected output:**
```
Build succeeded.
   x warnings - may have a few
    0 Error(s)
```

### Step 3: Run the Service

```powershell
cd bin\Release\net8.0-windows10.0.19041.0
.\MeetingAssistant.exe
```

**Expected output:**
```
================================================================================
  Meeting Assistant - Proactive Meeting Detection
================================================================================

This service monitors your microphone for meeting apps and offers to start
Qira's 'Pay Attention' feature when a meeting is detected.

Supported apps: Teams, Zoom, Webex, Google Meet, and more...

Press Ctrl+C to exit
================================================================================

[MeetingAssistant] Initializing...
[✓] MicrophoneMonitor initialized
[✓] State machine initialized
[✓] Notification service initialized

[MeetingAssistant] Running... (polling every 2 seconds)

[MeetingAssistant] No meeting detected (state: Idle)
```

---

## Testing the Demo

### Test 1: Meeting Detection

1. **Start MeetingAssistant** (see Step 3 above)

2. **Join a Teams or Zoom meeting**

3. **Verify detection:**
   ```
   [MeetingAssistant] Meeting detected: ms-teams.exe (PID: 19976) - Detected at 18:30:15
   [State] Idle → Detected: ms-teams.exe
   [Notification] Shown for ms-teams.exe
   ```

4. **Check Windows notification:**
   - Look for toast notification in bottom-right corner
   - Should say: "Meeting Detected! Want Qira to pay attention to your ms-teams.exe meeting?"
   - Should have [Start] and [Dismiss] buttons

### Test 2: User Interaction

1. **Click "Start" button** on the notification

2. **Verify mock SDK call:**
   ```
   ╔════════════════════════════════════════════════════════════╗
   ║  USER ACTION: Clicked 'Start'                              ║
   ╚════════════════════════════════════════════════════════════╝

   ================================================================================
   [2025-10-26 18:30:20] WOULD CALL PAY ATTENTION SDK
   ================================================================================
   Action: Start Meeting Transcription
   App Name: ms-teams.exe
   Process ID: 19976
   Meeting Started: 2025-10-26 18:30:15
   User Confirmed: 2025-10-26 18:30:20
   ```

3. **Check log file:**
   ```powershell
   notepad meeting_assistant.log
   ```
   Should contain the same mock SDK call details

### Test 3: Meeting End Detection

1. **Leave the meeting** in Teams/Zoom

2. **Verify state transition:**
   ```
   [State] PayingAttention → Idle (meeting ended)
   [MeetingAssistant] No meeting detected (state: Idle)
   ```

---

## What Reuses Existing Code?

✅ **MicrophoneMonitor.cpp** - 100% reused (compiled as DLL)
✅ **MicrophoneMonitor_AudioDetection.cpp** - 100% reused
✅ **Meeting app list** - All 15+ apps from existing code
✅ **Audio session detection logic** - Unchanged
✅ **Logger.cpp** - Reused in DLL

**NEW C# code:**
- P/Invoke wrapper (MicrophoneMonitorNative.cs) - 100 lines
- State machine (MeetingStateMachine.cs) - 80 lines
- Notification service (NotificationService.cs) - 60 lines
- Mock SDK bridge (PayAttentionBridge.cs) - 50 lines
- Main program (Program.cs) - 140 lines

**Total new code: ~430 lines** (vs ~1500 lines if porting C++ logic)

---

## Architecture Diagram

```
┌─────────────────────────────────────────┐
│  User joins Teams meeting               │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Windows WASAPI Audio Sessions          │
│  ms-teams.exe → ACTIVE on microphone    │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  MicrophoneMonitor.dll (C++)            │
│  [REUSED EXISTING CODE]                 │
│  • GetActiveMicrophoneSessions()        │
│  • IsMeetingApp()                       │
│  • 15+ meeting app list                 │
└──────────────┬──────────────────────────┘
               │ P/Invoke
               ▼
┌─────────────────────────────────────────┐
│  MeetingAssistant.exe (C#)              │
│  ┌──────────────────────────────────┐   │
│  │ MicrophoneMonitorNative          │   │
│  │ • Calls DLL every 2 seconds      │   │
│  └──────────┬───────────────────────┘   │
│             ▼                            │
│  ┌──────────────────────────────────┐   │
│  │ MeetingStateMachine              │   │
│  │ • Idle → Detected → Paying...    │   │
│  └──────────┬───────────────────────┘   │
│             ▼                            │
│  ┌──────────────────────────────────┐   │
│  │ NotificationService              │   │
│  │ • Windows toast notification     │   │
│  └──────────┬───────────────────────┘   │
│             │ User clicks "Start"       │
│             ▼                            │
│  ┌──────────────────────────────────┐   │
│  │ PayAttentionBridge (MOCK)        │   │
│  │ • Logs to file                   │   │
│  │ • TODO: Call real SDK            │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

---

## Troubleshooting

### Error: "Failed to create MicrophoneMonitor instance"

**Cause:** MicrophoneMonitor.dll not found or wrong architecture

**Fix:**
1. Check DLL exists: `dir ..\..\..\windows_code\build\bin\Release\MicrophoneMonitor.dll`
2. If missing, build it:
   - Open "x64 Native Tools Command Prompt"
   - `cd windows_code`
   - `build_microphone_monitor_dll.bat`
3. Rebuild C# project: `dotnet build -c Release`

### Error: "The system cannot find the file specified" (DLL)

**Cause:** DLL not copied to output directory

**Fix:** Check `.csproj` file includes this:
```xml
<ItemGroup>
  <None Update="..\..\..\windows_code\build\bin\Release\MicrophoneMonitor.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </None>
</ItemGroup>
```

### Notifications not appearing

**Cause:** Windows notification settings

**Fix:**
1. Open Windows Settings → System → Notifications
2. Enable notifications
3. Scroll down to MeetingAssistant → Enable
4. Try running as Administrator

### No meeting detected

**Cause:** Meeting app not using microphone

**Fix:**
1. Join an ACTIVE meeting (not just open the app)
2. Ensure microphone is unmuted in the meeting
3. Check Task Manager → Details → Find Teams/Zoom process
4. Verify meeting is actually in progress

---

## Next Steps

### For Demo to Adjacent Team

1. ✅ Build and run MeetingAssistant
2. ✅ Join Teams meeting
3. ✅ Show notification appearing
4. ✅ Click "Start" button
5. ✅ Show log file with mock SDK call

### For Production Integration

When you receive Pay Attention SDK:

1. **Install SDK NuGet package** (if available):
   ```powershell
   dotnet add package PayAttentionSDK
   ```

2. **Replace PayAttentionBridge.cs**:
   ```csharp
   using PayAttentionSDK;  // Add real SDK

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

3. **Test with real SDK**

4. **Add production notification UI** (themed design)

---

## File Locations

```
perception_engine/
├── windows_code/
│   ├── MicrophoneMonitor.cpp                  [EXISTING - reused]
│   ├── MicrophoneMonitor_AudioDetection.cpp   [EXISTING - reused]
│   ├── MicrophoneMonitorDLL.h                 [NEW - wrapper]
│   ├── MicrophoneMonitorDLL.cpp               [NEW - wrapper]
│   └── build_microphone_monitor_dll.bat       [NEW - build script]
│
└── sdk/csharp/MeetingAssistant/
    ├── MeetingAssistant.csproj
    ├── Program.cs                              [Main service loop]
    ├── MicrophoneMonitorNative.cs              [P/Invoke wrapper]
    ├── MeetingStateMachine.cs                  [State tracking]
    ├── NotificationService.cs                  [Toast notifications]
    ├── PayAttentionBridge.cs                   [Mock SDK - REPLACE THIS]
    ├── build.bat                               [Quick build script]
    ├── README.md                               [Full documentation]
    └── GETTING_STARTED.md                      [This file]
```

---

## Success Criteria ✓

- [x] Reuses existing MicrophoneMonitor C++ code (no porting)
- [x] Detects meeting apps via microphone monitoring
- [x] Shows Windows toast notification
- [x] User can click "Start" or "Dismiss"
- [x] Logs mock SDK call to file
- [x] No changes to PerceptionEngine.exe (clean separation)
- [x] Ready for Pay Attention SDK integration (tomorrow)

---

## Questions?

See:
- **Full documentation**: `README.md`
- **Architecture details**: Main conversation summary
- **C++ code**: `windows_code/MicrophoneMonitor.cpp`

**Ready to demo!** 🚀
