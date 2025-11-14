#pragma once

#include <vector>
#include "common/Types.h"
#include "layer0/DataIngestion.h"

namespace perception {
namespace layer1 {

// Engagement calculator - calculates engagement scores from raw events
class EngagementCalculator {
public:
    EngagementCalculator() = default;
    
    // Calculate engagement metrics from raw events
    EngagementMetrics calculate(const std::vector<layer0::RawEvent>& rawEvents) const;
    
private:
    // Parse mouse events to detect interactions
    void analyzeMouseEvents(
        const std::vector<MouseEvent>& mouseEvents,
        bool& hasCopied,
        bool& hasSelected,
        int& copiedCount,
        int& selectionCount
    ) const;
    
    // Calculate base engagement score
    double calculateScore(
        bool hasCopied,
        bool hasSelected,
        int totalInteractions,
        int totalDwellTime,
        int copiedCount
    ) const;
};

} // namespace layer1
} // namespace perception
