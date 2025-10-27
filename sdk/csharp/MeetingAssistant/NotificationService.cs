using System;
using Microsoft.Toolkit.Uwp.Notifications;

namespace MeetingAssistant
{
    public class NotificationService
    {
        private readonly Action _onStartClicked;
        private readonly Action _onDismissClicked;

        public NotificationService(Action onStartClicked, Action onDismissClicked)
        {
            _onStartClicked = onStartClicked;
            _onDismissClicked = onDismissClicked;

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
                    .AddText($"Want Qira to pay attention to your {appName} meeting?")
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
            }
        }

        public void Dispose()
        {
            ToastNotificationManagerCompat.OnActivated -= OnNotificationActivated;
            ToastNotificationManagerCompat.Uninstall();
        }
    }
}
