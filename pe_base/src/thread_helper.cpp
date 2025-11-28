//
// Created by Administrator on 2023/6/5.
//
#include "pe_base/thread_helper.h"

#include <string>
#include <thread>

#include "pe_base/logger.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace pe_base {

thread_local char thread_name[128] = {0};

void ThreadHelper::sleep(uint64_t ms) {
  if (!ms) return;
#if defined(_WIN32)
  timeBeginPeriod(1);
#endif
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#if defined(_WIN32)
  timeEndPeriod(1);
#endif
}

void ThreadHelper::SetThisThreadName(const char* name) {
  std::string_view(name).copy(thread_name, sizeof(thread_name) - 1);
#if defined(_WIN32)
  auto input_len = (int)strnlen(name, 64);
  int len = MultiByteToWideChar(CP_UTF8, 0, name, input_len, nullptr, 0);
  std::wstring str;
  str.resize(len + 1);
  MultiByteToWideChar(CP_UTF8, 0, name, input_len, str.data(), len);
  HRESULT hr = SetThreadDescription(GetCurrentThread(), str.c_str());
  if (FAILED(hr)) {
    PE_ERROR("SetThreadDescription failed: " << std::hex << hr)
  }
#elif defined(__MACH__)
  int result = pthread_setname_np(name);
  if (result) {
    PE_ERROR("pthread_setname_np error= " << result)
  }
#elif defined(__ANDROID__)
  int result = pthread_setname_np(pthread_self(), name);
  if (result) {
    PE_ERROR("pthread_setname_np error= " << result)
  }
#else
  PE_ERROR("not support on this platform")
#endif
}

char* ThreadHelper::GetThisThreadName() {
  return thread_name;
}

void ThreadHelper::SetThreadName(int64_t native_handle, const char* name) {
  if (!native_handle) {
    PE_ERROR("invalid native_handle")
    return;
  }
#if defined(_WIN32)
  auto input_len = (int)strnlen(name, 64);
  auto platform_handle = (HANDLE)(native_handle);
  int len = MultiByteToWideChar(CP_UTF8, 0, name, input_len, nullptr, 0);
  std::wstring str;
  str.resize(len + 1);
  MultiByteToWideChar(CP_UTF8, 0, name, input_len, str.data(), len);
  str.back() = '\0';
  SetThreadDescription(platform_handle, str.c_str());
#elif defined(__ANDROID__)
  auto platform_handle = (pthread_t)native_handle;
  int result = pthread_setname_np(platform_handle, name);
  if (result) {
    PE_ERROR("pthread_setname_np error= " << result)
  }
#else
  PE_ERROR("not support on this platform")
#endif
}

void ThreadHelper::InjectCrash() {
  PE_WARN("inject crash!!!")
  memset(nullptr, 0x5C, 100);
}

bool ThreadHelper::SetCurrentThreadPriority(int32_t policy, int32_t priority) {
  return false;
}

}  // namespace pe_base


