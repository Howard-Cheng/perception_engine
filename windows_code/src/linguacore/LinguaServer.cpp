#include "linguacore/LinguaServer.h"
#include "link_lingua/LinguaClientImpl.h"  // For LinguaCommand enum
#include "pe_base/logger.h"
#include "pe_base/thread_sync.h"
#include <cstring>

namespace linguacore {

    LinguaServer::LinguaServer(HANDLE hStdout, std::shared_ptr<LinguaCore> core)
        : hStdout_(hStdout), core_(core), shouldShutdown_(false) {
        // Create task queue for serializing command processing
        taskQueue_ = pe_base::TaskQueue::Create(1, "LinguaServerQueue");
        PE_INFO("LinguaServer initialized");
    }

    LinguaServer::~LinguaServer() {
        PE_INFO("LinguaServer shutting down");
        pe_base::ThreadEvent complete;
        taskQueue_->PostTask([this, &complete] {
            size_t finish = MAXSIZE_T;
            WriteToPipe(reinterpret_cast<uint8_t*>(&finish),
                sizeof(finish));
            complete.Set();
            });
        complete.Wait();
    }

    void LinguaServer::NotifyReceiveData(const std::vector<uint8_t>& data) {
        // Post to task queue for thread-safe processing
        taskQueue_->PostTask([this, buffer = data]() {
            link_lingua::LinguaCommand cmd =
                *reinterpret_cast<const link_lingua::LinguaCommand*>(buffer.data());

            PE_DEBUG("Processing command: " << static_cast<uint32_t>(cmd));

            switch (cmd) {
            case link_lingua::kLinguaCommandPing:
                ProcessPing(buffer);
                break;
            case link_lingua::kLinguaCommandGetStatus:
                ProcessGetStatus(buffer);
                break;
            case link_lingua::kLinguaCommandProcessText:
                ProcessProcessText(buffer);
                break;
            case link_lingua::kLinguaCommandGetVersion:
                ProcessGetVersion(buffer);
                break;
            case link_lingua::kLinguaCommandShutdown:
                ProcessShutdown(buffer);
                break;
            case link_lingua::kLinguaCommandSendMouseEvent:
                ProcessSendMouseEvent(buffer);
                break;
            case link_lingua::kLinguaCommandSendKeyboardEvent:
                ProcessSendKeyboardEvent(buffer);
                break;
            case link_lingua::kLinguaCommandSendInputEvent:
                ProcessSendInputEvent(buffer);
                break;
            default:
                PE_WARN("Unknown command: " << static_cast<uint32_t>(cmd));
                break;
            }
            });
    }

    void LinguaServer::ProcessPing(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing Ping command");
        // Simple ping response - just send completion
        SendCommand(link_lingua::kLinguaCommandPingComplete, nullptr, 0);
    }

    void LinguaServer::ProcessGetStatus(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing GetStatus command");

        // Get status from LinguaCore
        std::string statusJson = core_->getStatus();

        // Send response: [bool success] + [status string]
        std::vector<uint8_t> response(sizeof(bool) + statusJson.size() + 1);
        bool success = true;
        std::memcpy(response.data(), &success, sizeof(bool));
        std::memcpy(response.data() + sizeof(bool), statusJson.c_str(), statusJson.size() + 1);

        SendCommand(link_lingua::kLinguaCommandGetStatusComplete, response.data(), response.size());
    }

    void LinguaServer::ProcessProcessText(const std::vector<uint8_t>& buffer) {
        if (buffer.size() <= sizeof(link_lingua::LinguaCommand)) {
            PE_ERROR("ProcessText: Invalid buffer size");
            bool success = false;
            SendCommand(link_lingua::kLinguaCommandProcessTextComplete, &success, sizeof(bool));
            return;
        }

        // Extract text from buffer
        const char* text = reinterpret_cast<const char*>(
            buffer.data() + sizeof(link_lingua::LinguaCommand));

        PE_DEBUG("Processing text: " << text);

        // Process with LinguaCore
        std::string result = core_->processText(text);

        // Send response: [bool success] + [result string]
        std::vector<uint8_t> response(sizeof(bool) + result.size() + 1);
        bool success = true;
        std::memcpy(response.data(), &success, sizeof(bool));
        std::memcpy(response.data() + sizeof(bool), result.c_str(), result.size() + 1);

        SendCommand(link_lingua::kLinguaCommandProcessTextComplete, response.data(), response.size());
    }

    void LinguaServer::ProcessGetVersion(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing GetVersion command");

        // Get version from LinguaCore
        std::string version = core_->getVersion();

        // Send version string (no bool prefix needed for this command)
        SendCommand(link_lingua::kLinguaCommandGetVersionComplete,
            version.c_str(), version.size() + 1);
    }

    void LinguaServer::ProcessShutdown(const std::vector<uint8_t>& buffer) {
        PE_INFO("Processing Shutdown command");

        shouldShutdown_ = true;

        // Send shutdown complete
        SendCommand(link_lingua::kLinguaCommandShutdownComplete, nullptr, 0);
    }

    void LinguaServer::ProcessSendMouseEvent(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing mouse event (fire-and-forget)");
        // Mouse events are fire-and-forget, no response needed
        // Could forward to QtCoreManager if needed
    }

    void LinguaServer::ProcessSendKeyboardEvent(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing keyboard event (fire-and-forget)");
        // Keyboard events are fire-and-forget, no response needed
    }

    void LinguaServer::ProcessSendInputEvent(const std::vector<uint8_t>& buffer) {
        PE_DEBUG("Processing input event (fire-and-forget)");
        // Input events are fire-and-forget, no response needed
    }

    void LinguaServer::SendCommand(uint32_t cmd, const void* data, size_t size) {
        // Protocol: [size_t total_size] [payload]
        // payload = [uint32_t cmd] [data...]

        size_t totalSize = sizeof(cmd) + size;
        std::vector<uint8_t> sendBuffer(sizeof(totalSize) + totalSize);

        std::memcpy(sendBuffer.data(), &totalSize, sizeof(totalSize));
        std::memcpy(sendBuffer.data() + sizeof(totalSize), &cmd, sizeof(cmd));

        if (size > 0 && data) {
            std::memcpy(sendBuffer.data() + sizeof(totalSize) + sizeof(cmd), data, size);
        }

        if (!WriteToPipe(sendBuffer.data(), sendBuffer.size())) {
            PE_ERROR("Failed to send command: " << cmd);
        }
        else {
            PE_INFO("Write success, send command:" << cmd)
        }
    }

    bool LinguaServer::WriteToPipe(const uint8_t* data, size_t size) {
        DWORD written = 0;
        const uint8_t* ptr = data;
        size_t remaining = size;

        while (remaining > 0) {
            BOOL ok = WriteFile(hStdout_, ptr, static_cast<DWORD>(remaining), &written, nullptr);
            if (!ok || written == 0) {
                DWORD err = ::GetLastError();
                PE_ERROR("WriteToPipe failed, error: " << err);
                return false;
            }
            remaining -= written;
            ptr += written;
        }

        return true;
    }

} // namespace linguacore
