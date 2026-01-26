#include "providers/SystemContextProvider.h"

#include <sstream>

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
    //location_ = WindowsAPIs::WindowsAPIsManager::GetInstance().GetLocation();
    location_ = WindowsAPIs::WindowsAPIsManager::GetInstance().GetOnlineLocation();
    
    // FIX: GetCurrentTimestamp returns std::string, convert to timestamp
    timestamp_ = std::time(nullptr);
}

void SystemContextProvider::collectContext(nlohmann::json& context) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Battery
    context["battery"] = battery_;
    context["isCharging"] = isCharging_;
    
    // CPU
    if (cpuUsage_ >= 0) {
        context["cpuUsage"] = cpuUsage_;
    } else {
        context["cpuUsage"] = nullptr;
    }
    
    // Memory
    if (memoryUsage_ >= 0) {
        context["memoryUsage"] = memoryUsage_;
    } else {
        context["memoryUsage"] = nullptr;
    }
    
    if (memoryUsed_ >= 0) {
        context["memoryUsedGB"] = memoryUsed_;
    } else {
        context["memoryUsedGB"] = nullptr;
    }
    
    if (totalMemory_ >= 0) {
        context["totalMemoryGB"] = totalMemory_;
    } else {
        context["totalMemoryGB"] = nullptr;
    }
    
    // Network
    context["networkConnected"] = networkConnected_;
    context["networkType"] = networkType_;
    
    // Location
    if (location_.valid && location_.latitude != 0.0 && location_.longitude != 0.0) {
        context["locationLat"] = location_.latitude;
        context["locationLon"] = location_.longitude;
        context["locationValid"] = true;
        context["locationDescription"] = location_.description;
    } else {
        context["locationLat"] = nullptr;
        context["locationLon"] = nullptr;
        context["locationValid"] = false;
        context["locationDescription"] = nullptr;
    }
    
    // Timestamp
    context["timestamp"] = timestamp_;
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
