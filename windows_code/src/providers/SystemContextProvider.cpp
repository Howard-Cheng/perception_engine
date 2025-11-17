#include "providers/SystemContextProvider.h"

bool SystemContextProvider::initialize() {
    // System APIs are always available
    return true;
}

void SystemContextProvider::update() {
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

void SystemContextProvider::collectContext(Json& context) const {
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

std::string SystemContextProvider::getName() const {
    return "SystemContext";
}

bool SystemContextProvider::isAvailable() const {
    return true; // System APIs always available
}

void SystemContextProvider::shutdown() {
    // Nothing to cleanup
}
