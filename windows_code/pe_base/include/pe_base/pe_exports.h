#pragma once

#if defined(PE_BASE_LIB_SHARED) && \
    (defined(_WIN32) || defined(_WINDOWS) || defined(_WINDLL))
#ifdef PE_BASE_EXPORTS
#define PE_BASE_API __declspec(dllexport)
#else
#define PE_BASE_API __declspec(dllimport)
#endif  // PE_BASE_EXPORTS
#else
#define PE_BASE_API
#endif

