@echo off
setlocal

rem ===== Build UI Managers Test Tool =====
echo [INFO] Building UI Managers Test Tool...

rem 设置Visual Studio环境
set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"

if not exist "%VSDEV%" (
    echo [ERROR] Visual Studio environment not found
    exit /b 1
)

call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize VS environment
    exit /b 1
)

rem 创建测试程序目录
if not exist "build\Debug\test" mkdir "build\Debug\test"

echo [INFO] Compiling UI Managers Test Program...
cl.exe /nologo /W3 /EHsc /MTd /Zi /Od ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
    /I "." /I "include" ^
    /Fo"build\Debug\test\\" ^
    /c test_ui_managers.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [INFO] Linking test_ui_managers.exe...
link.exe /nologo /OUT:build\Debug\test\test_ui_managers.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    build\Debug\test\test_ui_managers.obj ^
    build\Debug\obj\UIStateManager.obj ^
    build\Debug\obj\TransmissionStateManager.obj ^
    build\Debug\obj\ButtonStateManager.obj ^
    build\Debug\obj\ThreadSafeUIUpdater.obj ^
    build\Debug\obj\ThreadSafeProgressManager.obj ^
    build\Debug\obj\pch.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [SUCCESS] UI Managers Test Tool built successfully
echo [INFO] Executable: build\Debug\test\test_ui_managers.exe
exit /b 0