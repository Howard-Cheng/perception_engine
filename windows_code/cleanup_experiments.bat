@echo off
echo ========================================
echo Cleaning up test and experimental files
echo ========================================
echo.

echo Deleting test programs...
del /F /Q test_vision_encoder.cpp 2>nul
del /F /Q test_audio.cpp 2>nul
del /F /Q test_mic_monitor.cpp 2>nul
del /F /Q test_meeting_detection_and_capture.cpp 2>nul

echo Deleting test build scripts...
del /F /Q build_test_mic_monitor.bat 2>nul
del /F /Q build_test_meeting_capture.bat 2>nul
del /F /Q build_meeting_test_simple.bat 2>nul

echo Deleting experimental audio transcription code (buggy)...
del /F /Q AudioCaptureEngine_MeetingMode.cpp 2>nul

echo Deleting test logs and outputs...
del /F /Q test_meeting_capture.log 2>nul
del /F /Q MicrophoneMonitor.log 2>nul

echo.
echo ========================================
echo Cleanup complete!
echo ========================================
echo.
echo Next steps:
echo 1. Run move_native_source.bat to move C++ source files
echo 2. Review remaining files
echo.
pause
