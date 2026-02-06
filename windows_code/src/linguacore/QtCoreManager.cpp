#include "linguacore/QtCoreManager.h"
#include "pe_base/logger.h"
#include <mutex>
#include <map>
#include <vector>
#include <sstream>
#include <type_traits>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
using json = nlohmann::json;

// static instance
QtCoreManager* QtCoreManager::s_instance_ = nullptr;

QtCoreManager::QtCoreManager()
{
    s_instance_ = this;
    // Create task queue with single thread for async operations
    task_queue_ = pe_base::TaskQueue::Create(1, "QtCoreManagerQueue");
}

QtCoreManager::~QtCoreManager()
{
    Stop();
    // Reset task queue (will wait for pending tasks to complete)
    task_queue_.reset();
    if (s_instance_ == this) s_instance_ = nullptr;
}

bool QtCoreManager::Initialize(const std::string& dllPath)
{
    if (hDll_) return true;
    hDll_ = LoadLibraryA(dllPath.c_str());
    if (!hDll_) {
        PE_ERROR("LoadLibraryA failed: " << GetLastError());
        return false;
    }
    return BindFunctions();
}

bool QtCoreManager::BindFunctions()
{
    if (!hDll_) return false;
    // helper lambda - use void* intermediate to avoid reference issues
    auto load = [this](auto& out, const char* name)->bool {
        using FuncType = std::decay_t<decltype(out)>;
        void* proc = reinterpret_cast<void*>(GetProcAddress(hDll_, name));
        if (!proc) {
            PE_ERROR("Failed to load function " << name << " error=" << GetLastError());
            return false;
        }
        out = reinterpret_cast<FuncType>(proc);
        return true;
        };

    if (!load(qc_create_blob_data_, "quantum_create_blob_data")) return false;
    if (!load(qc_create_data_container_, "quantum_create_data_container")) return false;
    if (!load(qc_create_input_data_, "quantum_create_input_data")) return false;
    if (!load(qc_get_client_, "quantum_get_client")) return false;
    if (!load(qc_connect_, "quantum_connect")) return false;
    if (!load(qc_send_command_, "quantum_send_command")) return false;
    if (!load(qc_disconnect_, "quantum_disconnect")) return false;
    if (!load(qc_release_client_, "quantum_release_client")) return false;
    if (!load(qc_free_string_, "quantum_free_string")) return false;
    if (!load(qc_free_ref_, "quantum_free_ref")) return false;

    return true;
}

bool QtCoreManager::Start()
{
    if (!qc_get_client_) return false;
    clientHandle_ = qc_get_client_();
    if (clientHandle_ == 0) {
        PE_ERROR("qc_get_client_ returned 0");
        return false;
    }
    // connect with callbacks
    if (!qc_connect_) return false;
    int rc = qc_connect_(clientHandle_, &QtCoreManager::OnConnectionStatusStatic, &QtCoreManager::OnResultStatic, nullptr);
    if (rc != 0) {
        PE_ERROR("qc_connect_ returned " << rc);
        return false;
    }
    return true;
}

void QtCoreManager::Stop()
{
    if (clientHandle_ != 0 && qc_disconnect_) {
        qc_disconnect_(clientHandle_);
    }
    if (clientHandle_ != 0 && qc_release_client_) {
        qc_release_client_(clientHandle_);
        clientHandle_ = 0;
    }
    if (hDll_) {
        FreeLibrary(hDll_);
        hDll_ = nullptr;
    }
}

bool QtCoreManager::AddMemory(const std::string& model, const std::string& userText, const std::string& date)
{
    if (!qc_create_data_container_ || !qc_create_input_data_ || !qc_send_command_) return false;
    
    json payload;
    payload["action"] = "add_memory";
    payload["model"] = model;
    payload["userText"] = userText;
    payload["date"] = date;
    
    std::string payloadStr = payload.dump();

    void* dataContainer = qc_create_data_container_(payloadStr.c_str(), nullptr, 0, nullptr);
    if (!dataContainer) {
        PE_ERROR("qc_create_data_container_ failed");
        return false;
    }
    // Note: dataContainer is used below, do NOT free it here
    void* inputData = qc_create_input_data_("fkb_memory", sessionId_.empty() ? nullptr : sessionId_.c_str(), -1LL, dataContainer);
    if (!inputData) {
        PE_ERROR("qc_create_input_data_ failed");
        qc_free_ref_(dataContainer);
        return false;
    }
    int64_t jobId = qc_send_command_(clientHandle_, inputData);
    qc_free_ref_(inputData);
    qc_free_ref_(dataContainer);
    PE_INFO("AddMemory jobId=" << jobId);
    return jobId != 0;
}

bool QtCoreManager::GetMemory()
{
    if (!qc_create_data_container_ || !qc_create_input_data_ || !qc_send_command_) return false;
    
    json payload;
    payload["action"] = "get_all_memory";
    payload["model"] = "default";
    payload["fields"] = json::array();
    payload["fields"].push_back("id");
    payload["fields"].push_back("content");
    payload["fields"].push_back("bucket");
    
    std::string payloadStr = payload.dump();
    
    void* dataContainer = qc_create_data_container_(payloadStr.c_str(), nullptr, 0, nullptr);
    if (!dataContainer) {
        PE_ERROR("qc_create_data_container_ failed");
        return false;
    }
    void* inputData = qc_create_input_data_("fkb_memory", sessionId_.empty() ? nullptr : sessionId_.c_str(), -1LL, dataContainer);
    if (!inputData) {
        PE_ERROR("qc_create_input_data_ failed");
        qc_free_ref_(dataContainer);
        return false;
    }
    int64_t jobId = qc_send_command_(clientHandle_, inputData);
    qc_free_ref_(inputData);
    qc_free_ref_(dataContainer);
    PE_INFO("GetMemory jobId=" << jobId);
    return jobId != 0;
}

bool QtCoreManager::SendSessionFinalize(const std::string& action)
{
    if (!qc_create_data_container_ || !qc_create_input_data_ || !qc_send_command_) return false;
    std::string sid = sessionId_;
    if (sid.empty()) sid = "";
    
    json finalizeJson;
    finalizeJson["action"] = action;
    finalizeJson["sessionID"] = sid;
    std::string finalizeText = finalizeJson.dump();

    if (action == "finalize") {
        std::string uploadContent = "jintiantianqizenmeyang";
        void* blob = qc_create_blob_data_(uploadContent.c_str(), nullptr, static_cast<int>(uploadContent.size()));
        void* dataContainer = qc_create_data_container_(finalizeText.c_str(), &blob, 1, nullptr);
        if (!dataContainer) { qc_free_ref_(blob); PE_ERROR("create data container failed"); return false; }
        void* inputData = qc_create_input_data_("session", sid.empty() ? nullptr : sid.c_str(), -1LL, dataContainer);
        if (!inputData) { qc_free_ref_(dataContainer); qc_free_ref_(blob); PE_ERROR("create input failed"); return false; }
        int64_t jobId = qc_send_command_(clientHandle_, inputData);
        qc_free_ref_(inputData);
        qc_free_ref_(dataContainer);
        qc_free_ref_(blob);
        PE_INFO("SendSessionFinalize jobId=" << jobId);
        return jobId != 0;
    }
    else {
        void* dataContainer = qc_create_data_container_(finalizeText.c_str(), nullptr, 0, nullptr);
        if (!dataContainer) { PE_ERROR("create data container failed"); return false; }
        void* inputData = qc_create_input_data_("session", sid.empty() ? nullptr : sid.c_str(), -1LL, dataContainer);
        if (!inputData) { qc_free_ref_(dataContainer); PE_ERROR("create input failed"); return false; }
        int64_t jobId = qc_send_command_(clientHandle_, inputData);
        qc_free_ref_(inputData);
        qc_free_ref_(dataContainer);
        PE_INFO("SendSessionFinalize jobId=" << jobId);
        return jobId != 0;
    }
}

std::string QtCoreManager::GetSessionId()
{
    return sessionId_;
}

// static callbacks
void __cdecl QtCoreManager::OnConnectionStatusStatic(int status)
{
    if (s_instance_) s_instance_->OnConnectionStatus(status);
}

void __cdecl QtCoreManager::OnResultStatic(void* outputDataPtr)
{
    if (s_instance_) s_instance_->OnResult(outputDataPtr);
}

// helpers for resource free - mimic QuantumResourceGuard simplified behavior
struct ResourceGuard {
    void** ptr;
    void (*freeFunc)(void*);
    ResourceGuard(void** p, void (*f)(void*)) : ptr(p), freeFunc(f) {}
    ~ResourceGuard() { if (ptr && *ptr) { freeFunc(*ptr); *ptr = nullptr; } }
};

void QtCoreManager::OnConnectionStatus(int status)
{
    PE_INFO("QtCoreManager connection status=" << status);
    if (status == 1) {
        // Use task queue instead of detached thread for better lifecycle management
        if (task_queue_) {
            task_queue_->PostTask([this]() {
                json createJson;
                createJson["action"] = "create";
                std::string finalizeText = createJson.dump();
                
                void* dataContainer = qc_create_data_container_(finalizeText.c_str(), nullptr, 0, nullptr);
                if (!dataContainer) { PE_ERROR("create data container failed"); return; }
                ResourceGuard g(&dataContainer, qc_free_ref_);
                void* inputData = qc_create_input_data_("session", nullptr, -1LL, dataContainer);
                if (!inputData) { PE_ERROR("create input failed"); return; }
                ResourceGuard g2(&inputData, qc_free_ref_);
                int64_t jobId = qc_send_command_(clientHandle_, inputData);
                PE_INFO("session create jobId=" << jobId);
            });
        }
    }
}

void QtCoreManager::OnResult(void* outputDataPtr)
{
    if (!outputDataPtr) return;
    // call quantum_output_get_job_id, quantum_output_get_status, quantum_output_get_data, quantum_data_get_text
    // We didn't bind these in header to keep minimal; however OnResult wants to access sessionID text returned by the SDK.
    // So attempt to load needed functions dynamically here.
    auto loadFn = [this](const char* name)->FARPROC { return GetProcAddress(hDll_, name); };
    using QuantumOutputGetJobIdFunc = int64_t(__cdecl*)(void*);
    using QuantumOutputGetDataFunc = void* (__cdecl*)(void*);
    using QuantumDataGetTextFunc = void* (__cdecl*)(void*);
    using QuantumFreeStringFuncLocal = void(__cdecl*)(void*);
    using QuantumFreeRefFuncLocal = void(__cdecl*)(void*);

    QuantumOutputGetJobIdFunc fn_jobid = reinterpret_cast<QuantumOutputGetJobIdFunc>(loadFn("quantum_output_get_job_id"));
    auto fn_output_get_data = reinterpret_cast<QuantumOutputGetDataFunc>(loadFn("quantum_output_get_data"));
    auto fn_data_get_text = reinterpret_cast<QuantumDataGetTextFunc>(loadFn("quantum_data_get_text"));
    auto fn_free_string = reinterpret_cast<QuantumFreeStringFuncLocal>(loadFn("quantum_free_string"));
    auto fn_free_ref = reinterpret_cast<QuantumFreeRefFuncLocal>(loadFn("quantum_free_ref"));

    if (!fn_jobid || !fn_output_get_data || !fn_data_get_text) {
        PE_ERROR("OnResult: required functions not found");
        return;
    }

    int64_t jobId = fn_jobid(outputDataPtr);
    void* dataPtr = fn_output_get_data(outputDataPtr);
    if (!dataPtr) return;
    ResourceGuard dataGuard(&dataPtr, fn_free_ref);
    void* textPtr = fn_data_get_text(dataPtr);
    ResourceGuard textGuard(&textPtr, fn_free_string);
    std::string text = textPtr ? reinterpret_cast<const char*>(textPtr) : "";
    PE_INFO("OnResult jobId=" << jobId << " text=" << text);

    // Parse JSON only if it is valid JSON to avoid exceptions being thrown (which breaks in debugger)
    try {
        if (json::accept(text)) {
            json responseJson = json::parse(text);
            if (responseJson.contains("sessionID")) {
                sessionId_ = responseJson["sessionID"].get<std::string>();
                PE_INFO("Got sessionID=" << sessionId_);
            }
        } else {
            PE_WARN("OnResult: response text is not valid JSON, skipping JSON parse");
        }
    }
    catch (const json::exception& e) {
        PE_ERROR("JSON parsing failed in OnResult: " << e.what() << " text=" << text);
    }

    // Store result for synchronous callers if jobId is valid
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        result_map_[jobId] = text;
    }
}

// Synchronous summary generation implementation
std::string QtCoreManager::summarize(const std::string& content, int timeout_ms)
{
    if (!qc_create_data_container_ || !qc_create_input_data_ || !qc_send_command_) {
        PE_ERROR("GenerateSummary: SDK functions not bound");
        return "";
    }

    // Build prompt JSON similar to blobSyncDemo::summarize()
    json promptMessages = json::array();
    promptMessages.push_back({{"role", "system"}, {"content", "Please summarize the following text concisely."}});
    promptMessages.push_back({{"role", "user"}, {"content", content}});

    json optParams;
    optParams["temperature"] = 3;
    optParams["n_ctx"] = 8192;
    optParams["n_predict"] = 2048;
    optParams["repeat_penalty"] = 1.1;
    optParams["top_p"] = 1;
    optParams["top_k"] = 5;

    json promptWrapper;
    promptWrapper["OptionParams"] = optParams;
    promptWrapper["messages"] = promptMessages;

    json payload;
    payload["modelName"] = "llm";
    payload["modelVersion"] = "1.0";
    payload["prompt"] = promptWrapper.dump();

    std::string payloadStr = payload.dump();

    void* dataContainer = qc_create_data_container_(payloadStr.c_str(), nullptr, 0, nullptr);
    if (!dataContainer) {
        PE_ERROR("GenerateSummary: qc_create_data_container_ failed");
        return "";
    }

    void* inputData = qc_create_input_data_("model_call", sessionId_.empty() ? nullptr : sessionId_.c_str(), -1LL, dataContainer);
    if (!inputData) {
        PE_ERROR("GenerateSummary: qc_create_input_data_ failed");
        qc_free_ref_(dataContainer);
        return "";
    }

    int64_t jobId = qc_send_command_(clientHandle_, inputData);
    qc_free_ref_(inputData);
    qc_free_ref_(dataContainer);

    if (jobId == 0) {
        PE_ERROR("GenerateSummary: qc_send_command_ returned 0");
        return "";
    }

    // Wait synchronously for result or timeout by polling result_map_ using jobId
    std::string text;
    auto start = std::chrono::steady_clock::now();
    while (true) {
        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            auto it = result_map_.find(jobId);
            if (it != result_map_.end()) {
                text = it->second;
                result_map_.erase(it);
                break;
            }
        }
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= timeout_ms) {
            PE_ERROR("GenerateSummary: timeout waiting for job result, jobId=" << jobId);
            return "";
        }
        // Sleep briefly to avoid busy spin
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Try parsing JSON response only if it's valid JSON to avoid thrown exceptions in the parser
    if (json::accept(text)) {
        try {
            json responseJson = json::parse(text);
            // Common pattern: response contains 'choices' or 'result' fields. Try to extract main text.
            if (responseJson.contains("choices")) {
                auto ch = responseJson["choices"];
                if (ch.is_array() && !ch.empty()) {
                    if (ch[0].contains("message") && ch[0]["message"].contains("content")) {
                        return ch[0]["message"]["content"].get<std::string>();
                    }
                }
            }
            // If response contains 'result' as text
            if (responseJson.contains("result") && responseJson["result"].is_string()) {
                return responseJson["result"].get<std::string>();
            }
        }
        catch (const json::exception& e) {
            PE_WARN("GenerateSummary: cannot parse response JSON: " << e.what());
        }
    }
    else {
        PE_WARN("GenerateSummary: response is not valid JSON, returning raw text");
    }

    // Fallback: return raw text
    return text;
}
