#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <memory>
#include <chrono>
#include "dataprocess/DataProcessProtocol.h"
#include "dataprocess/TaskQueue.h"
#include "pe_base/logger.h"

namespace {

    // Read exactly n bytes from pipe
    bool ReadFromPipe(HANDLE hPipe, uint8_t* buffer, size_t size) {
        DWORD totalRead = 0;
        while (totalRead < size) {
            DWORD bytesRead = 0;
            if (!ReadFile(hPipe, buffer + totalRead, size - totalRead, &bytesRead, nullptr)) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    PE_INFO("Pipe closed by client");
                    return false;
                }
                PE_ERROR("ReadFile failed:" << error)
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

    // Write exactly n bytes to pipe
    bool WriteToPipe(HANDLE hPipe, const uint8_t* buffer, size_t size) {
        DWORD totalWritten = 0;
        while (totalWritten < size) {
            DWORD bytesWritten = 0;
            if (!WriteFile(hPipe, buffer + totalWritten, size - totalWritten, &bytesWritten, nullptr)) {
                PE_ERROR("WriteFile failed:" << GetLastError())
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
            PE_INFO("DataProcessServer started, waiting for commands...");

            while (running_.load()) {
                // Read message header
                dataprocess::MessageHeader header;
                if (!ReadFromPipe(hStdin_, reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
                    PE_INFO("Failed to read header, exiting...");
                    break;
                }

                // Check for shutdown command
                if (header.command == dataprocess::kCommandShutdown) {
                    PE_INFO("Received shutdown command");
                    SendShutdownAck();
                    break;
                }

                // Read payload if present
                std::vector<uint8_t> payload;
                if (header.dataSize > 0) {
                    payload.resize(header.dataSize);
                    if (!ReadFromPipe(hStdin_, payload.data(), header.dataSize)) {
                        PE_ERROR("Failed to read payload");
                        break;
                    }
                }

                // Process command
                ProcessCommand(header, payload);
            }

            PE_INFO("DataProcessServer exiting normally");
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
                PE_WARN("Unknown command:" << header.command)
                    break;
            }
        }

        void HandlePing() {
            taskQueue_->PostTask([this]() {
                PE_DEBUG("Handling ping request");
                SendResponse(dataprocess::kCommandPingResponse, nullptr, 0);
                });
        }

        void HandleProcessText(const std::vector<uint8_t>& payload) {
            taskQueue_->PostTask([this, payload]() {
                if (payload.size() < sizeof(dataprocess::TextProcessRequest)) {
                    PE_ERROR("Invalid text process request size");
                    return;
                }

                const auto* request = reinterpret_cast<const dataprocess::TextProcessRequest*>(payload.data());
                PE_ERROR("Processing text:" << request->text)

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
                    PE_ERROR("Invalid image process request size");
                    return;
                }

                const auto* request = reinterpret_cast<const dataprocess::ImageProcessRequest*>(payload.data());
                PE_ERROR("Processing image: " << request->width << request->height << ", channels = % u" <<
                    request->channels);

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
                    PE_ERROR("Invalid audio process request size");
                    return;
                }

                const auto* request = reinterpret_cast<const dataprocess::AudioProcessRequest*>(payload.data());
                PE_INFO_THIS("Processing audio: rate=" << request->sampleRate << ", channels = ,"
                    << request->channels << "samples = " << request->numSamples);

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
                PE_ERROR("Failed to write response header");
                return;
            }

            // Send payload if present
            if (dataSize > 0 && data != nullptr) {
                if (!WriteToPipe(hStdout_, data, dataSize)) {
                    PE_ERROR("Failed to write response payload");
                    return;
                }
            }

            PE_DEBUG_THIS("Sent response: cmd=" << cmd << ", size = " << dataSize);
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
    std::filesystem::path log_path = "";
    if (auto* p_appdata = getenv("APPDATA")) {
        log_path =
            std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
    }
    pe_base::LogWriter::SetLogFilePrefix(
        (log_path / "DataProcessServer").generic_string());

    PE_INFO("=====================================");
    PE_INFO("DataProcessServer v1.0");
    PE_INFO("=====================================");

    // Get standard input/output handles
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
        PE_ERROR("Failed to get standard handles");
        return 1;
    }

    try {
        DataProcessServer server(hStdin, hStdout);
        server.Run();
    }
    catch (const std::exception& e) {
        PE_ERROR("Exception:" << e.what())
        return 1;
    }

    PE_INFO("DataProcessServer exiting");
    return 0;
}
