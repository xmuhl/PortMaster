@echo off
REM 编译简单回路测试工具

echo ========================================
echo Building Simple Loopback Test
echo ========================================

REM 设置Visual Studio环境
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
    echo [ERROR] Visual Studio 2022 not found
    exit /b 1
)

call %VCVARS% >nul

REM 创建输出目录
set BUILD_DIR=obj_simple
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 编译所有源文件
echo [STEP 1/2] Compiling source files...

cl.exe /nologo /W3 /EHsc /MDd /Zi /Od /std:c++17 ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" /D "_AFXDLL" ^
    /I ".." /I "..\include" /I "..\Common" /I "..\Protocol" /I "..\Transport" ^
    /Fo"%BUILD_DIR%\\" ^
    /c SimpleLoopbackTest.cpp ^
       ..\Transport\LoopbackTransport.cpp ^
       ..\Protocol\ReliableChannel.cpp ^
       ..\Protocol\FrameCodec.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [OK] Compilation succeeded

REM 链接可执行文件
echo [STEP 2/2] Linking SimpleLoopbackTest.exe...

link.exe /nologo /OUT:SimpleLoopbackTest.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    %BUILD_DIR%\SimpleLoopbackTest.obj ^
    %BUILD_DIR%\LoopbackTransport.obj ^
    %BUILD_DIR%\ReliableChannel.obj ^
    %BUILD_DIR%\FrameCodec.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [OK] Linking succeeded
echo.
echo ========================================
echo Build Complete: SimpleLoopbackTest.exe
echo ========================================
echo.

REM 运行测试
echo ========================================
echo Running Simple Loopback Test...
echo ========================================
echo.

SimpleLoopbackTest.exe

set TEST_RESULT=%ERRORLEVEL%

echo.
echo ========================================
if %TEST_RESULT% == 0 (
    echo [PASS] Test passed
) else (
    echo [FAIL] Test failed
)
echo ========================================

exit /b %TEST_RESULT%
