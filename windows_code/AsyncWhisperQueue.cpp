#include "AsyncWhisperQueue.h"
#include <iostream>
#include "whisper.h"

AsyncWhisperQueue::AsyncWhisperQueue(whisper_context* ctx)
    : whisperContext(ctx)
    , running(true)
    , processing(false)
    , processedCount(0)
    , lastLatencyMs(0.0f)
{
    if (!whisperContext) {
        throw std::runtime_error("AsyncWhisperQueue: whisper context is null!");
    }

    // Start worker thread
    workerThread = std::thread(&AsyncWhisperQueue::WorkerThread, this);

    std::cout << "[AsyncQueue] Worker thread started" << std::endl;
}

AsyncWhisperQueue::~AsyncWhisperQueue() {
    // Signal worker to stop
    running.store(false);
    cv.notify_all();

    // Wait for worker to finish current transcription
    if (workerThread.joinable()) {
        workerThread.join();
    }

    std::cout << "[AsyncQueue] Worker thread stopped. Processed "
              << processedCount.load() << " utterances" << std::endl;
}

void AsyncWhisperQueue::QueueAudio(const std::vector<float>& audio) {
    {
        std::lock_guard<std::mutex> lock(audioQueueMutex);
        audioQueue.push(audio);
    }

    // Wake up worker thread
    cv.notify_one();

    std::cout << "[AsyncQueue] Queued audio (" << audio.size() / 16000
              << "s), queue size: " << GetQueueSize() << std::endl;
}

std::string AsyncWhisperQueue::GetLatestResult() {
    std::lock_guard<std::mutex> lock(resultsQueueMutex);

    if (resultsQueue.empty()) {
        return "";
    }

    // Get latest result
    std::string result = resultsQueue.front();
    resultsQueue.pop();

    return result;
}

bool AsyncWhisperQueue::IsProcessing() const {
    return processing.load();
}

size_t AsyncWhisperQueue::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(audioQueueMutex);
    return audioQueue.size();
}

size_t AsyncWhisperQueue::GetProcessedCount() const {
    return processedCount.load();
}

float AsyncWhisperQueue::GetLastLatencyMs() const {
    return lastLatencyMs.load();
}

void AsyncWhisperQueue::WorkerThread() {
    std::cout << "[AsyncQueue] Worker thread running" << std::endl;

    while (running.load()) {
        std::vector<float> audioToProcess;

        // Wait for audio in queue
        {
            std::unique_lock<std::mutex> lock(audioQueueMutex);

            // Wait until we have audio or should stop
            cv.wait(lock, [this] {
                return !audioQueue.empty() || !running.load();
            });

            if (!running.load()) {
                break;  // Exit thread
            }

            // Get audio from queue
            audioToProcess = audioQueue.front();
            audioQueue.pop();
        }

        // Transcribe (this takes 6+ seconds, but doesn't block main thread!)
        processing.store(true);

        auto startTime = std::chrono::high_resolution_clock::now();
        std::string transcription = TranscribeAudio(audioToProcess);
        auto endTime = std::chrono::high_resolution_clock::now();

        float latencyMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        lastLatencyMs.store(latencyMs);

        processing.store(false);

        // Add result to results queue
        if (!transcription.empty()) {
            std::lock_guard<std::mutex> lock(resultsQueueMutex);
            resultsQueue.push(transcription);
            processedCount++;

            std::cout << "[AsyncQueue] Transcribed: \"" << transcription
                      << "\" (" << (int)latencyMs << "ms)" << std::endl;
        }
    }

    std::cout << "[AsyncQueue] Worker thread exiting" << std::endl;
}

std::string AsyncWhisperQueue::TranscribeAudio(const std::vector<float>& audioData) {
    if (!whisperContext || audioData.empty()) {
        return "";
    }

    // Set up whisper parameters (same as before)
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = "en";
    params.n_threads = 4;  // Use 4 CPU threads
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.translate = false;
    params.no_context = true;
    params.single_segment = false;

    // Run inference
    int result = whisper_full(
        whisperContext,
        params,
        audioData.data(),
        static_cast<int>(audioData.size())
    );

    if (result != 0) {
        std::cerr << "[AsyncQueue ERROR] whisper_full failed with code: " << result << std::endl;
        return "";
    }

    // Extract transcription
    const int n_segments = whisper_full_n_segments(whisperContext);
    std::string transcription;

    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(whisperContext, i);
        if (text) {
            transcription += text;
        }
    }

    // Trim whitespace
    if (!transcription.empty()) {
        size_t start = transcription.find_first_not_of(" \t\n\r");
        size_t end = transcription.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            transcription = transcription.substr(start, end - start + 1);
        }
    }

    return transcription;
}
