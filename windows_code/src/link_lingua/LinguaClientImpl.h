#pragma once
#include "link_lingua/LinkLingua.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

// Undefine Windows macros that conflict with our code
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef GetLastError
#undef GetLastError
#endif

#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

namespace link_lingua {

// Command IDs for communication protocol
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

class LinguaClientImpl {
public:
    LinguaClientImpl();
    ~LinguaClientImpl();

    bool Start(const std::wstring& exePath);
    void Stop();
    bool IsRunning() const;

    // Text-based messaging
    bool SendMessage(const std::string& message);
    bool ReceiveMessage(std::string& out, unsigned int timeoutMs);
    void SetMessageCallback(MessageCallback cb);

    // Binary command API
    void SendCommand(LinguaCommand cmd, const void* data, size_t dataSize);
    
    // Synchronous call mechanism (like CaptureClientWin)
    bool WaitForCall();
    void CallReturn();
    
    // Query last response
    LinguaCommand GetLastResponseCommand() const;
    const std::vector<uint8_t>& GetLastResponsePayload() const;
    bool GetLastRetValue() const;
    size_t CopyResponseData(void* outBuffer, size_t maxBytes) const;
    
    std::string GetLastError() const;

    // Notify data received (called from reader thread)
    void NotifyReceiveData(const std::vector<uint8_t>& data);

private:
    bool ReadExact(void* buf, size_t size);
    bool WriteToPipe(const uint8_t* data, size_t size);
    void ReaderLoop();
    void CloseHandlePair();
    
    // Response handlers
    void ProcessBoolResponse(const std::vector<uint8_t>& buffer);
    void ProcessDataResponse(const std::vector<uint8_t>& buffer);

private:
    HANDLE stdoutRead_ = nullptr;
    HANDLE stdoutWrite_ = nullptr;
    HANDLE stdinRead_ = nullptr;
    HANDLE stdinWrite_ = nullptr;

    HANDLE childStdout_ = nullptr;
    HANDLE childStdin_ = nullptr;

    HANDLE processHandle_ = nullptr;
    HANDLE threadHandle_ = nullptr;

    std::thread readerThread_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;

    // Text message queue
    std::queue<std::string> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;

    MessageCallback callback_ = nullptr;
    mutable std::mutex callbackMutex_;

    mutable std::mutex mutex_;
    std::string lastError_;

    // Synchronous call state (like CaptureClientWin)
    mutable std::mutex callMutex_;
    std::condition_variable callCV_;
    bool callReturned_ = false;
    
    // Response data
    LinguaCommand lastResponseCommand_ = static_cast<LinguaCommand>(0);
    std::vector<uint8_t> lastResponsePayload_;
    bool retValue_ = false;
};

} // namespace link_lingua
