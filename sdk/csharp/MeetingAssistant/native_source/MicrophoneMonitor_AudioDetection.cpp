// MicrophoneMonitor_AudioDetection.cpp
// WASAPI audio level detection implementation
// This file contains the production-grade audio output detection methods

#include "MicrophoneMonitor.h"
#include "Logger.h"
#include <algorithm>  // For std::transform
#include <cmath>

/**
 * UsesProperSessionState - Determines if app properly reports ACTIVE audio sessions
 *
 * TEAMS AUDIO QUIRK:
 * Microsoft Teams keeps speaker audio sessions in INACTIVE state even when
 * audio is actively playing. This is a deliberate design choice by Microsoft.
 *
 * Apps that properly report ACTIVE:
 * - Zoom, Webex, Discord, Skype (standard WASAPI usage)
 * - Chrome, Edge, Firefox (browser-based meetings)
 *
 * Apps with INACTIVE quirk:
 * - Microsoft Teams (ms-teams.exe, Teams.exe)
 * - Potentially other Microsoft communication apps
 */
bool MicrophoneMonitor::UsesProperSessionState(const std::string& processName) {
    // Convert to lowercase for comparison
    std::string lowerName = processName;
    std::transform(lowerName.begin(), lowerName.end(),
                   lowerName.begin(), ::tolower);

    // Microsoft Teams does NOT properly report ACTIVE state
    if (lowerName == "ms-teams.exe" || lowerName == "teams.exe") {
        return false;
    }

    // All other meeting apps are assumed to properly report
    return true;
}

/**
 * InitializeAudioClient - Lazy initialization of WASAPI audio capture client
 *
 * Sets up loopback audio capture for detecting actual speaker output.
 * This is only initialized when needed (for Teams-style detection).
 */
bool MicrophoneMonitor::InitializeAudioClient() {
    if (audioClientInitialized) {
        return (pAudioClient != nullptr && pCaptureClient != nullptr);
    }

    audioClientInitialized = true;  // Mark as attempted

    if (!pSpeakerDevice) {
        LOG_ERROR("Cannot initialize audio client: no speaker device");
        return false;
    }

    // Activate audio client
    HRESULT hr = pSpeakerDevice->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        NULL, (void**)&pAudioClient);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio client for loopback capture");
        return false;
    }

    // Get audio format
    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClient->GetMixFormat(&pwfx);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio mix format");
        pAudioClient->Release();
        pAudioClient = nullptr;
        return false;
    }

    // Initialize in loopback mode (capture what's playing through speakers)
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,  // Capture speaker output
        10000000,  // 1 second buffer
        0,
        pwfx,
        NULL);

    CoTaskMemFree(pwfx);

    if (FAILED(hr)) {
        LOG_ERROR_FMT("Failed to initialize audio client in loopback mode (HRESULT: 0x%08X)", hr);
        pAudioClient->Release();
        pAudioClient = nullptr;
        return false;
    }

    // Get capture client
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pCaptureClient);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio capture client");
        pAudioClient->Release();
        pAudioClient = nullptr;
        return false;
    }

    // Start capture
    hr = pAudioClient->Start();

    if (FAILED(hr)) {
        LOG_ERROR("Failed to start audio capture");
        pCaptureClient->Release();
        pCaptureClient = nullptr;
        pAudioClient->Release();
        pAudioClient = nullptr;
        return false;
    }

    LOG_DEBUG("Audio client initialized for speaker output detection");
    return true;
}

/**
 * GetCurrentAudioLevel - Sample current speaker output level
 *
 * Returns: RMS (root mean square) level from 0.0 to 1.0
 * - 0.0 = silence
 * - 1.0 = maximum volume
 */
float MicrophoneMonitor::GetCurrentAudioLevel() {
    if (!InitializeAudioClient()) {
        return 0.0f;
    }

    UINT32 packetLength = 0;
    HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);

    if (FAILED(hr) || packetLength == 0) {
        return 0.0f;  // No audio data available
    }

    BYTE* pData = nullptr;
    UINT32 numFramesAvailable = 0;
    DWORD flags = 0;

    hr = pCaptureClient->GetBuffer(
        &pData,
        &numFramesAvailable,
        &flags,
        NULL,
        NULL);

    if (FAILED(hr)) {
        return 0.0f;
    }

    // Calculate RMS level (assumes 16-bit PCM or float format)
    float rms = 0.0f;

    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        rms = 0.0f;  // Explicitly marked as silent
    } else {
        // Treat as float samples (most common loopback format)
        float* samples = (float*)pData;
        UINT32 sampleCount = numFramesAvailable * 2;  // Stereo = 2 channels

        double sumSquares = 0.0;
        for (UINT32 i = 0; i < sampleCount; i++) {
            sumSquares += samples[i] * samples[i];
        }

        rms = (float)sqrt(sumSquares / sampleCount);
    }

    pCaptureClient->ReleaseBuffer(numFramesAvailable);

    return rms;
}

/**
 * IsAudioCurrentlyPlaying - Check if any audio is playing through speakers
 *
 * Uses WASAPI loopback capture to detect actual speaker output.
 * This is the PRODUCTION solution for detecting Teams audio.
 *
 * Returns: true if audio level exceeds silence threshold (1% of max volume)
 */
bool MicrophoneMonitor::IsAudioCurrentlyPlaying() {
    float level = GetCurrentAudioLevel();

    bool isPlaying = (level > SILENCE_THRESHOLD);

    LOG_DEBUG_FMT("Audio level: %.4f (threshold: %.4f) - Playing: %s",
                 level, SILENCE_THRESHOLD, isPlaying ? "YES" : "NO");

    return isPlaying;
}

/**
 * IsProcessPlayingAudio - Check if specific process is outputting audio
 *
 * NOTE: This is a future enhancement. Currently we detect ANY audio output
 * when Teams has an INACTIVE session. Per-process audio level detection
 * requires IAudioSessionControl::GetAudioMeterInformation which is more complex.
 *
 * For production v1, we use:
 * - INACTIVE Teams session + ANY audio output = meeting detected
 *
 * This has acceptable false positive rate:
 * - User must be in Teams meeting (INACTIVE session exists)
 * - AND audio must be playing
 * - False positive only if user plays music while in silent Teams meeting
 */
bool MicrophoneMonitor::IsProcessPlayingAudio(DWORD processId) {
    // TODO: Implement per-process audio level detection
    // For now, fall back to global audio detection
    return IsAudioCurrentlyPlaying();
}
