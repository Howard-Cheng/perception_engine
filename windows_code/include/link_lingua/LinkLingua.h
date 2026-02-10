#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace link_lingua {

using MessageCallback = void(*)(const std::string& message);
using CommandId = uint32_t;

// Forward declarations
class LinguaClientImpl;
enum LinguaCommand : uint32_t;

/**
 * @brief Client for communicating with LinguaCore.exe via anonymous pipes
 * Similar to CaptureClientWin design pattern
 */
class LinguaClient {
public:
    LinguaClient();
    ~LinguaClient();

    // Lifecycle management
    bool Start(const std::wstring& linguaExePath);
    void Stop();
    bool IsRunning() const;

    // Text-based messaging (line-based protocol)
    bool SendMessage(const std::string& message);
    bool ReceiveMessage(std::string& out, unsigned int timeoutMs = 2000);
    void SetMessageCallback(MessageCallback cb);

    // ========== Synchronous API (like CaptureClientWin) ==========
    
    // Fire-and-forget commands (no response needed)
    void SendMouseEvent(const void* eventData, size_t dataSize);
    void SendKeyboardEvent(const void* eventData, size_t dataSize);
    void SendInputEvent(const void* eventData, size_t dataSize);
    
    // Synchronous commands (wait for response)
    bool Ping();
    bool GetStatus(void* statusOut, size_t maxSize, size_t* actualSize = nullptr);
    bool ProcessText(const char* text, void* resultOut, size_t maxSize, size_t* actualSize = nullptr);
    bool GetVersion(char* versionOut, size_t maxSize);
    void Shutdown();

    // Error handling
    std::string GetLastError() const;

private:
    LinguaClientImpl* impl_;

    // Non-copyable
    LinguaClient(const LinguaClient&) = delete;
    LinguaClient& operator=(const LinguaClient&) = delete;
};

} // namespace link_lingua
