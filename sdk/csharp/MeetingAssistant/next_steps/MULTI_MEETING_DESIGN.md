# Multi-Meeting Scenario Design

## Problem Statement

User has multiple meeting scenarios:

### Scenario 1: Two Meeting Apps, Different Purposes
```
User's Computer:
├─ Teams Meeting (work standup) → Want Qira to transcribe
└─ Zoom Meeting (podcast interview) → User listening live, no transcription
```

**Challenge:** How to let user choose which meeting to transcribe?

### Scenario 2: Simultaneous Meetings
```
User's Computer:
├─ Teams Meeting (project team) → Qira transcribing
└─ Zoom Meeting (client call) → Qira transcribing (2nd instance)
```

**Challenge:** Can we run multiple Pay Attention sessions simultaneously?

### Scenario 3: Sequential Meetings
```
Timeline:
10:00 AM - Teams Meeting (standup) → Qira transcribing
10:30 AM - Zoom Meeting (1-on-1) → Auto-switch Qira to new meeting?
11:00 AM - Teams Meeting (design review) → Back to Teams
```

**Challenge:** Auto-switch vs manual control?

---

## Current Limitation

**Single Meeting Tracking:**
```csharp
class MeetingStateMachine {
    MeetingInfo? CurrentMeeting;  // ❌ Only ONE meeting tracked
}
```

When multiple meeting apps use microphone:
- Detects first match only
- User cannot choose which to transcribe
- Second meeting silently ignored

---

## Solution: Multi-Meeting Orchestration

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│  MeetingOrchestrator                                    │
│  ┌────────────────────────────────────────────────┐    │
│  │  Active Meetings (List)                        │    │
│  │  ┌──────────────┐  ┌──────────────┐            │    │
│  │  │ Teams        │  │ Zoom         │            │    │
│  │  │ PID: 19976   │  │ PID: 23451   │            │    │
│  │  │ State: IDLE  │  │ State: IDLE  │            │    │
│  │  └──────────────┘  └──────────────┘            │    │
│  └────────────────────────────────────────────────┘    │
│                                                         │
│  ┌────────────────────────────────────────────────┐    │
│  │  Pay Attention Sessions                        │    │
│  │  ┌──────────────┐  ┌──────────────┐            │    │
│  │  │ Session #1   │  │ Session #2   │            │    │
│  │  │ → Teams      │  │ → Zoom       │            │    │
│  │  │ Status: ON   │  │ Status: OFF  │            │    │
│  │  └──────────────┘  └──────────────┘            │    │
│  └────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

### Data Model

```csharp
class MeetingInfo {
    string AppName;          // e.g., "ms-teams.exe"
    uint ProcessId;
    DateTime DetectedAt;
    string WindowTitle;      // e.g., "Standup | Microsoft Teams"
    MeetingState State;      // DETECTED, TRANSCRIBING, LISTENING_ONLY, ENDED
}

class PayAttentionSession {
    string SessionId;        // Unique ID
    MeetingInfo Meeting;     // Which meeting this session is for
    SessionStatus Status;    // ACTIVE, PAUSED, STOPPED
    DateTime StartedAt;
    DateTime? EndedAt;
}

class MeetingOrchestrator {
    List<MeetingInfo> ActiveMeetings;          // All detected meetings
    List<PayAttentionSession> TranscriptionSessions;  // Active transcriptions

    // Max concurrent sessions (configurable)
    int MaxConcurrentSessions = 2;
}
```

---

## User Flows

### Flow 1: Single Meeting (Current Behavior)

```
User joins Teams
     ↓
Detect Teams meeting
     ↓
Show notification: "Want Qira to pay attention to Teams meeting?"
     │
     ├─ [Yes] → Start transcription
     └─ [No] → Dismiss
```

**No changes needed - works as is.**

---

### Flow 2: Two Meetings - User Choice

```
User's State:
├─ Teams: ACTIVE (detected)
└─ Zoom: ACTIVE (detected)

Notification (NEW - Multi-Select UI):
┌────────────────────────────────────────────┐
│ Multiple meetings detected!                │
│                                            │
│ Select which meeting(s) Qira should        │
│ pay attention to:                          │
│                                            │
│ □ Teams Meeting                            │
│   "Standup | Microsoft Teams"              │
│   [Started 2 min ago]                      │
│                                            │
│ ☑ Zoom Meeting                             │
│   "Client Call with ABC Corp"              │
│   [Started 5 min ago]                      │
│                                            │
│ [Start Transcription]  [Dismiss]           │
└────────────────────────────────────────────┘
```

**Implementation:**
```csharp
void ShowMultiMeetingNotification(List<MeetingInfo> meetings) {
    var notification = new MultiSelectNotification {
        Title = "Multiple meetings detected!",
        Message = "Select which meeting(s) Qira should pay attention to:",
        Options = meetings.Select(m => new NotificationOption {
            Text = $"{m.AppName} - {m.WindowTitle}",
            Value = m.ProcessId.ToString(),
            Checked = false  // User selects
        }).ToList(),
        Buttons = new[] {
            new Button("Start Transcription", OnStartSelected),
            new Button("Dismiss", OnDismiss)
        }
    };

    notification.Show();
}

void OnStartSelected(List<string> selectedPids) {
    foreach (var pidStr in selectedPids) {
        var meeting = ActiveMeetings.Find(m => m.ProcessId.ToString() == pidStr);
        StartTranscription(meeting);
    }
}
```

---

### Flow 3: Sequential Auto-Switch (Smart)

```
Timeline:
10:00 AM - User in Teams meeting, Qira transcribing
     ↓
10:30 AM - Teams meeting ends (mic inactive)
     ↓
10:31 AM - User joins Zoom meeting (new mic active)
     ↓
Decision Point: Auto-switch or ask?

Option A: Auto-Switch (Aggressive)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Notification:
┌────────────────────────────────────────────┐
│ Switched to new meeting                    │
│ Qira is now paying attention to:           │
│ Zoom Meeting - "1-on-1 with Manager"       │
│                                            │
│ [Stop]  [Undo]                             │
└────────────────────────────────────────────┘

Option B: Ask User (Conservative) - RECOMMENDED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Notification:
┌────────────────────────────────────────────┐
│ New meeting detected                       │
│ Previous: Teams Meeting (ended)            │
│ New: Zoom Meeting                          │
│                                            │
│ Continue transcription to new meeting?     │
│                                            │
│ [Yes, Continue]  [No, Stop]                │
└────────────────────────────────────────────┘
```

**Implementation:**
```csharp
void OnMeetingEnded(MeetingInfo oldMeeting) {
    // Stop transcription for ended meeting
    StopTranscription(oldMeeting);

    // Check if new meeting detected within 5 minutes
    var newMeeting = ActiveMeetings
        .Where(m => m.DetectedAt > oldMeeting.EndedAt)
        .Where(m => m.DetectedAt - oldMeeting.EndedAt < TimeSpan.FromMinutes(5))
        .FirstOrDefault();

    if (newMeeting != null) {
        // Ask user if they want to continue to new meeting
        ShowContinueToNewMeetingNotification(oldMeeting, newMeeting);
    }
}
```

---

### Flow 4: Simultaneous Dual Transcription

```
User's Intent:
├─ Teams Meeting (internal) → Transcribe for notes
└─ Zoom Webinar (external) → Transcribe for reference

Notification:
┌────────────────────────────────────────────┐
│ Starting dual transcription                │
│                                            │
│ Session 1: Teams Meeting                   │
│ Session 2: Zoom Webinar                    │
│                                            │
│ Note: This will use 2x resources.         │
│ Battery impact: ~15% per hour              │
│                                            │
│ [Continue]  [Cancel]                       │
└────────────────────────────────────────────┘
```

**Pay Attention SDK Constraint Check:**
```csharp
bool CanStartNewSession(MeetingInfo meeting) {
    // Check if Pay Attention SDK supports multiple sessions
    if (!PayAttentionSDK.SupportsMultipleSessions) {
        ShowErrorNotification(
            "Cannot start second session",
            "Pay Attention SDK does not support simultaneous transcriptions. " +
            "Please stop the first session before starting a new one."
        );
        return false;
    }

    // Check resource limits
    if (TranscriptionSessions.Count >= MaxConcurrentSessions) {
        ShowErrorNotification(
            $"Maximum {MaxConcurrentSessions} concurrent sessions reached",
            "Please stop an existing session before starting a new one."
        );
        return false;
    }

    // Check system resources (CPU, memory)
    if (GetCPUUsage() > 80% || GetMemoryUsage() > 90%) {
        ShowWarningNotification(
            "High system resource usage",
            "Starting additional transcription may impact performance. Continue anyway?",
            onConfirm: () => StartTranscription(meeting)
        );
        return false;  // Wait for user confirmation
    }

    return true;
}
```

---

## UI Design - Meeting Management

### System Tray Icon (Windows Notification Area)

```
┌────────────────────────────────┐
│ 🎯 Qira Meeting Assistant      │
├────────────────────────────────┤
│ Active Transcriptions:         │
│                                │
│ 📹 Teams Meeting               │
│    "Standup | Microsoft Teams" │
│    [Started 15 min ago]        │
│    [⏸ Pause] [⏹ Stop]          │
│                                │
│ 📹 Zoom Meeting                │
│    "Client Call"               │
│    [Started 5 min ago]         │
│    [⏸ Pause] [⏹ Stop]          │
│                                │
├────────────────────────────────┤
│ Other Active Meetings:         │
│                                │
│ 📞 Slack Call (listening)      │
│    [▶ Start Transcription]     │
│                                │
├────────────────────────────────┤
│ ⚙ Settings                     │
│ ℹ About                        │
│ ❌ Quit                         │
└────────────────────────────────┘
```

### Dashboard (Optional Web UI)

```
┌─────────────────────────────────────────────────────┐
│ Qira Meeting Assistant - Dashboard                 │
├─────────────────────────────────────────────────────┤
│                                                     │
│ Active Meetings                                     │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━    │
│                                                     │
│ ┌─────────────────────────────────────────────┐   │
│ │ Teams Meeting                                │   │
│ │ "Weekly Standup"                             │   │
│ │                                              │   │
│ │ Status: 🔴 Transcribing                      │   │
│ │ Started: 10:00 AM (23 minutes ago)           │   │
│ │ Participants: 12                             │   │
│ │                                              │   │
│ │ [⏸ Pause] [⏹ Stop] [📋 View Transcript]     │   │
│ └─────────────────────────────────────────────┘   │
│                                                     │
│ ┌─────────────────────────────────────────────┐   │
│ │ Zoom Webinar                                 │   │
│ │ "Product Launch Event"                       │   │
│ │                                              │   │
│ │ Status: 👂 Listening Only                    │   │
│ │ Started: 10:15 AM (8 minutes ago)            │   │
│ │ Participants: 150                            │   │
│ │                                              │   │
│ │ [▶ Start Transcription]                      │   │
│ └─────────────────────────────────────────────┘   │
│                                                     │
│ Meeting History (Today)                            │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━    │
│                                                     │
│ 9:00 AM - 9:30 AM                                  │
│ Teams: "1-on-1 with Manager" ✅ Transcribed        │
│ [📋 View Transcript] [📥 Download]                 │
│                                                     │
│ 8:00 AM - 8:15 AM                                  │
│ Zoom: "Quick Sync" ✅ Transcribed                  │
│ [📋 View Transcript] [📥 Download]                 │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## Implementation Strategy

### Phase 1: Multi-Meeting Detection (1 week)

**Goal:** Detect multiple concurrent meetings

```csharp
class MeetingOrchestrator {
    List<MeetingInfo> ActiveMeetings = new();

    void Update() {
        // Detect all active meeting apps (not just first)
        var detectedMeetings = detector.GetAllActiveMeetings();

        // Add new meetings
        foreach (var meeting in detectedMeetings) {
            if (!ActiveMeetings.Any(m => m.ProcessId == meeting.ProcessId)) {
                ActiveMeetings.Add(meeting);
                OnMeetingDetected(meeting);
            }
        }

        // Remove ended meetings
        var endedMeetings = ActiveMeetings
            .Where(m => !detectedMeetings.Any(d => d.ProcessId == m.ProcessId))
            .ToList();

        foreach (var meeting in endedMeetings) {
            ActiveMeetings.Remove(meeting);
            OnMeetingEnded(meeting);
        }
    }
}
```

**Notification Logic:**
```csharp
void OnMeetingDetected(MeetingInfo newMeeting) {
    if (ActiveMeetings.Count == 1) {
        // Single meeting - simple notification (current behavior)
        ShowSingleMeetingNotification(newMeeting);
    }
    else if (ActiveMeetings.Count > 1) {
        // Multiple meetings - show selection UI
        ShowMultiMeetingNotification(ActiveMeetings);
    }
}
```

---

### Phase 2: User Selection UI (1 week)

**Goal:** Let user choose which meeting(s) to transcribe

**Windows Toast with Buttons:**
```csharp
void ShowMultiMeetingNotification(List<MeetingInfo> meetings) {
    var toast = new ToastContentBuilder()
        .AddText("Multiple meetings detected!")
        .AddText($"Detected {meetings.Count} active meetings");

    // Add button for each meeting
    foreach (var meeting in meetings) {
        toast.AddButton(new ToastButton()
            .SetContent($"Transcribe {meeting.AppName}")
            .AddArgument("action", "start")
            .AddArgument("pid", meeting.ProcessId.ToString()));
    }

    toast.AddButton(new ToastButton()
        .SetContent("Dismiss All")
        .AddArgument("action", "dismiss"));

    toast.Show();
}
```

**Alternative: Richer UI (WinUI 3 Dialog):**
```csharp
class MeetingSelectionDialog : Window {
    List<MeetingInfo> Meetings;
    List<CheckBox> Checkboxes;

    void ShowDialog() {
        // Create checkbox for each meeting
        foreach (var meeting in Meetings) {
            var checkbox = new CheckBox {
                Content = $"{meeting.AppName} - {meeting.WindowTitle}",
                Tag = meeting
            };
            Checkboxes.Add(checkbox);
        }

        // Show dialog
        var result = await this.ShowAsync();

        if (result == DialogResult.OK) {
            var selected = Checkboxes
                .Where(cb => cb.IsChecked == true)
                .Select(cb => (MeetingInfo)cb.Tag)
                .ToList();

            foreach (var meeting in selected) {
                StartTranscription(meeting);
            }
        }
    }
}
```

---

### Phase 3: Simultaneous Transcription (2 weeks)

**Goal:** Support multiple concurrent Pay Attention sessions

**Depends on Pay Attention SDK capability:**

**Option A: SDK Supports Multiple Sessions**
```csharp
// Each meeting gets its own session
var session1 = await PayAttentionClient.StartTranscriptionAsync(teamsMeeting);
var session2 = await PayAttentionClient.StartTranscriptionAsync(zoomMeeting);

// Track both
TranscriptionSessions.Add(new PayAttentionSession {
    SessionId = session1.Id,
    Meeting = teamsMeeting,
    Status = SessionStatus.ACTIVE
});

TranscriptionSessions.Add(new PayAttentionSession {
    SessionId = session2.Id,
    Meeting = zoomMeeting,
    Status = SessionStatus.ACTIVE
});
```

**Option B: SDK Only Supports One Session (Workaround)**
```csharp
// Serialize: Stop current before starting new
await PayAttentionClient.StopTranscriptionAsync(currentSession);
var newSession = await PayAttentionClient.StartTranscriptionAsync(newMeeting);

// Warn user
ShowWarning("Cannot transcribe multiple meetings simultaneously. " +
            "Stopping Teams transcription to start Zoom transcription.");
```

---

### Phase 4: Smart Auto-Switching (1 week)

**Goal:** Intelligently switch between sequential meetings

**Decision Logic:**
```csharp
void OnMeetingEnded(MeetingInfo endedMeeting) {
    // Check if this meeting had active transcription
    var session = TranscriptionSessions.Find(s => s.Meeting.ProcessId == endedMeeting.ProcessId);
    if (session == null) return;  // Wasn't transcribing

    // Stop transcription
    await PayAttentionClient.StopTranscriptionAsync(session.SessionId);
    TranscriptionSessions.Remove(session);

    // Check for new meeting within 5 minutes
    var newMeeting = ActiveMeetings
        .Where(m => m.DetectedAt > endedMeeting.EndedAt)
        .Where(m => (m.DetectedAt - endedMeeting.EndedAt).TotalMinutes < 5)
        .FirstOrDefault();

    if (newMeeting != null) {
        // Ask user if they want to continue
        var result = await ShowContinueDialog(endedMeeting, newMeeting);

        if (result == DialogResult.Yes) {
            StartTranscription(newMeeting);
        }
    }
}
```

**Calendar Integration (Smarter):**
```csharp
// If next calendar event is within 5 minutes, auto-suggest
var nextCalendarMeeting = await GetNextCalendarMeeting();

if (nextCalendarMeeting != null &&
    (nextCalendarMeeting.Start - DateTime.Now).TotalMinutes < 5) {

    // Pre-emptively suggest transcription for next meeting
    ShowUpcomingMeetingNotification(nextCalendarMeeting);
}
```

---

## Configuration

### Settings

```csharp
class MultiMeetingConfig {
    // Multi-meeting behavior
    bool AutoDetectMultipleMeetings = true;
    bool ShowMultiSelectNotification = true;

    // Simultaneous transcription
    int MaxConcurrentSessions = 2;  // SDK limit
    bool WarnBeforeDualTranscription = true;

    // Auto-switching
    bool EnableAutoSwitch = false;  // Default: ask user
    int AutoSwitchDelaySeconds = 5;  // Wait 5s before suggesting switch

    // Meeting priority (if auto-switch enabled)
    Dictionary<string, int> AppPriority = new() {
        { "ms-teams.exe", 3 },  // High priority (work)
        { "Zoom.exe", 2 },      // Medium priority
        { "chrome.exe", 1 }     // Low priority (browser-based)
    };
}
```

---

## Resource Considerations

### Dual Transcription Impact

**CPU:**
- Single session: ~10-15% (Pay Attention SDK)
- Dual session: ~20-30% (2x)

**Memory:**
- Single session: ~500MB
- Dual session: ~1GB

**Battery:**
- Single session: ~15% per hour
- Dual session: ~25% per hour

**Recommendation:**
- Limit to 2 concurrent sessions max
- Warn user about resource impact
- Suggest power-saving mode (transcribe primary meeting only)

---

## User Workflows - Summary

### Workflow 1: Single Meeting (Unchanged)
```
Detect → Notify → User confirms → Transcribe
```

### Workflow 2: Two Meetings - User Choice
```
Detect multiple → Show selection UI → User picks one/both → Transcribe
```

### Workflow 3: Sequential Meetings - Smart Switch
```
Meeting ends → Detect new meeting → Ask to continue → Transcribe new
```

### Workflow 4: Calendar-Aware Proactive
```
5 min before meeting → Notify user → Pre-start transcription → Ready when meeting starts
```

### Workflow 5: Dual Transcription
```
User selects both → Check SDK support → Warn about resources → Start both sessions
```

---

## Next Steps

### Immediate (This Week)
1. ✅ **Document current design** (this file)
2. **Validate with Pay Attention team:**
   - Does SDK support multiple simultaneous sessions?
   - What are resource limits?
   - API for session management?

### Short-term (Next 2 Weeks)
1. **Phase 1: Multi-meeting detection** (1 week)
   - Detect all active meetings (not just first)
   - Track list of active meetings
   - Basic notification for multiple meetings

2. **Phase 2: User selection UI** (1 week)
   - Multi-select notification
   - Start transcription for selected meeting(s)

### Mid-term (Next Month)
1. **Phase 3: Simultaneous transcription** (2 weeks)
   - Integrate with Pay Attention SDK multi-session support
   - Resource management (limit to 2 sessions)
   - Session lifecycle management

2. **Phase 4: Smart auto-switching** (1 week)
   - Detect meeting end → new meeting flow
   - Calendar integration for proactive switching
   - User preference learning

### Long-term (2-3 Months)
1. **Advanced features:**
   - Meeting priority rules
   - Auto-pause low-priority meetings
   - Transcript merging (combine multiple sessions)
   - Analytics (which meetings transcribed most)

---

## Questions for Pay Attention Team

Before implementing multi-meeting support, clarify:

1. **SDK Capability:**
   - ✅ Does SDK support multiple simultaneous transcription sessions?
   - ✅ What is the maximum number of concurrent sessions?
   - ✅ How are sessions identified/managed (session IDs)?

2. **Resource Limits:**
   - ❓ CPU/memory overhead per session?
   - ❓ Recommended max concurrent sessions?
   - ❓ Does SDK handle resource contention internally?

3. **API:**
   - ❓ How to start/stop individual sessions?
   - ❓ How to pause/resume sessions?
   - ❓ How to query active sessions?
   - ❓ Callback for session events (started, paused, ended)?

4. **Business Logic:**
   - ❓ Billing: Per session or per user?
   - ❓ Should we discourage dual transcription (cost reasons)?
   - ❓ Analytics: Track session count per user?

---

## Conclusion

**Multi-meeting support is achievable with phased approach:**

✅ **Phase 1 (1 week):** Detect multiple meetings - Easy
✅ **Phase 2 (1 week):** User selection UI - Moderate
⚠️ **Phase 3 (2 weeks):** Simultaneous transcription - Depends on SDK
✅ **Phase 4 (1 week):** Smart switching - Easy

**Your scenarios are all solvable:**
1. Two meetings, user chooses → Phase 2
2. Dual transcription → Phase 3 (SDK dependent)
3. Sequential auto-switch → Phase 4

**Recommended priority:**
1. **This week:** Validate with Pay Attention team (SDK capabilities)
2. **Next week:** Phase 1 + Phase 2 (detection + selection)
3. **Following weeks:** Phase 3 + Phase 4 (based on SDK support)

**Key success factors:**
- Pay Attention SDK multi-session support
- Clear user controls (selection, pause, stop)
- Resource management (limit concurrent sessions)
- Smart defaults (auto-switch with confirmation)
