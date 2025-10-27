using System;

namespace MeetingAssistant
{
    public enum MeetingState
    {
        Idle,              // No meeting detected
        Detected,          // Meeting detected, notification shown
        WaitingForUser,    // User saw notification, waiting for action
        PayingAttention,   // User confirmed, "pay attention" active
        Dismissed          // User dismissed notification
    }

    public class MeetingStateMachine
    {
        public MeetingState CurrentState { get; private set; } = MeetingState.Idle;
        public MeetingInfo? CurrentMeeting { get; private set; }

        /// <summary>
        /// Update state based on meeting detection status
        /// </summary>
        /// <param name="meetingDetected">Whether a meeting is currently detected</param>
        /// <param name="meetingInfo">Info about the detected meeting (null if not detected)</param>
        /// <returns>True if state changed</returns>
        public bool Update(bool meetingDetected, MeetingInfo? meetingInfo)
        {
            var oldState = CurrentState;

            switch (CurrentState)
            {
                case MeetingState.Idle:
                    if (meetingDetected && meetingInfo != null)
                    {
                        // NEW MEETING DETECTED
                        CurrentState = MeetingState.Detected;
                        CurrentMeeting = meetingInfo;
                        Console.WriteLine($"[State] Idle → Detected: {meetingInfo.AppName}");
                    }
                    break;

                case MeetingState.Detected:
                    if (!meetingDetected)
                    {
                        // Meeting ended before user responded
                        CurrentState = MeetingState.Idle;
                        CurrentMeeting = null;
                        Console.WriteLine("[State] Detected → Idle (meeting ended)");
                    }
                    break;

                case MeetingState.WaitingForUser:
                case MeetingState.PayingAttention:
                case MeetingState.Dismissed:
                    if (!meetingDetected)
                    {
                        // Meeting ended
                        CurrentState = MeetingState.Idle;
                        CurrentMeeting = null;
                        Console.WriteLine($"[State] {oldState} → Idle (meeting ended)");
                    }
                    break;
            }

            return oldState != CurrentState;
        }

        /// <summary>
        /// Handle user clicking "Start" on notification
        /// </summary>
        public void OnUserConfirmed()
        {
            if (CurrentState == MeetingState.Detected || CurrentState == MeetingState.WaitingForUser)
            {
                var oldState = CurrentState;
                CurrentState = MeetingState.PayingAttention;
                Console.WriteLine($"[State] {oldState} → PayingAttention");
            }
        }

        /// <summary>
        /// Handle user clicking "Dismiss" on notification
        /// </summary>
        public void OnUserDismissed()
        {
            if (CurrentState == MeetingState.Detected || CurrentState == MeetingState.WaitingForUser)
            {
                var oldState = CurrentState;
                CurrentState = MeetingState.Dismissed;
                Console.WriteLine($"[State] {oldState} → Dismissed");
            }
        }

        /// <summary>
        /// Reset state machine (for testing)
        /// </summary>
        public void Reset()
        {
            CurrentState = MeetingState.Idle;
            CurrentMeeting = null;
            Console.WriteLine("[State] Reset to Idle");
        }
    }
}
