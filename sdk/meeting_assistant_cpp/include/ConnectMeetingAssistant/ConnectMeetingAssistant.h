#pragma once

#ifdef BUILDING_CONNECT_DLL
#define CONNECT_API __declspec(dllexport)
#else
#define CONNECT_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Meeting status enumeration
typedef enum {
    MEETING_STATUS_STARTED = 1,
    MEETING_STATUS_ENDED = 2
} MeetingStatus;

// User decision enumeration
typedef enum {
    USER_DECISION_ACCEPT = 1,      // User chose to start recording/summarization
    USER_DECISION_DECLINE = 2      // User chose not to start recording/summarization
} UserDecision;

// Callback function type for meeting status changes
// Parameters:
//   status: MEETING_STATUS_STARTED or MEETING_STATUS_ENDED
//   appName: Name of the meeting application (e.g., "Teams.exe")
//   processId: Process ID of the meeting application
// Returns:
//   USER_DECISION_ACCEPT or USER_DECISION_DECLINE
typedef UserDecision(__cdecl* OnMeetingStatusChangedCallback)(
    MeetingStatus status,
    const char* appName,
    unsigned long processId
);

/**
 * Connect to MeetingAssistant's named pipe server and register callback
 * @param callback: Function to be called when meeting status changes
 * @return: 0 on success, error code on failure
 */
CONNECT_API int ConnectMeetingAssistant(OnMeetingStatusChangedCallback callback);

/**
 * Disconnect from MeetingAssistant's named pipe server
 * @return: 0 on success, error code on failure
 */
CONNECT_API int DisconnectMeetingAssistant();

/**
 * Check if currently connected to MeetingAssistant
 * @return: 1 if connected, 0 if not connected
 */
CONNECT_API int IsConnected();

#ifdef __cplusplus
}
#endif
