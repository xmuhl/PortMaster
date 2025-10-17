@echo off
setlocal

rem ===== Static Code Analysis Build =====
set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"

echo [INFO] Initializing Visual Studio environment...
if not exist "%VSDEV%" (
    echo [ERROR] VsDevCmd.bat not found. Please check your Visual Studio installation.
    exit /b 1
)
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize VS environment.
    exit /b 1
)
set VCPKG_APPLOCAL_DEPS_DISABLE=1
echo [INFO] VS environment initialized.

echo ==============================================================
echo = Running Static Code Analysis (Win32 Debug)
echo ==============================================================
set "LOG_FILE=%~dp0msbuild_static_analysis.log"
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%" >nul 2>&1

msbuild "%~dp0PortMaster.sln" ^
    /t:Rebuild ^
    /p:Configuration=Debug;Platform=Win32 ^
    /p:RunCodeAnalysis=true ^
    /p:CodeAnalysisTreatWarningsAsErrors=false ^
    /p:VcpkgApplocalDeps=false ^
    /p:UseMultiToolTask=true ^
    /p:EnforceProcessCountAcrossBuilds=true ^
    /maxcpucount ^
    /verbosity:normal ^
    /consoleloggerparameters:ShowTimestamp;ShowEventId;Verbosity=normal ^
    /flp:logfile="%LOG_FILE%";verbosity=detailed;encoding=utf-8
set "BUILD_RESULT=%ERRORLEVEL%"

if %BUILD_RESULT%==0 (
    echo --------------------------------------------------------------
    echo = Static Analysis Succeeded.
    echo --------------------------------------------------------------
) else (
    echo --------------------------------------------------------------
    echo = Static Analysis FAILED.
    echo --------------------------------------------------------------
    exit /b %BUILD_RESULT%
)

echo.
echo Static analysis log saved to %LOG_FILE%
exit /b 0
