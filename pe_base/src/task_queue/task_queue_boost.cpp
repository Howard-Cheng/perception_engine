#include "task_queue/task_queue_boost.h"

#if defined(_WIN32)
#include <timeapi.h>
#endif

#include <algorithm>
#include <string>

#include "pe_base/logger.h"
#include "pe_base/task_queue/task.h"
#include "pe_base/thread_helper.h"
#include "task_queue/task_queue_internal.h"
namespace pe_base {
namespace {
inline void CheckTaskDeplay(
    std::chrono::high_resolution_clock::time_point expect_tp) {
  auto exec_tp = std::chrono::high_resolution_clock::now();
  if (auto delay = exec_tp - expect_tp; delay > std::chrono::milliseconds (100)) {
    PE_WARN(
        "task queue: "
        << ThreadHelper::GetThisThreadName() << " unexpected delay exec: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(delay).count()
        << "ms")
  }
}
}  // namespace

TaskQueueBoost::TaskQueueBoost(size_t thread_num, const char* queue_name)
    : ap_context_(std::make_shared<io_context>()),
      work_(make_work_guard(*ap_context_)),
      strand_(*ap_context_),
      init_sync_(thread_num + 1) {
  static const size_t kMinThreadNum = 1U;
  static const size_t kMaxThreadNum = 16U;
  thread_num = (std::min)(thread_num, kMaxThreadNum);
  thread_num = (std::max)(kMinThreadNum, thread_num);
  std::string thread_name = queue_name ? queue_name : "TaskQueue";
  if (thread_name.size() > 15) {
    thread_name = thread_name.substr(0, 15);
  }
  if (thread_name.empty()) {
    PE_DEBUG_THIS("no queue name")
  } else {
    name_ = thread_name;
  }

  while (threads_.size() < thread_num) {
    auto t = new std::thread([this] { ThreadProcess(); });
    threads_.emplace_back(t);
  }

  init_sync_.Arrive();
}

TaskQueueBoost::~TaskQueueBoost() {
  ap_context_->stop();
  for (auto& cur : threads_) {
    if (std::this_thread::get_id() == cur->get_id()) {
      if (cur->joinable()) {
        cur->detach();
      }
      continue;
    }
    cur->join();
  }
}

void TaskQueueBoost::PostTask(std::unique_ptr<Task> task,
                              bool in_order /*= false*/) {
  // TODO(by Hanson Drew): Check whether Boost.Asio support generalized lambda
  //                     capture.
  std::shared_ptr<Task> task_shared(task.release());
  auto post_func = [task_shared,
                    etp = std::chrono::high_resolution_clock::now()] {
    CheckTaskDeplay(etp);
    task_shared->Run();
  };
  if (in_order) {
    boost::asio::post(strand_, post_func);
  } else {
    boost::asio::post(*ap_context_, post_func);
  }
}

void TaskQueueBoost::DispatchTask(std::unique_ptr<Task> task,
                                  bool in_order /*= false*/) {
  std::shared_ptr<Task> task_shared(task.release());
  auto post_func = [task_shared,
                    etp = std::chrono::high_resolution_clock::now()]() {
    CheckTaskDeplay(etp);
    task_shared->Run();
  };
  if (in_order) {
    boost::asio::dispatch(strand_, post_func);
  } else {
    boost::asio::dispatch(*ap_context_, post_func);
  }
}

void TaskQueueBoost::PostDelayedTask(std::unique_ptr<Task> task,
                                     uint32_t delay_mills) {
  std::shared_ptr<boost::asio::steady_timer> timer =
      std::make_shared<boost::asio::steady_timer>(*ap_context_);
  std::shared_ptr<Task> task_shared(task.release());
  auto post_func = [task_shared, timer,
                    etp = std::chrono::high_resolution_clock::now() +
                          std::chrono::milliseconds(delay_mills)](
                       const boost::system::error_code& error) {
    CheckTaskDeplay(etp);
    task_shared->Run();
    timer->cancel();
  };
  timer->expires_after(std::chrono::milliseconds(delay_mills));
  timer->async_wait(post_func);
}

bool TaskQueueBoost::IsQueueThread() {
  return ap_context_->get_executor().running_in_this_thread();
}

TaskQueueBoost::ContextType& TaskQueueBoost::GetContext() {
  return *ap_context_;
}

void TaskQueueBoost::ThreadProcess() {
#if defined(_WIN32)
  timeBeginPeriod(1);
#endif
  ThreadHelper::SetThisThreadName(name_.c_str());
  std::shared_ptr<io_context> ap_context = ap_context_;
  init_sync_.Arrive();

  if (!TaskQueueWorkerCountIncrement()) {
    PE_ERROR_THIS("increment task queue worker count fail")
    return;
  }
  ap_context->run();
  ap_context.reset();
  TaskQueueWorkerCountDecrement();
#if defined(_WIN32)
  timeEndPeriod(1);
#endif
}
}  // namespace pe_base


