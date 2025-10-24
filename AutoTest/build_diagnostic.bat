@echo off
REM 编译文件传输诊断工具

echo ========================================
echo Building File Transfer Diagnostic Tool
echo ========================================

REM 设置Visual Studio环境
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
    echo [ERROR] Visual Studio 2022 not found
    exit /b 1
)

call %VCVARS% >nul

REM 创建输出目录
set BUILD_DIR=obj_diagnostic
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 编译所有源文件
echo [STEP 1/2] Compiling source files...

cl.exe /nologo /W3 /EHsc /MDd /Zi /Od /std:c++17 ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" /D "_AFXDLL" ^
    /I ".." /I "..\include" /I "..\Common" /I "..\Protocol" /I "..\Transport" ^
    /Fo"%BUILD_DIR%\\" ^
    /c DiagnoseFileTransfer.cpp ^
       ..\Transport\LoopbackTransport.cpp ^
       ..\Protocol\ReliableChannel.cpp ^
       ..\Protocol\FrameCodec.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [OK] Compilation succeeded

REM 链接可执行文件
echo [STEP 2/2] Linking DiagnoseFileTransfer.exe...

link.exe /nologo /OUT:DiagnoseFileTransfer.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    /MACHINE:X86 ^
    %BUILD_DIR%\DiagnoseFileTransfer.obj ^
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
echo Build Complete: DiagnoseFileTransfer.exe
echo ========================================
echo.

REM 运行诊断工具
echo ========================================
echo Running Diagnostic Tests...
echo ========================================
echo.

DiagnoseFileTransfer.exe

set TEST_RESULT=%ERRORLEVEL%

echo.
echo ========================================
if %TEST_RESULT% == 0 (
    echo [PASS] All diagnostic tests passed
) else (
    echo [FAIL] Diagnostic tests found issues
)
echo ========================================

exit /b %TEST_RESULT%
