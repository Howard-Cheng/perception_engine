#pragma once

#include <vector>
#include <memory>
#include "common/Types.h"
#include "common/DatabaseConfig.h"
#include "layer0/DataIngestion.h"

namespace perception {
namespace layer1 {

// Session structure for Layer 1
class Session {
public:
    SessionId sessionId;
    DeviceId deviceId;
    
    // Session boundaries
    Timestamp startTime;
    Timestamp endTime;
    int durationSeconds;
    
    // Classification
    std::string appName;
    ContentType contentType;
    Domain domain;
    std::optional<std::string> subdomain;
    
    // Engagement metrics
    EngagementMetrics engagement;
    
    // Session hierarchy
    std::optional<SessionId> workSessionId;
    std::optional<SessionId> daySessionId;
    
    Timestamp createdAt;
    
    Session();
};

// Session detector - detects interaction sessions from raw events
class SessionDetector {
public:
    explicit SessionDetector(const SessionConfig& config);
    
    // Detect interaction session boundaries from raw events
    std::vector<std::vector<layer0::RawEvent>> detectInteractionSessions(
        const std::vector<layer0::RawEvent>& rawEvents
    );
    
    // Detect work sessions from interaction sessions
    std::vector<std::vector<Session>> detectWorkSessions(
        const std::vector<Session>& interactionSessions
    );
    
private:
    bool shouldBreakSession(
        const layer0::RawEvent& prevEvent,
        const layer0::RawEvent& currEvent
    ) const;
    
    double calculateSessionDuration(
        const std::vector<layer0::RawEvent>& events
    ) const;
    
    double calculateEntityOverlap(
        const Session& session1,
        const Session& session2
    ) const;
    
    SessionConfig config_;
};

} // namespace layer1
} // namespace perception
