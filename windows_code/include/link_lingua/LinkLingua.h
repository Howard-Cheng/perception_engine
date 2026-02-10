#pragma once
#include "link_lingua/link_lingua_export.h"
#include <string>
#include <cstdint>
#include <vector>

namespace link_lingua {

using MessageCallback = void(*)(const std::string& message);
using CommandId = uint32_t;

// ============================================================================
// Command IDs for communication protocol (moved from LinguaClientImpl.h)
// ============================================================================
enum LinguaCommand : uint32_t {
    // Request commands (client -> server)
    kLinguaCommandPing = 0x0001,
    kLinguaCommandGetStatus = 0x0002,
    kLinguaCommandProcessText = 0x0003,
    kLinguaCommandGetVersion = 0x0004,
    kLinguaCommandShutdown = 0x0005,
    
    // Input events (fire-and-forget, no response needed)
    kLinguaCommandSendMouseEvent = 0x0100,
    kLinguaCommandSendKeyboardEvent = 0x0101,
    kLinguaCommandSendInputEvent = 0x0102,
    
    // Response commands (server -> client)
    kLinguaCommandPingComplete = 0x1001,
    kLinguaCommandGetStatusComplete = 0x1002,
    kLinguaCommandProcessTextComplete = 0x1003,
    kLinguaCommandGetVersionComplete = 0x1004,
    kLinguaCommandShutdownComplete = 0x1005,
    
    // Async notifications (server -> client, no wait)
    kLinguaCommandOnError = 0x2001,
    kLinguaCommandOnStatusChanged = 0x2002,
};

// Forward declarations
class LinguaClientImpl;

/**
 * @brief Client for communicating with LinguaCore.exe via anonymous pipes
 * Similar to CaptureClientWin design pattern
 */
class LINK_LINGUA_API LinguaClient {
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
