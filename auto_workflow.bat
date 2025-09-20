@echo off
setlocal enabledelayedexpansion

rem ===== PortMaster 自动化工作流脚本 =====
rem 用途：实现完整的开发工作流，包括环境检测、编译验证、自动推送
rem 基于CLAUDE.md中的工作流规范
rem 作者：PortMaster开发团队
rem 版本：v1.0

echo ===============================================
echo = PortMaster 自动化工作流
echo ===============================================

rem 设置项目根目录
set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"

echo [INFO] 项目根目录: %PROJECT_ROOT%
echo [INFO] 开始执行自动化工作流...
echo.

rem 检测当前环境
echo [STEP 1] 环境检测与初始化...
call :detect_environment
if errorlevel 1 goto :ERROR

rem 同步远程仓库
echo [STEP 2] 同步远程仓库...
call :sync_remote
if errorlevel 1 goto :ERROR

rem 检查代码变更
echo [STEP 3] 检查代码变更...
call :check_changes
if errorlevel 1 goto :ERROR

rem 编译验证（如果需要）
echo [STEP 4] 编译验证...
call :build_verify
if errorlevel 1 goto :ERROR

rem 自动推送
echo [STEP 5] 自动推送...
call :auto_push
if errorlevel 1 goto :ERROR

echo.
echo [SUCCESS] ✅ 自动化工作流执行完成！
echo ===============================================
goto :END

rem ================= 子函数定义 =================

:detect_environment
echo [INFO] 检测运行环境...

rem 检测是否为WSL环境
wsl.exe -l >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] 检测到WSL环境可用
    set "ENV_TYPE=WSL"
) else (
    echo [INFO] 检测到PowerShell环境
    set "ENV_TYPE=POWERSHELL"
)

rem 设置工作目录
set "WORK_DIR=%PROJECT_ROOT%"
echo [INFO] 工作目录: %WORK_DIR%

rem 检查Git配置
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git未安装或不在PATH中
    exit /b 1
)

echo [INFO] Git版本:
git --version

rem 检查远程仓库
git remote -v
git remote | findstr /C:"backup" >nul
if %errorlevel% neq 0 (
    echo [WARNING] 未找到backup远程仓库
) else (
    echo [INFO] backup远程仓库配置正常
)

exit /b 0

:sync_remote
echo [INFO] 同步远程仓库变更...

rem 获取当前分支
for /f "tokens=*" %%i in ('git rev-parse --abbrev-ref HEAD') do set "CURRENT_BRANCH=%%i"
echo [INFO] 当前分支: %CURRENT_BRANCH%

rem 尝试拉取远程变更
git pull --rebase
if %errorlevel% neq 0 (
    echo [WARNING] 同步远程仓库失败，继续执行本地工作流
) else (
    echo [INFO] 远程仓库同步完成
)

exit /b 0

:check_changes
echo [INFO] 检查代码变更...

rem 检查是否有未提交的变更
git status --porcelain > temp_changes.txt
set "HAS_CHANGES=0"
for /f "tokens=*" %%i in (temp_changes.txt) do set "HAS_CHANGES=1"

del temp_changes.txt >nul 2>&1

if %HAS_CHANGES% equ 0 (
    echo [INFO] 无代码变更，工作流结束
    exit /b 2
) else (
    echo [INFO] 检测到代码变更，继续执行工作流
    git status --short
)

exit /b 0

:build_verify
echo [INFO] 检查是否需要编译验证...

rem 检查变更的文件类型
set "NEED_BUILD=0"
git diff --name-only HEAD > temp_files.txt

for /f "tokens=*" %%i in (temp_files.txt) do (
    set "FILE=%%i"
    echo !FILE! | findstr /E:"\.cpp" >nul && set "NEED_BUILD=1"
    echo !FILE! | findstr /E:"\.h" >nul && set "NEED_BUILD=1"
    echo !FILE! | findstr /E:"\.rc" >nul && set "NEED_BUILD=1"
    echo !FILE! | findstr /E:"\.vcxproj" >nul && set "NEED_BUILD=1"
)

del temp_files.txt >nul 2>&1

if %NEED_BUILD% equ 0 (
    echo [INFO] 无源码文件变更，跳过编译验证
    exit /b 0
)

echo [INFO] 检测到源码文件变更，执行编译验证...

rem 检查编译脚本是否存在
if exist "autobuild_x86_debug.bat" (
    echo [INFO] 使用 autobuild_x86_debug.bat 进行编译
    call autobuild_x86_debug.bat
) else if exist "autobuild.bat" (
    echo [INFO] 使用 autobuild.bat 进行编译
    call autobuild.bat
) else (
    echo [ERROR] 未找到编译脚本
    exit /b 1
)

if %errorlevel% neq 0 (
    echo [ERROR] 编译失败，请检查编译日志
    exit /b 1
)

rem 使用build_check.bat验证编译结果
if exist "build_check.bat" (
    echo [INFO] 使用 build_check.bat 验证编译结果
    call build_check.bat
    if %errorlevel% neq 0 (
        echo [ERROR] 编译验证未通过
        exit /b 1
    )
) else (
    echo [WARNING] 未找到 build_check.bat，手动检查编译日志
)

echo [INFO] 编译验证通过
exit /b 0

:auto_push
echo [INFO] 执行自动推送...

rem 生成提交信息
echo [INFO] 生成提交信息...
call :generate_commit_message

rem 添加所有变更
echo [INFO] 添加变更文件...
git add -A

rem 提交变更
echo [INFO] 提交变更...
git commit -m "%COMMIT_MESSAGE%"
if %errorlevel% neq 0 (
    echo [ERROR] 提交失败
    exit /b 1
)

rem 获取提交ID
for /f "tokens=*" %%i in ('git rev-parse --short HEAD') do set "COMMIT_ID=%%i"
echo [INFO] 提交完成，Commit ID: %COMMIT_ID%

rem 推送到远程仓库
echo [INFO] 推送到远程仓库...

rem 检查backup远程仓库
git remote | findstr /C:"backup" >nul
if %errorlevel% equ 0 (
    echo [INFO] 推送到 backup 远程仓库...
    git push backup HEAD
    if %errorlevel% neq 0 (
        echo [WARNING] 推送到 backup 失败
    )
)

rem 检查origin远程仓库
git remote | findstr /C:"origin" >nul
if %errorlevel% equ 0 (
    echo [INFO] 推送到 origin 远程仓库...
    git push origin HEAD
    if %errorlevel% neq 0 (
        echo [WARNING] 推送到 origin 失败
    )
)

rem 生成时间戳标签
echo [INFO] 生成存档标签...
for /f "tokens=*" %%i in ('powershell -Command "Get-Date -Format 'yyyyMMdd-HHmmss'"') do set "TIMESTAMP=%%i"
set "TAG_NAME=save-%TIMESTAMP%"

echo [INFO] 创建标签: %TAG_NAME%
git tag %TAG_NAME%

rem 推送标签
echo [INFO] 推送标签...
git push --tags

echo [INFO] 自动推送完成
exit /b 0

:generate_commit_message
echo [INFO] 分析文件变更生成提交信息...

rem 获取变更的文件列表
git diff --cached --name-only > temp_files.txt

rem 分析文件类型和变更内容
set "CODE_CHANGES=0"
set "DOC_CHANGES=0"
set "RESOURCE_CHANGES=0"

for /f "tokens=*" %%i in (temp_files.txt) do (
    set "FILE=%%i"
    echo !FILE! | findstr /E:"\.cpp" >nul && set "CODE_CHANGES=1"
    echo !FILE! | findstr /E:"\.h" >nul && set "CODE_CHANGES=1"
    echo !FILE! | findstr /E:"\.md" >nul && set "DOC_CHANGES=1"
    echo !FILE! | findstr /E:"\.txt" >nul && set "DOC_CHANGES=1"
    echo !FILE! | findstr /E:"\.rc" >nul && set "RESOURCE_CHANGES=1"
)

del temp_files.txt >nul 2>&1

rem 根据变更类型生成提交信息
if %CODE_CHANGES% equ 1 (
    if %DOC_CHANGES% equ 1 (
        set "COMMIT_MESSAGE=fix: 修复代码问题并更新相关文档"
    ) else (
        set "COMMIT_MESSAGE=fix: 修复代码问题并优化功能"
    )
) else if %DOC_CHANGES% equ 1 (
    set "COMMIT_MESSAGE=docs: 更新项目文档和说明"
) else if %RESOURCE_CHANGES% equ 1 (
    set "COMMIT_MESSAGE=chore: 更新资源文件和配置"
) else (
    set "COMMIT_MESSAGE=chore: 更新项目文件"
)

echo [INFO] 提交信息: %COMMIT_MESSAGE%
exit /b 0

:ERROR
echo.
echo [ERROR] ❌ 工作流执行失败！
echo ===============================================
exit /b 1

:END
echo.
echo [INFO] 工作流执行结束
echo ===============================================
endlocal