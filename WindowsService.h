#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include <string>
#include <windows.h>

// Windows Service wrapper class
class WindowsService {
private:
    static WindowsService* instance;
    static SERVICE_STATUS serviceStatus;
    static SERVICE_STATUS_HANDLE serviceStatusHandle;
    
    std::string serviceName;
    std::string displayName;
    bool running;
    
    static VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv);
    static VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode);
    
    void UpdateServiceStatus(DWORD currentState, DWORD exitCode = NO_ERROR);
    
protected:
    void SetRunning(bool isRunning) { running = isRunning; }
    bool IsRunning() const { return running; }
    
public:
    WindowsService(const std::string& name, const std::string& display);
    ~WindowsService();
    
    bool Install();
    bool Uninstall();
    bool Start();
    bool Stop();
    
    void Run(); // Entry point for service
    virtual void OnStart() = 0;
    virtual void OnStop() = 0;
    virtual void OnRunning() = 0;
    
    static void RunAsService(WindowsService* service);
};