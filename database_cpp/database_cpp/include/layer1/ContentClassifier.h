#pragma once

#include <string>
#include <utility>
#include "common/Types.h"

namespace perception {
namespace layer1 {

// Content classifier - classifies content type and domain from context
class ContentClassifier {
public:
    ContentClassifier() = default;
    
    // Classify content type and domain
    std::pair<ContentType, Domain> classify(
        const std::string& appName,
        const std::optional<std::string>& url = std::nullopt,
        const std::optional<std::string>& windowTitle = std::nullopt
    ) const;
    
private:
    ContentType classifyByApp(const std::string& appName) const;
    ContentType classifyByUrl(const std::string& url) const;
    Domain classifyDomain(ContentType contentType) const;
    std::string extractUrlDomain(const std::string& url) const;
};

} // namespace layer1
} // namespace perception
