#pragma once

#include "MicrophoneMonitor.h"
#include <string>
#include <chrono>
#include <functional>

/**
 * MeetingInfo - Represents a detected meeting
 */
struct MeetingInfo {
    std::string meetingId;       // UUID
    std::string appName;         // "Zoom.exe", "Teams.exe", etc.
    std::string appDisplayName;  // "Zoom Meeting", etc.
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    bool isActive;
    int durationSeconds;         // Calculated when meeting ends
};

/**
 * Callback types for meeting events
 */
using MeetingStartCallback = std::function<void(const MeetingInfo&)>;
using MeetingEndCallback = std::function<void(const MeetingInfo&)>;

/**
 * MeetingDetector - Intelligent meeting detection using audio device monitoring
 *
 * Detection Strategy (Notion's approach):
 * 1. Monitor which apps are using microphone + speakers
 * 2. Check if app is a known meeting app (Zoom, Teams, Meet, etc.)
 * 3. Apply heuristics to reduce false positives:
 *    - Bidirectional audio (both mic + speaker active)
 *    - Duration threshold (>2 minutes = likely a meeting)
 *    - Meeting app priority
 *
 * False Positive Mitigation:
 * - Voice memos: mic only, no speaker → filtered
 * - Speech-to-text: mic only, short duration → filtered
 * - Games: not in meeting app list → filtered
 * - Music playback: speaker only → filtered
 *
 * Usage:
 *   MeetingDetector detector;
 *   detector.SetMeetingStartCallback([](const MeetingInfo& meeting) {
 *       // Start recording, show notification
 *   });
 *   detector.SetMeetingEndCallback([](const MeetingInfo& meeting) {
 *       // Stop recording, generate summary
 *   });
 *
 *   // In main loop (every 5 seconds):
 *   detector.CheckForMeeting();
 */
class MeetingDetector {
public:
    MeetingDetector();
    ~MeetingDetector();

    /**
     * Check for active meetings (call this periodically, e.g., every 5 seconds)
     * Returns: pointer to current meeting if active, nullptr otherwise
     */
    MeetingInfo* CheckForMeeting();

    /**
     * Get current active meeting (if any)
     */
    MeetingInfo* GetCurrentMeeting() { return currentMeeting; }

    /**
     * Set callbacks for meeting start/end events
     */
    void SetMeetingStartCallback(MeetingStartCallback callback);
    void SetMeetingEndCallback(MeetingEndCallback callback);

    /**
     * Configuration: set timeout for meeting end detection
     * Default: 60 seconds (if no meeting signals for 60s, meeting has ended)
     */
    void SetMeetingEndTimeout(int seconds) { meetingEndTimeoutSeconds = seconds; }

    /**
     * Configuration: set minimum duration to consider as meeting
     * Default: 120 seconds (2 minutes)
     */
    void SetMinimumMeetingDuration(int seconds) { minimumMeetingDurationSeconds = seconds; }

private:
    // Core detection logic
    bool DetectMeetingActivity();
    bool IsLikelyMeeting(const AudioSession& micSession);
    bool HasBidirectionalAudio(const std::string& processName);
    bool HasMeetingEnded();

    // Generate unique meeting ID (UUID)
    std::string GenerateUUID();

    // Trigger callbacks
    void OnMeetingStarted(const MeetingInfo& meeting);
    void OnMeetingEnded(const MeetingInfo& meeting);

    // Members
    MicrophoneMonitor micMonitor;
    MeetingInfo* currentMeeting;

    // Timing tracking
    std::chrono::steady_clock::time_point lastMeetingSignalTime;
    std::chrono::steady_clock::time_point lastCheckTime;

    // Configuration
    int meetingEndTimeoutSeconds;        // Default: 60 seconds
    int minimumMeetingDurationSeconds;   // Default: 120 seconds (2 minutes)

    // Callbacks
    MeetingStartCallback meetingStartCallback;
    MeetingEndCallback meetingEndCallback;
};
