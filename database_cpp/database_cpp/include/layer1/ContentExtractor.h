#pragma once

#include "common/Types.h"
#include "layer0/DataIngestion.h"

namespace perception {
namespace layer1 {

// Content extractor - extracts high-attention content and entities
class ContentExtractor {
public:
    ContentExtractor() = default;
    
    // Extract high-attention content (copied, selected, clicked)
    HighAttentionContent extractHighAttentionContent(
        const std::vector<layer0::RawEvent>& rawEvents
    ) const;
    
    // Extract entities (numbers, dates, URLs, emails)
    ExtractedEntities extractEntities(
        const std::vector<layer0::RawEvent>& rawEvents,
        const HighAttentionContent& highAttention
    ) const;
    
    // Extract content-specific metadata
    ContentMetadata extractContentSpecificMetadata(
        ContentType contentType,
        const std::vector<layer0::RawEvent>& rawEvents
    ) const;
    
private:
    std::string combineContent(
        const std::vector<layer0::RawEvent>& rawEvents,
        const HighAttentionContent& highAttention
    ) const;
};

} // namespace layer1
} // namespace perception
