using System;
using System.Runtime.InteropServices;
using System.Text;

namespace MeetingAssistant
{
    /// <summary>
    /// P/Invoke wrapper for MicrophoneMonitor.dll (native C++ code)
    /// Provides access to existing meeting detection logic
    /// </summary>
    public class MicrophoneMonitorNative : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed = false;

        #region P/Invoke Declarations

        [DllImport("MicrophoneMonitor.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr MicrophoneMonitor_Create();

        [DllImport("MicrophoneMonitor.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void MicrophoneMonitor_Destroy(IntPtr handle);

        [DllImport("MicrophoneMonitor.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int MicrophoneMonitor_IsMeetingAppUsingMicrophone(IntPtr handle);

        [DllImport("MicrophoneMonitor.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int MicrophoneMonitor_GetMeetingAppName(IntPtr handle, StringBuilder buffer, int bufferSize);

        [DllImport("MicrophoneMonitor.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern uint MicrophoneMonitor_GetMeetingAppPID(IntPtr handle);

        #endregion

        public MicrophoneMonitorNative()
        {
            _handle = MicrophoneMonitor_Create();
            if (_handle == IntPtr.Zero)
            {
                throw new Exception("Failed to create MicrophoneMonitor instance");
            }
        }

        /// <summary>
        /// Check if a meeting app is currently using the microphone
        /// </summary>
        /// <returns>True if meeting app detected, false otherwise</returns>
        public bool IsMeetingAppUsingMicrophone()
        {
            ThrowIfDisposed();
            return MicrophoneMonitor_IsMeetingAppUsingMicrophone(_handle) != 0;
        }

        /// <summary>
        /// Get the name of the detected meeting app (e.g., "ms-teams.exe", "Zoom.exe")
        /// </summary>
        /// <returns>App name, or null if no meeting app detected</returns>
        public string? GetMeetingAppName()
        {
            ThrowIfDisposed();

            var buffer = new StringBuilder(256);
            int length = MicrophoneMonitor_GetMeetingAppName(_handle, buffer, buffer.Capacity);

            return length > 0 ? buffer.ToString() : null;
        }

        /// <summary>
        /// Get the process ID of the detected meeting app
        /// </summary>
        /// <returns>Process ID, or 0 if no meeting app detected</returns>
        public uint GetMeetingAppPID()
        {
            ThrowIfDisposed();
            return MicrophoneMonitor_GetMeetingAppPID(_handle);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(MicrophoneMonitorNative));
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                if (_handle != IntPtr.Zero)
                {
                    MicrophoneMonitor_Destroy(_handle);
                    _handle = IntPtr.Zero;
                }
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        ~MicrophoneMonitorNative()
        {
            Dispose();
        }
    }

    /// <summary>
    /// Information about a detected meeting
    /// </summary>
    public class MeetingInfo
    {
        public string AppName { get; set; } = string.Empty;
        public uint ProcessId { get; set; }
        public DateTime DetectedAt { get; set; }

        public override string ToString()
        {
            return $"{AppName} (PID: {ProcessId}) - Detected at {DetectedAt:HH:mm:ss}";
        }
    }
}
