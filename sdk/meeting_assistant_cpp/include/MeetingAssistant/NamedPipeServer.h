#pragma once
#include <Windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

namespace MeetingAssistant {

// Meeting event types
enum class MeetingEvent {
    Started,
    Ended
};

// User response types
enum class UserResponse {
    Accept,
    Decline,
    NoResponse
};

// Callback function type for getting user decision
using MeetingEventCallback = std::function<UserResponse(MeetingEvent event, const std::string& appName, unsigned long processId)>;

class NamedPipeServer {
public:
    NamedPipeServer();
    ~NamedPipeServer();

    // Start the named pipe server
    bool Start();

    // Stop the named pipe server
    void Stop();

    // Check if server is running
    bool IsRunning() const { return running_; }

    // Notify connected clients about meeting status change
    // Returns the user's decision (Accept/Decline/NoResponse)
    UserResponse NotifyMeetingEvent(MeetingEvent event, const std::string& appName, unsigned long processId);

private:
    // Pipe name
    static constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\MeetingAssistantPipe";

    // Server thread function
    void ServerThread();

    // Monitor client connection state (non-blocking)
    void MonitorClientConnection(HANDLE hPipe);

    // Handle client connection (DEPRECATED - kept for compatibility)
    void HandleClient(HANDLE hPipe);

    // Process client message (not currently used)
    UserResponse ProcessMessage(const std::string& message);

    std::atomic<bool> running_;
    std::thread serverThread_;
    HANDLE hPipe_;
    
    // Client connection tracking
    std::mutex clientMutex_;
    HANDLE hClientPipe_;
    bool clientConnected_;
};

} // namespace MeetingAssistant
