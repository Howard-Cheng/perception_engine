#pragma once
//
// Created by Hanson Drew
//
#ifdef WIN32
#include <comdef.h>
#include "pe_base/pe_exports.h"
#include "pe_base/transport/data_buffer.h"
namespace pe_base {
class PE_BASE_API WindowsHelper {
 public:
  static DataBuffer ToString(HRESULT hr);
  static DataBuffer ToString(const _com_error& error);
  static DataBuffer ToString(DWORD error_code);
  static DataBuffer GetLastErrorString();

  static DataBuffer ConvertToChar(const wchar_t * str);
};
}  // namespace pe_base
#endif


