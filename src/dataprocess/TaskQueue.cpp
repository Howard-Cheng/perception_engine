#include "dataprocess/TaskQueue.h"
#include "pe_base/logger.h"

namespace dataprocess {

TaskQueue::TaskQueue(const std::string& name)
    : name_(name), running_(true) {
    worker_ = std::make_unique<std::thread>(&TaskQueue::WorkerThread, this);
    PE_INFO_THIS("TaskQueue " << name_.c_str() << " started")
}

TaskQueue::~TaskQueue() {
    Stop();
}

void TaskQueue::PostTask(Task task) {
    if (!running_.load()) {
        PE_INFO_THIS("TaskQueue" << name_.c_str() <<  " is not running, task ignored")
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

    PE_INFO_THIS("Stopping TaskQueue '" << name_.c_str() << "  '...")
    running_.store(false);
    cv_.notify_all();

    if (worker_ && worker_->joinable()) {
        worker_->join();
    }
    PE_INFO_THIS("TaskQueue  '" << name_.c_str() << "  ' stopped")
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
                PE_ERROR_THIS("TaskQueue '" << name_.c_str() << " ' task exception: %s" << e.what())
            } catch (...) {
                PE_ERROR_THIS("TaskQueue '%s' task unknown exception", name_.c_str());
            }
        }
    }

    PE_INFO("TaskQueue '" << name_.c_str() << "' worker thread exiting")
}

} // namespace dataprocess
