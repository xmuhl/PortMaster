@echo off
setlocal

rem ===== 简化的PortMaster测试运行器 =====
echo [INFO] 开始PortMaster测试...

cd /d "%~dp0"

rem 检查可执行文件
if not exist "build\Debug\PortMaster.exe" (
    echo [ERROR] 可执行文件不存在
    pause
    exit /b 1
)

rem 创建截图目录
if not exist "test_screenshots" mkdir test_screenshots

rem 生成时间戳
for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set "timestamp=%dt:~0,8%-%dt:~8,6%"

echo [INFO] 时间戳: %timestamp%

rem 启动前截图（使用Windows内置截图工具）
echo [INFO] 启动前截图...
rem 这里我们创建一个标记文件表示截图时间
echo 启动前截图时间 > "test_screenshots\before_start_%timestamp%.txt"

rem 清理现有进程
echo [INFO] 清理现有进程...
taskkill /F /IM PortMaster.exe >nul 2>&1
timeout /t 2 >nul

rem 启动程序
echo [INFO] 启动程序...
start "" "build\Debug\PortMaster.exe"

rem 等待启动
echo [INFO] 等待程序启动...
timeout /t 5 >nul

rem 检查进程是否运行
tasklist /FI "IMAGENAME eq PortMaster.exe" 2>NUL | find /I /N "PortMaster.exe">NUL
if %ERRORLEVEL% equ 0 (
    echo [INFO] 程序正在运行

    rem 启动后截图标记
    echo 启动后截图时间 > "test_screenshots\after_start_%timestamp%.txt"

    rem 等待观察
    echo [INFO] 等待15秒进行观察...
    timeout /t 15 >nul

    rem 检查是否有错误窗口
    echo [INFO] 检查错误窗口...
    tasklist /FI "WINDOWTITLE eq *Debug*" 2>NUL | find /I /N "Debug">NUL
    if %ERRORLEVEL% equ 0 (
        echo [WARNING] 检测到Debug窗口
        echo 检测到Debug窗口 > "test_screenshots\debug_window_detected_%timestamp%.txt"
    )

    tasklist /FI "WINDOWTITLE eq *Assertion*" 2>NUL | find /I /N "Assertion">NUL
    if %ERRORLEVEL% equ 0 (
        echo [WARNING] 检测到Assertion窗口
        echo 检测到Assertion窗口 > "test_screenshots\assertion_window_detected_%timestamp%.txt"
    )

    rem 最终截图标记
    echo 最终观察时间 > "test_screenshots\final_observation_%timestamp%.txt"

    echo [INFO] 测试观察完成
) else (
    echo [ERROR] 程序未运行或已退出
    echo 程序未运行 > "test_screenshots\program_not_running_%timestamp%.txt"
)

rem 生成测试报告
echo === PortMaster 测试报告 === > "test_screenshots\test_report_%timestamp%.txt"
echo 测试时间: %date% %time% >> "test_screenshots\test_report_%timestamp%.txt"
echo 可执行文件: build\Debug\PortMaster.exe >> "test_screenshots\test_report_%timestamp%.txt"
echo. >> "test_screenshots\test_report_%timestamp%.txt"
echo 测试结果: >> "test_screenshots\test_report_%timestamp%.txt"
dir "test_screenshots\*%timestamp%.txt" /B >> "test_screenshots\test_report_%timestamp%.txt"

rem 清理进程
echo [INFO] 清理测试进程...
taskkill /F /IM PortMaster.exe >nul 2>&1

echo.
echo [INFO] 测试完成
echo [INFO] 查看test_screenshots目录中的测试文件
dir "test_screenshots\*%timestamp%.*"

pause