#include "MeetingDetector.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <random>

MeetingDetector::MeetingDetector()
    : currentMeeting(nullptr)
    , lastMeetingSignalTime(std::chrono::steady_clock::now())
    , lastCheckTime(std::chrono::steady_clock::now())
    , meetingEndTimeoutSeconds(60)        // 60 seconds timeout
    , minimumMeetingDurationSeconds(120)  // 2 minutes minimum
    , meetingStartCallback(nullptr)
    , meetingEndCallback(nullptr)
{
    LOG_INFO("MeetingDetector initialized");
}

MeetingDetector::~MeetingDetector() {
    if (currentMeeting) {
        delete currentMeeting;
        currentMeeting = nullptr;
    }
}

void MeetingDetector::SetMeetingStartCallback(MeetingStartCallback callback) {
    meetingStartCallback = callback;
}

void MeetingDetector::SetMeetingEndCallback(MeetingEndCallback callback) {
    meetingEndCallback = callback;
}

MeetingInfo* MeetingDetector::CheckForMeeting() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime);

    // Check every 5 seconds
    if (elapsed.count() < 5) {
        return currentMeeting;
    }

    lastCheckTime = now;

    // Detect meeting activity
    bool meetingDetected = DetectMeetingActivity();

    if (meetingDetected) {
        // Meeting is active - update signal time
        lastMeetingSignalTime = now;

        if (!currentMeeting) {
            // New meeting started!
            // Get the meeting app details
            auto micSessions = micMonitor.GetActiveMicrophoneSessions();

            for (const auto& session : micSessions) {
                if (MicrophoneMonitor::IsMeetingApp(session.processName) &&
                    IsLikelyMeeting(session)) {

                    currentMeeting = new MeetingInfo{
                        GenerateUUID(),
                        session.processName,
                        session.displayName,
                        std::chrono::system_clock::now(),
                        std::chrono::system_clock::time_point(),  // endTime (will be set later)
                        true,
                        0  // durationSeconds (will be calculated on end)
                    };

                    LOG_INFO_FMT("Meeting detected: %s (%s)",
                                currentMeeting->appName.c_str(),
                                currentMeeting->meetingId.c_str());

                    OnMeetingStarted(*currentMeeting);
                    break;
                }
            }
        }

        return currentMeeting;
    }

    // No meeting detected - check if current meeting has ended
    if (currentMeeting && HasMeetingEnded()) {
        LOG_INFO_FMT("Meeting ended: %s (duration: %d seconds)",
                    currentMeeting->appName.c_str(),
                    currentMeeting->durationSeconds);

        // Calculate duration
        currentMeeting->endTime = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            currentMeeting->endTime - currentMeeting->startTime);
        currentMeeting->durationSeconds = static_cast<int>(duration.count());
        currentMeeting->isActive = false;

        OnMeetingEnded(*currentMeeting);

        // Clean up
        MeetingInfo* endedMeeting = currentMeeting;
        currentMeeting = nullptr;

        return endedMeeting;
    }

    return currentMeeting;
}

bool MeetingDetector::DetectMeetingActivity() {
    // Get active microphone sessions
    auto micSessions = micMonitor.GetActiveMicrophoneSessions();

    // Check if any meeting app is using microphone
    for (const auto& session : micSessions) {
        if (MicrophoneMonitor::IsMeetingApp(session.processName)) {
            // Meeting app using microphone - apply heuristics
            if (IsLikelyMeeting(session)) {
                return true;
            }
        }
    }

    return false;
}

bool MeetingDetector::IsLikelyMeeting(const AudioSession& micSession) {
    // Heuristic 1: Known meeting app in priority list
    // (Already filtered by IsMeetingApp check in caller)

    // Heuristic 2: Bidirectional audio (both mic + speaker active)
    // This filters out voice memos, speech-to-text, etc.
    if (HasBidirectionalAudio(micSession.processName)) {
        LOG_DEBUG_FMT("Bidirectional audio detected for %s - likely a meeting",
                     micSession.processName.c_str());
        return true;
    }

    // Heuristic 3: If we already have a meeting active, be less strict
    // (user might have muted mic temporarily)
    if (currentMeeting) {
        // Allow mic-only if meeting already active (user might be muted)
        return true;
    }

    // Heuristic 4: Check duration for new sessions
    // For new potential meetings, require some duration before confirming
    // (This will be checked on subsequent polls, not first detection)
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - micSession.startTime);

    if (duration.count() < 30) {
        // Too short - might be testing audio, voice memo, etc.
        LOG_DEBUG_FMT("Audio session < 30s for %s - waiting for duration confirmation",
                     micSession.processName.c_str());
        return false;
    }

    // If we reach here: meeting app + sufficient duration but no bidirectional audio yet
    // Could be: user joined but hasn't unmuted, or one-way webinar
    // For safety, require bidirectional audio for initial detection
    LOG_DEBUG_FMT("No bidirectional audio for %s - not confirming as meeting yet",
                 micSession.processName.c_str());
    return false;
}

bool MeetingDetector::HasBidirectionalAudio(const std::string& processName) {
    // Check if the same app is also using speakers
    auto speakerSessions = micMonitor.GetActiveSpeakerSessions();

    for (const auto& session : speakerSessions) {
        if (session.processName == processName) {
            return true;
        }
    }

    return false;
}

bool MeetingDetector::HasMeetingEnded() {
    if (!currentMeeting) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastMeetingSignalTime);

    // Check if timeout exceeded
    if (elapsed.count() >= meetingEndTimeoutSeconds) {
        // Check if meeting duration meets minimum threshold
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - currentMeeting->startTime);

        if (duration.count() < minimumMeetingDurationSeconds) {
            // Too short - probably a false positive (audio test, etc.)
            LOG_INFO_FMT("Meeting duration too short (%d seconds), discarding",
                        duration.count());
            return true;  // End it, but won't trigger full meeting end processing
        }

        return true;
    }

    return false;
}

std::string MeetingDetector::GenerateUUID() {
    // Simple UUID generation (UUID v4 format)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 8; i++) ss << std::setw(1) << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::setw(1) << dis(gen);
    ss << "-4";  // Version 4
    for (int i = 0; i < 3; i++) ss << std::setw(1) << dis(gen);
    ss << "-";
    ss << std::setw(1) << (dis(gen) & 0x3 | 0x8);  // Variant bits
    for (int i = 0; i < 3; i++) ss << std::setw(1) << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << std::setw(1) << dis(gen);

    return ss.str();
}

void MeetingDetector::OnMeetingStarted(const MeetingInfo& meeting) {
    // Log meeting start
    LOG_INFO_FMT("=== MEETING STARTED ===");
    LOG_INFO_FMT("  ID: %s", meeting.meetingId.c_str());
    LOG_INFO_FMT("  App: %s", meeting.appName.c_str());
    LOG_INFO_FMT("  Display Name: %s", meeting.appDisplayName.c_str());

    // Trigger callback
    if (meetingStartCallback) {
        meetingStartCallback(meeting);
    }
}

void MeetingDetector::OnMeetingEnded(const MeetingInfo& meeting) {
    // Log meeting end
    LOG_INFO_FMT("=== MEETING ENDED ===");
    LOG_INFO_FMT("  ID: %s", meeting.meetingId.c_str());
    LOG_INFO_FMT("  App: %s", meeting.appName.c_str());
    LOG_INFO_FMT("  Duration: %d seconds (%d minutes)",
                meeting.durationSeconds,
                meeting.durationSeconds / 60);

    // Trigger callback
    if (meetingEndCallback) {
        meetingEndCallback(meeting);
    }
}
