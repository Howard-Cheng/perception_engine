#pragma once

#include <windows.h>
#include <memory>
#include <string>
#include <vector>

#include "linguacore/LinguaCore.h"
#include "pe_base/task_queue/task_queue.h"

namespace linguacore {

/**
 * @brief Server-side implementation for LinguaCore subprocess
 * 
 * This class runs in the LinguaCore.exe subprocess and handles:
 * - Receiving commands from parent process via stdin pipe
 * - Processing NLP requests using LinguaCore
 * - Sending responses back via stdout pipe
 */
class LinguaServer {
public:
    /**
     * @brief Constructor
     * @param hStdout Handle to stdout for sending responses to parent
     * @param core LinguaCore instance for NLP processing
     */
    LinguaServer(HANDLE hStdout, std::shared_ptr<LinguaCore> core);
    ~LinguaServer();

    /**
     * @brief Process received data from parent process
     * @param data Raw data buffer containing command + payload
     */
    void NotifyReceiveData(const std::vector<uint8_t>& data);

private:
    // Command handlers
    void ProcessPing(const std::vector<uint8_t>& buffer);
    void ProcessGetStatus(const std::vector<uint8_t>& buffer);
    void ProcessProcessText(const std::vector<uint8_t>& buffer);
    void ProcessGetVersion(const std::vector<uint8_t>& buffer);
    void ProcessShutdown(const std::vector<uint8_t>& buffer);
    
    // Fire-and-forget input events (no response needed)
    void ProcessSendMouseEvent(const std::vector<uint8_t>& buffer);
    void ProcessSendKeyboardEvent(const std::vector<uint8_t>& buffer);
    void ProcessSendInputEvent(const std::vector<uint8_t>& buffer);

    /**
     * @brief Send command response to parent process
     * @param cmd Response command ID
     * @param data Payload data (can be nullptr)
     * @param size Payload size
     */
    void SendCommand(uint32_t cmd, const void* data, size_t size);

    /**
     * @brief Write data to pipe
     * @return true if successful, false otherwise
     */
    bool WriteToPipe(const uint8_t* data, size_t size);

private:
    HANDLE hStdout_;  ///< Stdout handle for writing to parent
    std::shared_ptr<LinguaCore> core_;  ///< LinguaCore instance
    pe_base::TaskQueue::Ptr_t taskQueue_;  ///< Task queue for serializing operations
    bool shouldShutdown_;  ///< Flag to signal shutdown
};

} // namespace linguacore
