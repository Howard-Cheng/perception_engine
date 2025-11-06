#include "NamedPipeServer.h"
#include <iostream>
#include <sstream>

namespace MeetingAssistant {

NamedPipeServer::NamedPipeServer()
    : running_(false)
    , hPipe_(INVALID_HANDLE_VALUE)
    , hClientPipe_(INVALID_HANDLE_VALUE)
    , clientConnected_(false) {
}

NamedPipeServer::~NamedPipeServer() {
    Stop();
}

bool NamedPipeServer::Start() {
    if (running_) {
        std::cout << "[PipeServer] Already running\n";
        return true;
    }

    running_ = true;
    serverThread_ = std::thread(&NamedPipeServer::ServerThread, this);
    
    std::cout << "[PipeServer] Started on pipe: " << PIPE_NAME << "\n";
    return true;
}

void NamedPipeServer::Stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Close client pipe if connected
    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        if (hClientPipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hClientPipe_);
            hClientPipe_ = INVALID_HANDLE_VALUE;
        }
        clientConnected_ = false;
    }

    // Close server pipe
    if (hPipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    std::cout << "[PipeServer] Stopped\n";
}

void NamedPipeServer::ServerThread() {
    while (running_) {
        // Create named pipe
        hPipe_ = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, // Max instances
            4096, // Output buffer size
            4096, // Input buffer size
            0, // Default timeout
            NULL
        );

        if (hPipe_ == INVALID_HANDLE_VALUE) {
            std::cout << "[PipeServer] Failed to create pipe. Error: " << GetLastError() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        std::cout << "[PipeServer] Waiting for client connection...\n";

        // Wait for client to connect
        BOOL connected = ConnectNamedPipe(hPipe_, NULL);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            std::cout << "[PipeServer] ConnectNamedPipe failed. Error: " << GetLastError() << "\n";
            CloseHandle(hPipe_);
            hPipe_ = INVALID_HANDLE_VALUE;
            continue;
        }

        std::cout << "[PipeServer] Client connected!\n";

        // Store client pipe handle
        {
            std::lock_guard<std::mutex> lock(clientMutex_);
            hClientPipe_ = hPipe_;
            clientConnected_ = true;
        }

        // Wait for client to disconnect (monitor pipe state)
        // NOTE: Removed HandleClient() - no longer continuously reading
        // Server only writes when NotifyMeetingEvent is called
        MonitorClientConnection(hPipe_);

        // Client disconnected
        {
            std::lock_guard<std::mutex> lock(clientMutex_);
            if (hClientPipe_ == hPipe_) {
                hClientPipe_ = INVALID_HANDLE_VALUE;
                clientConnected_ = false;
            }
        }

        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
        
        std::cout << "[PipeServer] Client disconnected\n";
    }
}

void NamedPipeServer::MonitorClientConnection(HANDLE hPipe) {
    // Simply monitor the pipe state without reading
    // This allows NotifyMeetingEvent to write when needed
    char buffer[1];
    DWORD bytesRead;
    
    while (running_) {
        // Peek at the pipe to check if client is still connected
        DWORD bytesAvailable = 0;
        BOOL peekResult = PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, NULL);
        
        if (!peekResult) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                std::cout << "[PipeServer] Client disconnected (pipe broken)\n";
                break;
            }
        }
        
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void NamedPipeServer::HandleClient(HANDLE hPipe) {
    // DEPRECATED: This function is no longer used
    // Keeping it for backward compatibility but it should not be called
}

UserResponse NamedPipeServer::NotifyMeetingEvent(MeetingEvent event, const std::string& appName, unsigned long processId) {
    std::lock_guard<std::mutex> lock(clientMutex_);

    if (!clientConnected_ || hClientPipe_ == INVALID_HANDLE_VALUE) {
        std::cout << "[PipeServer] No client connected, cannot notify\n";
        return UserResponse::NoResponse;
    }

    // Build message: "EVENT|appName|processId"
    // EVENT: 1=Started, 2=Ended
    std::ostringstream oss;
    oss << (event == MeetingEvent::Started ? "1" : "2") << "|" 
        << appName << "|" 
        << processId;
    
    std::string message = oss.str();
    
    std::cout << "[PipeServer] Notifying client: " << message << "\n";

    // Send message to client
    DWORD bytesWritten;
    BOOL success = WriteFile(
        hClientPipe_,
        message.c_str(),
        (DWORD)message.length(),
        &bytesWritten,
        NULL
    );

    if (!success) {
        DWORD error = GetLastError();
        std::cout << "[PipeServer] Failed to send message. Error: " << error << "\n";
        
        // If pipe is broken, mark as disconnected
        if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
            clientConnected_ = false;
        }
        return UserResponse::NoResponse;
    }

    // Flush the pipe to ensure message is sent
    FlushFileBuffers(hClientPipe_);
    
    std::cout << "[PipeServer] Message sent, waiting for response...\n";

    // Wait for response from client with timeout
    char buffer[4096];
    DWORD bytesRead;
    
    // Set a timeout for reading response (5 seconds)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;         // Max time between bytes
    timeouts.ReadTotalTimeoutConstant = 5000;  // Total timeout: 5 seconds
    timeouts.ReadTotalTimeoutMultiplier = 0;
    
    // Note: SetCommTimeouts doesn't work for named pipes, we'll use PeekNamedPipe instead
    
    // Wait for response with timeout
    auto startTime = std::chrono::steady_clock::now();
    const int timeoutMs = 5000; // 5 seconds
    
    while (true) {
        // Check if data is available
        DWORD bytesAvailable = 0;
        BOOL peekResult = PeekNamedPipe(hClientPipe_, NULL, 0, NULL, &bytesAvailable, NULL);
        
        if (!peekResult) {
            DWORD error = GetLastError();
            std::cout << "[PipeServer] PeekNamedPipe failed. Error: " << error << "\n";
            if (error == ERROR_BROKEN_PIPE) {
                clientConnected_ = false;
            }
            return UserResponse::NoResponse;
        }
        
        if (bytesAvailable > 0) {
            // Data is available, read it
            success = ReadFile(
                hClientPipe_,
                buffer,
                sizeof(buffer) - 1,
                &bytesRead,
                NULL
            );
            
            if (!success || bytesRead == 0) {
                std::cout << "[PipeServer] Failed to read response. Error: " << GetLastError() << "\n";
                return UserResponse::NoResponse;
            }
            
            buffer[bytesRead] = '\0';
            std::string response(buffer);
            
            std::cout << "[PipeServer] Received response: " << response << "\n";

            // Parse response: "1" = Accept, "2" = Decline
            if (response == "1") {
                return UserResponse::Accept;
            } else if (response == "2") {
                return UserResponse::Decline;
            }
            
            return UserResponse::NoResponse;
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();
        
        if (elapsed >= timeoutMs) {
            std::cout << "[PipeServer] Timeout waiting for response\n";
            return UserResponse::NoResponse;
        }
        
        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace MeetingAssistant
