//
// SpeechRecognitionExport.h
// DLL Export/Import Macro Definitions
//

#pragma once

#ifdef _WIN32
    #ifdef SPEECHRECOGNITION_EXPORTS
        // Building the DLL
        #define SPEECHRECOGNITION_API __declspec(dllexport)
    #else
        // Using the DLL
        #define SPEECHRECOGNITION_API __declspec(dllimport)
    #endif
#else
    // Non-Windows platforms
    #ifdef SPEECHRECOGNITION_EXPORTS
        #define SPEECHRECOGNITION_API __attribute__((visibility("default")))
    #else
        #define SPEECHRECOGNITION_API
    #endif
#endif
