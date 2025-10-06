#include "WindowsService.h"
#include <iostream>

WindowsService* WindowsService::instance = nullptr;
SERVICE_STATUS WindowsService::serviceStatus;
SERVICE_STATUS_HANDLE WindowsService::serviceStatusHandle = 0;

WindowsService::WindowsService(const std::string& name, const std::string& display)
    : serviceName(name), displayName(display), running(false) {
    instance = this;
}

WindowsService::~WindowsService() {
    instance = nullptr;
}

VOID WINAPI WindowsService::ServiceMain(DWORD argc, LPSTR* argv) {
    if (!instance) return;
    
    serviceStatusHandle = RegisterServiceCtrlHandlerA(
        instance->serviceName.c_str(), ServiceCtrlHandler);
    
    if (!serviceStatusHandle) return;
    
    // Initialize service status
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwWin32ExitCode = 0;
    serviceStatus.dwServiceSpecificExitCode = 0;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;
    
    instance->UpdateServiceStatus(SERVICE_START_PENDING);
    
    try {
        instance->OnStart();
        instance->UpdateServiceStatus(SERVICE_RUNNING);
        instance->running = true;
        
        // Main service loop
        while (instance->running) {
            instance->OnRunning();
            Sleep(1000); // Sleep for 1 second
        }
    }
    catch (...) {
        instance->UpdateServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
        return;
    }
    
    instance->OnStop();
    instance->UpdateServiceStatus(SERVICE_STOPPED);
}

VOID WINAPI WindowsService::ServiceCtrlHandler(DWORD ctrlCode) {
    if (!instance) return;
    
    switch (ctrlCode) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            instance->UpdateServiceStatus(SERVICE_STOP_PENDING);
            instance->running = false;
            break;
            
        case SERVICE_CONTROL_INTERROGATE:
            break;
            
        default:
            break;
    }
    
    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

void WindowsService::UpdateServiceStatus(DWORD currentState, DWORD exitCode) {
    serviceStatus.dwCurrentState = currentState;
    serviceStatus.dwWin32ExitCode = exitCode;
    
    if (currentState == SERVICE_START_PENDING || currentState == SERVICE_STOP_PENDING) {
        serviceStatus.dwControlsAccepted = 0;
    } else {
        serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }
    
    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        serviceStatus.dwCheckPoint = 0;
    } else {
        serviceStatus.dwCheckPoint++;
    }
    
    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

bool WindowsService::Install() {
    SC_HANDLE scManager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scManager) return false;
    
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    SC_HANDLE service = CreateServiceA(
        scManager,
        serviceName.c_str(),
        displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    
    bool success = (service != nullptr);
    
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    return success;
}

bool WindowsService::Uninstall() {
    SC_HANDLE scManager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) return false;
    
    SC_HANDLE service = OpenServiceA(scManager, serviceName.c_str(), SERVICE_ALL_ACCESS);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }
    
    bool success = DeleteService(service);
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    return success;
}

bool WindowsService::Start() {
    SC_HANDLE scManager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) return false;
    
    SC_HANDLE service = OpenServiceA(scManager, serviceName.c_str(), SERVICE_START);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }
    
    bool success = StartService(service, 0, nullptr);
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    return success;
}

bool WindowsService::Stop() {
    SC_HANDLE scManager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) return false;
    
    SC_HANDLE service = OpenServiceA(scManager, serviceName.c_str(), SERVICE_STOP);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }
    
    SERVICE_STATUS status;
    bool success = ControlService(service, SERVICE_CONTROL_STOP, &status);
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    return success;
}

void WindowsService::RunAsService(WindowsService* service) {
    SERVICE_TABLE_ENTRYA serviceTable[] = {
        { const_cast<char*>(service->serviceName.c_str()), ServiceMain },
        { nullptr, nullptr }
    };
    
    StartServiceCtrlDispatcherA(serviceTable);
}