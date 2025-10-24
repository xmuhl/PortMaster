@echo off
REM 编译可靠传输独立测试工具（链接已编译的对象文件）

echo ========================================
echo 编译可靠传输测试工具
echo ========================================

REM 设置Visual Studio环境
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
    echo 错误: 找不到Visual Studio 2022
    exit /b 1
)

call %VCVARS% >nul

REM 编译测试主程序
echo 编译test_main.cpp...

cl.exe /nologo /W3 /EHsc /MTd /Zi /Od ^
    /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
    /I ".." /I "..\include" /I "..\Common" /I "..\Protocol" /I "..\Transport" ^
    /Fp"..\build\Debug\obj\PortMaster.pch" /Yu"pch.h" ^
    /c test_main.cpp

if %ERRORLEVEL% NEQ 0 (
    echo 编译失败！请先编译主项目
    exit /b 1
)

REM 链接（使用主项目已编译的对象文件）
echo 链接可执行文件...

link.exe /nologo /OUT:test_reliable.exe ^
    /SUBSYSTEM:CONSOLE /DEBUG ^
    test_main.obj ^
    ..\build\Debug\obj\LoopbackTransport.obj ^
    ..\build\Debug\obj\ReliableChannel.obj ^
    ..\build\Debug\obj\FrameCodec.obj ^
    ..\build\Debug\obj\pch.obj ^
    kernel32.lib user32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo 链接失败！
    exit /b 1
)

echo.
echo ========================================
echo 编译成功: test_reliable.exe
echo ========================================
echo.

REM 清理对象文件
del test_main.obj

exit /b 0