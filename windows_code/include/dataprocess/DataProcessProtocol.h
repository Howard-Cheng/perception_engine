#pragma once

#include <cstdint>
#include <cstddef>

namespace dataprocess {

// Command IDs for pipe communication between PerceptionEngine and DataProcessServer
enum DataProcessCommand : uint32_t {
    kCommandNone = 0,
    kCommandPing,                       // Ping to check if server is alive
    kCommandPingResponse,               // Response to ping
    kCommandProcessText,                // Process text data
    kCommandProcessTextComplete,        // Text processing complete
    kCommandProcessImage,               // Process image data
    kCommandProcessImageComplete,       // Image processing complete
    kCommandProcessAudio,               // Process audio data
    kCommandProcessAudioComplete,       // Audio processing complete
    kCommandShutdown,                   // Graceful shutdown
    kCommandMax
};

// Result codes
enum ResultCode : uint32_t {
    kResultSuccess = 0,
    kResultError = 1,
    kResultInvalidParam = 2,
    kResultTimeout = 3
};

// Message header for all pipe messages
#pragma pack(push, 1)
struct MessageHeader {
    DataProcessCommand command;
    uint32_t dataSize;      // Size of data following the header
    uint64_t timestamp;     // Timestamp in milliseconds
};
#pragma pack(pop)

// Text processing request
struct TextProcessRequest {
    char text[4096];
};

// Text processing response
struct TextProcessResponse {
    ResultCode result;
    char processedText[4096];
};

// Image processing request
struct ImageProcessRequest {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    // Image data follows in pipe
};

// Image processing response
struct ImageProcessResponse {
    ResultCode result;
    char description[1024];
};

// Audio processing request
struct AudioProcessRequest {
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t numSamples;
    // Audio data follows in pipe
};

// Audio processing response
struct AudioProcessResponse {
    ResultCode result;
    char transcription[2048];
};

} // namespace dataprocess
