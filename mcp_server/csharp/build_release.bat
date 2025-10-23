@echo off
echo Building MCP Server Release version...
echo.

cd /d "%~dp0"

echo Publishing to ./publish folder...
dotnet publish -c Release -r win-x64 --self-contained false -o ./publish

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo Build successful!
    echo ============================================
    echo.
    echo Executable location:
    echo %~dp0publish\PerceptionMcpBridge.exe
    echo.
    echo Next steps:
    echo 1. Update Claude Desktop config to use this path
    echo 2. Restart Claude Desktop
    echo.
) else (
    echo.
    echo ============================================
    echo Build FAILED!
    echo ============================================
    echo Please check for errors above.
)

pause
