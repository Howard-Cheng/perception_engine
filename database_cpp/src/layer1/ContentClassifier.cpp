#include "layer1/ContentClassifier.h"
#include "common/Utils.h"
#include <map>

namespace perception {
namespace layer1 {

std::pair<ContentType, Domain> ContentClassifier::classify(
    const std::string& appName,
    const std::optional<std::string>& url,
    const std::optional<std::string>& windowTitle
) const {
    
    // First, try app-based classification
    ContentType contentType = classifyByApp(appName);
    
    // If browser and URL available, refine by URL
    if (contentType == ContentType::UNKNOWN && url.has_value()) {
        contentType = classifyByUrl(url.value());
    }
    
    // If still unknown and not browser, use default WEB_PAGE for browsers
    if (contentType == ContentType::UNKNOWN) {
        std::string lowerApp = utils::toLower(appName);
        if (utils::contains(lowerApp, "chrome") || 
            utils::contains(lowerApp, "edge") || 
            utils::contains(lowerApp, "firefox")) {
            contentType = ContentType::WEB_PAGE;
        }
    }
    
    // Classify domain based on content type
    Domain domain = classifyDomain(contentType);
    
    return {contentType, domain};
}

ContentType ContentClassifier::classifyByApp(const std::string& appName) const {
    // Map of app names to content types
    static const std::map<std::string, ContentType> appMap = {
        // Communication
        {"outlook.exe", ContentType::EMAIL},
        {"OUTLOOK.EXE", ContentType::EMAIL},
        {"thunderbird.exe", ContentType::EMAIL},
        {"Teams.exe", ContentType::CHAT},
        {"teams.exe", ContentType::CHAT},
        {"slack.exe", ContentType::CHAT},
        {"Slack.exe", ContentType::CHAT},
        {"discord.exe", ContentType::CHAT},
        {"Discord.exe", ContentType::CHAT},
        {"wechat.exe", ContentType::CHAT},
        {"WeChat.exe", ContentType::CHAT},
        
        // Development
        {"Code.exe", ContentType::CODE},
        {"code.exe", ContentType::CODE},
        {"idea64.exe", ContentType::CODE},
        {"pycharm64.exe", ContentType::CODE},
        {"devenv.exe", ContentType::CODE},
        {"VisualStudio.exe", ContentType::CODE},
        {"WindowsTerminal.exe", ContentType::CODE},
        {"cmd.exe", ContentType::CODE},
        {"powershell.exe", ContentType::CODE},
        {"vim.exe", ContentType::CODE},
        
        // Documents
        {"WINWORD.EXE", ContentType::DOCUMENT},
        {"winword.exe", ContentType::DOCUMENT},
        {"EXCEL.EXE", ContentType::DOCUMENT},
        {"excel.exe", ContentType::DOCUMENT},
        {"POWERPNT.EXE", ContentType::DOCUMENT},
        {"powerpnt.exe", ContentType::DOCUMENT},
        {"AcroRd32.exe", ContentType::DOCUMENT},
        {"Acrobat.exe", ContentType::DOCUMENT},
        {"Notion.exe", ContentType::DOCUMENT},
        {"notion.exe", ContentType::DOCUMENT},
        {"libreoffice.exe", ContentType::DOCUMENT},
        
        // Meetings
        {"Zoom.exe", ContentType::MEETING},
        {"zoom.exe", ContentType::MEETING},
        {"ms-teams.exe", ContentType::MEETING},
        {"webex.exe", ContentType::MEETING},
    };
    
    auto it = appMap.find(appName);
    if (it != appMap.end()) {
        return it->second;
    }
    
    return ContentType::UNKNOWN;
}

ContentType ContentClassifier::classifyByUrl(const std::string& url) const {
    std::string domain = extractUrlDomain(url);
    
    // Map of URL domains to content types
    static const std::map<std::string, ContentType> urlMap = {
        // Development
        {"github.com", ContentType::CODE},
        {"gitlab.com", ContentType::CODE},
        {"bitbucket.org", ContentType::CODE},
        {"stackoverflow.com", ContentType::CODE},
        {"stackexchange.com", ContentType::CODE},
        {"codepen.io", ContentType::CODE},
        
        // Social/Entertainment
        {"linkedin.com", ContentType::SOCIAL},
        {"twitter.com", ContentType::SOCIAL},
        {"x.com", ContentType::SOCIAL},
        {"facebook.com", ContentType::SOCIAL},
        {"instagram.com", ContentType::SOCIAL},
        {"reddit.com", ContentType::SOCIAL},
        {"youtube.com", ContentType::VIDEO},
        {"youtu.be", ContentType::VIDEO},
        {"netflix.com", ContentType::VIDEO},
        {"twitch.tv", ContentType::VIDEO},
        {"vimeo.com", ContentType::VIDEO},
        
        // News/Articles
        {"wsj.com", ContentType::WEB_ARTICLE},
        {"nytimes.com", ContentType::WEB_ARTICLE},
        {"washingtonpost.com", ContentType::WEB_ARTICLE},
        {"techcrunch.com", ContentType::WEB_ARTICLE},
        {"medium.com", ContentType::WEB_ARTICLE},
        {"theverge.com", ContentType::WEB_ARTICLE},
        {"arstechnica.com", ContentType::WEB_ARTICLE},
        {"bbc.com", ContentType::WEB_ARTICLE},
        {"cnn.com", ContentType::WEB_ARTICLE},
        
        // Research
        {"arxiv.org", ContentType::RESEARCH_PAPER},
        {"acm.org", ContentType::RESEARCH_PAPER},
        {"ieee.org", ContentType::RESEARCH_PAPER},
        {"scholar.google.com", ContentType::RESEARCH_PAPER},
        
        // Shopping/E-commerce
        {"amazon.com", ContentType::WEB_PAGE},
        {"ebay.com", ContentType::WEB_PAGE},
        {"jd.com", ContentType::WEB_PAGE},
        {"taobao.com", ContentType::WEB_PAGE},
        {"aliexpress.com", ContentType::WEB_PAGE},
    };
    
    // Try exact match first
    auto it = urlMap.find(domain);
    if (it != urlMap.end()) {
        return it->second;
    }
    
    // Try partial match (e.g., "news.google.com" contains "google.com")
    for (const auto& [key, value] : urlMap) {
        if (utils::contains(domain, key)) {
            return value;
        }
    }
    
    return ContentType::WEB_PAGE;  // Default for unknown URLs
}

Domain ContentClassifier::classifyDomain(ContentType contentType) const {
    static const std::map<ContentType, Domain> domainMap = {
        // WORK indicators
        {ContentType::EMAIL, Domain::WORK},
        {ContentType::CHAT, Domain::WORK},
        {ContentType::CODE, Domain::WORK},
        {ContentType::DOCUMENT, Domain::WORK},
        {ContentType::MEETING, Domain::WORK},
        {ContentType::RESEARCH_PAPER, Domain::WORK},
        
        // ENTERTAINMENT indicators
        {ContentType::VIDEO, Domain::ENTERTAINMENT},
        {ContentType::SOCIAL, Domain::ENTERTAINMENT},
        
        // Default to INTERACTION for unknown
        {ContentType::UNKNOWN, Domain::INTERACTION},
        {ContentType::WEB_PAGE, Domain::INTERACTION},
        {ContentType::WEB_ARTICLE, Domain::INTERACTION},
    };
    
    auto it = domainMap.find(contentType);
    if (it != domainMap.end()) {
        return it->second;
    }
    
    return Domain::INTERACTION;
}

std::string ContentClassifier::extractUrlDomain(const std::string& url) const {
    return utils::extractDomain(url);
}

} // namespace layer1
} // namespace perception
