@echo off

setlocal enabledelayedexpansion

rem ======================================================================
rem  SD24PDrv 自动编译脚本 - 支持Visual Studio 2022 + WDK
rem  功能：
rem    1) 自动查找解决方案文件（一个 .sln
rem    2) 编译 Win32 / x64 的 Debug / Release  
rem    3) 每次编译生成独立详细日志文件（共4个）
rem    4) 与IDE一致的编译环境和使用体验
rem ======================================================================

:: ==== 配置参数 ====
set "DEBUG_PAUSE=0"
set "KEEP_LOGS=1"
:: 根据安装版本修改路径
set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
:: ========================

echo.
echo [信息] 清理旧的编译日志文件...
if "%KEEP_LOGS%"=="0" (
    for /r "%~dp0" %%L in (msbuild_*.log) do del /f /q "%%L" >nul 2>&1
)
echo [信息] 清理完成
echo.

:: ---------- 查找解决方案文件 ----------
set "SOLUTION="
for /f "delims=" %%I in ('dir "%~dp0*.sln" /s /b 2^>nul') do (
    set "SOLUTION=%%I"
    goto :FOUND_SOLUTION
)

echo [错误] 未找到 .sln 文件，请将脚本放在解决方案同级目录内。
if "%DEBUG_PAUSE%"=="1" pause
exit /b 1

:FOUND_SOLUTION
echo [信息] 找到解决方案：
echo   %SOLUTION%
echo.

:: ---------- 解析解决方案配置 ----------
echo [信息] 解析解决方案配置...
set "CONFIG_COUNT=0"
set "CONFIG_LIST="

:: 创建临时文件存储配置信息
set "TEMP_CONFIG=%TEMP%\sln_configs_%RANDOM%.tmp"

:: 提取SolutionConfigurationPlatforms部分，排除包含GUID的行
powershell -Command "Get-Content '%SOLUTION%' | Where-Object { $_ -match '^\s*[^=]*\|[^=]*\s*=' -and $_ -notmatch '\{.*\}' } | ForEach-Object { ($_ -split '=')[0].Trim() }" > "%TEMP_CONFIG%"

:: 检查是否成功提取到配置
if not exist "%TEMP_CONFIG%" (
    echo [错误] 无法解析解决方案配置
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

:: 读取并验证配置
for /f "usebackq delims=" %%C in ("%TEMP_CONFIG%") do (
    set "CONFIG_LINE=%%C"
    if not "!CONFIG_LINE!"=="" (
        set /a CONFIG_COUNT+=1
        if !CONFIG_COUNT! leq 20 (
            echo   配置 !CONFIG_COUNT!: !CONFIG_LINE!
        )
    )
)

:: 清理临时文件
del /f /q "%TEMP_CONFIG%" >nul 2>&1

if %CONFIG_COUNT% equ 0 (
    echo [警告] 未找到有效配置，回退到默认配置
    echo [信息] 使用默认配置: Debug^|Win32, Debug^|x64, Release^|Win32, Release^|x64
    goto :DEFAULT_CONFIGS
)

echo [信息] 共找到 %CONFIG_COUNT% 个配置组合
echo.

:: ---------- 初始化VS环境（一次性） ----------
echo [信息] 初始化 Visual Studio 开发环境...
if not exist "%VSDEV%" (
    echo [错误] 未找到 VsDevCmd.bat，请检查 Visual Studio 2022 安装路径
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

:: 初始化VS环境，支持所有架构
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [错误] 无法初始化 VS2022 环境
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

set VCPKG_APPLOCAL_DEPS_DISABLE=1
echo [信息] VS环境初始化成功
echo.

:: ---------- 编译所有配置 ----------
set "TOTAL_BUILDS=0"
set "SUCCESS_BUILDS=0"
set "FAILED_BUILDS=0"

:: 重新读取配置并编译
set "TEMP_CONFIG=%TEMP%\sln_configs_%RANDOM%.tmp"
powershell -Command "Get-Content '%SOLUTION%' | Where-Object { $_ -match '^\s*[^=]*\|[^=]*\s*=' -and $_ -notmatch '\{.*\}' } | ForEach-Object { ($_ -split '=')[0].Trim() }" > "%TEMP_CONFIG%"

for /f "usebackq delims=" %%C in ("%TEMP_CONFIG%") do (
    set "CONFIG_LINE=%%C"
    if not "!CONFIG_LINE!"=="" (
        call :PARSE_AND_BUILD "!CONFIG_LINE!"
    )
)

:: 清理临时文件
del /f /q "%TEMP_CONFIG%" >nul 2>&1
goto :SHOW_SUMMARY

:DEFAULT_CONFIGS
:: 默认配置回退
for %%P in (Win32 x64) do (
    for %%C in (Debug Release) do (
        call :BUILD_CONFIG "%%P" "%%C"
    )
)

:SHOW_SUMMARY
:: ---------- 编译结果汇总 ----------
echo.
echo ================================================================
echo = 编译结果汇总
echo ================================================================
echo 总计编译配置: %TOTAL_BUILDS%
echo 成功: %SUCCESS_BUILDS%
echo 失败: %FAILED_BUILDS%
echo ================================================================

if %FAILED_BUILDS% gtr 0 (
    echo [错误] 有 %FAILED_BUILDS% 个配置编译失败，请检查日志文件
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
) else (
    echo [成功] 所有配置编译完成！
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 0
)

:: ========== 解析并编译配置子程序 ==========
:PARSE_AND_BUILD
set "FULL_CONFIG=%~1"

:: 分离Configuration和Platform（使用|分隔符）
for /f "tokens=1,2 delims=|" %%A in ("%FULL_CONFIG%") do (
    set "CONFIG=%%A"
    set "PLATFORM=%%B"
)

:: 去除可能的空格
set "CONFIG=%CONFIG: =%"
set "PLATFORM=%PLATFORM: =%"

:: 验证解析结果
if "%CONFIG%"=="" (
    echo [警告] 无法解析配置名称：%FULL_CONFIG%
    exit /b 0
)
if "%PLATFORM%"=="" (
    echo [警告] 无法解析平台名称：%FULL_CONFIG%
    exit /b 0
)

:: 调用编译子程序
call :BUILD_CONFIG "%PLATFORM%" "%CONFIG%"
exit /b 0

:: ========== 单个配置编译子程序 ==========
:BUILD_CONFIG
set "PLATFORM=%~1"
set "CONFIG=%~2"
set "LOG_FILE=%~dp0msbuild_%PLATFORM%_%CONFIG%.log"

set /a TOTAL_BUILDS+=1

echo ==============================================================
echo = 开始编译  平台: %PLATFORM%   配置: %CONFIG%
echo ==============================================================

:: 删除旧日志
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%" >nul 2>&1

:: 执行编译 - 强制重新编译以显示警告信息
msbuild "%SOLUTION%" ^
    /t:Rebuild ^
    /p:Configuration=%CONFIG%;Platform=%PLATFORM% ^
    /p:WarningLevel=3 ^
    /p:TreatWarningsAsErrors=false ^
    /p:VcpkgApplocalDeps=false ^
    /p:UseMultiToolTask=true ^
    /p:EnforceProcessCountAcrossBuilds=true ^
    /maxcpucount ^
    /verbosity:normal ^
    /consoleloggerparameters:ShowTimestamp;ShowEventId;Verbosity=normal;ShowCommandLine=false ^
    /flp:logfile="%LOG_FILE%";verbosity=normal;encoding=utf-8

set "BUILD_RESULT=%ERRORLEVEL%"

if %BUILD_RESULT% equ 0 (
    echo --------------------------------------------------------------
    echo = 编译成功  平台: %PLATFORM%   配置: %CONFIG%
    echo --------------------------------------------------------------
    set /a SUCCESS_BUILDS+=1
) else (
    echo --------------------------------------------------------------
    echo = 编译失败  平台: %PLATFORM%   配置: %CONFIG%
    echo = 错误代码: %BUILD_RESULT%
    echo = 详细日志: %LOG_FILE%
    echo --------------------------------------------------------------
    set /a FAILED_BUILDS+=1
    
    :: 显示关键错误信息
    if exist "%LOG_FILE%" (
        echo [关键错误信息:]
        type "%LOG_FILE%" | findstr /i "error\|fatal\|failed"
    )
)

echo.
exit /b %BUILD_RESULT%
