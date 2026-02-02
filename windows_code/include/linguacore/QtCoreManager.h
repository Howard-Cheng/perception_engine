#pragma once

#include <string>
#include <functional>
#include <windows.h>
#include <cstdint>
#include <memory>
#include "pe_base/task_queue/task_queue.h"

class QtCoreManager
{
public:
    QtCoreManager();
    ~QtCoreManager();

    // Initialize and load DLL. Provide path to quantum-sdk DLL (eg "quantum-sdk-1.0.10.dll").
    bool Initialize(const std::string& dllPath);
    // Start client and connect to service.
    bool Start();
    // Stop client and release resources.
    void Stop();

    // Add and get memory commands
    bool AddMemory(const std::string& model, const std::string& userText, const std::string& date);
    bool GetMemory();

    // Send a generic session finalize (action is string like "finalize" or "get")
    bool SendSessionFinalize(const std::string& action);

    // Get current session id
    std::string GetSessionId();

private:
    // non-copyable
    QtCoreManager(const QtCoreManager&) = delete;
    QtCoreManager& operator=(const QtCoreManager&) = delete;

    // internal helpers
    bool BindFunctions();
    void Cleanup();

    // callbacks routed from C-style callbacks
    static void __cdecl OnConnectionStatusStatic(int status);
    static void __cdecl OnResultStatic(void* outputDataPtr);
    void OnConnectionStatus(int status);
    void OnResult(void* outputDataPtr);

private:
    HMODULE hDll_{ nullptr };
    int64_t clientHandle_{ 0 };
    std::string sessionId_;

    // function pointers - mirror minimal set used by demo
    typedef void* (__cdecl* QuantumCreateBlobDataFunc)(const char* mime, void* data, int dataSize);
    typedef void* (__cdecl* QuantumCreateDataContainerFunc)(const char* text, void** binaryDataArray, int binaryCount, const char* uriJson);
    typedef void* (__cdecl* QuantumCreateInputDataFunc)(const char* command, const char* sessionId, int64_t jobId, void* dataContainer);
    typedef int64_t(__cdecl* QuantumGetClientFunc)();
    typedef int(__cdecl* QuantumConnectFunc)(int64_t clientId, void(__cdecl* onConnectionStatus)(int), void(__cdecl* onResult)(void*), const char* configJson);
    typedef int64_t(__cdecl* QuantumSendCommandFunc)(int64_t clientId, void* inputData);
    typedef int(__cdecl* QuantumDisconnectFunc)(int64_t clientId);
    typedef void(__cdecl* QuantumReleaseClientFunc)(int64_t clientId);
    typedef void(__cdecl* QuantumFreeStringFunc)(void* str);
    typedef void(__cdecl* QuantumFreeRefFunc)(void* ptr);

    QuantumCreateBlobDataFunc qc_create_blob_data_ = nullptr;
    QuantumCreateDataContainerFunc qc_create_data_container_ = nullptr;
    QuantumCreateInputDataFunc qc_create_input_data_ = nullptr;
    QuantumGetClientFunc qc_get_client_ = nullptr;
    QuantumConnectFunc qc_connect_ = nullptr;
    QuantumSendCommandFunc qc_send_command_ = nullptr;
    QuantumDisconnectFunc qc_disconnect_ = nullptr;
    QuantumReleaseClientFunc qc_release_client_ = nullptr;
    QuantumFreeStringFunc qc_free_string_ = nullptr;
    QuantumFreeRefFunc qc_free_ref_ = nullptr;

    // Task queue for async operations (replaces detached threads)
    std::shared_ptr<pe_base::TaskQueue> task_queue_;

    // static instance pointer for callbacks (single instance assumption)
    static QtCoreManager* s_instance_;
};
