#pragma once
#include <string>
#include <chrono>
#include <optional>

namespace MeetingAssistant {

enum class MeetingState {
    Idle,              // No meeting detected
    Detected,          // Meeting detected, notification shown
    WaitingForUser,    // User saw notification, waiting for action
    PayingAttention,   // User confirmed, "pay attention" active
    Dismissed          // User dismissed notification
};

struct MeetingInfo {
    std::string appName;
    unsigned long processId;
    std::chrono::system_clock::time_point detectedAt;

    std::string ToString() const;
};

class MeetingStateMachine {
public:
    MeetingStateMachine();

    // Update state based on meeting detection status
    // Returns true if state changed
    bool Update(bool meetingDetected, const std::optional<MeetingInfo>& meetingInfo);

    // Handle user clicking "Start" on notification
    void OnUserConfirmed();

    // Handle user clicking "Dismiss" on notification
    void OnUserDismissed();

    // Reset state machine (for testing)
    void Reset();

    // Getters
    MeetingState GetCurrentState() const { return currentState_; }
    MeetingState GetLastState() const { return lastState_; }
    const std::optional<MeetingInfo>& GetCurrentMeeting() const { return currentMeeting_; }

private:
    MeetingState currentState_;
    MeetingState lastState_;
    std::optional<MeetingInfo> currentMeeting_;

    // Minimum duration filter to avoid false positives
    std::optional<std::chrono::system_clock::time_point> firstDetectedAt_;
    static constexpr int MIN_DURATION_SECONDS = 2;
};

} // namespace MeetingAssistant
