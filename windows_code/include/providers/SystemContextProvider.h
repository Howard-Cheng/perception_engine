#pragma once

#include "providers/IContextProvider.h"
#include "platform/WindowsAPIs.h"
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
    
    bool initialize() override;
    void update() override;
    void collectContext(Json& context) const override;
    std::string getName() const override;
    bool isAvailable() const override;
    void shutdown() override;
    
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
    WindowsAPIs::Location location_;
    long long timestamp_ = 0;
};
