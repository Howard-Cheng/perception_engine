#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <chrono>

/**
 * AudioSession - Represents an app using audio device
 */
struct AudioSession {
    DWORD processId;
    std::string processName;      // e.g., "Zoom.exe"
    std::string displayName;      // e.g., "Zoom Meeting"
    bool isActive;
    std::chrono::system_clock::time_point startTime;
};

/**
 * MicrophoneMonitor - Monitors system-level microphone and speaker usage
 *
 * Purpose: Detect which applications are actively using audio devices
 * This is used for meeting detection (Notion's approach)
 *
 * Key Features:
 * - Enumerate active microphone sessions (which apps are capturing audio)
 * - Enumerate active speaker sessions (which apps are playing audio)
 * - Identify meeting apps (Zoom, Teams, Meet, etc.)
 * - Privacy-safe: monitors which apps use devices, not the audio content
 *
 * Windows API Used: IAudioSessionManager2 (WASAPI)
 */
class MicrophoneMonitor {
public:
    MicrophoneMonitor();
    ~MicrophoneMonitor();

    /**
     * Get all apps currently using microphone
     * Returns: Vector of active audio sessions on capture device
     */
    std::vector<AudioSession> GetActiveMicrophoneSessions();

    /**
     * Get all apps currently using speakers/headphones
     * Returns: Vector of active audio sessions on render device
     */
    std::vector<AudioSession> GetActiveSpeakerSessions();

    /**
     * Check if a meeting app is using microphone
     * Returns: true if Zoom/Teams/Meet/etc. is capturing audio
     */
    bool IsMeetingAppUsingMicrophone();

    /**
     * Check if a meeting app is using speakers
     * Returns: true if Zoom/Teams/Meet/etc. is playing audio
     *
     * PRODUCTION NOTE: Uses hybrid detection strategy:
     * - Primary: Checks for ACTIVE audio sessions (works for Zoom, Chrome)
     * - Fallback: For Teams/apps with INACTIVE sessions, checks actual audio output
     */
    bool IsMeetingAppUsingSpeakers();

    /**
     * Check if audio is currently playing through speakers
     * Uses WASAPI to sample actual audio output levels
     * Returns: true if audio level exceeds silence threshold
     */
    bool IsAudioCurrentlyPlaying();

    /**
     * Check if specific process is currently outputting audio
     * Combines session detection with audio level checking
     */
    bool IsProcessPlayingAudio(DWORD processId);

    /**
     * Check if process name is a known meeting app
     * e.g., "Zoom.exe", "Teams.exe", "chrome.exe" (for Meet)
     */
    static bool IsMeetingApp(const std::string& processName);

    /**
     * Check if meeting app uses proper ACTIVE session reporting
     * Some apps (Zoom) properly report, others (Teams) don't
     */
    static bool UsesProperSessionState(const std::string& processName);

private:
    // Internal helper: enumerate sessions for a given device
    std::vector<AudioSession> EnumerateAudioSessions(IMMDevice* device);

    // Get process name from process ID
    std::string GetProcessName(DWORD processId);

    // WASAPI audio level detection
    float GetCurrentAudioLevel();  // Sample current speaker output level
    bool InitializeAudioClient();  // Lazy init for audio capture client

    // WASAPI members
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pMicDevice;
    IMMDevice* pSpeakerDevice;

    // Audio level detection (lazy initialized)
    IAudioClient* pAudioClient;
    IAudioCaptureClient* pCaptureClient;
    bool audioClientInitialized;

    // Audio detection thresholds
    static constexpr float SILENCE_THRESHOLD = 0.01f;  // 1% of max volume
    static constexpr int SAMPLE_SIZE = 480;  // ~10ms at 48kHz
};
