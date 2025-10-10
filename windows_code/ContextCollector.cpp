#include "ContextCollector.h"
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <vector>

static std::atomic<bool> updateThreadRunning{false};
static std::thread updateThread;
static std::atomic<bool> activeAppMonitoringInitialized{false};

ContextCollector::ContextCollector()
    : latestCameraLatency(0.0f)
    , latestVoiceLatency(0.0f)
    , latestContextUpdateLatency(0.0f)
{
    lastUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(2);

    // Initialize active app monitoring
    if (!activeAppMonitoringInitialized.load()) {
        if (WindowsAPIs::InitializeActiveAppMonitoring()) {
            activeAppMonitoringInitialized.store(true);
        }
    }
}

bool ContextCollector::ShouldUpdateCache() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate);
    return elapsed.count() >= 1; // Update every 1 second
}

void ContextCollector::UpdateCache() {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Collect all context data (BEFORE locking cacheMutex to avoid deadlock)
    auto activeApp = WindowsAPIs::GetForegroundAppName();
    auto battery = WindowsAPIs::GetBatteryPercentage();
    auto isCharging = WindowsAPIs::IsCharging();
    
    // Collect system performance data
    auto cpuUsage = WindowsAPIs::GetCPUUsage();          // CPUʹ�ðٷֱ�
    auto memoryUsage = WindowsAPIs::GetMemoryUsage();    // �ڴ�ʹ�ðٷֱ�
    auto memoryUsed = WindowsAPIs::GetMemoryUsed();      // �����ڴ�GB
    auto totalMemory = WindowsAPIs::GetTotalMemory();    // ���ڴ�GB
    
    auto networkConnected = WindowsAPIs::IsNetworkConnected();
    auto networkType = WindowsAPIs::GetNetworkType();
    auto location = WindowsAPIs::GetLocation();
    auto timestamp = WindowsAPIs::GetCurrentTimestamp();
    
    // Get recent active apps list
    auto recentApps = WindowsAPIs::GetRecentPeriodActiveAppList();

    // Lock cacheMutex for the entire JSON building process
    std::lock_guard<std::mutex> lock(cacheMutex);

    // Clear and rebuild the context
    cachedContext = Json();

    // Build JSON response
    cachedContext.set("activeApp", activeApp);
    cachedContext.set("battery", battery);
    cachedContext.set("isCharging", isCharging);
    
    // Add system performance data with proper formatting
    if (cpuUsage >= 0) {
        std::ostringstream cpuStream;
        cpuStream << std::fixed << std::setprecision(2) << cpuUsage;
        cachedContext.setRaw("cpuUsage", cpuStream.str());
    } else {
        cachedContext.setRaw("cpuUsage", "null");
    }
    
    // memoryUsage �ڴ�ʹ�ðٷֱ�
    if (memoryUsage >= 0) {
        std::ostringstream memPercentStream;
        memPercentStream << std::fixed << std::setprecision(2) << memoryUsage;
        cachedContext.setRaw("memoryUsage", memPercentStream.str());
    } else {
        cachedContext.setRaw("memoryUsage", "null");
    }
    
    // memoryUsed ��GBΪ��λ
    if (memoryUsed >= 0) {
        std::ostringstream memUsedStream;
        memUsedStream << std::fixed << std::setprecision(2) << memoryUsed;
        cachedContext.setRaw("memoryUsedGB", memUsedStream.str());
    } else {
        cachedContext.setRaw("memoryUsedGB", "null");
    }
    
    // totalMemory ��GBΪ��λ
    if (totalMemory >= 0) {
        std::ostringstream totalMemStream;
        totalMemStream << std::fixed << std::setprecision(2) << totalMemory;
        cachedContext.setRaw("totalMemoryGB", totalMemStream.str());
    } else {
        cachedContext.setRaw("totalMemoryGB", "null");
    }
    
    cachedContext.set("networkConnected", networkConnected);
    cachedContext.set("networkType", networkType);
    
    // ��ȷ����WinRT location����
    if (location.valid && location.latitude != 0.0 && location.longitude != 0.0) {
        // ��Ч��GPS����
        std::ostringstream latStream, lonStream;
        latStream << std::fixed << std::setprecision(8) << location.latitude;
        lonStream << std::fixed << std::setprecision(8) << location.longitude;
        cachedContext.setRaw("locationLat", latStream.str());
        cachedContext.setRaw("locationLon", lonStream.str());
        cachedContext.setRaw("locationValid", "true");
    } else {
        // ��Ч���޷���ȡλ��
        cachedContext.setRaw("locationLat", "null");
        cachedContext.setRaw("locationLon", "null");
        cachedContext.setRaw("locationValid", "false");
    }
    
    // Add recent active apps to JSON as a properly formatted array
    std::ostringstream recentAppsStream;
    recentAppsStream << "[";
    bool firstApp = true;
    for (const auto& record : recentApps) {
        if (!firstApp) {
            recentAppsStream << ",";
        }
        
        // Format timestamp as ISO string (using local time)
        auto time_t_val = std::chrono::system_clock::to_time_t(record.timestamp);
        struct tm timeinfo;
        std::string timestampStr = "1970-01-01T00:00:00.000+00:00";
        if (localtime_s(&timeinfo, &time_t_val) == 0) {
            std::ostringstream timeStream;
            timeStream << std::put_time(&timeinfo, "%Y-%m-%dT%H:%M:%S");
            timeStream << ".000";
            
            // Add timezone offset
            char tz_offset[16];
            strftime(tz_offset, sizeof(tz_offset), "%z", &timeinfo);
            std::string tz_str(tz_offset);
            if (tz_str.length() >= 5) {
                // Convert +0800 to +08:00 format
                tz_str = tz_str.substr(0, 3) + ":" + tz_str.substr(3);
            } else {
                tz_str = "+00:00"; // fallback
            }
            timeStream << tz_str;
            timestampStr = timeStream.str();
        }
        
        // Build individual app record as JSON object (escape quotes and backslashes in strings)
        std::string escapedAppName = record.appName;
        std::string escapedWindowTitle = record.windowTitle;
        
        // First escape backslashes (must be done before escaping quotes)
        size_t pos = 0;
        while ((pos = escapedAppName.find("\\", pos)) != std::string::npos) {
            escapedAppName.replace(pos, 1, "\\\\");
            pos += 2; // Skip the escaped backslash
        }
        pos = 0;
        while ((pos = escapedWindowTitle.find("\\", pos)) != std::string::npos) {
            escapedWindowTitle.replace(pos, 1, "\\\\");
            pos += 2; // Skip the escaped backslash
        }
        
        // Then escape quotes
        pos = 0;
        while ((pos = escapedAppName.find("\"", pos)) != std::string::npos) {
            escapedAppName.replace(pos, 1, "\\\"");
            pos += 2; // Skip the escaped quote
        }
        pos = 0;
        while ((pos = escapedWindowTitle.find("\"", pos)) != std::string::npos) {
            escapedWindowTitle.replace(pos, 1, "\\\"");
            pos += 2; // Skip the escaped quote
        }
        
        recentAppsStream << "{";
        recentAppsStream << "\"appName\":\"" << escapedAppName << "\",";
        recentAppsStream << "\"windowTitle\":\"" << escapedWindowTitle << "\",";
        recentAppsStream << "\"durationSeconds\":" << record.durationSeconds << ",";
        recentAppsStream << "\"timestamp\":\"" << timestampStr << "\"";
        recentAppsStream << "}";
        
        firstApp = false;
    }
    recentAppsStream << "]";

    // Use setRaw to set the array as a proper JSON array (not a quoted string)
    cachedContext.setRaw("RecentPeriodActiveApps", recentAppsStream.str());
    cachedContext.set("timestamp", timestamp);

    // cacheMutex is released here automatically

    // Calculate and store latency (AFTER releasing cacheMutex to avoid deadlock)
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::lock_guard<std::mutex> metricsLock(metricsMutex);
        latestContextUpdateLatency = duration.count() / 1000.0f;
    }

    lastUpdate = std::chrono::steady_clock::now();
}

Json ContextCollector::CollectCurrentContext() {
    if (ShouldUpdateCache()) {
        UpdateCache();
    }

    std::lock_guard<std::mutex> lock(cacheMutex);

    // Get voice text first (before it's needed by GenerateFusedContext)
    std::string voiceText;
    {
        std::lock_guard<std::mutex> voiceLock(voiceMutex);
        voiceText = latestVoiceTranscription;
        if (!latestVoiceTranscription.empty()) {
            cachedContext.set("voiceTranscription", latestVoiceTranscription);
        } else {
            cachedContext.setRaw("voiceTranscription", "null");
        }
    }

    // Add camera vision to context (thread-safe)
    {
        std::lock_guard<std::mutex> cameraLock(cameraMutex);
        if (!latestCameraDescription.empty()) {
            cachedContext.set("cameraDescription", latestCameraDescription);
            cachedContext.set("cameraLatency", static_cast<int>(latestCameraLatency));
        } else {
            cachedContext.setRaw("cameraDescription", "null");
            cachedContext.set("cameraLatency", 0);
        }
    }

    // Add pipeline latency metrics (thread-safe)
    {
        std::lock_guard<std::mutex> metricsLock(metricsMutex);
        std::ostringstream voiceLatencyStream, contextLatencyStream;
        voiceLatencyStream << std::fixed << std::setprecision(2) << latestVoiceLatency;
        contextLatencyStream << std::fixed << std::setprecision(2) << latestContextUpdateLatency;

        cachedContext.setRaw("voiceLatency", voiceLatencyStream.str());
        cachedContext.setRaw("contextUpdateLatency", contextLatencyStream.str());
    }

    // Add fused context summary (pass voiceText to avoid re-locking voiceMutex)
    cachedContext.set("fusedContext", GenerateFusedContext(voiceText));

    return cachedContext;
}

void ContextCollector::UpdateVoiceContext(const std::string& transcription) {
    std::lock_guard<std::mutex> lock(voiceMutex);

    // Clean up common Whisper hallucinations
    std::string cleaned = transcription;

    // List of common hallucination patterns to remove
    const std::vector<std::string> hallucinations = {
        "[no audio]", "[NO AUDIO]",
        "[BLANK_AUDIO]", "[blank_audio]",
        "[BLANK AUDIO]", "[blank audio]",
        "(silence)", "(Silence)", "(SILENCE)",
        "(blank)", "(Blank)", "(BLANK)",
        "[Music]", "[music]", "(Music)", "(music)",
        "[Applause]", "[applause]",
        "Thanks for watching!", "Thank you for watching!",
        "(upbeat music)", "(soft music)"
    };

    // Remove hallucination patterns (trim whitespace after removal)
    for (const auto& hallucination : hallucinations) {
        size_t pos = 0;
        while ((pos = cleaned.find(hallucination, pos)) != std::string::npos) {
            cleaned.erase(pos, hallucination.length());
        }
    }

    // Trim leading/trailing whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        cleaned = cleaned.substr(start, end - start + 1);
    } else {
        cleaned = "";
    }

    latestVoiceTranscription = cleaned;
}

void ContextCollector::UpdateVoiceContext(const std::string& transcription, float latencyMs) {
    // Update latency FIRST (before locking voiceMutex to avoid deadlock)
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        latestVoiceLatency = latencyMs;
    }

    // Then update transcription (locks voiceMutex)
    UpdateVoiceContext(transcription);
}

void ContextCollector::UpdateCameraContext(const std::string& description, float latencyMs) {
    std::lock_guard<std::mutex> lock(cameraMutex);

    // Store camera vision data in member variables (persists across cache rebuilds)
    latestCameraDescription = description;
    latestCameraLatency = latencyMs;
}

// Overload that fetches voice text itself (may cause deadlock if voiceMutex already locked)
std::string ContextCollector::GenerateFusedContext() const {
    std::string voiceText;
    {
        std::lock_guard<std::mutex> voiceLock(voiceMutex);
        voiceText = latestVoiceTranscription;
    }
    return GenerateFusedContext(voiceText);
}

// Overload that accepts voice text to avoid deadlock
std::string ContextCollector::GenerateFusedContext(const std::string& voiceText) const {
    // NOTE: cacheMutex must already be locked by caller!
    // Do not lock here to avoid deadlock.
    std::ostringstream fused;

    // Current activity
    std::string activeApp = cachedContext.getString("activeApp", "Unknown");
    if (activeApp != "Unknown" && !activeApp.empty()) {
        fused << "Active: " << activeApp;
    }

    // Voice transcription (if recent) - passed as parameter to avoid mutex deadlock
    if (!voiceText.empty()) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "Said: \"" << voiceText << "\"";
    }

    // Battery status (if critical)
    int battery = cachedContext.getInt("battery", 100);
    bool isCharging = cachedContext.getBool("isCharging", false);
    if (battery < 20 && !isCharging) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "⚠️ Low battery: " << battery << "%";
    }

    // Network status
    bool networkConnected = cachedContext.getBool("networkConnected", true);
    if (!networkConnected) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "⚠️ Offline";
    }

    // CPU usage (if high)
    double cpuUsage = cachedContext.getDouble("cpuUsage", 0.0);
    if (cpuUsage > 80.0) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "⚠️ High CPU: " << static_cast<int>(cpuUsage) << "%";
    }

    std::string result = fused.str();
    return result.empty() ? "System running normally" : result;
}

void ContextCollector::StartPeriodicUpdate() {
    if (updateThreadRunning.load()) {
        return; // Already running
    }
    
    updateThreadRunning.store(true);
    updateThread = std::thread([this]() {
        while (updateThreadRunning.load()) {
            if (ShouldUpdateCache()) {
                UpdateCache();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
}

void ContextCollector::StopPeriodicUpdate() {
    updateThreadRunning.store(false);
    if (updateThread.joinable()) {
        updateThread.join();
    }
}

ContextCollector::~ContextCollector() {
    StopPeriodicUpdate();
    
    // Cleanup active app monitoring when the collector is destroyed
    if (activeAppMonitoringInitialized.load()) {
        WindowsAPIs::CleanupActiveAppMonitoring();
        activeAppMonitoringInitialized.store(false);
    }
}