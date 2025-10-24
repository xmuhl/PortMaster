@echo off
REM 编译自动化测试工具（不使用预编译头）

echo ========================================
echo Building Automated Test Tool
echo ========================================

REM 检查主项目是否已编译
if not exist "..\build\Debug\obj\ReliableChannel.obj" (
    echo [ERROR] Main project not compiled
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

REM 创建输出目录
if not exist "obj" mkdir "obj"

REM 编译所有源文件（不使用预编译头，不包含MFC）
echo [STEP 1/2] Compiling source files...

cl.exe /nologo /W3 /EHsc /MDd /Zi /Od /std:c++17 ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" /D "_AFXDLL" ^
    /I ".." /I "..\include" /I "..\Common" /I "..\Protocol" /I "..\Transport" ^
    /Fo"obj\\" ^
    /c main.cpp ^
       ..\Transport\LoopbackTransport.cpp ^
       ..\Transport\SerialTransport.cpp ^
       ..\Protocol\ReliableChannel.cpp ^
       ..\Protocol\FrameCodec.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [OK] Compilation succeeded

REM 链接可执行文件
echo [STEP 2/2] Linking AutoTest.exe...

link.exe /nologo /OUT:AutoTest.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    obj\main.obj ^
    obj\LoopbackTransport.obj ^
    obj\SerialTransport.obj ^
    obj\ReliableChannel.obj ^
    obj\FrameCodec.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [OK] Linking succeeded
echo.
echo ========================================
echo Build Complete: AutoTest.exe
echo ========================================

exit /b 0