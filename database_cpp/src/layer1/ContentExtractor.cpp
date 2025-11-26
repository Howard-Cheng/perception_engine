#include "layer1/ContentExtractor.h"
#include "common/Utils.h"
#include "common/Logger.h"
#include <set>
#include <algorithm>

namespace perception {
namespace layer1 {

HighAttentionContent ContentExtractor::extractHighAttentionContent(
    const std::vector<layer0::RawEvent>& rawEvents
) const {
    
    HighAttentionContent highAttention;
    
    std::vector<std::string> copiedItems;
    std::vector<std::string> selectedItems;
    std::vector<std::string> clickedItems;
    
    for (const auto& event : rawEvents) {
        for (const auto& mouseEvent : event.mouseEvents) {
            const std::string& eventType = mouseEvent.eventType;
            const std::string& content = mouseEvent.content;
            
            // Skip empty or noise content
            if (content.empty() || 
                content == "[No Content Found]" || 
                content == "[No Text Selected]") {
                continue;
            }
            
            // Categorize by event type
            if (eventType.find("Copy") != std::string::npos) {
                copiedItems.push_back(content);
            } else if (eventType.find("Selection") != std::string::npos ||
                       eventType.find("TextSelection") != std::string::npos) {
                selectedItems.push_back(content);
            } else if (eventType.find("Click") != std::string::npos ||
                       eventType.find("DoubleClick") != std::string::npos) {
                clickedItems.push_back(content);
            }
        }
    }
    
    // Deduplicate while preserving order
    highAttention.copiedContent = utils::deduplicate(copiedItems);
    highAttention.selectedText = utils::deduplicate(selectedItems);
    highAttention.clickedElements = utils::deduplicate(clickedItems);
    
    return highAttention;
}

ExtractedEntities ContentExtractor::extractEntities(
    const std::vector<layer0::RawEvent>& rawEvents,
    const HighAttentionContent& highAttention
) const {
    
    // Combine all text, prioritizing high-attention content
    std::string combinedText = combineContent(rawEvents, highAttention);
    
    ExtractedEntities entities;
    
    // Extract different entity types
    auto numbers = utils::extractNumbers(combinedText);
    auto dates = utils::extractDates(combinedText);
    auto urls = utils::extractUrls(combinedText);
    auto emails = utils::extractEmails(combinedText);
    
    // Deduplicate and limit to top 15 of each type
    entities.numbers = utils::deduplicate(numbers);
    entities.dates = utils::deduplicate(dates);
    entities.urls = utils::deduplicate(urls);
    entities.emails = utils::deduplicate(emails);
    
    if (entities.numbers.size() > 15) {
        entities.numbers.resize(15);
    }
    if (entities.dates.size() > 15) {
        entities.dates.resize(15);
    }
    if (entities.urls.size() > 15) {
        entities.urls.resize(15);
    }
    if (entities.emails.size() > 15) {
        entities.emails.resize(15);
    }
    
    return entities;
}

ContentMetadata ContentExtractor::extractContentSpecificMetadata(
    ContentType contentType,
    const std::vector<layer0::RawEvent>& rawEvents
) const {
    
    ContentMetadata metadata;
    
    if (rawEvents.empty()) {
        return metadata;
    }
    
    const auto& firstEvent = rawEvents[0];
    std::string windowTitle = firstEvent.windowTitle.value_or("");
    std::string url = firstEvent.url.value_or("");
    
    switch (contentType) {
        case ContentType::EMAIL: {
            // Parse email metadata from window title
            // Example: "RE: Q4 Budget - Outlook"
            if (!windowTitle.empty()) {
                size_t dashPos = windowTitle.find(" - ");
                if (dashPos != std::string::npos) {
                    metadata.subject = utils::trim(windowTitle.substr(0, dashPos));
                }
            }
            break;
        }
        
        case ContentType::CODE: {
            // Extract file path from window title
            // Example: "main.cpp - perception_engine - Visual Studio Code"
            if (!windowTitle.empty()) {
                auto parts = utils::split(windowTitle, '-');
                if (parts.size() >= 1) {
                    metadata.fileName = utils::trim(parts[0]);
                }
                if (parts.size() >= 2) {
                    metadata.projectName = utils::trim(parts[1]);
                }
            }
            
            // Extract repo URL if available
            if (utils::contains(url, "github.com") || utils::contains(url, "gitlab.com")) {
                metadata.repoUrl = url;
            }
            break;
        }
        
        case ContentType::DOCUMENT: {
            // Extract file name from window title
            // Example: "Q4_Report.docx - Word"
            if (!windowTitle.empty()) {
                size_t dashPos = windowTitle.find(" - ");
                if (dashPos != std::string::npos) {
                    std::string fileName = utils::trim(windowTitle.substr(0, dashPos));
                    metadata.fileName = fileName;
                    
                    // Detect document type from extension
                    if (utils::endsWith(fileName, ".docx") || utils::endsWith(fileName, ".doc")) {
                        metadata.docType = "word";
                    } else if (utils::endsWith(fileName, ".xlsx") || utils::endsWith(fileName, ".xls")) {
                        metadata.docType = "excel";
                    } else if (utils::endsWith(fileName, ".pptx") || utils::endsWith(fileName, ".ppt")) {
                        metadata.docType = "powerpoint";
                    } else if (utils::endsWith(fileName, ".pdf")) {
                        metadata.docType = "pdf";
                    }
                }
            }
            break;
        }
        
        case ContentType::WEB_ARTICLE:
        case ContentType::WEB_PAGE: {
            metadata.url = url;
            
            // Clean up window title (remove browser suffix)
            std::string cleanTitle = windowTitle;
            cleanTitle = utils::trim(cleanTitle);
            
            // Remove common browser suffixes
            const std::vector<std::string> browserSuffixes = {
                " - Google Chrome",
                " - Microsoft Edge",
                " - Mozilla Firefox",
                " - Opera"
            };
            
            for (const auto& suffix : browserSuffixes) {
                if (utils::endsWith(cleanTitle, suffix)) {
                    cleanTitle = cleanTitle.substr(0, cleanTitle.length() - suffix.length());
                }
            }
            
            metadata.title = cleanTitle;
            break;
        }
        
        case ContentType::MEETING: {
            metadata.meetingTitle = windowTitle;
            
            // Check if there are voice transcriptions
            int transcriptionCount = 0;
            for (const auto& event : rawEvents) {
                if (event.voiceTranscription.has_value() && 
                    !event.voiceTranscription->empty()) {
                    transcriptionCount++;
                }
            }
            
            if (transcriptionCount > 0) {
                metadata.additionalFields["has_transcription"] = "true";
                metadata.additionalFields["transcription_count"] = std::to_string(transcriptionCount);
            }
            break;
        }
        
        default:
            // For other types, just store basic info
            if (!windowTitle.empty()) {
                metadata.title = windowTitle;
            }
            if (!url.empty()) {
                metadata.url = url;
            }
            break;
    }
    
    return metadata;
}

std::string ContentExtractor::combineContent(
    const std::vector<layer0::RawEvent>& rawEvents,
    const HighAttentionContent& highAttention
) const {
    
    std::string combined;
    
    // Priority 1: High-attention content (copied and selected text)
    for (const auto& item : highAttention.copiedContent) {
        combined += item + " ";
    }
    for (const auto& item : highAttention.selectedText) {
        combined += item + " ";
    }
    
    // Priority 2: Screen content (limit to first 10000 chars to avoid regex timeout)
    for (const auto& event : rawEvents) {
        if (event.screenContent.has_value()) {
            combined += event.screenContent.value() + " ";
            
            // Limit total size
            if (combined.size() > 10000) {
                break;
            }
        }
    }
    
    // Truncate if still too long
    if (combined.size() > 10000) {
        combined = combined.substr(0, 10000);
    }
    
    return combined;
}

} // namespace layer1
} // namespace perception
