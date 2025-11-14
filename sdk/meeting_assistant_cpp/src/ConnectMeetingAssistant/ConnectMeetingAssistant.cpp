#include "ConnectMeetingAssistant.h"
#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

// Global state
static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static OnMeetingStatusChangedCallback g_callback = nullptr;
static std::atomic<bool> g_running(false);
static std::thread g_listenerThread;

// DLL entry point - ensure cleanup on unload
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_DETACH:
        // Force cleanup before DLL unloads to prevent std::thread destructor crash
        if (g_running || g_listenerThread.joinable()) {
            std::cout << "[ConnectMA] DLL unloading - forcing cleanup\n";
            DisconnectMeetingAssistant();
        }
        break;
    }
    return TRUE;
}

// Pipe name (must match server)
static constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\MeetingAssistantPipe";

// Listener thread function
void PipeListenerThread() {
    char buffer[4096];
    DWORD bytesRead;

    while (g_running) {
        BOOL success = ReadFile(
            g_hPipe,
            buffer,
            sizeof(buffer) - 1,
            &bytesRead,
            NULL
        );

        if (!success || bytesRead == 0) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                std::cout << "[ConnectMA] Server disconnected\n";
            }
            break;
        }

        buffer[bytesRead] = '\0';
        std::string message(buffer);
        
        std::cout << "[ConnectMA] Received message: " << message << "\n";

        // Parse message: "EVENT|appName|processId"
        // EVENT: 1=Started, 2=Ended
        size_t pos1 = message.find('|');
        size_t pos2 = message.find('|', pos1 + 1);
        
        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            std::cout << "[ConnectMA] Invalid message format\n";
            continue;
        }

        std::string eventStr = message.substr(0, pos1);
        std::string appName = message.substr(pos1 + 1, pos2 - pos1 - 1);
        std::string pidStr = message.substr(pos2 + 1);

        MeetingStatus status = (eventStr == "1") ? MEETING_STATUS_STARTED : MEETING_STATUS_ENDED;
        unsigned long processId = std::stoul(pidStr);

        std::cout << "[ConnectMA] Event: " << (status == MEETING_STATUS_STARTED ? "STARTED" : "ENDED") 
                  << ", App: " << appName << ", PID: " << processId << "\n";

        // Call user callback
        UserDecision decision = USER_DECISION_DECLINE;
        if (g_callback != nullptr) {
            decision = g_callback(status, appName.c_str(), processId);
            std::cout << "[ConnectMA] User decision: " << (decision == USER_DECISION_ACCEPT ? "ACCEPT" : "DECLINE") << "\n";
        }
    }

    g_running = false;
}

CONNECT_API int ConnectMeetingAssistant(OnMeetingStatusChangedCallback callback) {
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        std::cout << "[ConnectMA] Already connected\n";
        return 0; // Already connected
    }

    if (callback == nullptr) {
        std::cout << "[ConnectMA] Callback is NULL\n";
        return -1; // Invalid callback
    }

    g_callback = callback;

    std::cout << "[ConnectMA] Connecting to pipe: " << PIPE_NAME << "\n";

    // Try to connect to the pipe
    while (true) {
        g_hPipe = CreateFileW(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (g_hPipe != INVALID_HANDLE_VALUE) {
            break; // Connected successfully
        }

        DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY) {
            std::cout << "[ConnectMA] Failed to connect. Error: " << error << "\n";
            return -2; // Connection failed
        }

        // Pipe is busy, wait and retry
        std::cout << "[ConnectMA] Pipe is busy, waiting...\n";
        if (!WaitNamedPipeW(PIPE_NAME, 5000)) {
            std::cout << "[ConnectMA] Timeout waiting for pipe\n";
            return -3; // Timeout
        }
    }

    std::cout << "[ConnectMA] Connected to MeetingAssistant!\n";

    // Set pipe to message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL success = SetNamedPipeHandleState(g_hPipe, &mode, NULL, NULL);
    if (!success) {
        std::cout << "[ConnectMA] Failed to set pipe mode. Error: " << GetLastError() << "\n";
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
        return -4;
    }

    // Start listener thread
    g_running = true;
    g_listenerThread = std::thread(PipeListenerThread);

    return 0; // Success
}

CONNECT_API int DisconnectMeetingAssistant() {
    if (g_hPipe == INVALID_HANDLE_VALUE && !g_listenerThread.joinable()) {
        return 0; // Not connected
    }

    std::cout << "[ConnectMA] Disconnecting...\n";

    // Signal thread to stop
    g_running = false;

    // Close pipe handle first - this will unblock ReadFile in the thread
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
    }

    // Wait for thread to finish
    if (g_listenerThread.joinable()) {
        g_listenerThread.join();
    }

    g_callback = nullptr;

    std::cout << "[ConnectMA] Disconnected\n";
    return 0;
}

CONNECT_API int IsConnected() {
    return (g_hPipe != INVALID_HANDLE_VALUE && g_running) ? 1 : 0;
}
