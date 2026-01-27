#include "pe_base/windows_helper.h"

#include <iomanip>
#include <sstream>
namespace pe_base {
DataBuffer WindowsHelper::ToString(HRESULT hr) {
  return ToString(_com_error(hr));
}
DataBuffer WindowsHelper::ToString(const _com_error& error) {
  std::stringstream ss;
  _bstr_t error_message(error.ErrorMessage());
  ss << "Error= 0x" << std::hex << std::setw(8) << std::setfill('0')
     << error.Error();
  ss << ", msg= " << static_cast<char*>(error_message);
  auto str = ss.str();
  DataBuffer buffer;
  if (!str.empty()) buffer.SetData((uint8_t*)str.c_str(), str.size() + 1);
  return buffer;
}
DataBuffer WindowsHelper::GetLastErrorString() {
  return ToString(GetLastError());
}
DataBuffer WindowsHelper::ToString(DWORD error_code) {
  std::stringstream ss;
  ss << "Error= 0x" << std::hex << std::setw(8) << std::setfill('0')
     << error_code;
  LPVOID lpMsgBuf = nullptr;
  auto res = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error_code, 0, (LPTSTR)&lpMsgBuf, 0, nullptr);
  if (res == S_OK) {
    ss << ": " << lpMsgBuf;
  }
  if (lpMsgBuf) {
    LocalFree(lpMsgBuf);
  }
  auto str = ss.str();
  DataBuffer buffer;
  if (!str.empty()) buffer.SetData((uint8_t*)str.c_str(), str.size() + 1);
  return buffer;
}
DataBuffer WindowsHelper::ConvertToChar(const wchar_t* str) {
  int str_len =
      WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
  if (!str_len) {
    return {};
  }
  DataBuffer output;
  output.SetSize_(str_len);
  WideCharToMultiByte(CP_UTF8, 0, str, -1, (char*)output.Data_(), str_len,
                      nullptr, nullptr);
  return output;
}
}  // namespace pe_base
