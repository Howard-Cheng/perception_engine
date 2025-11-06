// MicrophoneMonitorDLL.h
// C-compatible wrapper for MicrophoneMonitor (for P/Invoke from C# or direct C++ usage)

#pragma once

#ifdef BUILDING_DLL
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to MicrophoneMonitor instance
typedef void* MicrophoneMonitorHandle;

/**
 * Create a new MicrophoneMonitor instance
 * Returns: Handle to use in subsequent calls, or NULL on failure
 */
DLL_EXPORT MicrophoneMonitorHandle MicrophoneMonitor_Create();

/**
 * Destroy a MicrophoneMonitor instance
 * @param handle: Handle returned from MicrophoneMonitor_Create
 */
DLL_EXPORT void MicrophoneMonitor_Destroy(MicrophoneMonitorHandle handle);

/**
 * Check if a meeting app is using the microphone
 * @param handle: MicrophoneMonitor instance handle
 * @return: 1 if meeting app detected, 0 otherwise
 */
DLL_EXPORT int MicrophoneMonitor_IsMeetingAppUsingMicrophone(MicrophoneMonitorHandle handle);

/**
 * Get the name of the detected meeting app
 * @param handle: MicrophoneMonitor instance handle
 * @param buffer: Buffer to receive app name (e.g., "ms-teams.exe")
 * @param bufferSize: Size of buffer in bytes
 * @return: Number of characters written (excluding null terminator), or 0 if no meeting app
 */
DLL_EXPORT int MicrophoneMonitor_GetMeetingAppName(MicrophoneMonitorHandle handle, char* buffer, int bufferSize);

/**
 * Get the process ID of the detected meeting app
 * @param handle: MicrophoneMonitor instance handle
 * @return: Process ID, or 0 if no meeting app detected
 */
DLL_EXPORT unsigned long MicrophoneMonitor_GetMeetingAppPID(MicrophoneMonitorHandle handle);

#ifdef __cplusplus
}
#endif
