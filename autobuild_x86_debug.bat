@echo off
setlocal

rem ===== Auto build Win32 Debug =====
set "DEBUG_PAUSE=0"
set "KEEP_LOGS=1"

set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"

echo [INFO] Cleaning up old build logs...
if "%KEEP_LOGS%"=="0" for /r "%~dp0" %%L in (msbuild_*.log) do del /f /q "%%L" >nul 2>&1
echo [INFO] Cleanup complete.

set "SOLUTION="
for /f "delims=" %%I in ('dir "%~dp0*.sln" /s /b 2^>nul') do (
    set "SOLUTION=%%I"
    goto :FOUND_SOLUTION
)
echo [ERROR] .sln file not found.
if "%DEBUG_PAUSE%"=="1" pause
exit /b 1

:FOUND_SOLUTION
echo [INFO] Found solution: %SOLUTION%

echo [INFO] Initializing Visual Studio environment...
if not exist "%VSDEV%" (
    echo [ERROR] VsDevCmd.bat not found. Please check your Visual Studio installation.
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize VS environment.
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b 1
)
set VCPKG_APPLOCAL_DEPS_DISABLE=1
echo [INFO] VS environment initialized.

echo ==============================================================
echo = Building Win32 Debug
echo ==============================================================
set "LOG_FILE=%~dp0msbuild_Win32_Debug.log"
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%" >nul 2>&1

msbuild "%SOLUTION%" ^
    /t:Rebuild ^
    /p:Configuration=Debug;Platform=Win32 ^
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

if %BUILD_RESULT%==0 (
    echo --------------------------------------------------------------
    echo = Build Succeeded. Platform: Win32   Config: Debug
    echo --------------------------------------------------------------
) else (
    echo --------------------------------------------------------------
    echo = Build FAILED. Platform: Win32   Config: Debug
    echo --------------------------------------------------------------
    if "%DEBUG_PAUSE%"=="1" pause
    exit /b %BUILD_RESULT%
)

echo.
echo Build log saved to %LOG_FILE%
if "%DEBUG_PAUSE%"=="1" pause
exit /b 0
