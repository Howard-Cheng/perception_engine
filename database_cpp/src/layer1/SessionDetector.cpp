#include "layer1/SessionDetector.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <algorithm>

namespace perception {
namespace layer1 {

Session::Session()
    : durationSeconds(0)
    , contentType(ContentType::UNKNOWN)
    , domain(Domain::INTERACTION)
    , createdAt(utils::now()) {
}

SessionDetector::SessionDetector(const SessionConfig& config)
    : config_(config) {
}

std::vector<std::vector<layer0::RawEvent>> SessionDetector::detectInteractionSessions(
    const std::vector<layer0::RawEvent>& rawEvents
) {
    
    if (rawEvents.empty()) {
        return {};
    }
    
    std::vector<std::vector<layer0::RawEvent>> sessions;
    std::vector<layer0::RawEvent> currentSession;
    currentSession.push_back(rawEvents[0]);
    
    for (size_t i = 1; i < rawEvents.size(); ++i) {
        const auto& prevEvent = rawEvents[i - 1];
        const auto& currEvent = rawEvents[i];
        
        // Check if we should break the session
        if (shouldBreakSession(prevEvent, currEvent)) {
            // Save current session if it meets minimum duration
            double duration = calculateSessionDuration(currentSession);
            if (duration >= config_.idleThresholdSeconds) {
                sessions.push_back(currentSession);
            }
            
            // Start new session
            currentSession.clear();
            currentSession.push_back(currEvent);
        } else {
            // Continue current session
            currentSession.push_back(currEvent);
        }
    }
    
    // Don't forget the last session
    if (!currentSession.empty()) {
        double duration = calculateSessionDuration(currentSession);
        if (duration >= config_.idleThresholdSeconds) {
            sessions.push_back(currentSession);
        }
    }
    
    LOG_INFO("Detected " + std::to_string(sessions.size()) + 
             " interaction sessions from " + std::to_string(rawEvents.size()) + " raw events");
    
    return sessions;
}

std::vector<std::vector<Session>> SessionDetector::detectWorkSessions(
    const std::vector<Session>& interactionSessions
) {
    
    if (interactionSessions.empty()) {
        return {};
    }
    
    std::vector<std::vector<Session>> workSessions;
    std::vector<Session> currentWorkSession;
    currentWorkSession.push_back(interactionSessions[0]);
    
    for (size_t i = 1; i < interactionSessions.size(); ++i) {
        const auto& prevSession = interactionSessions[i - 1];
        const auto& currSession = interactionSessions[i];
        
        // Calculate time gap between sessions (in minutes)
        auto prevEnd = prevSession.endTime;
        auto currStart = currSession.startTime;
        auto gapSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            currStart - prevEnd).count();
        double gapMinutes = gapSeconds / 60.0;
        
        bool shouldBreak = false;
        
        // Break condition 1: Long idle gap
        if (gapMinutes >= config_.workSessionIdleThresholdMinutes) {
            shouldBreak = true;
        }
        
        // Break condition 2: Domain change
        if (prevSession.domain != currSession.domain) {
            shouldBreak = true;
        }
        
        // Break condition 3: Low entity overlap (if available)
        double overlap = calculateEntityOverlap(prevSession, currSession);
        if (overlap < config_.workSessionOverlapThreshold) {
            shouldBreak = true;
        }
        
        if (shouldBreak) {
            workSessions.push_back(currentWorkSession);
            currentWorkSession.clear();
            currentWorkSession.push_back(currSession);
        } else {
            currentWorkSession.push_back(currSession);
        }
    }
    
    // Last work session
    if (!currentWorkSession.empty()) {
        workSessions.push_back(currentWorkSession);
    }
    
    LOG_INFO("Detected " + std::to_string(workSessions.size()) + 
             " work sessions from " + std::to_string(interactionSessions.size()) + 
             " interaction sessions");
    
    return workSessions;
}

bool SessionDetector::shouldBreakSession(
    const layer0::RawEvent& prevEvent,
    const layer0::RawEvent& currEvent
) const {
    
    // Calculate time gap
    auto prevTime = prevEvent.timestamp;
    auto currTime = currEvent.timestamp;
    auto gapSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        currTime - prevTime).count();
    
    // Condition 1: Idle threshold exceeded
    if (gapSeconds >= config_.idleThresholdSeconds) {
        LOG_DEBUG("Session break: idle gap " + std::to_string(gapSeconds) + "s");
        return true;
    }
    
    // Condition 2: App change
    if (config_.appTypeChangeTriggers && prevEvent.appName != currEvent.appName) {
        LOG_DEBUG("Session break: app change " + prevEvent.appName + " -> " + currEvent.appName);
        return true;
    }
    
    // Condition 3: Content type change
    if (config_.contentTypeChangeTriggers && 
        prevEvent.contentType.has_value() && 
        currEvent.contentType.has_value() &&
        prevEvent.contentType.value() != currEvent.contentType.value()) {
        LOG_DEBUG("Session break: content type change");
        return true;
    }
    
    // Condition 4: Domain change
    if (config_.domainChangeTriggers && 
        prevEvent.domain.has_value() && 
        currEvent.domain.has_value() &&
        prevEvent.domain.value() != currEvent.domain.value()) {
        LOG_DEBUG("Session break: domain change");
        return true;
    }
    
    // Condition 5: Window/tab change (for browsers)
    if (prevEvent.windowTitle.has_value() && 
        currEvent.windowTitle.has_value() &&
        prevEvent.windowTitle.value() != currEvent.windowTitle.value()) {
        
        std::string lowerApp = utils::toLower(currEvent.appName);
        if (utils::contains(lowerApp, "chrome") || 
            utils::contains(lowerApp, "edge") || 
            utils::contains(lowerApp, "firefox")) {
            LOG_DEBUG("Session break: browser tab change");
            return true;
        }
    }
    
    return false;
}

double SessionDetector::calculateSessionDuration(
    const std::vector<layer0::RawEvent>& events
) const {
    
    if (events.empty()) {
        return 0.0;
    }
    
    auto startTime = events.front().timestamp;
    auto endTime = events.back().timestamp;
    
    // Add the last event's dwell time
    int lastDwell = events.back().dwellTimeSeconds;
    
    auto durationSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        endTime - startTime).count();
    
    return durationSeconds + lastDwell;
}

double SessionDetector::calculateEntityOverlap(
    const Session& session1,
    const Session& session2
) const {
    
    // TODO: Implement entity overlap calculation
    // This requires access to extracted entities from sessions
    // For now, return a default value indicating we can't determine overlap
    
    // If same app and similar subdomain, assume high overlap
    if (session1.appName == session2.appName && 
        session1.subdomain == session2.subdomain) {
        return 0.7;  // High overlap
    }
    
    // Different domains = low overlap
    if (session1.domain != session2.domain) {
        return 0.1;
    }
    
    // Default: moderate overlap
    return 0.5;
}

} // namespace layer1
} // namespace perception
