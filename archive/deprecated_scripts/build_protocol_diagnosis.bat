@echo off
setlocal

rem ===== Build Protocol Diagnosis Test Tool =====
echo [INFO] Building Protocol Diagnosis Test Tool...

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

echo [INFO] Compiling Protocol Diagnosis Test Program...
cl.exe /nologo /W3 /EHsc /MTd /Zi /Od ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
    /I "." /I "include" ^
    /Fo"build\Debug\test\\" ^
    /c test_protocol_diagnosis.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [INFO] Linking test_protocol_diagnosis.exe...
link.exe /nologo /OUT:build\Debug\test\test_protocol_diagnosis.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    build\Debug\test\test_protocol_diagnosis.obj ^
    build\Debug\obj\ReliableChannel.obj ^
    build\Debug\obj\FrameCodec.obj ^
    build\Debug\obj\LoopbackTransport.obj ^
    build\Debug\obj\TransportFactory.obj ^
    build\Debug\obj\pch.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [SUCCESS] Protocol Diagnosis Test Tool built successfully
echo [INFO] Executable: build\Debug\test\test_protocol_diagnosis.exe
exit /b 0