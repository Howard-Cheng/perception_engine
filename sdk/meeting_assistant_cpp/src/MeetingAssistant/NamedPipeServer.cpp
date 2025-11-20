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

    // Check for existing pipe instance
    HANDLE hTest = CreateFileW(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (hTest != INVALID_HANDLE_VALUE) {
        std::cout << "[PipeServer] WARNING: Found existing pipe instance. Cleaning up...\n";
        CloseHandle(hTest);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
    const int MAX_RETRY = 3;
    int retryCount = 0;
    
    while (running_) {
        // Create named pipe with unlimited instances to avoid ERROR_PIPE_BUSY
        hPipe_ = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, // ✅ Changed from 1 to PIPE_UNLIMITED_INSTANCES
            4096, // Output buffer size
            4096, // Input buffer size
            0, // Default timeout
            NULL
        );

        if (hPipe_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            std::cout << "[PipeServer] Failed to create pipe. Error: " << error;
            
            if (error == ERROR_PIPE_BUSY) {
                std::cout << " (ERROR_PIPE_BUSY - Another instance may be running or cleanup pending)\n";
                
                if (retryCount < MAX_RETRY) {
                    retryCount++;
                    std::cout << "[PipeServer] Retry " << retryCount << "/" << MAX_RETRY << " after 2 seconds...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                } else {
                    std::cout << "[PipeServer] Max retries reached. Exiting server thread.\n";
                    running_ = false;
                    return;
                }
            } else {
                std::cout << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }
        
        // Reset retry count on success
        retryCount = 0;

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
    
    std::cout << "[PipeServer] Message sent successfully\n";
    return UserResponse::NoResponse;
}

} // namespace MeetingAssistant
