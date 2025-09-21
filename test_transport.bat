@echo off
echo 正在测试PortMaster程序...
cd /d "%~dp0"
cd build\Debug

REM 运行程序并显示帮助信息
echo.
echo 运行PortMaster.exe...
PortMaster.exe --help 2>&1
echo.
echo 程序退出码: %ERRORLEVEL%
echo.
echo 测试完成！
pause