@echo off
setlocal enabledelayedexpansion

rem ===== PortMaster 智能自动化修复系统 =====
echo [INFO] 启动PortMaster智能自动化修复系统...

cd /d "%~dp0"

rem 检查Python是否可用
echo [INFO] 检查Python环境...
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python未找到，请确保Python已安装并添加到PATH
    pause
    exit /b 1
)

echo [INFO] Python环境检查通过

rem 检查必要文件
echo [INFO] 检查必要文件...
set "MISSING_FILES="

if not exist "iterative_fix.py" set "MISSING_FILES=!MISSING_FILES! iterative_fix.py"
if not exist "error_diagnostic.py" set "MISSING_FILES=!MISSING_FILES! error_diagnostic.py"
if not exist "auto_fix.py" set "MISSING_FILES=!MISSING_FILES! auto_fix.py"
if not exist "simple_test_runner.bat" set "MISSING_FILES=!MISSING_FILES! simple_test_runner.bat"

if not "%MISSING_FILES%"=="" (
    echo [ERROR] 缺少必要文件: %MISSING_FILES%
    pause
    exit /b 1
)

echo [INFO] 所有必要文件检查通过

rem 清理之前的测试结果（可选）
echo [INFO] 清理之前的测试结果...
if exist "test_screenshots" (
    echo [INFO] 保留现有测试结果目录
) else (
    mkdir test_screenshots
)

rem 创建日志文件
echo [INFO] 创建日志文件...
set "LOG_FILE=auto_fix_log_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt"
set "LOG_FILE=%LOG_FILE: =0%"
set "LOG_FILE=%LOG_FILE::=_%"

echo 开始智能自动化修复 - %date% %time% > "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"

rem 显示系统信息
echo [INFO] 系统信息:
echo   - 工作目录: %CD%
echo   - 日志文件: %LOG_FILE%
echo   - 时间戳: %date% %time%

echo [INFO] 系统信息 >> "%LOG_FILE%"
echo   - 工作目录: %CD% >> "%LOG_FILE%"
echo   - Python版本: >> "%LOG_FILE%"
python --version >> "%LOG_FILE%"

echo.
echo [INFO] 开始智能自动化修复流程...
echo [INFO] 系统将自动执行以下步骤：
echo   1. 运行测试程序并检测错误
echo   2. 诊断问题根因
echo   3. 自动应用修复方案
echo   4. 重新编译验证
echo   5. 迭代执行直到测试通过
echo.

rem 记录开始
echo 开始智能自动化修复流程 - %date% %time% >> "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"

rem 运行智能自动化修复系统
echo [INFO] 执行迭代修复控制器...
python iterative_fix.py

set "FIX_RESULT=%ERRORLEVEL%"

echo.
echo ========================================= >> "%LOG_FILE%"
echo 修复流程结束 - %date% %time% >> "%LOG_FILE%"
echo 退出代码: %FIX_RESULT% >> "%LOG_FILE%"

rem 分析结果
echo [INFO] 分析修复结果...

if "%FIX_RESULT%"=="0" (
    echo.
    echo [SUCCESS] ✅ 智能自动化修复成功！
    echo [SUCCESS] 程序现在应该可以正常运行了。

    echo [SUCCESS] 修复成功！ >> "%LOG_FILE%"
    echo 程序现在应该可以正常运行了。 >> "%LOG_FILE%"

    rem 显示最新结果
    echo [INFO] 生成测试报告...
    if exist "test_screenshots\test_report_*.txt" (
        echo [INFO] 最新测试报告:
        dir "test_screenshots\test_report_*.txt" /B /O:-D | for /f "tokens=*" %%f in ('more +0 ^0') do (
            echo   - %%f
            echo [INFO] 报告内容:
            type "test_screenshots\%%f" | findstr /V "^$" | head -10
            goto :break_report
        )
        :break_report
    )

) else (
    echo.
    echo [ERROR] ❌ 智能自动化修复失败
    echo [ERROR] 可能需要手动介入解决剩余问题。

    echo [ERROR] 修复失败！ >> "%LOG_FILE%"
    echo 可能需要手动介入解决剩余问题。 >> "%LOG_FILE%"

    rem 显示错误详情
    echo [INFO] 查看详细错误信息...
    if exist "iterative_fix_log.txt" (
        echo [INFO] 错误详情:
        type "iterative_fix_log.txt" | tail -20
    )

    if exist "test_screenshots\test_result_*.json" (
        echo [INFO] 最新测试结果:
        dir "test_screenshots\test_result_*.json" /B /O:-D | for /f "tokens=*" %%f in ('more +0 ^0') do (
            echo   - %%f
            goto :break_json
        )
        :break_json
    )
)

echo.
echo [INFO] 查看完整日志: %LOG_FILE%
echo [INFO] 查看会话结果: test_screenshots\iterative_fix_session_*.json
echo [INFO] 查看测试结果: test_screenshots\test_result_*.json
echo [INFO] 查看诊断报告: test_screenshots\diagnostic_report_*.txt
echo [INFO] 查看修复记录: test_screenshots\fix_result_*.json

rem 显示文件列表
echo.
echo [INFO] 生成的文件列表:
if exist "test_screenshots\*.json" (
    echo JSON结果文件:
    dir "test_screenshots\*.json" /B | findstr "json"
)

if exist "test_screenshots\*.txt" (
    echo 文本报告文件:
    dir "test_screenshots\*.txt" /B | findstr "txt"
)

if exist "code_backups\*" (
    echo 备份文件:
    dir "code_backups" /B
)

echo.
echo [INFO] 自动化修复流程完成
echo [INFO] 如果问题仍然存在，请查看上述日志文件获取详细信息

rem 清理进程
echo [INFO] 清理可能残留的进程...
taskkill /F /IM PortMaster.exe >nul 2>&1

rem 根据结果设置退出代码
if "%FIX_RESULT%"=="0" (
    pause
    exit /b 0
) else (
    pause
    exit /b 1
)