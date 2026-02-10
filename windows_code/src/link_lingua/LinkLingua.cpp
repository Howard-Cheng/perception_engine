#include "link_lingua/LinkLingua.h"
#include "LinguaClientImpl.h"

// Undefine Windows macros that conflict with standard C++ and our code
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef GetLastError
#undef GetLastError
#endif

#include <cstring>
#include <algorithm>

namespace link_lingua {

// ============================================================================
// LinguaClientImpl Implementation
// ============================================================================

LinguaClientImpl::LinguaClientImpl()
    : running_(false), connected_(false), callReturned_(false), retValue_(false),
      lastResponseCommand_(static_cast<LinguaCommand>(0)) {}

LinguaClientImpl::~LinguaClientImpl() {
    Stop();
}

bool LinguaClientImpl::Start(const std::wstring& exePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&stdoutRead_, &stdoutWrite_, &saAttr, 0)) {
        lastError_ = "CreatePipe stdout failed";
        return false;
    }
    if (!SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0)) {}

    if (!CreatePipe(&stdinRead_, &stdinWrite_, &saAttr, 0)) {
        lastError_ = "CreatePipe stdin failed";
        CloseHandlePair();
        return false;
    }
    if (!SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0)) {}

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = stdoutWrite_;
    si.hStdOutput = stdoutWrite_;
    si.hStdInput = stdinRead_;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));
    std::wstring cmd = exePath;

    BOOL ok = CreateProcessW(
        nullptr,
        &cmd[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(stdoutWrite_);
    stdoutWrite_ = nullptr;
    CloseHandle(stdinRead_);
    stdinRead_ = nullptr;

    if (!ok) {
        lastError_ = "CreateProcess failed";
        CloseHandlePair();
        return false;
    }

    processHandle_ = pi.hProcess;
    threadHandle_ = pi.hThread;

    childStdout_ = stdoutRead_;
    childStdin_ = stdinWrite_;

    running_ = true;
    connected_ = true;
    readerThread_ = std::thread(&LinguaClientImpl::ReaderLoop, this);
    return true;
}

void LinguaClientImpl::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }

    if (childStdout_) CancelIoEx(childStdout_, nullptr);
    if (readerThread_.joinable()) readerThread_.join();

    if (childStdin_) { CloseHandle(childStdin_); childStdin_ = nullptr; }
    if (processHandle_) { WaitForSingleObject(processHandle_, 500); CloseHandle(processHandle_); processHandle_ = nullptr; }
    if (threadHandle_) { CloseHandle(threadHandle_); threadHandle_ = nullptr; }
    if (childStdout_) { CloseHandle(childStdout_); childStdout_ = nullptr; }

    CloseHandlePair();

    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        while (!messageQueue_.empty()) messageQueue_.pop();
    }
    queueCV_.notify_all();

    {
        std::lock_guard<std::mutex> lock(callMutex_);
        connected_ = false;
        callReturned_ = false;
        lastResponsePayload_.clear();
        lastResponseCommand_ = static_cast<LinguaCommand>(0);
        retValue_ = false;
    }
    callCV_.notify_all();
}

bool LinguaClientImpl::IsRunning() const {
    return running_.load();
}

bool LinguaClientImpl::SendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || !childStdin_) return false;
    std::string msg = message;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
    return WriteToPipe(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
}

// SendCommand - like CaptureClientWin::SendCommand
// Protocol: [size_t total_size][uint32_t cmd][payload...]
void LinguaClientImpl::SendCommand(LinguaCommand cmd, const void* data, size_t dataSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || !childStdin_) return;

    std::vector<uint8_t> sendBuffer;
    size_t totalSize = dataSize + sizeof(cmd);
    sendBuffer.resize(sizeof(totalSize) + sizeof(cmd) + dataSize);
    
    std::memcpy(sendBuffer.data(), &totalSize, sizeof(totalSize));
    std::memcpy(sendBuffer.data() + sizeof(totalSize), &cmd, sizeof(cmd));
    if (dataSize > 0 && data) {
        std::memcpy(sendBuffer.data() + sizeof(totalSize) + sizeof(cmd), data, dataSize);
    }
    
    WriteToPipe(sendBuffer.data(), sendBuffer.size());
}

bool LinguaClientImpl::ReceiveMessage(std::string& out, unsigned int timeoutMs) {
    std::unique_lock<std::mutex> lock(queueMutex_);
    if (!queueCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
        [this] { return !messageQueue_.empty() || !running_; })) return false;
    if (messageQueue_.empty()) return false;
    out = std::move(messageQueue_.front());
    messageQueue_.pop();
    return true;
}

void LinguaClientImpl::SetMessageCallback(MessageCallback cb) { 
    std::lock_guard<std::mutex> lock(callbackMutex_); 
    callback_ = cb; 
}

std::string LinguaClientImpl::GetLastError() const { 
    std::lock_guard<std::mutex> lock(mutex_); 
    return lastError_; 
}

// WaitForCall - like CaptureClientWin::WaitForCall
// Blocks until CallReturn() is called or connection is lost
bool LinguaClientImpl::WaitForCall() {
    std::unique_lock<std::mutex> lock(callMutex_);
    callCV_.wait(lock, [this] { return callReturned_ || !connected_; });
    bool ret = callReturned_;
    callReturned_ = false;
    return ret;
}

// CallReturn - like CaptureClientWin::CallReturn
// Called when a synchronous response is received
void LinguaClientImpl::CallReturn() {
    std::unique_lock<std::mutex> lock(callMutex_);
    callReturned_ = true;
    callCV_.notify_all();
}

LinguaCommand LinguaClientImpl::GetLastResponseCommand() const { 
    std::lock_guard<std::mutex> lock(callMutex_); 
    return lastResponseCommand_; 
}

const std::vector<uint8_t>& LinguaClientImpl::GetLastResponsePayload() const { 
    std::lock_guard<std::mutex> lock(callMutex_); 
    return lastResponsePayload_; 
}

bool LinguaClientImpl::GetLastRetValue() const { 
    std::lock_guard<std::mutex> lock(callMutex_); 
    return retValue_; 
}

size_t LinguaClientImpl::CopyResponseData(void* outBuffer, size_t maxBytes) const {
    std::lock_guard<std::mutex> lock(callMutex_);
    size_t toCopy = std::min(maxBytes, lastResponsePayload_.size());
    if (toCopy > 0 && outBuffer) {
        std::memcpy(outBuffer, lastResponsePayload_.data(), toCopy);
    }
    return toCopy;
}

bool LinguaClientImpl::WriteToPipe(const uint8_t* data, size_t size) {
    DWORD written = 0;
    const uint8_t* ptr = data;
    size_t remaining = size;
    while (remaining > 0) {
        BOOL ok = WriteFile(childStdin_, ptr, static_cast<DWORD>(remaining), &written, nullptr);
        if (!ok || written == 0) {
            lastError_ = "WriteToPipe failed";
            return false;
        }
        remaining -= written;
        ptr += written;
    }
    return true;
}

bool LinguaClientImpl::ReadExact(void* buf, size_t size) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(buf);
    size_t remaining = size;
    while (remaining > 0) {
        DWORD chunk = 0;
        BOOL ok = ReadFile(childStdout_, ptr, static_cast<DWORD>(remaining), &chunk, nullptr);
        if (!ok) {
            DWORD err = ::GetLastError();
            if (err == ERROR_BROKEN_PIPE) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (chunk == 0) return false;
        remaining -= chunk;
        ptr += chunk;
    }
    return true;
}

// NotifyReceiveData - like CaptureClientWin::NotifyReceiveData
// Processes received data and dispatches to appropriate handler
void LinguaClientImpl::NotifyReceiveData(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(LinguaCommand)) return;
    
    LinguaCommand cmd = *reinterpret_cast<const LinguaCommand*>(data.data());
    
    switch (cmd) {
        case kLinguaCommandPingComplete:
        case kLinguaCommandShutdownComplete:
            // Simple completion, just signal return
            CallReturn();
            break;
            
        case kLinguaCommandGetStatusComplete:
        case kLinguaCommandProcessTextComplete:
        case kLinguaCommandGetVersionComplete:
            // Response with data
            ProcessDataResponse(data);
            break;
            
        // Handle boolean responses
        // (if server sends bool result after command)
        default:
            // Check if it's a bool response
            if (data.size() >= sizeof(LinguaCommand) + sizeof(bool)) {
                ProcessBoolResponse(data);
            }
            break;
    }
}

void LinguaClientImpl::ProcessBoolResponse(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(LinguaCommand) + sizeof(bool)) return;
    
    lastResponseCommand_ = *reinterpret_cast<const LinguaCommand*>(buffer.data());
    retValue_ = *reinterpret_cast<const bool*>(buffer.data() + sizeof(LinguaCommand));
    CallReturn();
}

void LinguaClientImpl::ProcessDataResponse(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(LinguaCommand)) return;
    
    lastResponseCommand_ = *reinterpret_cast<const LinguaCommand*>(buffer.data());
    
    // Store payload (excluding command)
    size_t payloadSize = buffer.size() - sizeof(LinguaCommand);
    lastResponsePayload_.resize(payloadSize);
    if (payloadSize > 0) {
        std::memcpy(lastResponsePayload_.data(), 
                    buffer.data() + sizeof(LinguaCommand), 
                    payloadSize);
    }
    
    // Check if first byte of payload is bool for retValue_
    if (payloadSize >= sizeof(bool)) {
        retValue_ = lastResponsePayload_[0] != 0;
    }
    
    CallReturn();
}

// ReaderLoop - like CaptureClientWin::ReceiveThread
// Reads framed messages: [size_t size][payload...]
void LinguaClientImpl::ReaderLoop() {
    std::vector<uint8_t> receiveBuffer;
    
    while (running_) {
        // Read message size
        size_t size = 0;
        if (!ReadExact(&size, sizeof(size))) {
            break;
        }
        
        // Check for graceful disconnect signal
        if (size == SIZE_MAX) {
            break;
        }
        
        // Read payload
        receiveBuffer.resize(size);
        if (!ReadExact(receiveBuffer.data(), size)) {
            break;
        }
        
        // Process received data
        NotifyReceiveData(receiveBuffer);
    }
    
    // Signal disconnection
    {
        std::unique_lock<std::mutex> lock(callMutex_);
        connected_ = false;
        callCV_.notify_all();
    }
    
    running_ = false;
    queueCV_.notify_all();
}

void LinguaClientImpl::CloseHandlePair() {
    if (stdoutRead_) { CloseHandle(stdoutRead_); stdoutRead_ = nullptr; }
    if (stdoutWrite_) { CloseHandle(stdoutWrite_); stdoutWrite_ = nullptr; }
    if (stdinRead_) { CloseHandle(stdinRead_); stdinRead_ = nullptr; }
    if (stdinWrite_) { CloseHandle(stdinWrite_); stdinWrite_ = nullptr; }
}

// ============================================================================
// LinguaClient Public Interface Implementation
// ============================================================================

LinguaClient::LinguaClient() : impl_(new LinguaClientImpl()) {}

LinguaClient::~LinguaClient() {
    delete impl_;
}

bool LinguaClient::Start(const std::wstring& linguaExePath) {
    return impl_->Start(linguaExePath);
}

void LinguaClient::Stop() {
    impl_->Stop();
}

bool LinguaClient::IsRunning() const {
    return impl_->IsRunning();
}

bool LinguaClient::SendMessage(const std::string& message) {
    return impl_->SendMessage(message);
}

bool LinguaClient::ReceiveMessage(std::string& out, unsigned int timeoutMs) {
    return impl_->ReceiveMessage(out, timeoutMs);
}

void LinguaClient::SetMessageCallback(MessageCallback cb) {
    impl_->SetMessageCallback(cb);
}

std::string LinguaClient::GetLastError() const {
    return impl_->GetLastError();
}

// ========== Fire-and-forget commands (like SendMouseEvents) ==========

void LinguaClient::SendMouseEvent(const void* eventData, size_t dataSize) {
    impl_->SendCommand(kLinguaCommandSendMouseEvent, eventData, dataSize);
}

void LinguaClient::SendKeyboardEvent(const void* eventData, size_t dataSize) {
    impl_->SendCommand(kLinguaCommandSendKeyboardEvent, eventData, dataSize);
}

void LinguaClient::SendInputEvent(const void* eventData, size_t dataSize) {
    impl_->SendCommand(kLinguaCommandSendInputEvent, eventData, dataSize);
}

// ========== Synchronous commands (like CheckIfSupportSuchCodecType) ==========

bool LinguaClient::Ping() {
    impl_->SendCommand(kLinguaCommandPing, nullptr, 0);
    bool success = impl_->WaitForCall();
    return success;
}

bool LinguaClient::GetStatus(void* statusOut, size_t maxSize, size_t* actualSize) {
    impl_->SendCommand(kLinguaCommandGetStatus, nullptr, 0);
    bool success = impl_->WaitForCall();
    if (success && statusOut) {
        size_t copied = impl_->CopyResponseData(statusOut, maxSize);
        if (actualSize) *actualSize = copied;
    }
    return success && impl_->GetLastRetValue();
}

bool LinguaClient::ProcessText(const char* text, void* resultOut, size_t maxSize, size_t* actualSize) {
    size_t textLen = text ? strlen(text) + 1 : 0;
    impl_->SendCommand(kLinguaCommandProcessText, text, textLen);
    bool success = impl_->WaitForCall();
    if (success && resultOut) {
        size_t copied = impl_->CopyResponseData(resultOut, maxSize);
        if (actualSize) *actualSize = copied;
    }
    return success && impl_->GetLastRetValue();
}

bool LinguaClient::GetVersion(char* versionOut, size_t maxSize) {
    impl_->SendCommand(kLinguaCommandGetVersion, nullptr, 0);
    bool success = impl_->WaitForCall();
    if (success && versionOut && maxSize > 0) {
        size_t copied = impl_->CopyResponseData(versionOut, maxSize - 1);
        versionOut[copied] = '\0';
    }
    return success;
}

void LinguaClient::Shutdown() {
    impl_->SendCommand(kLinguaCommandShutdown, nullptr, 0);
    impl_->WaitForCall();
}

} // namespace link_lingua
