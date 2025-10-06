#include "ContextCollector.h"
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>

static std::atomic<bool> updateThreadRunning{false};
static std::thread updateThread;
static std::atomic<bool> activeAppMonitoringInitialized{false};

ContextCollector::ContextCollector() {
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
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    // Clear and rebuild the context
    cachedContext = Json();
    
    // Collect all context data
    auto activeApp = WindowsAPIs::GetForegroundAppName();
    auto battery = WindowsAPIs::GetBatteryPercentage();
    auto isCharging = WindowsAPIs::IsCharging();
    
    // Collect system performance data
    auto cpuUsage = WindowsAPIs::GetCPUUsage();          // CPU使用百分比
    auto memoryUsage = WindowsAPIs::GetMemoryUsage();    // 内存使用百分比
    auto memoryUsed = WindowsAPIs::GetMemoryUsed();      // 已用内存GB
    auto totalMemory = WindowsAPIs::GetTotalMemory();    // 总内存GB
    
    auto networkConnected = WindowsAPIs::IsNetworkConnected();
    auto networkType = WindowsAPIs::GetNetworkType();
    auto location = WindowsAPIs::GetLocation();
    auto timestamp = WindowsAPIs::GetCurrentTimestamp();
    
    // Get recent active apps list
    auto recentApps = WindowsAPIs::GetRecentPeriodActiveAppList();
    
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
    
    // memoryUsage 内存使用百分比
    if (memoryUsage >= 0) {
        std::ostringstream memPercentStream;
        memPercentStream << std::fixed << std::setprecision(2) << memoryUsage;
        cachedContext.setRaw("memoryUsage", memPercentStream.str());
    } else {
        cachedContext.setRaw("memoryUsage", "null");
    }
    
    // memoryUsed 以GB为单位
    if (memoryUsed >= 0) {
        std::ostringstream memUsedStream;
        memUsedStream << std::fixed << std::setprecision(2) << memoryUsed;
        cachedContext.setRaw("memoryUsedGB", memUsedStream.str());
    } else {
        cachedContext.setRaw("memoryUsedGB", "null");
    }
    
    // totalMemory 以GB为单位
    if (totalMemory >= 0) {
        std::ostringstream totalMemStream;
        totalMemStream << std::fixed << std::setprecision(2) << totalMemory;
        cachedContext.setRaw("totalMemoryGB", totalMemStream.str());
    } else {
        cachedContext.setRaw("totalMemoryGB", "null");
    }
    
    cachedContext.set("networkConnected", networkConnected);
    cachedContext.set("networkType", networkType);
    
    // 正确处理WinRT location数据
    if (location.valid && location.latitude != 0.0 && location.longitude != 0.0) {
        // 有效的GPS坐标
        std::ostringstream latStream, lonStream;
        latStream << std::fixed << std::setprecision(8) << location.latitude;
        lonStream << std::fixed << std::setprecision(8) << location.longitude;
        cachedContext.setRaw("locationLat", latStream.str());
        cachedContext.setRaw("locationLon", lonStream.str());
        cachedContext.setRaw("locationValid", "true");
    } else {
        // 无效或无法获取位置
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
    
    lastUpdate = std::chrono::steady_clock::now();
}

Json ContextCollector::CollectCurrentContext() {
    if (ShouldUpdateCache()) {
        UpdateCache();
    }
    
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cachedContext;
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