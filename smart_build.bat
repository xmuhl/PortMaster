@echo off
setlocal enabledelayedexpansion
echo ================================================================
echo = 智能编译脚本 - 自动处理进程占用问题
echo ================================================================

:RETRY_BUILD
echo.
echo [信息] 开始编译 Win32 Debug 配置...
call autobuild_x86_debug.bat > temp_build.log 2>&1

:: 检查编译结果
findstr /C:"已成功生成" temp_build.log > nul
if %ERRORLEVEL% == 0 (
    echo [成功] 编译完成 - Debug配置构建成功
    type temp_build.log | find "个警告"
    type temp_build.log | find "个错误"
    goto :CLEANUP_SUCCESS
)

:: 检查是否为进程占用错误
findstr /C:"LNK1104" temp_build.log > nul
if %ERRORLEVEL% == 0 (
    findstr /C:"无法打开文件" temp_build.log > nul
    if %ERRORLEVEL% == 0 (
        echo.
        echo [警告] 检测到文件被占用，可能是程序进程在运行
        echo [处理] 尝试关闭 PortMaster.exe 进程...
        
        :: 检查进程是否存在
        tasklist /FI "IMAGENAME eq PortMaster.exe" | findstr /I "PortMaster.exe" > nul
        if %ERRORLEVEL% == 0 (
            echo [信息] 发现 PortMaster.exe 进程，正在关闭...
            taskkill /F /IM PortMaster.exe > nul 2>&1
            if %ERRORLEVEL% == 0 (
                echo [成功] PortMaster.exe 进程已关闭
                timeout /t 2 > nul
                echo [信息] 等待2秒后重新编译...
                goto :RETRY_BUILD
            ) else (
                echo [错误] 无法关闭 PortMaster.exe 进程
            )
        ) else (
            echo [信息] 未发现 PortMaster.exe 进程，可能是其他文件占用
        )
    )
)

:: 如果不是进程占用问题，显示完整错误信息
echo.
echo [错误] 编译失败，错误信息：
type temp_build.log | find "error"
type temp_build.log | find "Error"
type temp_build.log | find "fatal"
echo.
echo [详情] 完整编译日志：
type temp_build.log
goto :CLEANUP_FAIL

:CLEANUP_SUCCESS
del temp_build.log > nul 2>&1
echo.
echo ================================================================
echo = 编译成功完成
echo ================================================================
exit /b 0

:CLEANUP_FAIL
del temp_build.log > nul 2>&1
echo.
echo ================================================================
echo = 编译失败
echo ================================================================
exit /b 1