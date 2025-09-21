@echo off
echo 正在运行简化测试...
cd /d "%~dp0"
cd build\Debug

REM 使用调试器运行程序
echo 启动调试器...
devenv /debugexe PortMaster.exe
echo.
echo 测试完成！
pause