#include "layer1/EngagementCalculator.h"
#include "common/Logger.h"
#include "layer0/ElasticsearchClient.h"
#include <algorithm>

namespace perception {
namespace layer1 {

EngagementMetrics EngagementCalculator::calculate(
    const std::vector<layer0::RawEvent>& rawEvents
) const {
    
    EngagementMetrics metrics;
    metrics.engagementScore = 0.0;
    metrics.interactionCount = 0;
    metrics.totalDwellTime = 0;
    metrics.hasCopied = false;
    metrics.hasSelected = false;
    metrics.copiedCount = 0;
    metrics.selectionCount = 0;
    
    if (rawEvents.empty()) {
        return metrics;
    }
    
    // Accumulate interaction counts and dwell time
    for (const auto& event : rawEvents) {
        metrics.interactionCount += event.interactionCount;
        metrics.totalDwellTime += event.dwellTimeSeconds;
        
        // Analyze mouse events
        bool eventHasCopy = false;
        bool eventHasSelection = false;
        int eventCopyCount = 0;
        int eventSelectionCount = 0;
        
        analyzeMouseEvents(
            event.mouseEvents,
            eventHasCopy,
            eventHasSelection,
            eventCopyCount,
            eventSelectionCount
        );
        
        if (eventHasCopy) metrics.hasCopied = true;
        if (eventHasSelection) metrics.hasSelected = true;
        metrics.copiedCount += eventCopyCount;
        metrics.selectionCount += eventSelectionCount;
    }
    
    // Calculate engagement score
    metrics.engagementScore = calculateScore(
        metrics.hasCopied,
        metrics.hasSelected,
        metrics.interactionCount,
        metrics.totalDwellTime,
        metrics.copiedCount
    );
    
    return metrics;
}

void EngagementCalculator::analyzeMouseEvents(
    const std::vector<MouseEvent>& mouseEvents,
    bool& hasCopied,
    bool& hasSelected,
    int& copiedCount,
    int& selectionCount
) const {
    
    hasCopied = false;
    hasSelected = false;
    copiedCount = 0;
    selectionCount = 0;
    
    for (const auto& mouseEvent : mouseEvents) {
        const std::string& eventType = mouseEvent.eventType;
        
        // Check for copy events
        if (eventType.find("Copy") != std::string::npos ||
            eventType.find("copy") != std::string::npos ||
            eventType.find("COPY") != std::string::npos) {
            hasCopied = true;
            copiedCount++;
        }
        
        // Check for selection events
        if (eventType.find("Selection") != std::string::npos ||
            eventType.find("selection") != std::string::npos ||
            eventType.find("TextSelection") != std::string::npos ||
            eventType.find("Select") != std::string::npos) {
            hasSelected = true;
            selectionCount++;
        }
    }
}

double EngagementCalculator::calculateScore(
    bool hasCopied,
    bool hasSelected,
    int totalInteractions,
    int totalDwellTime,
    int copiedCount
) const {
    
    double score = 0.0;
    
    // Copied content is the strongest signal (0.4)
    if (hasCopied) {
        score += 0.4;
    }
    
    // Selected text (0.2)
    if (hasSelected) {
        score += 0.2;
    }
    
    // High interaction count (0.2)
    if (totalInteractions > 5) {
        score += 0.2;
    }
    
    // Long dwell time (0.2) - more than 30 seconds
    if (totalDwellTime > 30) {
        score += 0.2;
    }
    
    // Boost for multiple copies (0.1)
    if (copiedCount > 3) {
        score += 0.1;
    }
    
    // Cap at 1.0
    score = std::min(score, 1.0);
    
    return score;
}

} // namespace layer1
} // namespace perception
