#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include "dataprocess/DataProcessProtocol.h"
#include "dataprocess/TaskQueue.h"
#include "utils/Logger.h"

namespace {

// Read exactly n bytes from pipe
bool ReadFromPipe(HANDLE hPipe, uint8_t* buffer, size_t size) {
    DWORD totalRead = 0;
    while (totalRead < size) {
        DWORD bytesRead = 0;
        if (!ReadFile(hPipe, buffer + totalRead, size - totalRead, &bytesRead, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                LOG_INFO("Pipe closed by client");
                return false;
            }
            LOG_ERROR_FMT("ReadFile failed: %lu", error);
            return false;
        }
        if (bytesRead == 0) {
            LOG_WARN("ReadFile returned 0 bytes");
            return false;
        }
        totalRead += bytesRead;
    }
    return true;
}

// Write exactly n bytes to pipe
bool WriteToPipe(HANDLE hPipe, const uint8_t* buffer, size_t size) {
    DWORD totalWritten = 0;
    while (totalWritten < size) {
        DWORD bytesWritten = 0;
        if (!WriteFile(hPipe, buffer + totalWritten, size - totalWritten, &bytesWritten, nullptr)) {
            LOG_ERROR_FMT("WriteFile failed: %lu", GetLastError());
            return false;
        }
        totalWritten += bytesWritten;
    }
    return true;
}

class DataProcessServer {
public:
    explicit DataProcessServer(HANDLE hStdin, HANDLE hStdout)
        : hStdin_(hStdin), hStdout_(hStdout), running_(true) {
        taskQueue_ = std::make_unique<dataprocess::TaskQueue>("DataProcessServer");
    }

    ~DataProcessServer() {
        running_.store(false);
        if (taskQueue_) {
            taskQueue_->Stop();
        }
    }

    void Run() {
        LOG_INFO("DataProcessServer started, waiting for commands...");

        while (running_.load()) {
            // Read message header
            dataprocess::MessageHeader header;
            if (!ReadFromPipe(hStdin_, reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
                LOG_INFO("Failed to read header, exiting...");
                break;
            }

            // Check for shutdown command
            if (header.command == dataprocess::kCommandShutdown) {
                LOG_INFO("Received shutdown command");
                SendShutdownAck();
                break;
            }

            // Read payload if present
            std::vector<uint8_t> payload;
            if (header.dataSize > 0) {
                payload.resize(header.dataSize);
                if (!ReadFromPipe(hStdin_, payload.data(), header.dataSize)) {
                    LOG_ERROR("Failed to read payload");
                    break;
                }
            }

            // Process command
            ProcessCommand(header, payload);
        }

        LOG_INFO("DataProcessServer exiting normally");
    }

private:
    void ProcessCommand(const dataprocess::MessageHeader& header, const std::vector<uint8_t>& payload) {
        switch (header.command) {
            case dataprocess::kCommandPing:
                HandlePing();
                break;
            case dataprocess::kCommandProcessText:
                HandleProcessText(payload);
                break;
            case dataprocess::kCommandProcessImage:
                HandleProcessImage(payload);
                break;
            case dataprocess::kCommandProcessAudio:
                HandleProcessAudio(payload);
                break;
            default:
                LOG_WARN_FMT("Unknown command: %u", header.command);
                break;
        }
    }

    void HandlePing() {
        taskQueue_->PostTask([this]() {
            LOG_DEBUG("Handling ping request");
            SendResponse(dataprocess::kCommandPingResponse, nullptr, 0);
        });
    }

    void HandleProcessText(const std::vector<uint8_t>& payload) {
        taskQueue_->PostTask([this, payload]() {
            if (payload.size() < sizeof(dataprocess::TextProcessRequest)) {
                LOG_ERROR("Invalid text process request size");
                return;
            }

            const auto* request = reinterpret_cast<const dataprocess::TextProcessRequest*>(payload.data());
            LOG_INFO_FMT("Processing text: %s", request->text);

            // Simulate text processing (e.g., NLP, sentiment analysis, etc.)
            dataprocess::TextProcessResponse response;
            response.result = dataprocess::kResultSuccess;
            snprintf(response.processedText, sizeof(response.processedText),
                     "PROCESSED: %s", request->text);

            SendResponse(dataprocess::kCommandProcessTextComplete, 
                         reinterpret_cast<const uint8_t*>(&response), 
                         sizeof(response));
        });
    }

    void HandleProcessImage(const std::vector<uint8_t>& payload) {
        taskQueue_->PostTask([this, payload]() {
            if (payload.size() < sizeof(dataprocess::ImageProcessRequest)) {
                LOG_ERROR("Invalid image process request size");
                return;
            }

            const auto* request = reinterpret_cast<const dataprocess::ImageProcessRequest*>(payload.data());
            LOG_INFO_FMT("Processing image: %ux%u, channels=%u", 
                         request->width, request->height, request->channels);

            // Simulate image processing (e.g., object detection, OCR, etc.)
            dataprocess::ImageProcessResponse response;
            response.result = dataprocess::kResultSuccess;
            snprintf(response.description, sizeof(response.description),
                     "Detected: image of size %ux%u", request->width, request->height);

            SendResponse(dataprocess::kCommandProcessImageComplete, 
                         reinterpret_cast<const uint8_t*>(&response), 
                         sizeof(response));
        });
    }

    void HandleProcessAudio(const std::vector<uint8_t>& payload) {
        taskQueue_->PostTask([this, payload]() {
            if (payload.size() < sizeof(dataprocess::AudioProcessRequest)) {
                LOG_ERROR("Invalid audio process request size");
                return;
            }

            const auto* request = reinterpret_cast<const dataprocess::AudioProcessRequest*>(payload.data());
            LOG_INFO_FMT("Processing audio: rate=%u, channels=%u, samples=%u", 
                         request->sampleRate, request->channels, request->numSamples);

            // Simulate audio processing (e.g., speech recognition, music analysis, etc.)
            dataprocess::AudioProcessResponse response;
            response.result = dataprocess::kResultSuccess;
            snprintf(response.transcription, sizeof(response.transcription),
                     "Transcribed: audio with %u samples", request->numSamples);

            SendResponse(dataprocess::kCommandProcessAudioComplete, 
                         reinterpret_cast<const uint8_t*>(&response), 
                         sizeof(response));
        });
    }

    void SendResponse(dataprocess::DataProcessCommand cmd, const uint8_t* data, size_t dataSize) {
        std::lock_guard<std::mutex> lock(writeMutex_);

        // Send header
        dataprocess::MessageHeader header;
        header.command = cmd;
        header.dataSize = static_cast<uint32_t>(dataSize);
        header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!WriteToPipe(hStdout_, reinterpret_cast<const uint8_t*>(&header), sizeof(header))) {
            LOG_ERROR("Failed to write response header");
            return;
        }

        // Send payload if present
        if (dataSize > 0 && data != nullptr) {
            if (!WriteToPipe(hStdout_, data, dataSize)) {
                LOG_ERROR("Failed to write response payload");
                return;
            }
        }

        LOG_DEBUG_FMT("Sent response: cmd=%u, size=%zu", cmd, dataSize);
    }

    void SendShutdownAck() {
        SendResponse(dataprocess::kCommandShutdown, nullptr, 0);
    }

    HANDLE hStdin_;
    HANDLE hStdout_;
    std::atomic<bool> running_;
    std::unique_ptr<dataprocess::TaskQueue> taskQueue_;
    std::mutex writeMutex_;
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Initialize logger
    Logger::GetInstance().Initialize("DataProcessServer.log", LogLevel::INFO_L);
    
    LOG_INFO("=====================================");
    LOG_INFO("DataProcessServer v1.0");
    LOG_INFO("=====================================");

    // Get standard input/output handles
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
        LOG_FATAL("Failed to get standard handles");
        return 1;
    }

    try {
        DataProcessServer server(hStdin, hStdout);
        server.Run();
    } catch (const std::exception& e) {
        LOG_FATAL_FMT("Exception: %s", e.what());
        Logger::GetInstance().Shutdown();
        return 1;
    }

    LOG_INFO("DataProcessServer exiting");
    Logger::GetInstance().Shutdown();
    return 0;
}
