@echo off
setlocal EnableDelayedExpansion
echo ============================================
echo Copy whisper.lib to Expected Location
echo ============================================
echo.

REM Check multiple possible build locations
set WHISPER_LIB_FOUND=0
set WHISPER_LIB_SOURCE=

REM Try CMakePresets location (windows-x64-release)
if exist "third-party\whisper.cpp\out\build\windows-x64-release\whisper.lib" (
    echo Found Visual Studio build output - CMakePresets windows-x64-release
    set WHISPER_LIB_SOURCE=third-party\whisper.cpp\out\build\windows-x64-release\whisper.lib
    set WHISPER_LIB_FOUND=1
    goto :copy_lib
)

REM Try standard x64-Release location
if exist "third-party\whisper.cpp\out\build\x64-Release\whisper.lib" (
    echo Found Visual Studio build output - x64-Release
    set WHISPER_LIB_SOURCE=third-party\whisper.cpp\out\build\x64-Release\whisper.lib
    set WHISPER_LIB_FOUND=1
    goto :copy_lib
)

REM Try Release subfolder
if exist "third-party\whisper.cpp\out\build\windows-x64-release\Release\whisper.lib" (
    echo Found Visual Studio build output - windows-x64-release\Release
    set WHISPER_LIB_SOURCE=third-party\whisper.cpp\out\build\windows-x64-release\Release\whisper.lib
    set WHISPER_LIB_FOUND=1
    goto :copy_lib
)

REM Try src subfolder
if exist "third-party\whisper.cpp\out\build\windows-x64-release\src\Release\whisper.lib" (
    echo Found Visual Studio build output - windows-x64-release\src\Release
    set WHISPER_LIB_SOURCE=third-party\whisper.cpp\out\build\windows-x64-release\src\Release\whisper.lib
    set WHISPER_LIB_FOUND=1
    goto :copy_lib
)

REM Check if Debug builds exist (warn user)
if exist "third-party\whisper.cpp\out\build\windows-x64-debug\whisper.lib" (
    echo.
    echo WARNING: Found Debug build, but we need Release
    echo Please switch Visual Studio configuration to windows-x64-release
    echo and rebuild.
    echo.
    pause
    exit /b 1
)

if exist "third-party\whisper.cpp\out\build\x64-Debug\whisper.lib" (
    echo.
    echo WARNING: Found Debug build, but we need Release
    echo Please switch Visual Studio configuration to x64-Release
    echo and rebuild.
    echo.
    pause
    exit /b 1
)

REM No library found
echo ERROR: whisper.lib not found
echo.
echo Checked locations:
echo   - third-party\whisper.cpp\out\build\windows-x64-release\whisper.lib
echo   - third-party\whisper.cpp\out\build\windows-x64-release\Release\whisper.lib
echo   - third-party\whisper.cpp\out\build\windows-x64-release\src\Release\whisper.lib
echo   - third-party\whisper.cpp\out\build\x64-Release\whisper.lib
echo.
echo Did you build whisper.cpp in Visual Studio?
echo Did the build complete successfully?
echo.
echo Steps:
echo   1. Open whisper.cpp in VS
echo   2. Select configuration: windows-x64-release
echo   3. Build - Build All
echo   4. Wait for Build succeeded message
echo   5. Re-run this script
echo.
pause
exit /b 1

:copy_lib
echo Source: !WHISPER_LIB_SOURCE!
echo.

REM Create target directory
if not exist "third-party\whisper.cpp\build\Release" mkdir "third-party\whisper.cpp\build\Release"

REM Copy library
copy "!WHISPER_LIB_SOURCE!" "third-party\whisper.cpp\build\Release\whisper.lib"

if %ERRORLEVEL% equ 0 (
    echo.
    echo ============================================
    echo SUCCESS
    echo ============================================
    echo whisper.lib copied to: third-party\whisper.cpp\build\Release\
    echo.
    echo File size:
    dir "third-party\whisper.cpp\build\Release\whisper.lib" | findstr whisper.lib
    echo.
    echo Next steps:
    echo   1. Update PerceptionEngine.vcxproj with AudioCaptureEngine
    echo   2. Build PerceptionEngine
    echo   3. Test audio capture
    echo ============================================
) else (
    echo ERROR: Failed to copy library
    pause
    exit /b 1
)

pause
