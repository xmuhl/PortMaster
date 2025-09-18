@echo off

setlocal enabledelayedexpansion

rem ======================================================================
rem  SD24PDrv �Զ�����ű� - ֧��Visual Studio 2022 + WDK
rem  ���ܣ�
rem    1) �Զ����ҽ�������ļ���һ�� .sln
rem    2) ���� Win32 / x64 �� Debug / Release  
rem    3) ÿ�α������ɶ�����ϸ��־�ļ�����4����
rem    4) ��IDEһ�µı��뻷����ʹ������
rem ======================================================================

:: ==== ���ò��� ====
set "DEBUG_PAUSE=0"
set "KEEP_LOGS=1"
:: ���ݰ�װ�汾�޸�·��
set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
:: ========================

echo.
echo [��Ϣ] ����ɵı�����־�ļ�...
if "%KEEP_LOGS%"=="0" (
    for /r "%~dp0" %%L in (msbuild_*.log) do del /f /q "%%L" >nul 2>&1
)
echo [��Ϣ] �������
echo.

:: ---------- ���ҽ�������ļ� ----------
set "SOLUTION="
for /f "delims=" %%I in ('dir "%~dp0*.sln" /s /b 2^>nul') do (
    set "SOLUTION=%%I"
    goto :FOUND_SOLUTION
)

echo [����] δ�ҵ� .sln �ļ����뽫�ű����ڽ������ͬ��Ŀ¼�ڡ�
if "%DEBUG_PAUSE%"=="1" pause
exit /b 1

:FOUND_SOLUTION
echo [��Ϣ] �ҵ����������
echo   %SOLUTION%
echo.

:: ---------- ��������������� ----------
echo [��Ϣ] ���������������...
set "CONFIG_COUNT=0"
set "CONFIG_LIST="

:: ������ʱ�ļ��洢������Ϣ
set "TEMP_CONFIG=%TEMP%\sln_configs_%RANDOM%.tmp"

:: ��ȡSolutionConfigurationPlatforms���֣��ų�����GUID����
powershell -Command "Get-Content '%SOLUTION%' | Where-Object { $_ -match '^\s*[^=]*\|[^=]*\s*=' -and $_ -notmatch '\{.*\}' } | ForEach-Object { ($_ -split '=')[0].Trim() }" > "%TEMP_CONFIG%"

:: ����Ƿ�ɹ���ȡ������
if not exist "%TEMP_CONFIG%" (
    echo [����] �޷����������������
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

:: ��ȡ����֤����
for /f "usebackq delims=" %%C in ("%TEMP_CONFIG%") do (
    set "CONFIG_LINE=%%C"
    if not "!CONFIG_LINE!"=="" (
        set /a CONFIG_COUNT+=1
        if !CONFIG_COUNT! leq 20 (
            echo   ���� !CONFIG_COUNT!: !CONFIG_LINE!
        )
    )
)

:: ������ʱ�ļ�
del /f /q "%TEMP_CONFIG%" >nul 2>&1

if %CONFIG_COUNT% equ 0 (
    echo [����] δ�ҵ���Ч���ã����˵�Ĭ������
    echo [��Ϣ] ʹ��Ĭ������: Debug^|Win32, Debug^|x64, Release^|Win32, Release^|x64
    goto :DEFAULT_CONFIGS
)

echo [��Ϣ] ���ҵ� %CONFIG_COUNT% ���������
echo.

:: ---------- ��ʼ��VS������һ���ԣ� ----------
echo [��Ϣ] ��ʼ�� Visual Studio ��������...
if not exist "%VSDEV%" (
    echo [����] δ�ҵ� VsDevCmd.bat������ Visual Studio 2022 ��װ·��
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

:: ��ʼ��VS������֧�����мܹ�
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [����] �޷���ʼ�� VS2022 ����
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

set VCPKG_APPLOCAL_DEPS_DISABLE=1
echo [��Ϣ] VS������ʼ���ɹ�
echo.

:: ---------- ������������ ----------
set "TOTAL_BUILDS=0"
set "SUCCESS_BUILDS=0"
set "FAILED_BUILDS=0"

:: ���¶�ȡ���ò�����
set "TEMP_CONFIG=%TEMP%\sln_configs_%RANDOM%.tmp"
powershell -Command "Get-Content '%SOLUTION%' | Where-Object { $_ -match '^\s*[^=]*\|[^=]*\s*=' -and $_ -notmatch '\{.*\}' } | ForEach-Object { ($_ -split '=')[0].Trim() }" > "%TEMP_CONFIG%"

for /f "usebackq delims=" %%C in ("%TEMP_CONFIG%") do (
    set "CONFIG_LINE=%%C"
    if not "!CONFIG_LINE!"=="" (
        call :PARSE_AND_BUILD "!CONFIG_LINE!"
    )
)

:: ������ʱ�ļ�
del /f /q "%TEMP_CONFIG%" >nul 2>&1
goto :SHOW_SUMMARY

:DEFAULT_CONFIGS
:: Ĭ�����û���
for %%P in (Win32 x64) do (
    for %%C in (Debug Release) do (
        call :BUILD_CONFIG "%%P" "%%C"
    )
)

:SHOW_SUMMARY
:: ---------- ���������� ----------
echo.
echo ================================================================
echo = ����������
echo ================================================================
echo �ܼƱ�������: %TOTAL_BUILDS%
echo �ɹ�: %SUCCESS_BUILDS%
echo ʧ��: %FAILED_BUILDS%
echo ================================================================

if %FAILED_BUILDS% gtr 0 (
    echo [����] �� %FAILED_BUILDS% �����ñ���ʧ�ܣ�������־�ļ�
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
) else (
    echo [�ɹ�] �������ñ�����ɣ�
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 0
)

:: ========== ���������������ӳ��� ==========
:PARSE_AND_BUILD
set "FULL_CONFIG=%~1"

:: ����Configuration��Platform��ʹ��|�ָ�����
for /f "tokens=1,2 delims=|" %%A in ("%FULL_CONFIG%") do (
    set "CONFIG=%%A"
    set "PLATFORM=%%B"
)

:: ȥ�����ܵĿո�
set "CONFIG=%CONFIG: =%"
set "PLATFORM=%PLATFORM: =%"

:: ��֤�������
if "%CONFIG%"=="" (
    echo [����] �޷������������ƣ�%FULL_CONFIG%
    exit /b 0
)
if "%PLATFORM%"=="" (
    echo [����] �޷�����ƽ̨���ƣ�%FULL_CONFIG%
    exit /b 0
)

:: ���ñ����ӳ���
call :BUILD_CONFIG "%PLATFORM%" "%CONFIG%"
exit /b 0

:: ========== �������ñ����ӳ��� ==========
:BUILD_CONFIG
set "PLATFORM=%~1"
set "CONFIG=%~2"
set "LOG_FILE=%~dp0msbuild_%PLATFORM%_%CONFIG%.log"

set /a TOTAL_BUILDS+=1

echo ==============================================================
echo = ��ʼ����  ƽ̨: %PLATFORM%   ����: %CONFIG%
echo ==============================================================

:: ɾ������־
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%" >nul 2>&1

:: ִ�б��� - ǿ�����±�������ʾ������Ϣ
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
    echo = ����ɹ�  ƽ̨: %PLATFORM%   ����: %CONFIG%
    echo --------------------------------------------------------------
    set /a SUCCESS_BUILDS+=1
) else (
    echo --------------------------------------------------------------
    echo = ����ʧ��  ƽ̨: %PLATFORM%   ����: %CONFIG%
    echo = �������: %BUILD_RESULT%
    echo = ��ϸ��־: %LOG_FILE%
    echo --------------------------------------------------------------
    set /a FAILED_BUILDS+=1
    
    :: ��ʾ�ؼ�������Ϣ
    if exist "%LOG_FILE%" (
        echo [�ؼ�������Ϣ:]
        type "%LOG_FILE%" | findstr /i "error\|fatal\|failed"
    )
)

echo.
exit /b %BUILD_RESULT%
