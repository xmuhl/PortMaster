@echo off
setlocal

rem ===== PortMaster 自动化测试脚本 =====
echo [INFO] 开始PortMaster自动化测试...

rem 设置工作目录
cd /d "%~dp0"

rem 检查可执行文件
if not exist "build\Debug\PortMaster.exe" (
    echo [ERROR] 可执行文件不存在，开始编译...
    call autobuild_x86_debug.bat
    if errorlevel 1 (
        echo [ERROR] 编译失败
        pause
        exit /b 1
    )
)

rem 检查PowerShell执行策略
echo [INFO] 检查PowerShell执行策略...
powershell.exe -Command "Get-ExecutionPolicy" | findstr /i "restricted" >nul
if errorlevel 1 (
    echo [INFO] PowerShell执行策略正常
) else (
    echo [WARNING] PowerShell执行策略受限，尝试设置临时策略...
    powershell.exe -Command "Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force"
)

rem 运行自动化测试
echo [INFO] 启动自动化测试...
powershell.exe -ExecutionPolicy Bypass -File "auto_test_runner.ps1" -WaitTime 5 -MaxRunTime 30

if errorlevel 1 (
    echo [ERROR] 自动化测试失败
    pause
    exit /b 1
)

echo [INFO] 自动化测试完成
echo [INFO] 检查test_screenshots目录中的截图文件
if exist "test_screenshots\*.png" (
    echo [INFO] 截图文件已生成
    start "" "test_screenshots"
) else (
    echo [WARNING] 未找到截图文件
)

pause