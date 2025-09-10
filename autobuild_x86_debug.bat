@echo off
setlocal enabledelayedexpansion

rem ======================================================================
rem  AutoDeal - Auto-Build (x86 Debug) - For Visual Studio 2022
rem  Features:
rem    1) Find .sln file automatically
rem    2) Build for Win32 Debug
rem    3) Generate detailed log file
rem ======================================================================

:: ==== Config ====
set "DEBUG_PAUSE=1"
set "KEEP_LOGS=1"
:: Set path to VS dev tools
set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
:: ========================

echo.
echo [INFO] Cleaning up old build logs...
if "%KEEP_LOGS%"=="0" (
    for /r "%~dp0" %%L in (msbuild_*.log) do del /f /q "%%L" >nul 2>&1
)
echo [INFO] Cleanup complete.
echo.

:: ---------- Find Solution File ----------
set "SOLUTION="
for /f "delims=" %%I in ('dir "%~dp0*.sln" /s /b 2^>nul') do (
    set "SOLUTION=%%I"
    goto :FOUND_SOLUTION
)

echo [ERROR] .sln file not found. Please place the script in the same directory as the solution.
if "%DEBUG_PAUSE%"=="1" pause
exit /b 1

:FOUND_SOLUTION
echo [INFO] Found solution:
echo   %SOLUTION%
echo.

:: ---------- Initialize VS Environment ----------
echo [INFO] Initializing Visual Studio environment...
if not exist "%VSDEV%" (
    echo [ERROR] VsDevCmd.bat not found. Please check your Visual Studio 2022 installation path.
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)

:: Initialize VS env to support all architectures
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize VS2022 environment.
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)
echo [INFO] VS environment initialized successfully.
echo.

:: ---------- Start Build ----------
set "TOTAL_BUILDS=0"
set "SUCCESS_BUILDS=0"
set "FAILED_BUILDS=0"

:: Call build for Win32 Debug directly
call :BUILD_CONFIG "Win32" "Debug"

goto :SHOW_SUMMARY


:SHOW_SUMMARY
:: ---------- Show Summary ----------
echo.
echo ================================================================
echo = Build Summary
echo ================================================================
echo Total builds: %TOTAL_BUILDS%
echo Succeeded: %SUCCESS_BUILDS%
echo Failed: %FAILED_BUILDS%
echo ================================================================

if %FAILED_BUILDS% gtr 0 (
    echo [ERROR] %FAILED_BUILDS% build(s) failed. Please check the log file.
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
) else (
    echo [SUCCESS] All builds completed!
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 0
)


:: ========== Build Subroutine ==========
:BUILD_CONFIG
set "PLATFORM=%~1"
set "CONFIG=%~2"
set "LOG_FILE=%~dp0msbuild_%PLATFORM%_%CONFIG%.log"

set /a TOTAL_BUILDS+=1

echo ==============================================================
echo = Building... Platform: %PLATFORM%   Config: %CONFIG%
echo ==============================================================

:: Delete old log
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%" >nul 2>&1

:: Execute build
msbuild "%SOLUTION%" ^
    /t:Rebuild ^
    /p:Configuration=%CONFIG%;Platform=%PLATFORM% ^
    /p:WarningLevel=3 ^
    /p:TreatWarningsAsErrors=false ^
    /p:UseMultiToolTask=true ^
    /p:EnforceProcessCountAcrossBuilds=true ^
    /maxcpucount ^
    /verbosity:normal ^
    /consoleloggerparameters:ShowTimestamp;ShowEventId;Verbosity=normal;ShowCommandLine=false ^
    /flp:logfile="%LOG_FILE%";verbosity=normal;encoding=utf-8

set "BUILD_RESULT=%ERRORLEVEL%"

if %BUILD_RESULT% equ 0 (
    echo --------------------------------------------------------------
    echo = Build Succeeded. Platform: %PLATFORM%   Config: %CONFIG%
    echo --------------------------------------------------------------
    set /a SUCCESS_BUILDS+=1
) else (
    echo --------------------------------------------------------------
    echo = Build FAILED. Platform: %PLATFORM%   Config: %CONFIG%
    echo = Error code: %BUILD_RESULT%
    echo = See log for details: %LOG_FILE%
    echo --------------------------------------------------------------
    set /a FAILED_BUILDS+=1
    
    :: Show key errors
    if exist "%LOG_FILE%" (
        echo [Key Errors:]
        type "%LOG_FILE%" | findstr /i "error\|fatal\|failed"
    )
)

echo.
exit /b %BUILD_RESULT%