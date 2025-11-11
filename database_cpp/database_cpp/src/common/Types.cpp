#include "common/Types.h"

namespace perception {

std::string ContentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::EMAIL:          return "email";
        case ContentType::CHAT:           return "chat";
        case ContentType::WEB_ARTICLE:    return "web_article";
        case ContentType::WEB_PAGE:       return "web_page";
        case ContentType::CODE:           return "code";
        case ContentType::DOCUMENT:       return "document";
        case ContentType::MEETING:        return "meeting";
        case ContentType::VIDEO:          return "video";
        case ContentType::SOCIAL:         return "social";
        case ContentType::RESEARCH_PAPER: return "research_paper";
        case ContentType::UNKNOWN:        return "unknown";
        default:                          return "unknown";
    }
}

ContentType StringToContentType(const std::string& str) {
    if (str == "email") return ContentType::EMAIL;
    if (str == "chat") return ContentType::CHAT;
    if (str == "web_article") return ContentType::WEB_ARTICLE;
    if (str == "web_page") return ContentType::WEB_PAGE;
    if (str == "code") return ContentType::CODE;
    if (str == "document") return ContentType::DOCUMENT;
    if (str == "meeting") return ContentType::MEETING;
    if (str == "video") return ContentType::VIDEO;
    if (str == "social") return ContentType::SOCIAL;
    if (str == "research_paper") return ContentType::RESEARCH_PAPER;
    return ContentType::UNKNOWN;
}

std::string DomainToString(Domain domain) {
    switch (domain) {
        case Domain::SYSTEM:        return "SYSTEM";
        case Domain::WORK:          return "WORK";
        case Domain::ENTERTAINMENT: return "ENTERTAINMENT";
        case Domain::LIFE:          return "LIFE";
        case Domain::INTERACTION:   return "INTERACTION";
        default:                    return "INTERACTION";
    }
}

Domain StringToDomain(const std::string& str) {
    if (str == "SYSTEM") return Domain::SYSTEM;
    if (str == "WORK") return Domain::WORK;
    if (str == "ENTERTAINMENT") return Domain::ENTERTAINMENT;
    if (str == "LIFE") return Domain::LIFE;
    if (str == "INTERACTION") return Domain::INTERACTION;
    return Domain::INTERACTION;
}

} // namespace perception
