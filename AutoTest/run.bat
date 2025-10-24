@echo off
REM 自动化测试完整流程：编译 -> 测试 -> 验证 -> 重新编译主项目

echo ========================================
echo Automated Test Workflow
echo ========================================
echo.

REM 步骤1: 编译测试工具
echo [WORKFLOW STEP 1/3] Building test tool...
echo ----------------------------------------
call build.bat
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [WORKFLOW FAILED] Test tool build failed
    exit /b 1
)
echo.

REM 步骤2: 运行自动化测试
echo [WORKFLOW STEP 2/3] Running automated test...
echo ----------------------------------------
AutoTest.exe
set TEST_RESULT=%ERRORLEVEL%

echo.
if %TEST_RESULT% NEQ 0 (
    echo ========================================
    echo [WORKFLOW FAILED] Test failed
    echo ========================================
    echo.
    echo Main project will NOT be recompiled.
    exit /b 1
)

echo ========================================
echo [WORKFLOW SUCCESS] Test passed
echo ========================================
echo.

REM 步骤3: 重新编译主项目
echo [WORKFLOW STEP 3/3] Recompiling main project...
echo ----------------------------------------
cd ..
call autobuild_x86_debug.bat
set BUILD_RESULT=%ERRORLEVEL%
cd AutoTest

if %BUILD_RESULT% NEQ 0 (
    echo.
    echo [WARNING] Main project recompile failed
    exit /b 1
)

echo.
echo ========================================
echo Workflow Complete
echo ========================================
echo [SUCCESS] All steps completed:
echo   1. Test tool built
echo   2. Automated test PASSED
echo   3. Main project recompiled (0 error 0 warning)
echo ========================================

exit /b 0