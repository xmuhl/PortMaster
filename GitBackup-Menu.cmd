@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
chcp 936 >nul

REM ============================================================
REM  GitBackup-Menu.cmd  增强版 v2，重写版本 + 直接 wsl -e git。
REM  功能：
REM    * 方案A：统一 Windows/WSL 的 insteadof 重写（localbackup:）
REM    * 统一项目 backup 远程为 localbackup:<RepoName>.git
REM    * 调用 GitLocalBackupToolkit.ps1 执行 增~强 功能
REM    * 配置记忆：.last.config
REM ============================================================

REM ---------- 配置：统一映射前缀（重要！版本化需要的）----------
set "WIN_URL_KEY=url.file:///D:/GitBackups/.insteadof"
set "WSL_URL_KEY=url./mnt/d/GitBackups/.insteadof"

REM ---------- 工具脚本路径 ----------
set "HERE=%~dp0"
set "TOOLKIT=%HERE%GitLocalBackupToolkit.ps1"
if not exist "%TOOLKIT%" (
  echo [错误] 未找到工具脚本：%TOOLKIT%
  echo 请将 GitLocalBackupToolkit.ps1 放到与本脚本同一目录下。
  goto :PAUSE_EXIT
)

REM ---------- 检测依赖 ----------
where git >nul 2>nul
if errorlevel 1 (
  echo [错误] 未找到 git，请先安装 Git for Windows 并加入到 PATH。
  goto :PAUSE_EXIT
)
where powershell >nul 2>nul
if errorlevel 1 (
  echo [错误] 未找到 powershell（Windows PowerShell），检查系统组件。
  goto :PAUSE_EXIT
)
REM ---------- 统一环境检测 ----------
set "WSL_AVAILABLE=0"
set "WSL_HAS_DISTRO=0"
set "WSL_HAS_GIT=0"

where wsl >nul 2>nul
if not errorlevel 1 (
  set "WSL_AVAILABLE=1"
  REM 检测是否有已安装的WSL发行版
  for /f "delims=" %%D in ('wsl -l -q 2^>nul') do (
    set "WSL_HAS_DISTRO=1"
    goto :WSL_DISTRO_FOUND
  )
  :WSL_DISTRO_FOUND
  REM 检测WSL中是否安装了Git
  if "%WSL_HAS_DISTRO%"=="1" (
    wsl -e git --version >nul 2>&1
    if not errorlevel 1 set "WSL_HAS_GIT=1"
  )
)

REM ---------- 参数加载/询问参数 ----------
set "CONF=%HERE%.last.config"
call :LOAD_CONFIG
if "%PROJECT_DIR%"==""  call :ASK_PARAMS_SAVE
if "%REPO_NAME%"==""    call :ASK_PARAMS_SAVE
if "%BACKUP_ROOT_WIN%"=="" call :ASK_PARAMS_SAVE

REM ---------- 确保备份根目录存在 ----------
if not exist "%BACKUP_ROOT_WIN%" (
  echo [信息] 正在创建备份根目录：%BACKUP_ROOT_WIN%
  mkdir "%BACKUP_ROOT_WIN%" 2>nul
)

:MENU
cls
echo ============================================================
echo   Git 本地备份：方案A）一键菜单  - 跨环境增强设置（增强版 v2）
echo ------------------------------------------------------------
echo   项目目录：%PROJECT_DIR%
echo   仓库名称：%REPO_NAME%
echo   逻辑 backup 远程：localbackup:%REPO_NAME%.git
echo   备份根目录（Windows）：%BACKUP_ROOT_WIN%
echo   备份根目录（WSL）    ：/mnt/d/GitBackups/
echo   WSL 可用：%WSL_AVAILABLE% / 有发行版：%WSL_HAS_DISTRO% / Git安装：%WSL_HAS_GIT%
echo   配置文件：%CONF%
echo ============================================================
echo  1) 一键设置）方案A）映射（Windows+WSL）统一 backup 远程
echo  2) 初始化：本地裸仓库并首次提交/推送
echo  3) 安装提交后自动推送钩子
echo  4) 运行一个 CLI 命令（成功后自动备份）
echo  5) 创建存档标签（savepoint）并推送
echo  6) 克隆验证：从备份仓库克隆一个验证
echo  7) 恢复到提交号/标签：创建新分支式恢复
echo ------------------------------------------------------------
echo  8) 映射、远程检查（显示 remotes）
echo  9) 修改/重设）方案A）映射（Windows+WSL）
echo 10) 修改项目参数（项目目录/仓库名/备份根目录）并保存
echo 11) 清空记忆设置（删除 .last.config）
echo 12) 环境诊断（检查 Git、WSL、路径配置等）
echo  0) 退出
echo ============================================================
set /p choice=请选择操作项并按回车：

if "%choice%"=="1" goto :CONF_SCHEME_A
if "%choice%"=="2" goto :DO_INIT
if "%choice%"=="3" goto :DO_HOOK
if "%choice%"=="4" goto :DO_RUN_TASK
if "%choice%"=="5" goto :DO_SAVEPOINT
if "%choice%"=="6" goto :DO_CLONE_TEST
if "%choice%"=="7" goto :DO_RESTORE
if "%choice%"=="8" goto :CHECK_STATUS
if "%choice%"=="9" goto :RESET_SCHEME_A
if "%choice%"=="10" goto :ASK_PARAMS_SAVE_FROM_MENU
if "%choice%"=="11" goto :CLEAR_MEMORY
if "%choice%"=="12" goto :DO_DIAGNOSE
if "%choice%"=="0" goto :EOF

echo [提示] 无效的选项
pause
goto :MENU

REM ================================
REM  参数：读取/保存/询问
REM ================================
:LOAD_CONFIG
if not exist "%CONF%" exit /b 0
for /f "usebackq tokens=1,* delims==" %%A in ("%CONF%") do (
  set "K=%%~A"
  set "V=%%~B"
  if /I "!K!"=="PROJECT_DIR" set "PROJECT_DIR=!V!"
  if /I "!K!"=="REPO_NAME" set "REPO_NAME=!V!"
  if /I "!K!"=="BACKUP_ROOT_WIN" set "BACKUP_ROOT_WIN=!V!"
)
exit /b 0

:SAVE_CONFIG
(
  echo PROJECT_DIR=%PROJECT_DIR%
  echo REPO_NAME=%REPO_NAME%
  echo BACKUP_ROOT_WIN=%BACKUP_ROOT_WIN%
) > "%CONF%"
exit /b 0

:ASK_PARAMS
echo.
set "DEFAULT_PROJECT_DIR=C:\Users\huangl\Desktop\PortMaster"
set "DEFAULT_BACKUP_ROOT=D:\GitBackups"

REM --- 输入项目目录 ---
if "%PROJECT_DIR%"=="" (set "PROMPT_PD=请输入项目目录（默认 %DEFAULT_PROJECT_DIR%）： ") else (set "PROMPT_PD=请输入项目目录（默认 %PROJECT_DIR%）： ")
set "TMP_PD="
set /p TMP_PD=%PROMPT_PD%
if "%PROJECT_DIR%"=="" (
  if "%TMP_PD%"=="" (set "PROJECT_DIR=%DEFAULT_PROJECT_DIR%") else (set "PROJECT_DIR=%TMP_PD%")
) else (
  if not "%TMP_PD%"=="" set "PROJECT_DIR=%TMP_PD%"
)

REM --- 推导仓库名：从项目目录名称 ---
for %%I in ("%PROJECT_DIR%") do set "DERIVED_REPO=%%~nI"

REM --- 输入仓库名：默认使用上次或推导的值 ---
if "%REPO_NAME%"=="" (set "DEFAULT_REPO=%DERIVED_REPO%") else (set "DEFAULT_REPO=%REPO_NAME%")
set "TMP_RN="
set /p TMP_RN=请输入仓库名称（默认 %DEFAULT_REPO%）：
if "%TMP_RN%"=="" (set "REPO_NAME=%DEFAULT_REPO%") else (set "REPO_NAME=%TMP_RN%")

REM --- 仓库名去空格（保险） ---
set "REPO_NAME=%REPO_NAME: =_%"

REM --- 输入备份根目录 ---
if "%BACKUP_ROOT_WIN%"=="" (set "PROMPT_BR=请输入备份根目录（默认 %DEFAULT_BACKUP_ROOT%）： ") else (set "PROMPT_BR=请输入备份根目录（默认 %BACKUP_ROOT_WIN%）： ")
set "TMP_BR="
set /p TMP_BR=%PROMPT_BR%
if "%BACKUP_ROOT_WIN%"=="" (
  if "%TMP_BR%"=="" (set "BACKUP_ROOT_WIN=%DEFAULT_BACKUP_ROOT%") else (set "BACKUP_ROOT_WIN=%TMP_BR%")
) else (
  if not "%TMP_BR%"=="" set "BACKUP_ROOT_WIN=%TMP_BR%"
)

echo.
echo [确认参数]
echo   项目目录：%PROJECT_DIR%
echo   仓库名称：%REPO_NAME%
echo   备份根目录：%BACKUP_ROOT_WIN%
echo.
exit /b 0

:ASK_PARAMS_SAVE
call :ASK_PARAMS
call :SAVE_CONFIG
exit /b 0

:ASK_PARAMS_SAVE_FROM_MENU
call :ASK_PARAMS
call :SAVE_CONFIG
echo [完成] 参数已保存到：%CONF%
pause
goto :MENU

:CLEAR_MEMORY
if exist "%CONF%" del /f /q "%CONF%"
set "PROJECT_DIR="
set "REPO_NAME="
set "BACKUP_ROOT_WIN="
echo [完成] 已清空记忆设置：%CONF%
echo [提示] 返回菜单将重新询问参数。
pause
goto :MENU

REM =================
REM  1) 一键设置（方案A）统一 backup 远程
REM =================
:CONF_SCHEME_A
set "LOGICAL_BACKUP_URL=localbackup:%REPO_NAME%.git"
echo.
echo [配置] 设置 Windows 全局映射：localbackup: => file:///D:/GitBackups/
git config --global %WIN_URL_KEY% localbackup:
if errorlevel 1 (
  echo [错误] 设置 Windows 映射失败：检查权限或 Git 安装。
) else (
  echo [完成] Windows insteadof 已设置：%WIN_URL_KEY% = localbackup:
)

if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="0" (
    echo [提示] 未发现已安装的 WSL 发行版，跳过 WSL 映射设置。
  ) elseif "%WSL_HAS_GIT%"=="0" (
    echo [警告] WSL 中未检测到 git，请在 WSL 中执行：sudo apt-get install -y git
    echo         跳过 WSL 映射设置。
  ) else (
      echo [配置] 设置 WSL 全局映射：localbackup: => /mnt/d/GitBackups/
      wsl -e git config --global %WSL_URL_KEY% localbackup:
      if errorlevel 1 (
        echo [错误] 设置 WSL 映射失败，请到 WSL 中手动执行：
        echo         git config --global %WSL_URL_KEY% localbackup:
      ) else (
        echo [完成] WSL insteadof 已设置：%WSL_URL_KEY% = localbackup:
      )
    )
  )
) else (
  echo [提示] 环境未检测到 wsl 命令，跳过 WSL 映射设置。
)

echo [配置] 统一项目的 backup 远程为逻辑地址：%LOGICAL_BACKUP_URL%
pushd "%PROJECT_DIR%" >nul 2>nul
if errorlevel 1 (
  echo [错误] 无法进入项目目录：%PROJECT_DIR%
  goto :AFTER_ACTION
)
git rev-parse --git-dir >nul 2>nul
if errorlevel 1 (
  echo [提示] 当前目录尚未初始化为 Git 仓库，正在执行 git init ...
  git init || (echo [错误] git init 失败 & popd >nul & goto :AFTER_ACTION)
)

git remote get-url backup >nul 2>nul
if not errorlevel 1 git remote remove backup
git remote add backup "%LOGICAL_BACKUP_URL%"
if errorlevel 1 (
  echo [错误] 设置 backup 远程失败，请确认仓库状态。
  popd >nul & goto :AFTER_ACTION
)
echo [完成] 方案A 映射和 backup 远程已经配置
popd >nul
goto :AFTER_ACTION

REM =================
REM  2) 初始化
REM =================
:DO_INIT
echo.
echo [执行] 初始化：本地裸仓库 + 首次提交/推送：
REM powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action init -ProjectDir "%PROJECT_DIR%" -RepoName "%REPO_NAME%"
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action init -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  3) 安装钩子
REM =================
:DO_HOOK
echo.
echo [执行] 安装提交后自动推送钩子
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action install-hook -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  4) 运行任务
REM =================
:DO_RUN_TASK
echo.
set "TASK_CMD=claude code -p .\spec.md"
set "TASK_MESSAGE=feat: 自动修订完成任务"
echo [执行] 运行一个 CLI 命令（成功后自动备份）
echo       命令：%TASK_CMD%
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action run -ProjectDir "%PROJECT_DIR%" -Command "%TASK_CMD%" -Message "%TASK_MESSAGE%"
goto :AFTER_ACTION

REM =================
REM  5) 存档标签
REM =================
:DO_SAVEPOINT
echo.
echo [执行] 创建存档标签（savepoint）并推送
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action savepoint -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  6) 克隆验证
REM =================
:DO_CLONE_TEST
echo.
set "DEF_CLONE_DEST=D:\TestRestore\%REPO_NAME%"
set /p CLONE_DEST=请输入克隆验证目标目录（回车默认 %DEF_CLONE_DEST%）：
if "%CLONE_DEST%"=="" set "CLONE_DEST=%DEF_CLONE_DEST%"
echo [执行] 克隆验证到：%CLONE_DEST%
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action clone-test -ProjectDir "%PROJECT_DIR%" -RepoName "%REPO_NAME%" -CloneDest "%CLONE_DEST%"
goto :AFTER_ACTION

REM =================
REM  7) 恢复历史
REM =================
:DO_RESTORE
echo.
set "RESTORE_REF="
set /p RESTORE_REF=请输入恢复到提交号或标签（如 save-YYYYMMDD-HHMMSS）：
if "%RESTORE_REF%"=="" ( echo [提示] 未输入目标，已取消。 & goto :AFTER_ACTION )
echo [执行] 恢复到 %RESTORE_REF% 创建新分支恢复
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action restore -ProjectDir "%PROJECT_DIR%" -Ref "%RESTORE_REF%" -NewBranch
goto :AFTER_ACTION

REM =================
REM  8) 检查状态
REM =================
:CHECK_STATUS
echo.
echo [信息] Windows 全局映射（包含 localbackup 应映射）
git config --global --get-regexp ^url\..*\.insteadof$ 2>nul | findstr /I localbackup
echo ------------------------------------------------------------
if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="1" (
    echo [信息] WSL 全局映射（以下是 url.*.insteadof=localbackup: 条目）
    wsl --cd ~ -e git config --global --get-regexp url\..*\.insteadof
  ) else (
    echo [提示] 未发现已安装的 WSL 发行版，无法 WSL 映射检查。
  )
  echo ------------------------------------------------------------
)
pushd "%PROJECT_DIR%" >nul 2>nul
if errorlevel 1 (
  echo [错误] 无法进入项目目录：%PROJECT_DIR%
  goto :AFTER_ACTION
)
echo [信息] 项目 remotes：
git remote -v
popd >nul
goto :AFTER_ACTION

REM =================
REM  9) 重设映射
REM =================
:RESET_SCHEME_A
echo.
echo [执行] 重新设置）方案A）映射（Windows+WSL）

REM Windows 映射配置：先清除旧配置，再设置新配置
git config --global --unset-all %WIN_URL_KEY% 2>nul
git config --global %WIN_URL_KEY% localbackup:
if errorlevel 1 (
  echo [错误] 设置 Windows 映射失败，请检查 Git 配置权限
  echo [诊断] 尝试手动执行：git config --global %WIN_URL_KEY% localbackup:
) else (
  echo [完成] Windows 映射重设完成：%WIN_URL_KEY% = localbackup:
)

REM WSL 配置重设
if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="1" (
    REM 先尝试清除旧配置（忽略错误，因为可能不存在）
    wsl --cd ~ -e git config --global --unset-all %WSL_URL_KEY% 2>nul
    REM 设置新配置并检查结果
    wsl --cd ~ -e git config --global %WSL_URL_KEY% localbackup:
    if errorlevel 1 (
      echo [错误] 设置 WSL 映射失败，请到 WSL 中手动执行：
      echo         git config --global --unset-all %WSL_URL_KEY%
      echo         git config --global %WSL_URL_KEY% localbackup:
      echo [诊断] 可能是 WSL 中 Git 配置权限问题或 Git 未正确安装
    ) else (
      echo [完成] WSL 映射重设完成：%WSL_URL_KEY% = localbackup:
    )
  ) else (
    echo [提示] 未发现已安装的 WSL 发行版，跳过 WSL 映射设置。
  )
)
goto :AFTER_ACTION

REM =================
REM  12) 环境诊断
REM =================
:DO_DIAGNOSE
echo.
echo [执行] 环境诊断检查
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action diagnose -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

:AFTER_ACTION
echo.
pause
goto :MENU

:PAUSE_EXIT
echo.
pause
exit /b 1