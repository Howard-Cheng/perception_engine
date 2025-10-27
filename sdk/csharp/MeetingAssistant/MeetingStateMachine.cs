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

        // Minimum duration filter to avoid false positives from screen recording software
        private DateTime? _firstDetectedAt = null;
        private const int MIN_DURATION_SECONDS = 2;

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
                        // First time detecting this meeting
                        if (_firstDetectedAt == null)
                        {
                            _firstDetectedAt = DateTime.Now;
                            // Console.WriteLine($"[State] Meeting detected (waiting {MIN_DURATION_SECONDS}s to confirm): {meetingInfo.AppName}");
                        }
                        else
                        {
                            // Check if meeting has been active for minimum duration
                            var duration = DateTime.Now - _firstDetectedAt.Value;
                            if (duration.TotalSeconds >= MIN_DURATION_SECONDS)
                            {
                                // NEW MEETING CONFIRMED
                                CurrentState = MeetingState.Detected;
                                CurrentMeeting = meetingInfo;
                                // Console.WriteLine($"[State] Idle → Detected: {meetingInfo.AppName} (confirmed after {duration.TotalSeconds:F1}s)");
                                _firstDetectedAt = null; // Reset for next detection
                            }
                        }
                    }
                    else
                    {
                        // Meeting no longer detected, reset timer
                        if (_firstDetectedAt != null)
                        {
                            var duration = DateTime.Now - _firstDetectedAt.Value;
                            Console.WriteLine($"[State] Meeting ended before {MIN_DURATION_SECONDS}s threshold (was {duration.TotalSeconds:F1}s) - likely false positive");
                            _firstDetectedAt = null;
                        }
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
