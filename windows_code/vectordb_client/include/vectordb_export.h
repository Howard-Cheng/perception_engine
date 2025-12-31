#pragma once

// DLL Export/Import macros for Windows
#if defined(_WIN32) || defined(_WIN64)
    #ifdef VECTORDB_EXPORTS
        #define VECTORDB_API __declspec(dllexport)
    #else
        #define VECTORDB_API __declspec(dllimport)
    #endif
#else
    #define VECTORDB_API __attribute__((visibility("default")))
#endif

// Disable C++ name mangling for C-style exports
#ifdef __cplusplus
    #define VECTORDB_C_API extern "C" VECTORDB_API
#else
    #define VECTORDB_C_API VECTORDB_API
#endif
