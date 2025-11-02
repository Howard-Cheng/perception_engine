using System;
using Microsoft.Toolkit.Uwp.Notifications;

namespace MeetingAssistant
{
    public class NotificationService
    {
        private readonly Action _onStartClicked;
        private readonly Action _onDismissClicked;
        private readonly Action _onStartSummaryClicked;
        private readonly Action _onCancelSummaryClicked;

        public NotificationService(Action onStartClicked, Action onDismissClicked, Action onStartSummaryClicked, Action onCancelSummaryClicked)
        {
            _onStartClicked = onStartClicked;
            _onDismissClicked = onDismissClicked;
            _onStartSummaryClicked = onStartSummaryClicked;
            _onCancelSummaryClicked = onCancelSummaryClicked;

            // Register notification activation handler
            ToastNotificationManagerCompat.OnActivated += OnNotificationActivated;
        }

        /// <summary>
        /// Show Windows toast notification for meeting detection
        /// </summary>
        public void ShowMeetingDetectedNotification(string appName)
        {
            try
            {
                new ToastContentBuilder()
                    .AddText("Meeting Detected!")
                    .AddText($"Want to pay attention to your {appName} meeting?")
                    .AddButton(new ToastButton()
                        .SetContent("Start")
                        .AddArgument("action", "start"))
                    .AddButton(new ToastButton()
                        .SetContent("Dismiss")
                        .AddArgument("action", "dismiss"))
                    .Show();

                Console.WriteLine($"[Notification] Shown for {appName}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Notification] Failed to show: {ex.Message}");
            }
        }

        /// <summary>
        /// Show Windows toast notification for meeting content summarization
        /// </summary>
        public void ShowMeetingSummaryNotification()
        {
            try
            {
                new ToastContentBuilder()
                    .AddText("Meeting ended Detected!")
                    .AddText($"Want to summarize your last meeting?")
                    .AddButton(new ToastButton()
                        .SetContent("Start")
                        .AddArgument("action", "startsummarize"))
                    .AddButton(new ToastButton()
                        .SetContent("Dismiss")
                        .AddArgument("action", "cancelsummarize"))
                    .Show();

                Console.WriteLine($"[Notification] Shown for summarize");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Notification] Failed to show: {ex.Message}");
            }
        }

        private void OnNotificationActivated(ToastNotificationActivatedEventArgsCompat args)
        {
            // Parse arguments
            var arguments = ToastArguments.Parse(args.Argument);

            if (arguments.TryGetValue("action", out var action))
            {
                Console.WriteLine($"[Notification] User clicked: {action}");

                if (action == "start")
                {
                    _onStartClicked?.Invoke();
                }
                else if (action == "dismiss")
                {
                    _onDismissClicked?.Invoke();
                }
                else if (action == "startsummarize")
                {
                    _onStartSummaryClicked.Invoke();
                }
                else if (action == "cancelsummarize")
                {
                    _onCancelSummaryClicked?.Invoke();
                }
            }
        }

        public void Dispose()
        {
            ToastNotificationManagerCompat.OnActivated -= OnNotificationActivated;
            ToastNotificationManagerCompat.Uninstall();
        }
    }
}
