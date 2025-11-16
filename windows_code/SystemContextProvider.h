#pragma once

#include "IContextProvider.h"
#include "WindowsAPIs.h"
#include <mutex>
#include <sstream>
#include <iomanip>

/**
 * @brief System Information Context Provider
 * 
 * Collects:
 * - Battery status
 * - CPU/Memory usage
 * - Network status
 * - GPS location
 * - Timestamp
 */
class SystemContextProvider : public IContextProvider {
public:
    SystemContextProvider() = default;
    ~SystemContextProvider() override = default;
    
    bool initialize() override {
        // System APIs are always available
        return true;
    }
    
    void update() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Cache system data
        battery_ = WindowsAPIs::GetBatteryPercentage();
        isCharging_ = WindowsAPIs::IsCharging();
        cpuUsage_ = WindowsAPIs::GetCPUUsage();
        memoryUsage_ = WindowsAPIs::GetMemoryUsage();
        memoryUsed_ = WindowsAPIs::GetMemoryUsed();
        totalMemory_ = WindowsAPIs::GetTotalMemory();
        networkConnected_ = WindowsAPIs::IsNetworkConnected();
        networkType_ = WindowsAPIs::GetNetworkType();
        location_ = WindowsAPIs::WindowsAPIsManager::GetInstance().GetLocation();
        
        // FIX: GetCurrentTimestamp returns std::string, convert to timestamp
        timestamp_ = std::time(nullptr);
    }
    
    void collectContext(Json& context) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Battery
        context.set("battery", battery_);
        context.set("isCharging", isCharging_);
        
        // CPU
        if (cpuUsage_ >= 0) {
            std::ostringstream cpuStream;
            cpuStream << std::fixed << std::setprecision(2) << cpuUsage_;
            context.setRaw("cpuUsage", cpuStream.str());
        } else {
            context.setRaw("cpuUsage", "null");
        }
        
        // Memory
        if (memoryUsage_ >= 0) {
            std::ostringstream memStream;
            memStream << std::fixed << std::setprecision(2) << memoryUsage_;
            context.setRaw("memoryUsage", memStream.str());
        } else {
            context.setRaw("memoryUsage", "null");
        }
        
        if (memoryUsed_ >= 0) {
            std::ostringstream memUsedStream;
            memUsedStream << std::fixed << std::setprecision(2) << memoryUsed_;
            context.setRaw("memoryUsedGB", memUsedStream.str());
        } else {
            context.setRaw("memoryUsedGB", "null");
        }
        
        if (totalMemory_ >= 0) {
            std::ostringstream totalMemStream;
            totalMemStream << std::fixed << std::setprecision(2) << totalMemory_;
            context.setRaw("totalMemoryGB", totalMemStream.str());
        } else {
            context.setRaw("totalMemoryGB", "null");
        }
        
        // Network
        context.set("networkConnected", networkConnected_);
        context.set("networkType", networkType_);
        
        // Location
        if (location_.valid && location_.latitude != 0.0 && location_.longitude != 0.0) {
            std::ostringstream latStream, lonStream;
            latStream << std::fixed << std::setprecision(8) << location_.latitude;
            lonStream << std::fixed << std::setprecision(8) << location_.longitude;
            context.setRaw("locationLat", latStream.str());
            context.setRaw("locationLon", lonStream.str());
            context.setRaw("locationValid", "true");
        } else {
            context.setRaw("locationLat", "null");
            context.setRaw("locationLon", "null");
            context.setRaw("locationValid", "false");
        }
        
        // Timestamp
        context.set("timestamp", timestamp_);
    }
    
    std::string getName() const override {
        return "SystemContext";
    }
    
    bool isAvailable() const override {
        return true; // System APIs always available
    }
    
    void shutdown() override {
        // Nothing to cleanup
    }
    
private:
    mutable std::mutex mutex_;
    
    // Cached data
    int battery_ = 0;
    bool isCharging_ = false;
    double cpuUsage_ = -1.0;
    double memoryUsage_ = -1.0;
    double memoryUsed_ = -1.0;
    double totalMemory_ = -1.0;
    bool networkConnected_ = false;
    std::string networkType_;
    WindowsAPIs::Location location_;  // FIX: Use Location (not LocationResult or LocationInfo)
    long long timestamp_ = 0;  // FIX: Use long long for timestamp
};
