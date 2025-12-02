#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "boost_wrapper/asio.h"
#include "pe_base/task_queue/task_queue.h"
#include "pe_base/thread_sync.h"

namespace pe_base {
using namespace boost::asio;

class TaskQueueBoost final : public TaskQueue {
 public:
  TaskQueueBoost(size_t thread_num, const char* queue_name);
  ~TaskQueueBoost() override;

  typedef boost::asio::io_context ContextType;
  void PostTask(std::unique_ptr<Task> task, bool in_order = false) override;
  void DispatchTask(std::unique_ptr<Task> task, bool in_order = false) override;
  void PostDelayedTask(std::unique_ptr<Task> task,
                       uint32_t delay_mills) override;
  bool IsQueueThread() override;
  ContextType& GetContext();

 private:
  void ThreadProcess();

 private:
  std::string name_;
  std::shared_ptr<io_context> ap_context_;
  executor_work_guard<io_context::executor_type> work_;
  io_context::strand strand_;
  std::list<std::unique_ptr<std::thread>> threads_;
  std::shared_ptr<std::atomic_size_t> task_in_queue_;

  ThreadBarrier init_sync_;
};
}  // namespace pe_base
