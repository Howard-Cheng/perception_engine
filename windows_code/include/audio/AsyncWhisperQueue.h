#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <chrono>

// Forward declaration for whisper.cpp
struct whisper_context;

/**
 * AsyncWhisperQueue - Non-blocking whisper transcription queue
 *
 * Allows audio capture to continue while transcription happens in background.
 * Critical for long-form speech where transcription latency would otherwise
 * block speech detection.
 *
 * Usage:
 *   AsyncWhisperQueue queue(whisperContext);
 *   queue.QueueAudio(audioData);  // Non-blocking
 *   std::string result = queue.GetLatestResult();  // Returns last completed
 */
class AsyncWhisperQueue {
public:
    explicit AsyncWhisperQueue(whisper_context* ctx);
    ~AsyncWhisperQueue();

    // Queue audio for transcription (non-blocking)
    void QueueAudio(const std::vector<float>& audio);

    // Get latest completed transcription (non-blocking)
    // Returns empty string if no new results
    std::string GetLatestResult();

    // Check if actively transcribing
    bool IsProcessing() const;

    // Get queue size
    size_t GetQueueSize() const;

    // Get total processed count
    size_t GetProcessedCount() const;

    // Get last transcription latency in milliseconds
    float GetLastLatencyMs() const;

private:
    void WorkerThread();
    std::string TranscribeAudio(const std::vector<float>& audioData);

    // Whisper context (shared, not owned)
    whisper_context* whisperContext;

    // Audio queue (input)
    std::queue<std::vector<float>> audioQueue;
    mutable std::mutex audioQueueMutex;

    // Results queue (output)
    std::queue<std::string> resultsQueue;
    mutable std::mutex resultsQueueMutex;

    // Worker thread
    std::thread workerThread;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::atomic<bool> processing;

    // Metrics
    std::atomic<size_t> processedCount;
    std::atomic<float> lastLatencyMs;
};
