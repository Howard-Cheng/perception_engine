#include "dataprocess/DataProcessClient.h"
#include "pe_base/logger.h"
#include <chrono>
#include <iostream>
#include <filesystem>

namespace dataprocess {

    DataProcessClient::DataProcessClient()
        : hChildStdinRead_(nullptr),
        hChildStdinWrite_(nullptr),
        hChildStdoutRead_(nullptr),
        hChildStdoutWrite_(nullptr),
        processInfo_{ 0 },
        isRunning_(false) {
    }

    DataProcessClient::~DataProcessClient() {
        Stop();
    }

    std::filesystem::path DllPath() {
        HMODULE hm = nullptr;
        wchar_t buffer[MAX_PATH];

        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&DllPath, &hm) == 0) {
            DWORD ret = GetLastError();
            std::cout << "GetModuleHandle failed, error = " << ret << std::endl;
            return "";
        }

        GetModuleFileNameW(hm, buffer, MAX_PATH);
        std::filesystem::path path = buffer;
        return path.parent_path();
    }

    bool DataProcessClient::Start(const std::string& serverExePath) {
        if (isRunning_.load()) {
            PE_WARN("DataProcessClient already running");
            return false;
        }

        PE_INFO_THIS("Starting DataProcessServer from: " << serverExePath.c_str())

            if (!CreateServerProcess(serverExePath)) {
                PE_ERROR("Failed to create server process");
                return false;
            }

        isRunning_.store(true);

        // Start receive thread
        receiveThread_ = std::make_unique<std::thread>(&DataProcessClient::ReceiveThread, this);

        PE_INFO("DataProcessClient started successfully");
        return true;
    }

    void DataProcessClient::Stop() {
        if (!isRunning_.load()) {
            return;
        }

        PE_INFO("Stopping DataProcessClient...");

        // Send shutdown command
        SendCommand(kCommandShutdown, nullptr, 0);

        // Wait a bit for graceful shutdown
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        isRunning_.store(false);

        // Wait for receive thread
        if (receiveThread_ && receiveThread_->joinable()) {
            receiveThread_->join();
            PE_INFO("Receive thread joined");
        }

        // Close handles
        if (hChildStdinWrite_) {
            CloseHandle(hChildStdinWrite_);
            hChildStdinWrite_ = nullptr;
        }
        if (hChildStdoutRead_) {
            CloseHandle(hChildStdoutRead_);
            hChildStdoutRead_ = nullptr;
        }

        // Terminate process if still running
        if (processInfo_.hProcess) {
            DWORD exitCode;
            if (GetExitCodeProcess(processInfo_.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                PE_WARN("Process still running, terminating...");
                TerminateProcess(processInfo_.hProcess, 1);
            }
            CloseHandle(processInfo_.hProcess);
            CloseHandle(processInfo_.hThread);
            processInfo_ = { 0 };
        }

        PE_INFO("DataProcessClient stopped");
    }

    bool DataProcessClient::CreateServerProcess(const std::string& serverExePath) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hChildStdoutWrite = nullptr;
        HANDLE hChildStdinRead = nullptr;

        // Create pipes for stdin
        if (!CreatePipe(&hChildStdinRead, &hChildStdinWrite_, &sa, 1024 * 1024)) {
            PE_ERROR_THIS("CreatePipe for stdin failed: " << GetLastError())
                return false;
        }

        // Ensure write handle is not inherited
        if (!SetHandleInformation(hChildStdinWrite_, HANDLE_FLAG_INHERIT, 0)) {
            PE_ERROR_THIS("SetHandleInformation for stdin failed:  " << GetLastError())
                CloseHandle(hChildStdinRead);
            CloseHandle(hChildStdinWrite_);
            return false;
        }

        // Create pipes for stdout
        if (!CreatePipe(&hChildStdoutRead_, &hChildStdoutWrite, &sa, 1024 * 1024)) {
            PE_ERROR_THIS("CreatePipe for stdout failed:  " << GetLastError())
                CloseHandle(hChildStdinRead);
            CloseHandle(hChildStdinWrite_);
            return false;
        }

        // Ensure read handle is not inherited
        if (!SetHandleInformation(hChildStdoutRead_, HANDLE_FLAG_INHERIT, 0)) {
            PE_ERROR_THIS("SetHandleInformation for stdout failed:  " << GetLastError())
                CloseHandle(hChildStdinRead);
            CloseHandle(hChildStdinWrite_);
            CloseHandle(hChildStdoutRead_);
            CloseHandle(hChildStdoutWrite);
            return false;
        }

        // Setup startup info
        STARTUPINFOA si = { 0 };
        si.cb = sizeof(STARTUPINFO);
        si.hStdError = hChildStdoutWrite;  // Redirect stderr to stdout
        si.hStdOutput = hChildStdoutWrite;
        si.hStdInput = hChildStdinRead;
        si.dwFlags |= STARTF_USESTDHANDLES;

        // Build full path to server executable
        std::filesystem::path fullServerPath = DllPath() / serverExePath;
        std::string serverPathStr = fullServerPath.string();

        // Create process
        BOOL success = CreateProcessA(
            serverPathStr.c_str(),  // Application name (fixed: proper path combination)
            nullptr,                // Command line
            nullptr,                // Process security attributes
            nullptr,                // Thread security attributes
            TRUE,                   // Inherit handles
            CREATE_NO_WINDOW,       // Creation flags - no console window
            nullptr,                // Environment
            nullptr,                // Current directory
            &si,                    // Startup info
            &processInfo_           // Process info
        );

        // Close child's ends of pipes (parent doesn't need them)
        CloseHandle(hChildStdinRead);
        CloseHandle(hChildStdoutWrite);

        if (!success) {
            DWORD error = GetLastError();
            PE_ERROR_THIS("CreateProcess failed:  " << error)
                CloseHandle(hChildStdinWrite_);
            CloseHandle(hChildStdoutRead_);
            hChildStdinWrite_ = nullptr;
            hChildStdoutRead_ = nullptr;
            return false;
        }

        PE_INFO_THIS("DataProcessServer process created, PID: " << processInfo_.dwProcessId)
            return true;
    }

    bool DataProcessClient::SendCommand(DataProcessCommand cmd, const void* data, size_t dataSize) {
        std::lock_guard<std::mutex> lock(writeMutex_);

        // Build header
        MessageHeader header;
        header.command = cmd;
        header.dataSize = static_cast<uint32_t>(dataSize);
        header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Write header
        if (!WriteToChildStdin(reinterpret_cast<const uint8_t*>(&header), sizeof(header))) {
            PE_ERROR("Failed to write command header");
            return false;
        }

        // Write payload if present
        if (dataSize > 0 && data != nullptr) {
            if (!WriteToChildStdin(reinterpret_cast<const uint8_t*>(data), dataSize)) {
                PE_ERROR("Failed to write command payload");
                return false;
            }
        }

        PE_DEBUG_THIS("Sent command: " << cmd << ", size: " << dataSize);
        return true;
    }

    bool DataProcessClient::WriteToChildStdin(const uint8_t* buffer, size_t size) {
        DWORD totalWritten = 0;
        while (totalWritten < size) {
            DWORD bytesWritten = 0;
            DWORD sizeToWrite = static_cast<DWORD>(size - totalWritten);
            if (!WriteFile(hChildStdinWrite_, buffer + totalWritten, sizeToWrite, &bytesWritten, nullptr)) {
                PE_ERROR_THIS("WriteFile failed: " << GetLastError())
                    return false;
            }
            totalWritten += bytesWritten;
        }
        return true;
    }

    bool DataProcessClient::ReadFromChildStdout(uint8_t* buffer, size_t size) {
        DWORD totalRead = 0;
        while (totalRead < size) {
            DWORD bytesRead = 0;
            DWORD sizeToRead = static_cast<DWORD>(size - totalRead);
            if (!ReadFile(hChildStdoutRead_, buffer + totalRead, sizeToRead, &bytesRead, nullptr)) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    PE_INFO("Server closed pipe");
                    return false;
                }
                PE_ERROR_THIS("ReadFile failed: " << error)
                    return false;
            }
            if (bytesRead == 0) {
                PE_WARN("ReadFile returned 0 bytes");
                return false;
            }
            totalRead += bytesRead;
        }
        return true;
    }

    void DataProcessClient::ReceiveThread() {
        PE_INFO("Receive thread started");

        while (isRunning_.load()) {
            // Read header
            MessageHeader header;
            if (!ReadFromChildStdout(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
                PE_INFO("Failed to read header, exiting receive thread");
                break;
            }

            // Read payload if present
            std::vector<uint8_t> payload;
            if (header.dataSize > 0) {
                payload.resize(header.dataSize);
                if (!ReadFromChildStdout(payload.data(), header.dataSize)) {
                    PE_ERROR("Failed to read payload");
                    break;
                }
            }

            PE_DEBUG_THIS("Received response: cmd= " << header.command << ", size = " << header.dataSize);

            // Call callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (currentCallback_) {
                    currentCallback_(header.command, payload.data(), payload.size());
                    currentCallback_ = nullptr;
                }
            }
        }

        PE_INFO("Receive thread exiting");
    }

    bool DataProcessClient::SendPing() {
        return SendCommand(kCommandPing, nullptr, 0);
    }

    bool DataProcessClient::SendProcessText(const char* text, ResponseCallback callback) {
        if (!isRunning_.load()) {
            PE_ERROR("Client not running");
            return false;
        }

        TextProcessRequest request;
        strncpy_s(request.text, sizeof(request.text), text, _TRUNCATE);

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            currentCallback_ = callback;
        }

        return SendCommand(kCommandProcessText, &request, sizeof(request));
    }

    bool DataProcessClient::SendProcessImage(uint32_t width, uint32_t height, uint32_t channels,
        const uint8_t* imageData, size_t imageSize,
        ResponseCallback callback) {
        if (!isRunning_.load()) {
            PE_ERROR("Client not running");
            return false;
        }

        ImageProcessRequest request;
        request.width = width;
        request.height = height;
        request.channels = channels;

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            currentCallback_ = callback;
        }

        // For now, just send the request header
        // In a real implementation, you would also send the image data
        return SendCommand(kCommandProcessImage, &request, sizeof(request));
    }

    bool DataProcessClient::SendProcessAudio(uint32_t sampleRate, uint32_t channels,
        uint32_t numSamples, const float* audioData,
        ResponseCallback callback) {
        if (!isRunning_.load()) {
            PE_ERROR("Client not running");
            return false;
        }

        AudioProcessRequest request;
        request.sampleRate = sampleRate;
        request.channels = channels;
        request.numSamples = numSamples;

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            currentCallback_ = callback;
        }

        // For now, just send the request header
        // In a real implementation, you would also send the audio data
        return SendCommand(kCommandProcessAudio, &request, sizeof(request));
    }

} // namespace dataprocess
