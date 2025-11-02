using System;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace MeetingAssistant
{
    /// <summary>
    /// Mock integration with Pay Attention SDK
    /// Logs actions to file and console until real SDK is available
    /// </summary>
    /// 
    public static class PayAttentionBridge
    {
        private static string MeetingContent = "";
        // QAASR
        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetListeningDevice(int deviceID);

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void SetASRLanguage(string language);

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetASRCallback(MSFT_ASRCallback callback);

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetSpeechStateCallback(MSFT_ST_Callback callback);

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void StartRecord();

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void StopRecord();

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void EnableSummary(string prompt, string content);

        [DllImport("D:\\quantum_payattention\\Quantum_PayAttention\\x64\\Release\\MSFTCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern IntPtr StartSummary();

        private static readonly string LogFilePath = Path.Combine(
            AppDomain.CurrentDomain.BaseDirectory,
            "meeting_assistant.log"
        );

        // Delegate definitions for callbacks
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void MSFT_ASRCallback(
            [MarshalAs(UnmanagedType.LPWStr)] string role,
            [MarshalAs(UnmanagedType.LPWStr)] string msg,
            [MarshalAs(UnmanagedType.LPWStr)] string targetLang,
            ulong offset,
            ulong duration,
            bool isFinal);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void MSFT_ST_Callback(bool state);

        // Static method for ASR callback
        private static void OnASRResult(
            string role,
            string msg,
            string targetLang,
            ulong offset,
            ulong duration,
            bool isFinal)
        {
            if (isFinal)
            {
                Console.WriteLine($"[ASR] Role: {role}, Msg: {msg}, Lang: {targetLang}, Offset: {offset}, Duration: {duration}, Final: {isFinal}");
                MeetingContent += msg + "\n";
            }
        }

        // Static method for Speech State callback
        private static void OnSpeechStateChanged(bool state)
        {
            Console.WriteLine($"[SpeechState] State: {(state ? "Speaking" : "Silent")}");
        }

        /// <summary>
        /// Start meeting transcription (MOCK - logs to file)
        /// Replace with real SDK call when available
        /// </summary>
        public static void StartMeetingTranscription(MeetingInfo meeting)
        {
            var logMessage = $@"
================================================================================
[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] WOULD CALL PAY ATTENTION SDK
================================================================================
Action: Start Meeting Transcription
App Name: {meeting.AppName}
Process ID: {meeting.ProcessId}
Meeting Started: {meeting.DetectedAt:yyyy-MM-dd HH:mm:ss}
User Confirmed: {DateTime.Now:yyyy-MM-dd HH:mm:ss}

TODO: Replace with actual SDK call:
--------------------------------------
// using PayAttentionSDK;
// var client = new PayAttentionClient();
// await client.StartTranscriptionAsync(new MeetingSession {{
//     AppName = ""{meeting.AppName}"",
//     ProcessId = {meeting.ProcessId},
//     StartTime = DateTime.Parse(""{meeting.DetectedAt:O}"")
// }});
================================================================================

";
            Console.WriteLine("Starting record...");
            StartRecord();


            // Log to file
            try
            {
                File.AppendAllText(LogFilePath, logMessage);
                Console.WriteLine($"[PayAttentionBridge] Logged to: {LogFilePath}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[PayAttentionBridge] Failed to write log: {ex.Message}");
            }

            // Log to console
            Console.WriteLine(logMessage);
        }

        /// <summary>
        /// Stop meeting transcription (MOCK - logs to file)
        /// </summary>
        public static void StopMeetingTranscription(MeetingInfo meeting)
        {
            var logMessage = $@"
[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] WOULD CALL PAY ATTENTION SDK: Stop Transcription
App: {meeting.AppName}, PID: {meeting.ProcessId}
";
            Console.WriteLine("Stopping record...");
            StopRecord();
            MeetingContent = "";

            try
            {
                File.AppendAllText(LogFilePath, logMessage);
            }
            catch { }

            Console.WriteLine(logMessage);
        }

        /// <summary>
        /// Initialize SDK (MOCK - logs to file)
        /// </summary>
        public static void Initialize()
        {
            // Set listening device (e.g., 1=Microphone, 2=Speaker, 3=Microphone+Speaker)
            SetListeningDevice(3);

            // Set ASR callback using the static method
            SetASRCallback(OnASRResult);

            // Set speech state callback using the static method
            SetSpeechStateCallback(OnSpeechStateChanged);

            Console.WriteLine("[PayAttentionBridge] SDK Initialized.");
        }

        public static void StartMeetingSummarization()
        {
            EnableSummary("You are a voice assistant. When given a piece of dialogue, " +
                "analyze it and extract the main points. Organize the key ideas as " +
                "a clear outline. Keep the summary within 300 words. Output in English only.", MeetingContent);

            IntPtr ptr = StartSummary();
            string summary = Marshal.PtrToStringUni(ptr) ?? "";
            Console.WriteLine($"[Meeting Summary]\n{summary}");
        }
    }
}
