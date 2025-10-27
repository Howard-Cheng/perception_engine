@echo off
echo ========================================
echo Moving C++ source files to MeetingAssistant
echo ========================================
echo.

set DEST=..\sdk\csharp\MeetingAssistant\native_source

echo Creating native_source directory...
if not exist "%DEST%" mkdir "%DEST%"

echo Moving MicrophoneMonitor source files...
move /Y MicrophoneMonitor.cpp "%DEST%\" 2>nul
move /Y MicrophoneMonitor.h "%DEST%\" 2>nul
move /Y MicrophoneMonitor_AudioDetection.cpp "%DEST%\" 2>nul

echo Moving DLL wrapper files...
move /Y MicrophoneMonitorDLL.cpp "%DEST%\" 2>nul
move /Y MicrophoneMonitorDLL.h "%DEST%\" 2>nul

echo Moving Logger (used by DLL)...
move /Y Logger.cpp "%DEST%\" 2>nul
move /Y Logger.h "%DEST%\" 2>nul

echo.
echo Creating build script in native_source...
echo @echo off > "%DEST%\build_dll.bat"
echo echo ======================================== >> "%DEST%\build_dll.bat"
echo echo Building MicrophoneMonitor.dll >> "%DEST%\build_dll.bat"
echo echo ======================================== >> "%DEST%\build_dll.bat"
echo echo. >> "%DEST%\build_dll.bat"
echo. >> "%DEST%\build_dll.bat"
echo REM Check for compiler >> "%DEST%\build_dll.bat"
echo where cl ^>nul 2^>^&1 >> "%DEST%\build_dll.bat"
echo if errorlevel 1 ( >> "%DEST%\build_dll.bat"
echo     echo ERROR: Visual Studio compiler not found! >> "%DEST%\build_dll.bat"
echo     echo Please run from "Developer Command Prompt for VS 2022" >> "%DEST%\build_dll.bat"
echo     pause >> "%DEST%\build_dll.bat"
echo     exit /b 1 >> "%DEST%\build_dll.bat"
echo ) >> "%DEST%\build_dll.bat"
echo. >> "%DEST%\build_dll.bat"
echo echo Compiling... >> "%DEST%\build_dll.bat"
echo cl /c /EHsc /std:c++17 /O2 /MD /DBUILDING_DLL /I. MicrophoneMonitor.cpp MicrophoneMonitor_AudioDetection.cpp MicrophoneMonitorDLL.cpp Logger.cpp >> "%DEST%\build_dll.bat"
echo. >> "%DEST%\build_dll.bat"
echo echo Linking DLL... >> "%DEST%\build_dll.bat"
echo link /DLL /OUT:..\bin\Release\net8.0-windows10.0.19041.0\MicrophoneMonitor.dll ole32.lib psapi.lib MicrophoneMonitor.obj MicrophoneMonitor_AudioDetection.obj MicrophoneMonitorDLL.obj Logger.obj >> "%DEST%\build_dll.bat"
echo. >> "%DEST%\build_dll.bat"
echo echo Cleaning up... >> "%DEST%\build_dll.bat"
echo del *.obj 2^>nul >> "%DEST%\build_dll.bat"
echo. >> "%DEST%\build_dll.bat"
echo echo Build complete! >> "%DEST%\build_dll.bat"
echo pause >> "%DEST%\build_dll.bat"

echo.
echo ========================================
echo Move complete!
echo ========================================
echo.
echo Files moved to: %DEST%
echo Build script created: %DEST%\build_dll.bat
echo.
echo You can now build the DLL from the MeetingAssistant directory:
echo   cd ..\sdk\csharp\MeetingAssistant\native_source
echo   build_dll.bat
echo.
pause
