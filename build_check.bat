@echo off
setlocal enabledelayedexpansion

rem ===== PortMaster 自动化构建检查脚本 =====
rem 用途：验证构建结果，分析编译日志，确保0 error 0 warning标准
rem 作者：PortMaster开发团队
rem 版本：v1.0

echo ===============================================
echo = PortMaster 构建检查工具
echo ===============================================

set "PROJECT_ROOT=%~dp0"
set "BUILD_LOG=%PROJECT_ROOT%msbuild_Win32_Debug.log"
set "CHECK_RESULT=0"

echo [INFO] 项目根目录: %PROJECT_ROOT%
echo [INFO] 构建日志文件: %BUILD_LOG%
echo.

rem 检查构建日志是否存在
if not exist "%BUILD_LOG%" (
    echo [ERROR] 构建日志文件不存在，请先运行 autobuild_x86_debug.bat
    set "CHECK_RESULT=1"
    goto :END_CHECK
)

echo [INFO] 开始分析构建日志...

rem 检查构建是否成功
findstr /C:"Build succeeded." "%BUILD_LOG%" >nul
if errorlevel 1 (
    echo [ERROR] 构建失败，请检查构建日志
    set "CHECK_RESULT=1"
    goto :SHOW_ERRORS
)

rem 统计错误数量
for /f %%i in ('findstr /C:" error " "%BUILD_LOG%" ^| find /c /v ""') do set "ERROR_COUNT=%%i"
for /f %%i in ('findstr /C:" Error " "%BUILD_LOG%" ^| find /c /v ""') do set "ERROR_COUNT2=%%i"
set /a "TOTAL_ERRORS=%ERROR_COUNT%+%ERROR_COUNT2%"

rem 统计警告数量
for /f %%i in ('findstr /C:" warning " "%BUILD_LOG%" ^| find /c /v ""') do set "WARNING_COUNT=%%i"
for /f %%i in ('findstr /C:" Warning " "%BUILD_LOG%" ^| find /c /v ""') do set "WARNING_COUNT2=%%i"
set /a "TOTAL_WARNINGS=%WARNING_COUNT%+%WARNING_COUNT2%"

echo [INFO] 构建统计结果:
echo       错误数量: %TOTAL_ERRORS%
echo       警告数量: %TOTAL_WARNINGS%
echo.

rem 检查是否符合0 error 0 warning标准
if %TOTAL_ERRORS% gtr 0 (
    echo [ERROR] 发现 %TOTAL_ERRORS% 个编译错误，不符合0 error标准
    set "CHECK_RESULT=1"
    goto :SHOW_ERRORS
)

if %TOTAL_WARNINGS% gtr 0 (
    echo [WARNING] 发现 %TOTAL_WARNINGS% 个编译警告，不符合0 warning标准
    set "CHECK_RESULT=1"
    goto :SHOW_WARNINGS
)

echo [SUCCESS] ✅ 构建检查通过！符合0 error 0 warning标准
goto :END_CHECK

:SHOW_ERRORS
echo.
echo [ERROR] 编译错误详情:
echo ----------------------------------------
findstr /C:" error " "%BUILD_LOG%"
findstr /C:" Error " "%BUILD_LOG%"
echo ----------------------------------------
goto :END_CHECK

:SHOW_WARNINGS
echo.
echo [WARNING] 编译警告详情:
echo ----------------------------------------
findstr /C:" warning " "%BUILD_LOG%"
findstr /C:" Warning " "%BUILD_LOG%"
echo ----------------------------------------
goto :END_CHECK

:END_CHECK
echo.
if %CHECK_RESULT%==0 (
    echo [RESULT] 构建检查: ✅ 通过
) else (
    echo [RESULT] 构建检查: ❌ 失败
)

echo.
echo 详细构建日志请查看: %BUILD_LOG%
echo ===============================================

exit /b %CHECK_RESULT%