// MicrophoneMonitorDLL.cpp
// C-compatible wrapper implementation for MicrophoneMonitor

#include "MicrophoneMonitorDLL.h"
#include "MicrophoneMonitor.h"
#include "Logger.h"
#include <string.h>

// Internal structure to track last detected meeting
struct MicrophoneMonitorWrapper {
    MicrophoneMonitor* monitor;
    std::string lastDetectedApp;
    DWORD lastDetectedPID;
};

extern "C" {

DLL_EXPORT MicrophoneMonitorHandle MicrophoneMonitor_Create() {
    try {
        // Initialize logger if not already initialized
        static bool loggerInitialized = false;
        if (!loggerInitialized) {
            Logger::GetInstance().Initialize("MicrophoneMonitor.log", LogLevel::INFO_L);
            loggerInitialized = true;
        }

        auto* wrapper = new MicrophoneMonitorWrapper();
        wrapper->monitor = new MicrophoneMonitor();
        wrapper->lastDetectedPID = 0;

        return static_cast<MicrophoneMonitorHandle>(wrapper);
    }
    catch (...) {
        return nullptr;
    }
}

DLL_EXPORT void MicrophoneMonitor_Destroy(MicrophoneMonitorHandle handle) {
    if (!handle) return;

    auto* wrapper = static_cast<MicrophoneMonitorWrapper*>(handle);
    delete wrapper->monitor;
    delete wrapper;
}

DLL_EXPORT int MicrophoneMonitor_IsMeetingAppUsingMicrophone(MicrophoneMonitorHandle handle) {
    if (!handle) return 0;

    auto* wrapper = static_cast<MicrophoneMonitorWrapper*>(handle);

    // Get all active microphone sessions
    auto sessions = wrapper->monitor->GetActiveMicrophoneSessions();

    // Check if any session is a meeting app
    for (const auto& session : sessions) {
        if (MicrophoneMonitor::IsMeetingApp(session.processName)) {
            // Cache the detected app info
            wrapper->lastDetectedApp = session.processName;
            wrapper->lastDetectedPID = session.processId;
            return 1;  // Meeting app detected
        }
    }

    // No meeting app detected
    wrapper->lastDetectedApp.clear();
    wrapper->lastDetectedPID = 0;
    return 0;
}

DLL_EXPORT int MicrophoneMonitor_GetMeetingAppName(MicrophoneMonitorHandle handle, char* buffer, int bufferSize) {
    if (!handle || !buffer || bufferSize <= 0) return 0;

    auto* wrapper = static_cast<MicrophoneMonitorWrapper*>(handle);

    if (wrapper->lastDetectedApp.empty()) {
        buffer[0] = '\0';
        return 0;
    }

    // Copy app name to buffer
    size_t copyLen = (std::min)(static_cast<size_t>(bufferSize - 1), wrapper->lastDetectedApp.length());
    strncpy_s(buffer, bufferSize, wrapper->lastDetectedApp.c_str(), copyLen);
    buffer[copyLen] = '\0';

    return static_cast<int>(copyLen);
}

DLL_EXPORT unsigned long MicrophoneMonitor_GetMeetingAppPID(MicrophoneMonitorHandle handle) {
    if (!handle) return 0;

    auto* wrapper = static_cast<MicrophoneMonitorWrapper*>(handle);
    return wrapper->lastDetectedPID;
}

} // extern "C"
