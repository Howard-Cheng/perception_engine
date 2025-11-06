#include "MeetingStateMachine.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace MeetingAssistant {

std::string MeetingInfo::ToString() const {
    std::time_t time = std::chrono::system_clock::to_time_t(detectedAt);
    std::tm tm;
    localtime_s(&tm, &time);
    
    std::ostringstream oss;
    oss << appName << " (PID: " << processId << ") - Detected at "
        << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

MeetingStateMachine::MeetingStateMachine()
    : currentState_(MeetingState::Idle)
    , lastState_(MeetingState::Idle)
    , firstDetectedAt_(std::nullopt) {
}

bool MeetingStateMachine::Update(bool meetingDetected, const std::optional<MeetingInfo>& meetingInfo) {
    lastState_ = currentState_;

    switch (currentState_) {
        case MeetingState::Idle: {
            if (meetingDetected && meetingInfo.has_value()) {
                // First time detecting this meeting
                if (!firstDetectedAt_.has_value()) {
                    firstDetectedAt_ = std::chrono::system_clock::now();
                } else {
                    // Check if meeting has been active for minimum duration
                    auto now = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - firstDetectedAt_.value());
                    
                    if (duration.count() >= MIN_DURATION_SECONDS) {
                        // NEW MEETING CONFIRMED
                        currentState_ = MeetingState::Detected;
                        currentMeeting_ = meetingInfo;
                        firstDetectedAt_ = std::nullopt; // Reset for next detection
                    }
                }
            } else {
                // Meeting no longer detected, reset timer
                if (firstDetectedAt_.has_value()) {
                    auto now = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - firstDetectedAt_.value());
                    std::cout << "[State] Meeting ended before " << MIN_DURATION_SECONDS 
                              << "s threshold (was " << duration.count() << "s) - likely false positive\n";
                    firstDetectedAt_ = std::nullopt;
                }
            }
            break;
        }

        case MeetingState::Detected: {
            if (!meetingDetected) {
                // Meeting ended before user responded
                currentState_ = MeetingState::Idle;
                currentMeeting_ = std::nullopt;
                std::cout << "[State] Detected ¡ú Idle (meeting ended)\n";
            }
            break;
        }

        case MeetingState::WaitingForUser:
        case MeetingState::PayingAttention:
        case MeetingState::Dismissed: {
            if (!meetingDetected) {
                // Meeting ended
                std::cout << "[State] " << static_cast<int>(lastState_) << " ¡ú Idle (meeting ended)\n";
                currentState_ = MeetingState::Idle;
                currentMeeting_ = std::nullopt;
            }
            break;
        }
    }

    return lastState_ != currentState_;
}

void MeetingStateMachine::OnUserConfirmed() {
    if (currentState_ == MeetingState::Detected || currentState_ == MeetingState::WaitingForUser) {
        auto oldState = currentState_;
        currentState_ = MeetingState::PayingAttention;
        std::cout << "[State] " << static_cast<int>(oldState) << " ¡ú PayingAttention\n";
    }
}

void MeetingStateMachine::OnUserDismissed() {
    if (currentState_ == MeetingState::Detected || currentState_ == MeetingState::WaitingForUser) {
        auto oldState = currentState_;
        currentState_ = MeetingState::Dismissed;
        std::cout << "[State] " << static_cast<int>(oldState) << " ¡ú Dismissed\n";
    }
}

void MeetingStateMachine::Reset() {
    currentState_ = MeetingState::Idle;
    currentMeeting_ = std::nullopt;
    std::cout << "[State] Reset to Idle\n";
}

} // namespace MeetingAssistant
