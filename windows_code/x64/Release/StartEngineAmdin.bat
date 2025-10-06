@echo off
chcp 65001 >nul
echo.
echo 正在请求管理员权限启动 PerceptionEngine.exe...

:: 检查是否已经是管理员
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if '%errorlevel%' NEQ '0' (
    echo 请求管理员权限...
    goto UACPrompt
) else (
    goto gotAdmin
)

:UACPrompt
echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
"%temp%\getadmin.vbs"
exit /B

:gotAdmin
if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )

:: 切换到批处理文件所在目录
cd /d "%~dp0"

echo.
echo 当前目录: %CD%
echo 正在启动 PerceptionEngine.exe --console...

:: 检查文件是否存在
if not exist "PerceptionEngine.exe" (
    echo 错误: 在当前目录下找不到 PerceptionEngine.exe
    echo 文件列表:
    dir *.exe
    pause
    exit /b 1
)

:: 启动程序
echo 成功以管理员权限启动!
PerceptionEngine.exe --console

:: 如果程序退出，暂停以便查看输出
echo.
echo PerceptionEngine.exe 已退出。
pause