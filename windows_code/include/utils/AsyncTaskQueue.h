#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <future>

/**
 * @brief Async Task Queue - Thread-safe task queue for asynchronous execution
 * 
 * Features:
 * - Non-blocking task submission (PostTask)
 * - Background worker thread
 * - Automatic task execution in FIFO order
 * - Graceful shutdown
 * - Task result support via std::future
 */
class AsyncTaskQueue {
public:
    using Task = std::function<void()>;
    
    AsyncTaskQueue(const std::string& name = "AsyncTaskQueue") 
        : name_(name)
        , running_(false) 
    {
    }
    
    ~AsyncTaskQueue() {
        Stop();
    }
    
    // Disable copy
    AsyncTaskQueue(const AsyncTaskQueue&) = delete;
    AsyncTaskQueue& operator=(const AsyncTaskQueue&) = delete;
    
    /**
     * @brief Start the worker thread
     */
    void Start() {
        if (running_.load()) {
            return; // Already running
        }
        
        running_.store(true);
        workerThread_ = std::thread([this]() {
            WorkerThreadFunc();
        });
        
        std::cout << "[" << name_ << "] Started worker thread" << std::endl;
    }
    
    /**
     * @brief Stop the worker thread and wait for pending tasks to complete
     */
    void Stop() {
        if (!running_.load()) {
            return;
        }
        
        std::cout << "[" << name_ << "] Stopping worker thread..." << std::endl;
        
        running_.store(false);
        condition_.notify_all();
        
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        
        std::cout << "[" << name_ << "] Worker thread stopped" << std::endl;
    }
    
    /**
     * @brief Post a task to the queue (non-blocking)
     * @param task The task to execute
     */
    void PostTask(Task task) {
        if (!running_.load()) {
            std::cerr << "[" << name_ << "] Cannot post task - queue is not running!" << std::endl;
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            taskQueue_.push(std::move(task));
            queuedTaskCount_++;
        }
        
        condition_.notify_one();
    }
    
    /**
     * @brief Post a task and get a future for the result
     * @tparam Func Function type
     * @tparam Args Argument types
     * @return std::future for the result
     */
    template<typename Func, typename... Args>
    auto PostTaskWithResult(Func&& func, Args&&... args) 
        -> std::future<typename std::result_of<Func(Args...)>::type>
    {
        using ReturnType = typename std::result_of<Func(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        PostTask([task]() {
            (*task)();
        });
        
        return result;
    }
    
    /**
     * @brief Get the number of pending tasks
     */
    size_t GetPendingTaskCount() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return taskQueue_.size();
    }
    
    /**
     * @brief Get total queued task count
     */
    size_t GetQueuedTaskCount() const {
        return queuedTaskCount_.load();
    }
    
    /**
     * @brief Get total executed task count
     */
    size_t GetExecutedTaskCount() const {
        return executedTaskCount_.load();
    }
    
    /**
     * @brief Check if queue is running
     */
    bool IsRunning() const {
        return running_.load();
    }
    
private:
    void WorkerThreadFunc() {
        std::cout << "[" << name_ << "] Worker thread started" << std::endl;
        
        while (running_.load()) {
            Task task;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                
                // Wait for task or shutdown signal
                condition_.wait(lock, [this] {
                    return !taskQueue_.empty() || !running_.load();
                });
                
                // Check if we should exit
                if (!running_.load() && taskQueue_.empty()) {
                    break;
                }
                
                // Get next task
                if (!taskQueue_.empty()) {
                    task = std::move(taskQueue_.front());
                    taskQueue_.pop();
                }
            }
            
            // Execute task (outside lock to avoid blocking queue)
            if (task) {
                try {
                    task();
                    executedTaskCount_++;
                } catch (const std::exception& e) {
                    std::cerr << "[" << name_ << "] Task execution exception: " 
                              << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[" << name_ << "] Task execution unknown exception" << std::endl;
                }
            }
        }
        
        std::cout << "[" << name_ << "] Worker thread exiting" << std::endl;
    }
    
private:
    std::string name_;
    std::atomic<bool> running_;
    std::thread workerThread_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::queue<Task> taskQueue_;
    
    std::atomic<size_t> queuedTaskCount_{0};
    std::atomic<size_t> executedTaskCount_{0};
};
