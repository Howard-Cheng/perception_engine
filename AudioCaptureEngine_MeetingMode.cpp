// AudioCaptureEngine - Meeting Mode Implementation
// This file contains meeting mode functionality for AudioCaptureEngine
//
// TODO: Merge these implementations into AudioCaptureEngine.cpp at the end

#include "AudioCaptureEngine.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

// ============================================================================
// MEETING MODE - Public Methods
// ============================================================================

void AudioCaptureEngine::StartMeetingMode(const std::string& meetingId) {
    if (inMeetingMode.load()) {
        LOG_WARN("Already in meeting mode - stopping previous meeting first");
        StopMeetingMode();
    }

    LOG_INFO_FMT("Starting meeting mode: %s", meetingId.c_str());

    // Set meeting mode flag
    inMeetingMode.store(true);
    currentMeetingId = meetingId;
    meetingStartTime = std::chrono::system_clock::now();

    // Clear previous transcript
    {
        std::lock_guard<std::mutex> lock(meetingTranscriptMutex);
        meetingTranscriptSegments.clear();
    }

    // Start audio capture if not already running
    if (!IsRunning()) {
        Start();
    }

    LOG_INFO("Meeting mode started - continuous transcription active");
}

std::string AudioCaptureEngine::StopMeetingMode() {
    if (!inMeetingMode.load()) {
        LOG_WARN("Not in meeting mode - nothing to stop");
        return "";
    }

    LOG_INFO_FMT("Stopping meeting mode: %s", currentMeetingId.c_str());

    // Turn off meeting mode
    inMeetingMode.store(false);

    // Save transcript to file
    SaveMeetingTranscript(currentMeetingId);

    // Generate transcript file path
    std::string transcriptPath = "meetings/" + currentMeetingId + "_transcript.txt";

    LOG_INFO_FMT("Meeting mode stopped - transcript saved to: %s", transcriptPath.c_str());

    return transcriptPath;
}

std::string AudioCaptureEngine::GetMeetingTranscript() {
    std::lock_guard<std::mutex> lock(meetingTranscriptMutex);

    if (meetingTranscriptSegments.empty()) {
        return "";
    }

    // Concatenate all segments
    std::ostringstream oss;
    for (const auto& segment : meetingTranscriptSegments) {
        if (!segment.text.empty()) {
            oss << segment.text << " ";
        }
    }

    return oss.str();
}

std::vector<AudioCaptureEngine::TranscriptSegment> AudioCaptureEngine::GetMeetingTranscriptSegments() {
    std::lock_guard<std::mutex> lock(meetingTranscriptMutex);
    return meetingTranscriptSegments;
}

// ============================================================================
// MEETING MODE - Private Methods
// ============================================================================

std::vector<float> AudioCaptureEngine::MixAudioSources(const std::vector<float>& mic, const std::vector<float>& device) {
    // Mix mic (user) + device (other speakers) audio
    // Strategy: average the two sources

    size_t maxSize = (std::max)(mic.size(), device.size());
    std::vector<float> mixed(maxSize, 0.0f);

    for (size_t i = 0; i < maxSize; i++) {
        float micSample = (i < mic.size()) ? mic[i] : 0.0f;
        float deviceSample = (i < device.size()) ? device[i] : 0.0f;

        // Average (could also use other mixing strategies)
        mixed[i] = (micSample + deviceSample) / 2.0f;

        // Clamp to [-1.0, 1.0] to prevent distortion
        if (mixed[i] > 1.0f) mixed[i] = 1.0f;
        if (mixed[i] < -1.0f) mixed[i] = -1.0f;
    }

    return mixed;
}

void AudioCaptureEngine::SaveMeetingTranscript(const std::string& meetingId) {
    std::lock_guard<std::mutex> lock(meetingTranscriptMutex);

    if (meetingTranscriptSegments.empty()) {
        LOG_WARN("No transcript segments to save");
        return;
    }

    // Create meetings directory if it doesn't exist
    CreateDirectoryA("meetings", NULL);

    // Save as plain text
    std::string txtPath = "meetings/" + meetingId + "_transcript.txt";
    std::ofstream txtFile(txtPath);

    if (!txtFile.is_open()) {
        LOG_ERROR_FMT("Failed to create transcript file: %s", txtPath.c_str());
        return;
    }

    // Write header
    txtFile << "Meeting Transcript\n";
    txtFile << "==================\n";
    txtFile << "Meeting ID: " << meetingId << "\n";

    // Format meeting start time
    std::time_t startTime = std::chrono::system_clock::to_time_t(meetingStartTime);
    struct tm timeinfo;
    localtime_s(&timeinfo, &startTime);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    txtFile << "Start Time: " << timeBuffer << "\n";

    txtFile << "Duration: " << meetingTranscriptSegments.size() << " segments\n";
    txtFile << "\n";

    // Write transcript segments with timestamps
    for (size_t i = 0; i < meetingTranscriptSegments.size(); i++) {
        const auto& segment = meetingTranscriptSegments[i];

        // Calculate elapsed time from meeting start
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            segment.timestamp - meetingStartTime);

        int minutes = elapsed.count() / 60;
        int seconds = elapsed.count() % 60;

        // Format: [MM:SS] text (confidence: 0.95)
        txtFile << "[" << std::setfill('0') << std::setw(2) << minutes
                << ":" << std::setfill('0') << std::setw(2) << seconds << "] "
                << segment.text
                << " (confidence: " << std::fixed << std::setprecision(2) << segment.confidence << ")\n";
    }

    txtFile.close();
    LOG_INFO_FMT("Transcript saved to: %s (%zu segments)",
                 txtPath.c_str(), meetingTranscriptSegments.size());

    // Also save as JSON (more structured)
    std::string jsonPath = "meetings/" + meetingId + "_transcript.json";
    std::ofstream jsonFile(jsonPath);

    if (jsonFile.is_open()) {
        jsonFile << "{\n";
        jsonFile << "  \"meetingId\": \"" << meetingId << "\",\n";
        jsonFile << "  \"startTime\": \"" << timeBuffer << "\",\n";
        jsonFile << "  \"segments\": [\n";

        for (size_t i = 0; i < meetingTranscriptSegments.size(); i++) {
            const auto& segment = meetingTranscriptSegments[i];

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                segment.timestamp - meetingStartTime);

            // Escape quotes in text
            std::string escapedText = segment.text;
            size_t pos = 0;
            while ((pos = escapedText.find("\"", pos)) != std::string::npos) {
                escapedText.replace(pos, 1, "\\\"");
                pos += 2;
            }

            jsonFile << "    {\n";
            jsonFile << "      \"timestampMs\": " << elapsed.count() << ",\n";
            jsonFile << "      \"text\": \"" << escapedText << "\",\n";
            jsonFile << "      \"confidence\": " << segment.confidence << "\n";
            jsonFile << "    }";

            if (i < meetingTranscriptSegments.size() - 1) {
                jsonFile << ",";
            }
            jsonFile << "\n";
        }

        jsonFile << "  ]\n";
        jsonFile << "}\n";
        jsonFile.close();

        LOG_INFO_FMT("JSON transcript saved to: %s", jsonPath.c_str());
    }
}

// ============================================================================
// INTEGRATION NOTES
// ============================================================================

// TODO: Modify ProcessingThread() in AudioCaptureEngine.cpp to handle meeting mode:
//
// In ProcessingThread(), after successful transcription, add:
//
//     if (inMeetingMode.load()) {
//         // In meeting mode - save transcript segment
//         TranscriptSegment segment;
//         segment.timestamp = std::chrono::system_clock::now();
//         segment.text = transcribedText;
//         segment.confidence = 0.95f;  // TODO: get actual confidence from Whisper
//
//         std::lock_guard<std::mutex> lock(meetingTranscriptMutex);
//         meetingTranscriptSegments.push_back(segment);
//
//         LOG_DEBUG_FMT("Meeting segment added: %s", transcribedText.c_str());
//     }
//
// TODO: Modify ProcessingThread() to mix audio in meeting mode:
//
//     // Get audio buffers
//     std::vector<float> micBuffer = GetMicrophoneBuffer();
//     std::vector<float> deviceBuffer = GetSystemAudioBuffer();
//
//     std::vector<float> audioToTranscribe;
//
//     if (inMeetingMode.load()) {
//         // Mix both sources for meeting transcription
//         audioToTranscribe = MixAudioSources(micBuffer, deviceBuffer);
//         LOG_DEBUG_FMT("Meeting mode: mixed %zu mic samples + %zu device samples",
//                      micBuffer.size(), deviceBuffer.size());
//     } else {
//         // Normal mode: just use microphone
//         audioToTranscribe = micBuffer;
//     }
//
//     // Transcribe the audio
//     std::string transcription = TranscribeAudio(audioToTranscribe);
