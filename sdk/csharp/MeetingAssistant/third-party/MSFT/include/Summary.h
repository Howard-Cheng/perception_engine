#pragma once
// ChatAPIClient.h
#ifndef CHAT_API_CLIENT_H
#define CHAT_API_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
// 流式响应回调函数类型
using StreamCallback = std::function<void(const std::string& content, bool isComplete)>;
namespace ChatAPI {
    // 消息结构体
    struct Message {
        std::string role;
        std::string content;

        std::string toJson() const;
        static Message fromJson(const std::string& json);
    };

    // API请求参数
    struct ChatRequest {
        std::string model;
        std::vector<Message> messages;
        int maxCompletionTokens = 1000;
        double temperature = 1.0;

        std::string toJson() const;
        static ChatRequest fromJson(const std::string& json);
    };

    

    // ChatAPI客户端类
    class ChatAPIClient {
    public:
        // 构造函数，接收API域名
        ChatAPIClient(const std::string& domain);

        // 析构函数
        ~ChatAPIClient();

        // 发送chatCompletions请求
        std::string chatCompletions(const ChatRequest& request);

        // 发送chatCompletionsStream请求
        bool chatCompletionsStream(const ChatRequest& request, const StreamCallback& callback);

        // 设置认证信息（目前文档中authServiceWillBeReadySoon）
        void setAuthorization(const std::string& auth);

    private:
        std::string m_domain;
        std::string m_authorization;
        std::mutex m_mutex;

        // 私有方法：构建完整的URL
        std::string buildUrl(const std::string& endpoint) const;
    };
    // 导出库函数
    extern "C" {
        // 创建ChatAPIClient实例
        ChatAPIClient* createChatAPIClient(const char* domain);

        // 释放ChatAPIClient实例
        void destroyChatAPIClient(ChatAPIClient* client);

        // 调用chatCompletions接口
        char* chatCompletions(ChatAPIClient* client, const char* requestJson, int* resultLen);

        // 调用chatCompletionsStream接口（简化版，返回合并的内容）
        char* chatCompletionsStream(ChatAPIClient* client, const char* requestJson, int* resultLen);

        // 设置认证信息
        void setAuthorization(ChatAPIClient* client, const char* auth);
    }
    
}

#endif // CHAT_API_CLIENT_H