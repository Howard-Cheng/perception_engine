#include "dataprocess/TaskQueue.h"
#include "utils/Logger.h"

namespace dataprocess {

TaskQueue::TaskQueue(const std::string& name)
    : name_(name), running_(true) {
    worker_ = std::make_unique<std::thread>(&TaskQueue::WorkerThread, this);
    LOG_INFO_FMT("TaskQueue '%s' started", name_.c_str());
}

TaskQueue::~TaskQueue() {
    Stop();
}

void TaskQueue::PostTask(Task task) {
    if (!running_.load()) {
        LOG_WARN_FMT("TaskQueue '%s' is not running, task ignored", name_.c_str());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void TaskQueue::Stop() {
    if (!running_.load()) {
        return;
    }

    LOG_INFO_FMT("Stopping TaskQueue '%s'...", name_.c_str());
    running_.store(false);
    cv_.notify_all();

    if (worker_ && worker_->joinable()) {
        worker_->join();
    }
    LOG_INFO_FMT("TaskQueue '%s' stopped", name_.c_str());
}

void TaskQueue::WorkerThread() {
    while (running_.load()) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !tasks_.empty() || !running_.load();
            });

            if (!running_.load() && tasks_.empty()) {
                break;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("TaskQueue '%s' task exception: %s", name_.c_str(), e.what());
            } catch (...) {
                LOG_ERROR_FMT("TaskQueue '%s' task unknown exception", name_.c_str());
            }
        }
    }

    LOG_INFO_FMT("TaskQueue '%s' worker thread exiting", name_.c_str());
}

} // namespace dataprocess
