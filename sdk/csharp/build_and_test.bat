@echo off
REM =================================================================
REM  Build and Test Script for Perception Engine C# SDK
REM =================================================================
echo.
echo ===================================================
echo   Building Perception Engine C# SDK
echo ===================================================
echo.

cd /d "%~dp0"

echo [1/3] Cleaning previous build...
dotnet clean PerceptionEngine.SDK.sln --configuration Release >nul 2>&1

echo [2/3] Building SDK and examples...
dotnet build PerceptionEngine.SDK.sln --configuration Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo [3/3] Creating NuGet package...
cd PerceptionEngine.SDK
dotnet pack --configuration Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Pack failed!
    pause
    exit /b 1
)

cd ..

echo.
echo ===================================================
echo   Build Successful!
echo ===================================================
echo.
echo SDK DLL:      sdk\csharp\PerceptionEngine.SDK\bin\Release\net6.0\PerceptionEngine.SDK.dll
echo NuGet Package: sdk\csharp\PerceptionEngine.SDK\bin\Release\PerceptionEngine.SDK.1.0.0.nupkg
echo Example:      sdk\csharp\Examples\BasicUsage\bin\Release\net6.0\BasicUsage.exe
echo.
echo To test the SDK:
echo   1. Make sure Perception Engine is running:
echo      cd ..\..\windows_code
echo      .\start_perception_engine.bat
echo.
echo   2. Run the example:
echo      cd sdk\csharp\Examples\BasicUsage\bin\Release\net6.0
echo      .\BasicUsage.exe
echo.
pause
