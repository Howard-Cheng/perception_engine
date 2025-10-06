#pragma once
#include <string>
#include <vector>
#include <chrono>

// Windows APIs wrapper functions
namespace WindowsAPIs {
    // Active Application
    std::string GetForegroundAppName();
    
    // Time utilities - for consistent timestamp formatting
    std::string FormatTimestampUTC(const std::chrono::system_clock::time_point& timepoint);
    std::string FormatTimestampLocal(const std::chrono::system_clock::time_point& timepoint);
    
    // Active Application History - New functionality
    struct ActiveAppRecord {
        std::string appName;
        std::string windowTitle;
        std::chrono::system_clock::time_point timestamp;
        int durationSeconds;
        
        ActiveAppRecord() : durationSeconds(0) {}
        ActiveAppRecord(const std::string& name, const std::string& title) 
            : appName(name), windowTitle(title), timestamp(std::chrono::system_clock::now()), durationSeconds(1) {}
    };
    
    // Initialize and start active app monitoring
    bool InitializeActiveAppMonitoring();
    
    // Stop active app monitoring and cleanup
    void CleanupActiveAppMonitoring();
    
    // Get recent active apps within the specified time period
    std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList();
    
    // Battery Status
    int GetBatteryPercentage();
    bool IsCharging();
    
    // System Performance
    double GetCPUUsage();
    double GetMemoryUsage();          // �����ڴ�ʹ�ðٷֱ�
    double GetMemoryUsed();           // ��GBΪ��λ�������ڴ�GB
    double GetTotalMemory();          // ���ڴ�GB
    
    // Network Status
    bool IsNetworkConnected();
    std::string GetNetworkType();
    double GetNetworkSpeed();         // ���ص�ǰ�����ٶ� (Mbps)
    
    // Location (optional)
    struct Location {
        double latitude = 0.0;
        double longitude = 0.0;
        bool valid = false;
    };
    Location GetLocation();
    
    // Timestamp
    std::string GetCurrentTimestamp();
    
    // Helper functions for Unicode/UTF-8 conversion
    std::string WideStringToUtf8(const std::wstring& wstr);
    std::string WideStringToUtf8(const wchar_t* wstr);
}