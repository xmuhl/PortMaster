@echo off
setlocal enabledelayedexpansion

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
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git未安装或不在PATH中
    exit /b 1
)
echo [INFO] Git版本:
git --version

rem 同步远程仓库
echo [STEP 2] 同步远程仓库...
git pull --rebase
if %errorlevel% neq 0 (
    echo [WARNING] 同步远程仓库失败，继续执行本地工作流
)

rem 检查代码变更
echo [STEP 3] 检查代码变更...
git status --porcelain > temp_changes.txt
set "HAS_CHANGES=0"
for /f "tokens=*" %%i in (temp_changes.txt) do set "HAS_CHANGES=1"
del temp_changes.txt >nul 2>&1

if %HAS_CHANGES% equ 0 (
    echo [INFO] 无代码变更，工作流结束
    goto :END
) else (
    echo [INFO] 检测到代码变更，继续执行工作流
    git status --short
)

rem 检查是否需要编译
echo [STEP 4] 检查是否需要编译验证...
set "NEED_BUILD=0"
git diff --name-only HEAD > temp_files.txt

for /f "tokens=*" %%i in (temp_files.txt) do (
    echo %%i | findstr /E:".cpp" >nul && set "NEED_BUILD=1"
    echo %%i | findstr /E:".h" >nul && set "NEED_BUILD=1"
    echo %%i | findstr /E:".rc" >nul && set "NEED_BUILD=1"
    echo %%i | findstr /E:".vcxproj" >nul && set "NEED_BUILD=1"
)

del temp_files.txt >nul 2>&1

if %NEED_BUILD% equ 1 (
    echo [INFO] 检测到源码文件变更，执行编译验证...
    if exist "autobuild_x86_debug.bat" (
        echo [INFO] 使用 autobuild_x86_debug.bat 进行编译
        call autobuild_x86_debug.bat
        if %errorlevel% neq 0 (
            echo [ERROR] 编译失败
            exit /b 1
        )
    ) else (
        echo [WARNING] 未找到编译脚本，跳过编译验证
    )
) else (
    echo [INFO] 无源码文件变更，跳过编译验证
)

rem 自动推送
echo [STEP 5] 执行自动推送...

rem 生成提交信息
echo [INFO] 生成提交信息...
set "COMMIT_MESSAGE=chore: 更新项目文件"

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
git remote | findstr "backup" >nul
if %errorlevel% equ 0 (
    echo [INFO] 推送到 backup 远程仓库...
    git push backup HEAD
    if %errorlevel% equ 0 (
        echo [INFO] 成功推送到 backup
    ) else (
        echo [WARNING] 推送到 backup 失败
    )
)

rem 检查origin远程仓库
git remote | findstr "origin" >nul
if %errorlevel% equ 0 (
    echo [INFO] 推送到 origin 远程仓库...
    git push origin HEAD
    if %errorlevel% equ 0 (
        echo [INFO] 成功推送到 origin
    ) else (
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

echo.
echo [SUCCESS] 自动化工作流执行完成！
echo ===============================================

:END
endlocal