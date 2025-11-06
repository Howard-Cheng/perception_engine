#include "MicrophoneMonitor.h"
#include "Logger.h"
#include <Psapi.h>
#include <algorithm>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "psapi.lib")

MicrophoneMonitor::MicrophoneMonitor()
    : pEnumerator(nullptr), pMicDevice(nullptr), pSpeakerDevice(nullptr),
      pAudioClient(nullptr), pCaptureClient(nullptr), audioClientInitialized(false) {

    CoInitialize(NULL);

    // Create device enumerator
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create MMDeviceEnumerator");
        return;
    }

    // Get default microphone device
    hr = pEnumerator->GetDefaultAudioEndpoint(
        eCapture,  // Microphone (capture device)
        eConsole,  // Console device role
        &pMicDevice);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get default microphone device");
    } else {
        LOG_DEBUG("MicrophoneMonitor: microphone device initialized");
    }

    // Get default speaker device
    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender,   // Speakers/headphones (render device)
        eConsole,
        &pSpeakerDevice);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get default speaker device");
    } else {
        LOG_DEBUG("MicrophoneMonitor: speaker device initialized");
    }
}

MicrophoneMonitor::~MicrophoneMonitor() {
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pSpeakerDevice) pSpeakerDevice->Release();
    if (pMicDevice) pMicDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    CoUninitialize();
}

std::vector<AudioSession> MicrophoneMonitor::GetActiveMicrophoneSessions() {
    if (!pMicDevice) {
        return std::vector<AudioSession>();
    }

    return EnumerateAudioSessions(pMicDevice);
}

std::vector<AudioSession> MicrophoneMonitor::GetActiveSpeakerSessions() {
    if (!pSpeakerDevice) {
        return std::vector<AudioSession>();
    }

    return EnumerateAudioSessions(pSpeakerDevice);
}

std::vector<AudioSession> MicrophoneMonitor::EnumerateAudioSessions(IMMDevice* device) {
    std::vector<AudioSession> sessions;

    if (!device) {
        return sessions;
    }

    // Activate audio session manager
    IAudioSessionManager2* pSessionManager = nullptr;
    HRESULT hr = device->Activate(
        __uuidof(IAudioSessionManager2), CLSCTX_ALL,
        NULL, (void**)&pSessionManager);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio session manager");
        return sessions;
    }

    // Get session enumerator
    IAudioSessionEnumerator* pSessionEnum = nullptr;
    hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);

    if (FAILED(hr)) {
        pSessionManager->Release();
        return sessions;
    }

    // Get session count
    int sessionCount = 0;
    pSessionEnum->GetCount(&sessionCount);

    // Enumerate sessions
    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* pSessionControl = nullptr;
        hr = pSessionEnum->GetSession(i, &pSessionControl);

        if (FAILED(hr)) continue;

        // Get IAudioSessionControl2 interface (needed for process ID)
        IAudioSessionControl2* pSessionControl2 = nullptr;
        hr = pSessionControl->QueryInterface(
            __uuidof(IAudioSessionControl2),
            (void**)&pSessionControl2);

        if (FAILED(hr)) {
            pSessionControl->Release();
            continue;
        }

        // Get session state
        AudioSessionState state;
        pSessionControl2->GetState(&state);

        // Log ALL sessions for debugging (not just active)
        DWORD processId = 0;
        pSessionControl2->GetProcessId(&processId);

        // Log even processId=0 sessions for debugging
        const char* stateStr = (state == AudioSessionStateActive) ? "ACTIVE" :
                               (state == AudioSessionStateInactive) ? "INACTIVE" :
                               (state == AudioSessionStateExpired) ? "EXPIRED" : "UNKNOWN";

        if (processId == 0) {
            LOG_DEBUG_FMT("Audio session found: [System Sounds PID=0] - State: %s", stateStr);
        } else {
            std::string processName = GetProcessName(processId);
            LOG_DEBUG_FMT("Audio session found: %s (PID: %d) - State: %s",
                         processName.c_str(), processId, stateStr);
        }

        // Only interested in active sessions
        if (state == AudioSessionStateActive) {
            // Get display name first (works even for processId=0)
            LPWSTR pDisplayName = nullptr;
            pSessionControl2->GetDisplayName(&pDisplayName);
            std::string displayName = "Unknown";

            if (pDisplayName) {
                // Convert wide string to UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, pDisplayName, -1, NULL, 0, NULL, NULL);
                if (size > 0) {
                    std::vector<char> buffer(size);
                    WideCharToMultiByte(CP_UTF8, 0, pDisplayName, -1, buffer.data(), size, NULL, NULL);
                    displayName = buffer.data();
                }
                CoTaskMemFree(pDisplayName);
            }

            // Get process name (empty for processId=0)
            std::string processName = GetProcessName(processId);

            // CRITICAL FIX: Don't skip processId=0 if it has a meaningful display name
            // Teams may use processId=0 for speaker sessions
            if (processId == 0) {
                LOG_DEBUG_FMT("ACTIVE session with PID=0 - Display name: \"%s\"", displayName.c_str());

                // Check if display name suggests this is a meeting app
                // Teams display names often contain "Microsoft Teams", "Meeting", etc.
                std::string lowerDisplayName = displayName;
                std::transform(lowerDisplayName.begin(), lowerDisplayName.end(),
                               lowerDisplayName.begin(), ::tolower);

                bool isMeetingRelated = (lowerDisplayName.find("teams") != std::string::npos ||
                                        lowerDisplayName.find("zoom") != std::string::npos ||
                                        lowerDisplayName.find("meeting") != std::string::npos ||
                                        lowerDisplayName.find("webex") != std::string::npos);

                if (!isMeetingRelated) {
                    LOG_DEBUG("Skipping PID=0 session - not meeting-related based on display name");
                    pSessionControl2->Release();
                    pSessionControl->Release();
                    continue;
                }

                LOG_DEBUG("PID=0 session appears to be meeting-related - including in results");
                processName = "SystemAudioSession";  // Placeholder name for processId=0
            }

            if (!processName.empty() || processId == 0) {
                // Create session info
                AudioSession session;
                session.processId = processId;
                session.processName = processName;
                session.displayName = displayName;
                session.isActive = true;
                session.startTime = std::chrono::system_clock::now();

                sessions.push_back(session);

                LOG_DEBUG_FMT("Added to results: %s (PID: %d, Display: %s)",
                             processName.c_str(), processId, displayName.c_str());
            }
        }

        pSessionControl2->Release();
        pSessionControl->Release();
    }

    pSessionEnum->Release();
    pSessionManager->Release();

    return sessions;
}

std::string MicrophoneMonitor::GetProcessName(DWORD processId) {
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, processId);

    if (!hProcess) {
        return "";
    }

    char processPath[MAX_PATH];
    DWORD result = GetModuleFileNameExA(hProcess, NULL, processPath, MAX_PATH);
    CloseHandle(hProcess);

    if (result == 0) {
        return "";
    }

    // Extract filename from full path
    std::string fullPath(processPath);
    size_t lastSlash = fullPath.find_last_of("\\/");

    return (lastSlash != std::string::npos)
        ? fullPath.substr(lastSlash + 1)
        : fullPath;
}

bool MicrophoneMonitor::IsMeetingApp(const std::string& processName) {
    // Special case: SystemAudioSession means we already verified it's meeting-related
    // via display name check (for processId=0 sessions)
    if (processName == "SystemAudioSession") {
        return true;
    }

    // List of known meeting app process names
    static const std::vector<std::string> meetingApps = {
        // Video conferencing - primary
        "Zoom.exe",
        "ZoomWebHost.exe",       // Zoom PWA
        "Teams.exe",
        "ms-teams.exe",          // New Teams

        // Browsers (for Google Meet, Zoom Web, Teams Web)
        "chrome.exe",
        "msedge.exe",
        "firefox.exe",
        "brave.exe",

        // Other enterprise meeting apps
        "Webex.exe",
        "CiscoCollabHost.exe",   // Webex Meetings
        "BlueJeans.exe",
        "GoToMeeting.exe",
        "join.me.exe",
        "RingCentral.exe",

        // Communication apps with voice/video
        "Discord.exe",
        "Skype.exe",
        "slack.exe",
        "Messenger.exe",         // Facebook Messenger

        // Lesser-known but valid
        "whereby.exe",
        "jitsi.exe"
    };

    // Case-insensitive comparison
    std::string lowerProcessName = processName;
    std::transform(lowerProcessName.begin(), lowerProcessName.end(),
                   lowerProcessName.begin(), ::tolower);

    for (const auto& appName : meetingApps) {
        std::string lowerAppName = appName;
        std::transform(lowerAppName.begin(), lowerAppName.end(),
                       lowerAppName.begin(), ::tolower);

        if (lowerProcessName == lowerAppName) {
            return true;
        }
    }

    return false;
}

bool MicrophoneMonitor::IsMeetingAppUsingMicrophone() {
    auto sessions = GetActiveMicrophoneSessions();

    for (const auto& session : sessions) {
        if (IsMeetingApp(session.processName)) {
            LOG_DEBUG_FMT("Meeting app using microphone: %s (PID: %d)",
                         session.processName.c_str(), session.processId);
            return true;
        }
    }

    return false;
}

bool MicrophoneMonitor::IsMeetingAppUsingSpeakers() {
    // PRODUCTION DETECTION STRATEGY:
    // Most meeting apps (Teams, Zoom, Chrome) properly report ACTIVE sessions
    // Check both microphone AND speaker devices for ACTIVE meeting app sessions
    //
    // Strategy:
    // 1. Check for ACTIVE meeting app sessions on SPEAKER device (fast, works for most apps)
    // 2. Check for ACTIVE meeting app sessions on MICROPHONE device (works for Teams)
    // 3. If no ACTIVE sessions found anywhere, return false

    // Step 1: Check SPEAKER device for ACTIVE meeting apps
    auto speakerSessions = GetActiveSpeakerSessions();

    for (const auto& session : speakerSessions) {
        if (IsMeetingApp(session.processName)) {
            LOG_DEBUG_FMT("Meeting app using speakers (ACTIVE): %s (PID: %d)",
                         session.processName.c_str(), session.processId);
            return true;
        }
    }

    // Step 2: Check MICROPHONE device for ACTIVE meeting apps
    // (Teams often has ACTIVE mic session but INACTIVE speaker session)
    auto micSessions = GetActiveMicrophoneSessions();

    for (const auto& session : micSessions) {
        if (IsMeetingApp(session.processName)) {
            LOG_DEBUG_FMT("Meeting app found on microphone (ACTIVE): %s (PID: %d) - assuming bidirectional audio",
                         session.processName.c_str(), session.processId);
            return true;
        }
    }

    // Step 3: No ACTIVE meeting app found on either device
    LOG_DEBUG("No ACTIVE meeting app sessions found on speaker or microphone devices");
    return false;
}
