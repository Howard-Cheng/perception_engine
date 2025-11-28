#include "pe_base/task_queue/task_queue.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include "pe_base/logger.h"
#include "task_queue/task_queue_boost.h"
#include "task_queue/task_queue_internal.h"

static bool in_exit = false;
static int worker_count = 0;
static std::mutex worker_count_mu;
static std::condition_variable worker_count_cv;

namespace pe_base {
bool TaskQueueWorkerCountIncrement() {
  std::unique_lock<std::mutex> lock(worker_count_mu);
  if (in_exit) {
    PE_ERROR("do not start task queue in exit process")
    return false;
  }
  ++worker_count;
  return true;
}

void TaskQueueWorkerCountDecrement() {
  std::unique_lock<std::mutex> lock(worker_count_mu);
  --worker_count;
  worker_count_cv.notify_all();
}

std::shared_ptr<pe_base::TaskQueue> TaskQueue::Create(size_t thread_num,
                                                      const char* queue_name) {
  return std::make_shared<TaskQueueBoost>(thread_num, queue_name);
}

void WaitAllTaskQueueExit() {
  std::unique_lock<std::mutex> lock(worker_count_mu);
  in_exit = true;
  worker_count_cv.wait(lock, [] { return worker_count == 0; });
}

}  // namespace pe_base

