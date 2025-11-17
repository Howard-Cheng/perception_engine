#include "ConnectMeetingAssistant.h"
#include "Logger.h"
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
    case DLL_PROCESS_ATTACH:
        // Initialize logger when DLL loads
        Logger::GetInstance().Initialize("ConnectMeetingAssistant.log", LogLevel::DEBUG_L);
        LOG_INFO("ConnectMeetingAssistant DLL loaded");
        break;
        
    case DLL_PROCESS_DETACH:
        // Force cleanup before DLL unloads to prevent std::thread destructor crash
        if (g_running || g_listenerThread.joinable()) {
            LOG_INFO("DLL unloading - forcing cleanup");
            DisconnectMeetingAssistant();
        }
        Logger::GetInstance().Shutdown();
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

    LOG_INFO("Pipe listener thread started");

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
                LOG_WARN("Server disconnected (broken pipe)");
            } else {
                LOG_ERROR_FMT("ReadFile failed with error: %lu", GetLastError());
            }
            break;
        }

        buffer[bytesRead] = '\0';
        std::string message(buffer);
        
        LOG_INFO_FMT("Received message: %s", message.c_str());

        // Parse message: "EVENT|appName|processId"
        // EVENT: 1=Started, 2=Ended
        size_t pos1 = message.find('|');
        size_t pos2 = message.find('|', pos1 + 1);
        
        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            LOG_WARN("Invalid message format");
            continue;
        }

        std::string eventStr = message.substr(0, pos1);
        std::string appName = message.substr(pos1 + 1, pos2 - pos1 - 1);
        std::string pidStr = message.substr(pos2 + 1);

        MeetingStatus status = (eventStr == "1") ? MEETING_STATUS_STARTED : MEETING_STATUS_ENDED;
        unsigned long processId = std::stoul(pidStr);

        LOG_INFO_FMT("Parsed event - Status: %s, App: %s, PID: %lu",
                     (status == MEETING_STATUS_STARTED ? "STARTED" : "ENDED"),
                     appName.c_str(), processId);

        // Call user callback
        UserDecision decision = USER_DECISION_DECLINE;
        if (g_callback != nullptr) {
            decision = g_callback(status, appName.c_str(), processId);
            LOG_INFO_FMT("User callback returned decision: %s",
                        (decision == USER_DECISION_ACCEPT ? "ACCEPT" : "DECLINE"));
        } else {
            LOG_WARN("No callback registered");
        }
    }

    g_running = false;
    LOG_INFO("Pipe listener thread exiting");
}

CONNECT_API int ConnectMeetingAssistant(OnMeetingStatusChangedCallback callback) {
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        LOG_WARN("Already connected to MeetingAssistant");
        return 0; // Already connected
    }

    if (callback == nullptr) {
        LOG_ERROR("Callback is NULL");
        return -1; // Invalid callback
    }

    g_callback = callback;

    LOG_INFO("Connecting to named pipe...");

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
            LOG_ERROR_FMT("Failed to connect to pipe. Error: %lu", error);
            return -2; // Connection failed
        }

        // Pipe is busy, wait and retry
        LOG_DEBUG("Pipe is busy, waiting...");
        if (!WaitNamedPipeW(PIPE_NAME, 5000)) {
            LOG_ERROR("Timeout waiting for pipe");
            return -3; // Timeout
        }
    }

    LOG_INFO("Successfully connected to MeetingAssistant pipe");

    // Set pipe to message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL success = SetNamedPipeHandleState(g_hPipe, &mode, NULL, NULL);
    if (!success) {
        DWORD error = GetLastError();
        LOG_ERROR_FMT("Failed to set pipe mode. Error: %lu", error);
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
        return -4;
    }

    // Start listener thread
    g_running = true;
    g_listenerThread = std::thread(PipeListenerThread);
    LOG_INFO("Listener thread started");

    return 0; // Success
}

CONNECT_API int DisconnectMeetingAssistant() {
    if (g_hPipe == INVALID_HANDLE_VALUE && !g_listenerThread.joinable()) {
        LOG_DEBUG("Not connected, nothing to disconnect");
        return 0; // Not connected
    }

    LOG_INFO("Disconnecting from MeetingAssistant...");

    // Signal thread to stop
    g_running = false;

    // Close pipe handle first - this will unblock ReadFile in the thread
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
        LOG_DEBUG("Pipe handle closed");
    }

    // Wait for thread to finish
    if (g_listenerThread.joinable()) {
        LOG_DEBUG("Waiting for listener thread to join...");
        g_listenerThread.join();
        LOG_DEBUG("Listener thread joined successfully");
    }

    g_callback = nullptr;

    LOG_INFO("Disconnected from MeetingAssistant");
    return 0;
}

CONNECT_API int IsConnected() {
    return (g_hPipe != INVALID_HANDLE_VALUE && g_running) ? 1 : 0;
}
