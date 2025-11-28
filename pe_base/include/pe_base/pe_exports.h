#pragma once

#if defined(pe_base_LIB_SHARED) && \
    (defined(_WIN32) || defined(_WINDOWS) || defined(_WINDLL))
#ifdef pe_base_EXPORTS
#define pe_base_API __declspec(dllexport)
#else
#define pe_base_API __declspec(dllimport)
#endif  // pe_base_EXPORTS
#else
#define pe_base_API
#endif

