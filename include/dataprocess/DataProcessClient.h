#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>
#include "dataprocess/DataProcessProtocol.h"

namespace dataprocess {

/**
 * @brief Client for communicating with DataProcessServer via pipes
 * Manages subprocess lifecycle and pipe communication
 */
class DataProcessClient {
public:
    using ResponseCallback = std::function<void(DataProcessCommand cmd, const uint8_t* data, size_t dataSize)>;

    DataProcessClient();
    ~DataProcessClient();

    // Start the DataProcessServer subprocess
    bool Start(const std::string& serverExePath);

    // Stop the DataProcessServer subprocess
    void Stop();

    // Check if server is running
    bool IsRunning() const { return isRunning_.load(); }

    // Send commands to server
    bool SendPing();
    bool SendProcessText(const char* text, ResponseCallback callback);
    bool SendProcessImage(uint32_t width, uint32_t height, uint32_t channels, 
                          const uint8_t* imageData, size_t imageSize, 
                          ResponseCallback callback);
    bool SendProcessAudio(uint32_t sampleRate, uint32_t channels, 
                          uint32_t numSamples, const float* audioData, 
                          ResponseCallback callback);

private:
    bool CreateServerProcess(const std::string& serverExePath);
    void ReceiveThread();
    bool SendCommand(DataProcessCommand cmd, const void* data, size_t dataSize);
    bool WriteToChildStdin(const uint8_t* buffer, size_t size);
    bool ReadFromChildStdout(uint8_t* buffer, size_t size);

    HANDLE hChildStdinRead_;
    HANDLE hChildStdinWrite_;
    HANDLE hChildStdoutRead_;
    HANDLE hChildStdoutWrite_;
    PROCESS_INFORMATION processInfo_;

    std::atomic<bool> isRunning_;
    std::unique_ptr<std::thread> receiveThread_;
    std::mutex writeMutex_;
    std::mutex callbackMutex_;
    ResponseCallback currentCallback_;
};

} // namespace dataprocess
