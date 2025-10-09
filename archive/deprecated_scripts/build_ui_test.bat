@echo off
echo ========================================
echo 编译UI测试控制台程序
echo ========================================

REM 设置编译环境
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist "%VCVARS%" (
    set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
)
if not exist "%VCVARS%" (
    echo 错误: 未找到Visual Studio环境
    pause
    exit /b 1
)

call "%VCVARS%"

REM 设置项目目录
set PROJECT_DIR=%~dp0
set SOURCE_DIR=%PROJECT_DIR%AutoTest
set BUILD_DIR=%PROJECT_DIR%build_ui_test

REM 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 编译UI测试控制台程序
echo 正在编译UI测试控制台程序...
cd /d "%BUILD_DIR%"

cl /EHsc /MD /std:c++17 ^
   /I"%PROJECT_DIR%" ^
   /I"%PROJECT_DIR%include" ^
   /I"%PROJECT_DIR%Protocol" ^
   /I"%PROJECT_DIR%Transport" ^
   /I"%PROJECT_DIR%Common" ^
   "%SOURCE_DIR%\UI_Test_Console.cpp" ^
   "%PROJECT_DIR%Protocol\ReliableChannel.cpp" ^
   "%PROJECT_DIR%Protocol\FrameCodec.cpp" ^
   "%PROJECT_DIR%Transport\LoopbackTransport.cpp" ^
   "%PROJECT_DIR%Transport\ITransport.cpp" ^
   "%PROJECT_DIR%Transport\TransportFactory.cpp" ^
   "%PROJECT_DIR%Common\ConfigStore.cpp" ^
   FeUser32.lib /Fe:UI_Test_Console.exe

if %ERRORLEVEL% EQU 0 (
    echo ✅ UI测试控制台程序编译成功
    echo 输出文件: %BUILD_DIR%\UI_Test_Console.exe
) else (
    echo ❌ UI测试控制台程序编译失败
    pause
    exit /b 1
)

echo ========================================
echo 编译完成
echo ========================================
pause