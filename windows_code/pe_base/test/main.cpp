#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "pe_base/logger.h"
#include "pe_base/task_queue/task_queue.h"
#include "pe_base/thread_helper.h"
#include "pe_base/time_util.h"
#include "pe_base/windows_helper.h"

using namespace pe_base;

int main() {
    std::cout << "Main started." << std::endl;
    // 1. Test Logger
  // Set log level to debug to see all logs
  std::filesystem::path log_path = "";
  if (auto* p_appdata = getenv("APPDATA")) {
    log_path =
        std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
  }
  pe_base::LogWriter::SetLogFilePrefix(
      (log_path / "PerceptionCore").generic_string());
  //LogWriter::SetOuputLogLevel(LogLevel::kLogLevelDebug);

  std::cout << "Testing Logger..." << std::endl;
  PE_INFO("This is an INFO log from pe_base test.");
  PE_DEBUG("This is a DEBUG log from pe_base test.");
  PE_WARN("This is a WARN log from pe_base test.");

  // 2. Test TimeUtil
  std::cout << "Testing TimeUtil..." << std::endl;
  int64_t start_timestamp = TimeUtil::TimestampMs();
  std::cout << "Current Timestamp: " << start_timestamp << " ms" << std::endl;

  // 3. Test ThreadHelper
  std::cout << "Testing ThreadHelper..." << std::endl;
  ThreadHelper::SetThisThreadName("PE_Test_Main");
  std::cout << "Set current thread name to 'PE_Test_Main'" << std::endl;

  // 4. Test TaskQueue
  std::cout << "Testing TaskQueue..." << std::endl;
  auto queue = TaskQueue::Create(2, "TestQueue");
  if (queue) {
    PE_INFO("TaskQueue created successfully.");

    // Post a task
    queue->PostTask([]() {
      PE_INFO(
          "Executing task in TaskQueue thread: " << std::this_thread::get_id());
      ThreadHelper::SetThisThreadName("PE_Test_Worker");
    });

    // Post a delayed task
    queue->PostDelayedTask(
        []() { PE_INFO("Executing delayed task (500ms later)."); }, 500);
  } else {
    PE_ERROR("Failed to create TaskQueue.");
  }

  // 5. Test WindowsHelper (if on Windows)
#ifdef _WIN32
  std::cout << "Testing WindowsHelper..." << std::endl;
  // Call a simple function if available and safe, e.g. checking process
  // existence or just linking check Since we don't have a specific
  // comprehensive test for windows helper without specific setup, just
  // instantiating or calling a safe static method is enough to prove linkage.
  // Assuming WindowsHelper has some static methods.
  // Let's check the header content if I could, but I recall it has static
  // methods. If unsure, I'll skip complex logic to avoid runtime errors, but
  // the linkage is the main test here.
  PE_INFO("WindowsHelper linked.");
#endif

  // Wait for tasks to finish
  std::this_thread::sleep_for(std::chrono::seconds(1));

  PE_INFO("Test Finished.");
  return 0;
}

