using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeetingAssistant
{
    class Program
    {
        private static bool _running = true;
        private static MicrophoneMonitorNative? _monitor;
        private static MeetingStateMachine? _stateMachine;
        private static NotificationService? _notificationService;

        static async Task Main(string[] args)
        {
            Console.WriteLine("================================================================================");
            Console.WriteLine("Proactive Pay Attention");
            Console.WriteLine("================================================================================");
            Console.WriteLine();
            Console.WriteLine("This service monitors meeting apps and offers to start");
            Console.WriteLine("Pay Attention feature when a meeting is detected.");
            Console.WriteLine();
            Console.WriteLine("Supported apps: Teams, Zoom, Webex, Google Meet, and more...");
            Console.WriteLine();
            Console.WriteLine("Press Ctrl+C to exit");
            Console.WriteLine("================================================================================");
            Console.WriteLine();

            // Set up Ctrl+C handler
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                _running = false;
                Console.WriteLine();
                Console.WriteLine("[MeetingAssistant] Shutting down...");
            };

            try
            {
                // Initialize components
                Console.WriteLine("[MeetingAssistant] Initializing...");

                _monitor = new MicrophoneMonitorNative();
                Console.WriteLine("[✓] MicrophoneMonitor initialized");

                _stateMachine = new MeetingStateMachine();
                Console.WriteLine("[✓] State machine initialized");

                _notificationService = new NotificationService(
                    onStartClicked: OnUserClickedStart,
                    onDismissClicked: OnUserClickedDismiss
                );
                Console.WriteLine("[✓] Notification service initialized");

                Console.WriteLine();
                Console.WriteLine("[MeetingAssistant] Running... (polling every 2 seconds)");
                Console.WriteLine();

                // Main service loop
                await RunServiceLoopAsync();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ERROR] {ex.Message}");
                Console.WriteLine($"Stack trace: {ex.StackTrace}");
            }
            finally
            {
                // Cleanup
                Console.WriteLine();
                Console.WriteLine("[MeetingAssistant] Cleaning up...");
                _monitor?.Dispose();
                _notificationService?.Dispose();
                Console.WriteLine("[MeetingAssistant] Goodbye!");
            }
        }

        static async Task RunServiceLoopAsync()
        {
            while (_running)
            {
                try
                {
                    // Check for meeting apps
                    bool meetingDetected = _monitor!.IsMeetingAppUsingMicrophone();
                    MeetingInfo? meetingInfo = null;

                    if (meetingDetected)
                    {
                        string? appName = _monitor.GetMeetingAppName();
                        uint pid = _monitor.GetMeetingAppPID();

                        if (!string.IsNullOrEmpty(appName))
                        {
                            meetingInfo = new MeetingInfo
                            {
                                AppName = appName,
                                ProcessId = pid,
                                DetectedAt = DateTime.Now
                            };
                        }
                    }

                    // Update state machine
                    bool stateChanged = _stateMachine!.Update(meetingDetected, meetingInfo);

                    // Handle state transitions
                    if (stateChanged)
                    {
                        if (_stateMachine.CurrentState == MeetingState.Detected && meetingInfo != null)
                        {
                            // NEW MEETING DETECTED - Show notification
                            Console.WriteLine($"[MeetingAssistant] Meeting detected: {meetingInfo}");
                            _notificationService!.ShowMeetingDetectedNotification(meetingInfo.AppName);
                        }
                        else if (_stateMachine.CurrentState == MeetingState.Idle)
                        {
                            Console.WriteLine("[MeetingAssistant] No meeting detected (state: Idle)");
                        }
                    }

                    // Wait before next check
                    await Task.Delay(2000);  // Poll every 2 seconds
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERROR] Loop iteration failed: {ex.Message}");
                    await Task.Delay(5000);  // Wait longer on error
                }
            }
        }

        static void OnUserClickedStart()
        {
            Console.WriteLine();
            Console.WriteLine("╔════════════════════════════════════════════════════════════╗");
            Console.WriteLine("║  USER ACTION: Clicked 'Start'                              ║");
            Console.WriteLine("╚════════════════════════════════════════════════════════════╝");

            if (_stateMachine!.CurrentMeeting != null)
            {
                // Call Pay Attention SDK (mock for now)
                PayAttentionBridge.StartMeetingTranscription(_stateMachine.CurrentMeeting);

                // Update state
                _stateMachine.OnUserConfirmed();
            }
            else
            {
                Console.WriteLine("[WARNING] No current meeting to start transcription for");
            }

            Console.WriteLine();
        }

        static void OnUserClickedDismiss()
        {
            Console.WriteLine();
            Console.WriteLine("╔════════════════════════════════════════════════════════════╗");
            Console.WriteLine("║  USER ACTION: Clicked 'Dismiss'                            ║");
            Console.WriteLine("╚════════════════════════════════════════════════════════════╝");

            // Update state
            _stateMachine!.OnUserDismissed();

            Console.WriteLine();
        }
    }
}
