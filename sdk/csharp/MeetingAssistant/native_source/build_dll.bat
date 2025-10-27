@echo off 
echo ======================================== 
echo Building MicrophoneMonitor.dll 
echo ======================================== 
echo. 
 
REM Check for compiler 
where cl >nul 2>&1 
if errorlevel 1 ( 
    echo ERROR: Visual Studio compiler not found! 
    echo Please run from "Developer Command Prompt for VS 2022" 
    pause 
    exit /b 1 
) 
 
echo Compiling... 
cl /c /EHsc /std:c++17 /O2 /MD /DBUILDING_DLL /I. MicrophoneMonitor.cpp MicrophoneMonitor_AudioDetection.cpp MicrophoneMonitorDLL.cpp Logger.cpp 
 
echo Linking DLL... 
link /DLL /OUT:..\bin\Release\net8.0-windows10.0.19041.0\MicrophoneMonitor.dll ole32.lib psapi.lib MicrophoneMonitor.obj MicrophoneMonitor_AudioDetection.obj MicrophoneMonitorDLL.obj Logger.obj 
 
echo Cleaning up... 
del *.obj 2>nul 
 
echo Build complete! 
pause 
