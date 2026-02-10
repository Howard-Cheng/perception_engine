#pragma once

/**
 * @file link_lingua_export.h
 * @brief Export macros for link_lingua shared library (DLL)
 * 
 * This file defines the export/import macros for the link_lingua library.
 * When building the DLL, LINK_LINGUA_EXPORTS should be defined.
 * When using the DLL, LINK_LINGUA_EXPORTS should NOT be defined.
 */

#ifdef _WIN32
    #ifdef LINK_LINGUA_EXPORTS
        // Building the DLL - export symbols
        #define LINK_LINGUA_API __declspec(dllexport)
    #else
        // Using the DLL - import symbols
        #define LINK_LINGUA_API __declspec(dllimport)
    #endif
#else
    // Non-Windows platforms
    #define LINK_LINGUA_API
#endif
