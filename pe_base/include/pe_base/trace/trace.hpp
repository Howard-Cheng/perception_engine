#pragma once

// Use perfetto or systrace for tracing call

//#define PE_TRACE_ON
#if defined(__ANDROID__)
#include <android/trace.h>
#define PE_TRACE_BEGIN(name) ATrace_beginSection(name)
#define PE_TRACE_END() ATrace_endSection()
#else
#define PE_TRACE_BEGIN(name)
#define PE_TRACE_END()
#endif

#define PE_TRACE_NAME(name) ScopedTrace PE_TRACEr(name)

#if defined(PE_TRACE_ON)
#define PE_TRACE_SCOPED() PE_TRACE_NAME(__FUNCTION__)
#else
#define PE_TRACE_SCOPED()
#endif

class ScopedTrace {
 public:
  inline ScopedTrace(const char* name) { PE_TRACE_BEGIN(name); }
  inline ~ScopedTrace() { PE_TRACE_END(); }
};

