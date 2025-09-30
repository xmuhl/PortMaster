@echo off
REM 自动化测试流程：编译测试工具 -> 运行测试 -> 测试成功后重新编译主项目

echo ========================================
echo Automated Test Workflow
echo ========================================
echo.

REM 步骤1: 编译测试工具
echo [WORKFLOW STEP 1] Building test tool...
echo ----------------------------------------
call build_test_auto.bat
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [WORKFLOW FAILED] Test tool build failed
    exit /b 1
)
echo.

REM 步骤2: 运行测试工具
echo [WORKFLOW STEP 2] Running automated test...
echo ----------------------------------------
build\Debug\test_reliable_auto.exe
set TEST_RESULT=%ERRORLEVEL%

echo.
if %TEST_RESULT% NEQ 0 (
    echo ========================================
    echo [WORKFLOW FAILED] Test failed
    echo ========================================
    echo.
    echo Test did not pass. Main project will NOT be recompiled.
    echo Please check the test output above for errors.
    exit /b 1
)

echo ========================================
echo [WORKFLOW SUCCESS] Test passed
echo ========================================
echo.

REM 步骤3: 测试成功，重新编译主项目
echo [WORKFLOW STEP 3] Recompiling main project...
echo ----------------------------------------
call autobuild_x86_debug.bat
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [WARNING] Main project recompile failed
    exit /b 1
)

echo.
echo ========================================
echo Workflow Complete
echo ========================================
echo 1. Test tool built successfully
echo 2. Automated test PASSED
echo 3. Main project recompiled successfully
echo.
echo All steps completed successfully!
echo ========================================

exit /b 0