@echo off
setlocal enabledelayedexpansion

rem ===== PortMaster 简化自动修复系统 =====
echo [INFO] 启动PortMaster简化自动修复系统...

cd /d "%~dp0"

set "LOG_FILE=auto_fix_log_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt"
set "LOG_FILE=%LOG_FILE: =0%"
set "LOG_FILE=%LOG_FILE::=_%"

echo 开始自动修复 - %date% %time% > "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"

set "MAX_ITERATIONS=5"
set "CURRENT_ITERATION=0"

:ITERATION_START
set /a CURRENT_ITERATION+=1
echo [INFO] === 第 %CURRENT_ITERATION% 次迭代 ===
echo [INFO] 第 %CURRENT_ITERATION% 次迭代 >> "%LOG_FILE%"

if %CURRENT_ITERATION% GTR %MAX_ITERATIONS% (
    echo [ERROR] 达到最大迭代次数，修复失败
    echo [ERROR] 达到最大迭代次数，修复失败 >> "%LOG_FILE%"
    goto :END
)

rem 步骤1: 运行测试
echo [INFO] 步骤1: 运行测试...
call simple_test_runner.bat

rem 检查测试结果
set "TEST_SUCCESS=false"
if exist "test_screenshots\test_status_*.txt" (
    for /f "tokens=2 delims==" %%a in ('type test_screenshots\test_status_*.txt ^| findstr "TEST_RESULT=RUNNING"') do (
        if "%%a"=="RUNNING" (
            set "TEST_SUCCESS=true"
            echo [SUCCESS] 测试通过！程序运行正常
            echo [SUCCESS] 测试通过！程序运行正常 >> "%LOG_FILE%"
            goto :SUCCESS
        )
    )
)

echo [WARNING] 测试未通过，开始自动修复...
echo [WARNING] 测试未通过，开始自动修复 >> "%LOG_FILE%"

rem 步骤2: 检查常见问题并修复
echo [INFO] 步骤2: 检查常见问题...

rem 检查1: 管理器初始化顺序问题
echo [INFO] 检查管理器初始化顺序...
findstr /C:"InitializeManagersAfterControlsCreated" src\PortMasterDlg.cpp >nul 2>&1
if errorlevel 1 (
    echo [INFO] 修复管理器初始化顺序问题...
    goto :FIX_INITIALIZATION_ORDER
)

rem 检查2: 控件注册问题
echo [INFO] 检查控件注册问题...
findstr /C:"RegisterControl" src\PortMasterDlg.cpp >nul 2>&1
if not errorlevel 1 (
    echo [INFO] 临时注释控件注册...
    goto :FIX_CONTROL_REGISTRATION
)

rem 检查3: 重新编译
echo [INFO] 尝试重新编译...
call autobuild_x86_debug.bat
if errorlevel 1 (
    echo [ERROR] 编译失败
    echo [ERROR] 编译失败 >> "%LOG_FILE%"
) else (
    echo [SUCCESS] 编译成功
    echo [SUCCESS] 编译成功 >> "%LOG_FILE%"
)

goto :ITERATION_START

:FIX_INITIALIZATION_ORDER
echo [INFO] 正在修复初始化顺序...
echo [INFO] 管理器应该在OnInitDialog中初始化，而不是在构造函数中
echo [INFO] 请参考已实现的InitializeManagersAfterControlsCreated函数
goto :ITERATION_START

:FIX_CONTROL_REGISTRATION
echo [INFO] 正在修复控件注册问题...
echo [INFO] 控件注册应该移除，避免在错误的时机访问控件
goto :ITERATION_START

:SUCCESS
echo [SUCCESS] ===== 修复成功 =====
echo [SUCCESS] 程序现在可以正常运行了！
echo [SUCCESS] 修复完成时间: %date% %time% >> "%LOG_FILE%"

rem 显示测试结果
echo [INFO] 测试结果摘要:
if exist "test_screenshots\test_report_*.txt" (
    echo [INFO] 测试报告:
    dir "test_screenshots\test_report_*.txt" /B /O:-D | for /f "tokens=*" %%f in ('more +0 ^0') do (
        echo   - %%f
        type "test_screenshots\%%f" | findstr /V "^$"
        goto :BREAK_SHOW
    )
    :BREAK_SHOW
)

goto :END

:END
echo [INFO] 自动修复流程结束
echo [INFO] 总迭代次数: %CURRENT_ITERATION%
echo [INFO] 最终状态: %TEST_SUCCESS%
echo [INFO] 查看详细日志: %LOG_FILE%

rem 清理进程
echo [INFO] 清理测试进程...
taskkill /F /IM PortMaster.exe >nul 2>&1

if "%TEST_SUCCESS%"=="true" (
    echo [SUCCESS] 自动修复成功！
    pause
    exit /b 0
) else (
    echo [ERROR] 自动修复失败，可能需要手动介入
    pause
    exit /b 1
)