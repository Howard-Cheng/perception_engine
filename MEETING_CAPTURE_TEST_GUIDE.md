# Meeting Detection + Audio Capture Test Guide

## Overview

This test verifies that we can:
1. ✅ Detect when you join a Teams/Zoom meeting (via MicrophoneMonitor)
2. ✅ Automatically capture and transcribe meeting audio (via AudioCaptureEngine)
3. ✅ Verify that system audio loopback captures the meeting participants' speech

## What This Test Does

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Wait for Meeting Detection                               │
│    └─> Polls MicrophoneMonitor every 2 seconds             │
│    └─> Looking for: ms-teams.exe ACTIVE on microphone      │
│                                                              │
│ 2. When Meeting Detected                                    │
│    └─> Starts AudioCaptureEngine.StartMeetingMode()        │
│    └─> Captures BOTH microphone + system audio (loopback)  │
│                                                              │
│ 3. Capture 30 Seconds                                       │
│    └─> Silero VAD detects speech                           │
│    └─> Whisper.cpp transcribes                             │
│    └─> Shows progress bar                                  │
│                                                              │
│ 4. Stop and Show Results                                    │
│    └─> Displays transcript preview                         │
│    └─> Saves full transcript to file                       │
│    └─> Shows performance metrics                           │
└─────────────────────────────────────────────────────────────┘
```

## Prerequisites

### Required Files
- ✅ Whisper model: `models/whisper/ggml-small.bin`
- ✅ Silero VAD model: `models/vad/silero_vad.onnx`
- ✅ ONNX Runtime DLLs in PATH or same directory

### Test Environment
- Windows 10/11
- Teams or Zoom meeting with **people actively speaking**
- Microphone enabled (you're in the meeting)
- Speakers/headphones enabled (you can hear participants)

## Build Instructions

### Step 1: Open Developer Command Prompt

```powershell
# Find and run:
"Developer Command Prompt for VS 2022"

# Or manually load environment:
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### Step 2: Navigate to Project

```powershell
cd "c:\Users\hc001\OneDrive - Lenovo\Desktop\perception_engine\windows_code"
```

### Step 3: Build the Test

```powershell
build_test_meeting_capture.bat
```

**Expected output:**
```
========================================
Building Meeting Capture Test
========================================

✓ Visual Studio compiler found

Compiling...

[... compilation messages ...]

========================================
✓ Build SUCCESS!
========================================
```

## Running the Test

### Step 1: Join a Meeting

1. Open Teams/Zoom
2. Join a meeting
3. **Important:** Make sure people are speaking (or play audio in the meeting)
4. Verify your microphone is unmuted
5. Verify you can hear audio

### Step 2: Run the Test

```powershell
cd test_build
test_meeting_capture.exe
```

### Step 3: Expected Behavior

```
========================================
Meeting Detection + Audio Capture Test
========================================

[1/4] Initializing MicrophoneMonitor...
      ✓ MicrophoneMonitor initialized

[2/4] Initializing AudioCaptureEngine...
      ✓ AudioCaptureEngine initialized

[3/4] Waiting for meeting detection...
      (Join a Teams/Zoom meeting now)
      (Press Q to quit)

      Poll #1 [15:30:45] - No meeting app detected...
      Poll #2 [15:30:47] - No meeting app detected...
      Poll #3 [15:30:49] - ✓ MEETING APP DETECTED ON MICROPHONE!
      Detected: ms-teams.exe (PID: 19976)

[4/4] Starting meeting audio capture...
      Capturing for 30 seconds...

      ✓ Audio capture started (Meeting ID: meeting_20251026_153049)

      Progress: [========================================] 100% (30s)

Stopping meeting mode...

========================================
Capture Complete!
========================================

✓ Transcript generated!

Preview (first 500 chars):
─────────────────────────────────────
[Transcript text will appear here...]
─────────────────────────────────────

Full transcript length: 1234 characters
Saved to: meetings/meeting_20251026_153049_transcript.txt

Performance Metrics:
  Average transcription latency: 450 ms
  Total segments processed: 12
  Using GPU: Yes

========================================
Test Complete
========================================
```

## Interpreting Results

### ✅ Success Case: Transcript Contains Meeting Audio

If you see transcript text with meeting participants' speech:
```
✓ Transcript generated!

Preview:
─────────────────────────────────────
so I think we should proceed with the
implementation as discussed yesterday
the timeline looks reasonable and we
can start next week...
─────────────────────────────────────
```

**This means:**
- ✅ Meeting detection works (microphone ACTIVE)
- ✅ System audio loopback captures meeting audio
- ✅ Whisper transcription works
- ✅ Ready to integrate into PerceptionEngine!

### ❌ Failure Case: Empty Transcript

If you see:
```
⚠ No transcript generated
  Possible reasons:
  - No speech detected during capture
  - Microphone level too low
  - Whisper model failed to load
  - System audio loopback not capturing meeting audio
```

**Troubleshooting steps:**

1. **Check log file:** `test_build/test_meeting_capture.log`
   - Look for: "VAD detected speech" messages
   - Look for: "Transcription complete" messages

2. **Verify audio is playing:**
   - Open Windows Volume Mixer (search "volume mixer")
   - While in meeting, confirm audio bars are moving
   - Check that your speaker device is set as default

3. **Test microphone capture:**
   - The test captures BOTH mic + system audio
   - If only mic works, transcript will only have your voice
   - This still proves the system works!

4. **Check Whisper model:**
   - Verify model exists: `ls ../models/whisper/ggml-small.bin`
   - Model should be ~465 MB
   - If missing, re-run setup.bat

### ⚠️ Partial Success: Only Your Voice in Transcript

If transcript only has **your own speech** (not meeting participants):

**This means:**
- ✅ Meeting detection works
- ✅ Microphone capture works
- ❌ System audio loopback NOT capturing meeting audio

**Why this happens:**
- Teams might be using a different audio device
- Communications device vs Default device mismatch
- Virtual audio driver issues

**Solution:**
We may need to:
1. Enumerate ALL playback devices (not just default)
2. Check which device Teams is using
3. Capture from the correct device

## What We Learn From This Test

### Test Scenario 1: Full Transcript (Your Voice + Others)
**Result:** System works perfectly! ✓
**Next step:** Integrate into PerceptionEngine automatic monitoring

### Test Scenario 2: Only Your Voice
**Result:** Mic works, speaker loopback has issues
**Next step:** Fix device enumeration to find Teams audio

### Test Scenario 3: No Transcript at All
**Result:** Deeper investigation needed
**Next step:** Check logs for Whisper/VAD errors

## Next Steps After Testing

### If Test Succeeds ✓

We can proceed to integrate into PerceptionEngine:

```cpp
// In PerceptionEngine main loop (every 5 seconds):

if (micMonitor.IsMeetingAppUsingMicrophone()) {
    if (!audioEngine.IsInMeetingMode()) {
        // Start automatic recording
        audioEngine.StartMeetingMode(GenerateMeetingId());
        LOG_INFO("Auto-started meeting recording");
    }
} else {
    if (audioEngine.IsInMeetingMode()) {
        // Stop recording, meeting ended
        std::string transcriptPath = audioEngine.StopMeetingMode();
        LOG_INFO_FMT("Meeting ended - transcript: %s", transcriptPath.c_str());

        // Add transcript to context collector
        contextCollector.UpdateMeetingTranscript(transcriptPath);
    }
}
```

### If Test Fails ✗

We need to debug:
1. Check which audio device Teams is actually using
2. Verify system audio loopback is enabled in Windows
3. Test with different meeting apps (Zoom vs Teams)
4. Check if virtual audio drivers interfere

## Files Created by Test

- `test_build/test_meeting_capture.exe` - The test executable
- `test_build/test_meeting_capture.log` - Debug log
- `meetings/meeting_YYYYMMDD_HHMMSS_transcript.txt` - Full transcript
- Console output - Real-time progress and results

## Troubleshooting

### Build Errors

**Error: "whisper.lib not found"**
```powershell
# Rebuild whisper.cpp
cd third-party/whisper.cpp
mkdir build_cuda
cd build_cuda
cmake .. -DGGML_CUDA=ON
cmake --build . --config Release
```

**Error: "onnxruntime.lib not found"**
```powershell
# Check if ONNX Runtime is present
ls third-party/onnxruntime/lib/onnxruntime.lib

# If missing, download from:
# https://github.com/microsoft/onnxruntime/releases
```

### Runtime Errors

**Error: "Failed to initialize AudioCaptureEngine"**
- Check whisper model path: `models/whisper/ggml-small.bin`
- Check VAD model path: `models/vad/silero_vad.onnx`
- Run setup.bat to download models

**Error: "No meeting app detected"**
- Verify Teams/Zoom is running
- Verify microphone is unmuted in meeting
- Check device permissions (Windows Privacy Settings)

## Summary

This test answers the critical question:

> **Can we automatically detect Teams meetings and capture the audio for transcription?**

Run this test and share the results. The output will tell us exactly what works and what needs fixing! 🎤🔊✨
