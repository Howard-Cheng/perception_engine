#include "link_lingua/LinkLingua.h"
#include "link_lingua/LinguaClientImpl.h"
//#include "pe_base/logger.h"

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
#include <filesystem>

namespace link_lingua {

    // ============================================================================
    // LinguaClientImpl Implementation
    // ============================================================================

    LinguaClientImpl::LinguaClientImpl()
        : running_(false), connected_(false), callReturned_(false), retValue_(false),
        lastResponseCommand_(static_cast<LinguaCommand>(0)) {
        // Create task queue for serializing response processing
        taskQueue_ = pe_base::TaskQueue::Create(1, "LinguaClientQueue");
    }

    LinguaClientImpl::~LinguaClientImpl() {
        taskQueue_->PostTask([this] {
            size_t finish = MAXSIZE_T;
            WriteToPipe(reinterpret_cast<uint8_t*>(&finish), sizeof(finish));
            });
        if (readerThread_.joinable()) {
            //PS_INFO("send finish signal")
            readerThread_.join();
            //PS_INFO("capture process exited")
        }
        else {
            //PS_ERROR("cannot join receive thread")
        }
        if (h_child_stdin_write_) {
            CloseHandle(h_child_stdin_write_);
        }
        if (h_child_stdout_read_) {
            CloseHandle(h_child_stdout_read_);
        }
        if (lingua_process_.hProcess) {
            CloseHandle(lingua_process_.hProcess);
            CloseHandle(lingua_process_.hThread);
        }
    }

    std::filesystem::path DllPath() {
        HMODULE hm = nullptr;
        wchar_t buffer[MAX_PATH];

        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&DllPath, &hm) == 0) {
            DWORD ret = GetLastError();
            //PS_ERROR("GetModuleHandle failed, error = " << ret)
            return "";
        }

        GetModuleFileNameW(hm, buffer, MAX_PATH);
        std::filesystem::path path = buffer;
        return path.parent_path();
    }

    bool LinguaClientImpl::Start(const std::wstring& exePath) {
        SECURITY_ATTRIBUTES sa;
        STARTUPINFOW si;
        HANDLE h_child_stdin = nullptr;
        HANDLE h_child_stdout = nullptr;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&lingua_process_, sizeof(lingua_process_));

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&h_child_stdout_read_, &h_child_stdout, &sa, 1024 * 1024)) {
            //PS_ERROR("create child stdout fail")
            goto cleanup;
        }

        if (!SetHandleInformation(h_child_stdout_read_, HANDLE_FLAG_INHERIT, 0)) {
            //PS_ERROR("config child stdout fail")
            goto cleanup;
        }

        if (!CreatePipe(&h_child_stdin, &h_child_stdin_write_, &sa, 1024 * 1024)) {
            //PS_ERROR("create child stdin fail")
            goto cleanup;
        }

        if (!SetHandleInformation(h_child_stdin_write_, HANDLE_FLAG_INHERIT, 0)) {
            //PS_ERROR("config child stdin fail")
            goto cleanup;
        }

        si.cb = sizeof(STARTUPINFO);
        si.hStdError = INVALID_HANDLE_VALUE;
        si.hStdOutput = h_child_stdout;
        si.hStdInput = h_child_stdin;
        si.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcessW(
            (DllPath() / exePath)
            .wstring()
            .c_str(),       // No module name (use command line)
            nullptr,            // Command line
            nullptr,            // Process handle not inheritable
            nullptr,            // Thread handle not inheritable
            TRUE,               // Set handle inheritance to FALSE
            CREATE_NO_WINDOW,   // No creation flags
            nullptr,            // Use parent's environment block
            nullptr,            // Use parent's starting directory
            &si,                // Pointer to STARTUPINFO structure
            &lingua_process_)  // Pointer to PROCESS_INFORMATION structure
            ) {
            //PS_ERROR("create administrator process failed " << GetLastError())
            goto cleanup;
        }

        CloseHandle(h_child_stdin);
        CloseHandle(h_child_stdout);

        running_ = true;
        connected_ = true;
        readerThread_ = std::thread(&LinguaClientImpl::ReaderLoop, this);
        return true;
    cleanup:
        if (h_child_stdin) {
            CloseHandle(h_child_stdin);
        }
        if (h_child_stdout) {
            CloseHandle(h_child_stdout);
        }
        if (h_child_stdin_write_) {
            CloseHandle(h_child_stdin_write_);
            h_child_stdin_write_ = nullptr;
        }
        if (h_child_stdout_read_) {
            CloseHandle(h_child_stdout_read_);
            h_child_stdout_read_ = nullptr;
        }
        return false;
    }

    void LinguaClientImpl::Stop() {
    }

    bool LinguaClientImpl::IsRunning() const {
        return running_.load();
    }

    bool LinguaClientImpl::SendMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || !h_child_stdin_write_) return false;
        std::string msg = message;
        if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
        return WriteToPipe(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
    }

    // SendCommand - like CaptureClientWin::SendCommand
    // Protocol: [size_t total_size][uint32_t cmd][payload...]
    void LinguaClientImpl::SendCommand(LinguaCommand cmd, const void* data, size_t dataSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || !h_child_stdin_write_) return;

        std::vector<uint8_t> sendBuffer;
        size_t totalSize = dataSize + sizeof(cmd);
        sendBuffer.resize(sizeof(totalSize) + sizeof(cmd) + dataSize);

        std::memcpy(sendBuffer.data(), &totalSize, sizeof(totalSize));
        std::memcpy(sendBuffer.data() + sizeof(totalSize), &cmd, sizeof(cmd));
        if (dataSize > 0 && data) {
            std::memcpy(sendBuffer.data() + sizeof(totalSize) + sizeof(cmd), data, dataSize);
        }

        if (taskQueue_->IsQueueThread()) {
            WriteToPipe(sendBuffer.data(), sendBuffer.size());
        }
        else {
            taskQueue_->PostTask([this, sendBuffer] {
                WriteToPipe(sendBuffer.data(), sendBuffer.size());
                });
        }

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
            BOOL ok = WriteFile(h_child_stdin_write_, ptr, static_cast<DWORD>(remaining), &written, nullptr);
            if (!ok || written == 0) {
                lastError_ = "WriteToPipe failed";
                return false;
            }
            remaining -= written;
            ptr += written;
        }
        return true;
    }

    bool LinguaClientImpl::ReadExact(uint8_t* p_buffer, size_t size) {
        size_t cursor = 0;
        while (cursor < size) {
            DWORD dw_read;
            BOOL success =
                ReadFile(h_child_stdout_read_, p_buffer + cursor, size - cursor, &dw_read, nullptr);
            if (!success) {
                //PS_ERROR("")
                return false;
            }
            cursor += dw_read;
        }
        return true;
    }

    // NotifyReceiveData - like CaptureClientWin::NotifyReceiveData
    // Processes received data and dispatches to appropriate handler
    void LinguaClientImpl::NotifyReceiveData(const std::vector<uint8_t>& data) {
        // Post to task queue for thread-safe processing
        taskQueue_->PostTask([this, buffer = data]() {
            if (buffer.size() < sizeof(LinguaCommand)) return;

            LinguaCommand cmd = *reinterpret_cast<const LinguaCommand*>(buffer.data());

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
                ProcessDataResponse(buffer);
                break;

                // Handle boolean responses
                // (if server sends bool result after command)
            default:
                // Check if it's a bool response
                if (buffer.size() >= sizeof(LinguaCommand) + sizeof(bool)) {
                    ProcessBoolResponse(buffer);
                }
                break;
            }
            });
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

        // Maximum reasonable message size (10 MB)
        constexpr size_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024;

        while (running_) {
            // Read message size
            size_t size;
            if (!ReadExact(reinterpret_cast<uint8_t*>(&size),
                sizeof(size))) {
                break;
            }
            if (size == MAXSIZE_T) {
                //PS_INFO("graceful disconnected")
                break;
            }
            receiveBuffer.resize(size);
            if (!ReadExact(receiveBuffer.data(), size)) {
                break;
            }
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
