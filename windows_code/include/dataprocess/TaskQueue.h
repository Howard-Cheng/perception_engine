#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace dataprocess {

/**
 * @brief Simple task queue for processing time-consuming operations
 * Similar to ap_task_queue_ in the reference implementation
 */
class TaskQueue {
public:
    using Task = std::function<void()>;

    explicit TaskQueue(const std::string& name = "TaskQueue");
    ~TaskQueue();

    // Post a task to the queue
    void PostTask(Task task);

    // Stop the queue and wait for pending tasks
    void Stop();

    // Check if queue is running
    bool IsRunning() const { return running_.load(); }

private:
    void WorkerThread();

    std::string name_;
    std::atomic<bool> running_{false};
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::unique_ptr<std::thread> worker_;
};

} // namespace dataprocess
