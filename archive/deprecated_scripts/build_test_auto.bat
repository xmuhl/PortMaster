@echo off
REM 编译自动化测试工具（不使用预编译头，独立编译）

echo ========================================
echo Building Reliable Transmission Auto Test
echo ========================================

REM 检查主项目是否已编译
if not exist "build\Debug\obj\ReliableChannel.obj" (
    echo [ERROR] Main project not compiled yet
    echo Please run autobuild_x86_debug.bat first
    exit /b 1
)

REM 设置Visual Studio环境
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
    echo [ERROR] Visual Studio 2022 not found
    exit /b 1
)

call %VCVARS% >nul

REM 创建临时目录
if not exist "build\Debug\test_obj" mkdir "build\Debug\test_obj"

REM 编译测试程序主文件（不使用预编译头）
echo [STEP 1] Compiling test_reliable_auto.cpp (without PCH)...

cl.exe /nologo /W3 /EHsc /MTd /Zi /Od ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
    /D "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING" ^
    /I "." /I "include" /I "Common" /I "Protocol" /I "Transport" ^
    /Fo"build\Debug\test_obj\\" ^
    /c test_reliable_auto.cpp include\pch.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [OK] Compilation succeeded

REM 链接生成可执行文件
echo [STEP 2] Linking test_reliable_auto.exe...

link.exe /nologo /OUT:build\Debug\test_reliable_auto.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    build\Debug\test_obj\test_reliable_auto.obj ^
    build\Debug\test_obj\pch.obj ^
    build\Debug\obj\LoopbackTransport.obj ^
    build\Debug\obj\ReliableChannel.obj ^
    build\Debug\obj\FrameCodec.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [OK] Linking succeeded
echo.
echo ========================================
echo Build Complete: build\Debug\test_reliable_auto.exe
echo ========================================

exit /b 0