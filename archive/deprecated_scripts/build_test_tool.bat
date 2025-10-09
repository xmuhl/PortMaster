@echo off
REM 编译独立测试工具

echo ========================================
echo 编译可靠传输测试工具
echo ========================================

set TEST_SRC=test_reliable_loopback.cpp
set TEST_EXE=build\Debug\test_reliable.exe

REM 设置编译器路径
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist %VCVARS% (
    echo 错误: 找不到Visual Studio 2022
    exit /b 1
)

REM 初始化编译环境
call %VCVARS% >nul

REM 编译测试工具（链接主程序的所有对象文件）
echo 开始编译测试工具...

cl.exe /nologo /W3 /EHsc /MTd /Zi /Od ^
    /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" ^
    /I "." /I "Common" /I "Protocol" /I "Transport" ^
    %TEST_SRC% ^
    build\Debug\obj\*.obj ^
    /link /OUT:%TEST_EXE% ^
    /SUBSYSTEM:CONSOLE ^
    kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib ^
    advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ^
    odbc32.lib odbccp32.lib

if %ERRORLEVEL% NEQ 0 (
    echo 编译失败！
    exit /b 1
)

echo 编译成功: %TEST_EXE%
echo ========================================