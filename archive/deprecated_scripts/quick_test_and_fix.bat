@echo off
setlocal

rem ===== 快速测试和修复 =====
echo [INFO] 快速测试和修复系统

cd /d "%~dp0"

rem 1. 检查可执行文件
echo [INFO] 检查可执行文件...
if not exist "build\Debug\PortMaster.exe" (
    echo [INFO] 编译程序...
    call autobuild_x86_debug.bat
)

rem 2. 创建测试结果目录
if not exist "test_screenshots" mkdir test_screenshots

rem 3. 运行简单测试
echo [INFO] 运行程序测试...
start "" /B "build\Debug\PortMaster.exe"

rem 等待程序启动
timeout /t 5 >nul

rem 4. 检查程序是否仍在运行
tasklist /FI "IMAGENAME eq PortMaster.exe" 2>NUL | find /I /N "PortMaster.exe">NUL
if errorlevel 1 (
    echo [ERROR] 程序启动失败或立即崩溃
    echo [ERROR] 程序启动失败或立即崩溃 > "test_screenshots\error_log.txt"
    goto :END
) else (
    echo [INFO] 程序正在运行，等待10秒进行稳定性检查...
    timeout /t 10 >nul

    rem 再次检查
    tasklist /FI "IMAGENAME eq PortMaster.exe" 2>NUL | find /I /N "PortMaster.exe">NUL
    if errorlevel 1 (
        echo [ERROR] 程序在运行期间崩溃
        echo [ERROR] 程序在运行期间崩溃 > "test_screenshots\error_log.txt"
    ) else (
        echo [SUCCESS] 程序稳定运行！
        echo [SUCCESS] 程序稳定运行！ > "test_screenshots\success_log.txt"

        rem 显示程序信息
        echo [INFO] 程序信息:
        tasklist /FI "IMAGENAME eq PortMaster.exe" /FO TABLE
    )
)

rem 5. 清理进程
taskkill /F /IM PortMaster.exe >nul 2>&1

:END
echo [INFO] 测试完成
if exist "test_screenshots\success_log.txt" (
    echo [SUCCESS] 测试通过
    type "test_screenshots\success_log.txt"
) else (
    echo [ERROR] 测试失败
    if exist "test_screenshots\error_log.txt" (
        type "test_screenshots\error_log.txt"
    )
)

pause