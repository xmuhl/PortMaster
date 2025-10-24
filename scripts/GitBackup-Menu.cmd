@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
chcp 936 >nul

REM ============================================================
REM  GitBackup-Menu.cmd  ��ǿ�� v2����д�汾 + ֱ�� wsl -e git��
REM  ���ܣ�
REM    * ����A��ͳһ Windows/WSL �� insteadof ��д��localbackup:��
REM    * ͳһ��Ŀ backup Զ��Ϊ localbackup:<RepoName>.git
REM    * ���� GitLocalBackupToolkit.ps1 ִ�� ��~ǿ ����
REM    * ���ü��䣺.last.config
REM ============================================================

REM ---------- ���ã�ͳһӳ��ǰ׺����Ҫ���汾����Ҫ�ģ�----------
set "WIN_URL_KEY=url.file:///D:/GitBackups/.insteadof"
set "WSL_URL_KEY=url./mnt/d/GitBackups/.insteadof"

REM ---------- ���߽ű�·�� ----------
set "HERE=%~dp0"
set "TOOLKIT=%HERE%GitLocalBackupToolkit.ps1"
if not exist "%TOOLKIT%" (
  echo [����] δ�ҵ����߽ű���%TOOLKIT%
  echo �뽫 GitLocalBackupToolkit.ps1 �ŵ��뱾�ű�ͬһĿ¼�¡�
  goto :PAUSE_EXIT
)

REM ---------- ������� ----------
where git >nul 2>nul
if errorlevel 1 (
  echo [����] δ�ҵ� git�����Ȱ�װ Git for Windows �����뵽 PATH��
  goto :PAUSE_EXIT
)
where powershell >nul 2>nul
if errorlevel 1 (
  echo [����] δ�ҵ� powershell��Windows PowerShell�������ϵͳ�����
  goto :PAUSE_EXIT
)
REM ---------- ͳһ������� ----------
set "WSL_AVAILABLE=0"
set "WSL_HAS_DISTRO=0"
set "WSL_HAS_GIT=0"

where wsl >nul 2>nul
if not errorlevel 1 (
  set "WSL_AVAILABLE=1"
  REM ����Ƿ����Ѱ�װ��WSL���а�
  for /f "delims=" %%D in ('wsl -l -q 2^>nul') do (
    set "WSL_HAS_DISTRO=1"
    goto :WSL_DISTRO_FOUND
  )
  :WSL_DISTRO_FOUND
  REM ���WSL���Ƿ�װ��Git
  if "%WSL_HAS_DISTRO%"=="1" (
    wsl -e git --version >nul 2>&1
    if not errorlevel 1 set "WSL_HAS_GIT=1"
  )
)

REM ---------- ��������/ѯ�ʲ��� ----------
set "CONF=%HERE%.last.config"
call :LOAD_CONFIG
if "%PROJECT_DIR%"==""  call :ASK_PARAMS_SAVE
if "%REPO_NAME%"==""    call :ASK_PARAMS_SAVE
if "%BACKUP_ROOT_WIN%"=="" call :ASK_PARAMS_SAVE

REM ---------- ȷ�����ݸ�Ŀ¼���� ----------
if not exist "%BACKUP_ROOT_WIN%" (
  echo [��Ϣ] ���ڴ������ݸ�Ŀ¼��%BACKUP_ROOT_WIN%
  mkdir "%BACKUP_ROOT_WIN%" 2>nul
)

:MENU
cls
echo ============================================================
echo   Git ���ر��ݣ�����A��һ���˵�  - �绷����ǿ���ã���ǿ�� v2��
echo ------------------------------------------------------------
echo   ��ĿĿ¼��%PROJECT_DIR%
echo   �ֿ����ƣ�%REPO_NAME%
echo   �߼� backup Զ�̣�localbackup:%REPO_NAME%.git
echo   ���ݸ�Ŀ¼��Windows����%BACKUP_ROOT_WIN%
echo   ���ݸ�Ŀ¼��WSL��    ��/mnt/d/GitBackups/
echo   WSL ���ã�%WSL_AVAILABLE% / �з��а棺%WSL_HAS_DISTRO% / Git��װ��%WSL_HAS_GIT%
echo   �����ļ���%CONF%
echo ============================================================
echo  1) һ�����ã�����A��ӳ�䣨Windows+WSL��ͳһ backup Զ��
echo  2) ��ʼ����������ֿⲢ�״��ύ/����
echo  3) ��װ�ύ���Զ����͹���
echo  4) ����һ�� CLI ����ɹ����Զ����ݣ�
echo  5) �����浵��ǩ��savepoint��������
echo  6) ��¡��֤���ӱ��ݲֿ��¡һ����֤
echo  7) �ָ����ύ��/��ǩ�������·�֧ʽ�ָ�
echo ------------------------------------------------------------
echo  8) ӳ�䡢Զ�̼�飨��ʾ remotes��
echo  9) �޸�/���裩����A��ӳ�䣨Windows+WSL��
echo 10) �޸���Ŀ��������ĿĿ¼/�ֿ���/���ݸ�Ŀ¼��������
echo 11) ��ռ������ã�ɾ�� .last.config��
echo 12) ������ϣ���� Git��WSL��·�����õȣ�
echo  0) �˳�
echo ============================================================
set /p choice=��ѡ���������س���

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

echo [��ʾ] ��Ч��ѡ��
pause
goto :MENU

REM ================================
REM  ��������ȡ/����/ѯ��
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

REM --- ������ĿĿ¼ ---
if "%PROJECT_DIR%"=="" (set "PROMPT_PD=��������ĿĿ¼��Ĭ�� %DEFAULT_PROJECT_DIR%���� ") else (set "PROMPT_PD=��������ĿĿ¼��Ĭ�� %PROJECT_DIR%���� ")
set "TMP_PD="
set /p TMP_PD=%PROMPT_PD%
if "%PROJECT_DIR%"=="" (
  if "%TMP_PD%"=="" (set "PROJECT_DIR=%DEFAULT_PROJECT_DIR%") else (set "PROJECT_DIR=%TMP_PD%")
) else (
  if not "%TMP_PD%"=="" set "PROJECT_DIR=%TMP_PD%"
)

REM --- �Ƶ��ֿ���������ĿĿ¼���� ---
for %%I in ("%PROJECT_DIR%") do set "DERIVED_REPO=%%~nI"

REM --- ����ֿ�����Ĭ��ʹ���ϴλ��Ƶ���ֵ ---
if "%REPO_NAME%"=="" (set "DEFAULT_REPO=%DERIVED_REPO%") else (set "DEFAULT_REPO=%REPO_NAME%")
set "TMP_RN="
set /p TMP_RN=������ֿ����ƣ�Ĭ�� %DEFAULT_REPO%����
if "%TMP_RN%"=="" (set "REPO_NAME=%DEFAULT_REPO%") else (set "REPO_NAME=%TMP_RN%")

REM --- �ֿ���ȥ�ո񣨱��գ� ---
set "REPO_NAME=%REPO_NAME: =_%"

REM --- ���뱸�ݸ�Ŀ¼ ---
if "%BACKUP_ROOT_WIN%"=="" (set "PROMPT_BR=�����뱸�ݸ�Ŀ¼��Ĭ�� %DEFAULT_BACKUP_ROOT%���� ") else (set "PROMPT_BR=�����뱸�ݸ�Ŀ¼��Ĭ�� %BACKUP_ROOT_WIN%���� ")
set "TMP_BR="
set /p TMP_BR=%PROMPT_BR%
if "%BACKUP_ROOT_WIN%"=="" (
  if "%TMP_BR%"=="" (set "BACKUP_ROOT_WIN=%DEFAULT_BACKUP_ROOT%") else (set "BACKUP_ROOT_WIN=%TMP_BR%")
) else (
  if not "%TMP_BR%"=="" set "BACKUP_ROOT_WIN=%TMP_BR%"
)

echo.
echo [ȷ�ϲ���]
echo   ��ĿĿ¼��%PROJECT_DIR%
echo   �ֿ����ƣ�%REPO_NAME%
echo   ���ݸ�Ŀ¼��%BACKUP_ROOT_WIN%
echo.
exit /b 0

:ASK_PARAMS_SAVE
call :ASK_PARAMS
call :SAVE_CONFIG
exit /b 0

:ASK_PARAMS_SAVE_FROM_MENU
call :ASK_PARAMS
call :SAVE_CONFIG
echo [���] �����ѱ��浽��%CONF%
pause
goto :MENU

:CLEAR_MEMORY
if exist "%CONF%" del /f /q "%CONF%"
set "PROJECT_DIR="
set "REPO_NAME="
set "BACKUP_ROOT_WIN="
echo [���] ����ռ������ã�%CONF%
echo [��ʾ] ���ز˵�������ѯ�ʲ�����
pause
goto :MENU

REM =================
REM  1) һ�����ã�����A��ͳһ backup Զ��
REM =================
:CONF_SCHEME_A
set "LOGICAL_BACKUP_URL=localbackup:%REPO_NAME%.git"
echo.
echo [����] ���� Windows ȫ��ӳ�䣺localbackup: => file:///D:/GitBackups/
git config --global %WIN_URL_KEY% localbackup:
if errorlevel 1 (
  echo [����] ���� Windows ӳ��ʧ�ܣ����Ȩ�޻� Git ��װ��
) else (
  echo [���] Windows insteadof �����ã�%WIN_URL_KEY% = localbackup:
)

if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="0" (
    echo [��ʾ] δ�����Ѱ�װ�� WSL ���а棬���� WSL ӳ�����á�
  ) elseif "%WSL_HAS_GIT%"=="0" (
    echo [����] WSL ��δ��⵽ git������ WSL ��ִ�У�sudo apt-get install -y git
    echo         ���� WSL ӳ�����á�
  ) else (
      echo [����] ���� WSL ȫ��ӳ�䣺localbackup: => /mnt/d/GitBackups/
      wsl -e git config --global %WSL_URL_KEY% localbackup:
      if errorlevel 1 (
        echo [����] ���� WSL ӳ��ʧ�ܣ��뵽 WSL ���ֶ�ִ�У�
        echo         git config --global %WSL_URL_KEY% localbackup:
      ) else (
        echo [���] WSL insteadof �����ã�%WSL_URL_KEY% = localbackup:
      )
    )
  )
) else (
  echo [��ʾ] ����δ��⵽ wsl ������� WSL ӳ�����á�
)

echo [����] ͳһ��Ŀ�� backup Զ��Ϊ�߼���ַ��%LOGICAL_BACKUP_URL%
pushd "%PROJECT_DIR%" >nul 2>nul
if errorlevel 1 (
  echo [����] �޷�������ĿĿ¼��%PROJECT_DIR%
  goto :AFTER_ACTION
)
git rev-parse --git-dir >nul 2>nul
if errorlevel 1 (
  echo [��ʾ] ��ǰĿ¼��δ��ʼ��Ϊ Git �ֿ⣬����ִ�� git init ...
  git init || (echo [����] git init ʧ�� & popd >nul & goto :AFTER_ACTION)
)

git remote get-url backup >nul 2>nul
if not errorlevel 1 git remote remove backup
git remote add backup "%LOGICAL_BACKUP_URL%"
if errorlevel 1 (
  echo [����] ���� backup Զ��ʧ�ܣ���ȷ�ϲֿ�״̬��
  popd >nul & goto :AFTER_ACTION
)
echo [���] ����A ӳ��� backup Զ���Ѿ�����
popd >nul
goto :AFTER_ACTION

REM =================
REM  2) ��ʼ��
REM =================
:DO_INIT
echo.
echo [ִ��] ��ʼ����������ֿ� + �״��ύ/���ͣ�
REM powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action init -ProjectDir "%PROJECT_DIR%" -RepoName "%REPO_NAME%"
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action init -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  3) ��װ����
REM =================
:DO_HOOK
echo.
echo [ִ��] ��װ�ύ���Զ����͹���
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action install-hook -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  4) ��������
REM =================
:DO_RUN_TASK
echo.
set "TASK_CMD=claude code -p .\spec.md"
set "TASK_MESSAGE=feat: �Զ��޶��������"
echo [ִ��] ����һ�� CLI ����ɹ����Զ����ݣ�
echo       ���%TASK_CMD%
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action run -ProjectDir "%PROJECT_DIR%" -Command "%TASK_CMD%" -Message "%TASK_MESSAGE%"
goto :AFTER_ACTION

REM =================
REM  5) �浵��ǩ
REM =================
:DO_SAVEPOINT
echo.
echo [ִ��] �����浵��ǩ��savepoint��������
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action savepoint -ProjectDir "%PROJECT_DIR%"
goto :AFTER_ACTION

REM =================
REM  6) ��¡��֤
REM =================
:DO_CLONE_TEST
echo.
set "DEF_CLONE_DEST=D:\TestRestore\%REPO_NAME%"
set /p CLONE_DEST=�������¡��֤Ŀ��Ŀ¼���س�Ĭ�� %DEF_CLONE_DEST%����
if "%CLONE_DEST%"=="" set "CLONE_DEST=%DEF_CLONE_DEST%"
echo [ִ��] ��¡��֤����%CLONE_DEST%
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action clone-test -ProjectDir "%PROJECT_DIR%" -RepoName "%REPO_NAME%" -CloneDest "%CLONE_DEST%"
goto :AFTER_ACTION

REM =================
REM  7) �ָ���ʷ
REM =================
:DO_RESTORE
echo.
set "RESTORE_REF="
set /p RESTORE_REF=������ָ����ύ�Ż��ǩ���� save-YYYYMMDD-HHMMSS����
if "%RESTORE_REF%"=="" ( echo [��ʾ] δ����Ŀ�꣬��ȡ���� & goto :AFTER_ACTION )
echo [ִ��] �ָ��� %RESTORE_REF% �����·�֧�ָ�
powershell -ExecutionPolicy Bypass -File "%TOOLKIT%" -Action restore -ProjectDir "%PROJECT_DIR%" -Ref "%RESTORE_REF%" -NewBranch
goto :AFTER_ACTION

REM =================
REM  8) ���״̬
REM =================
:CHECK_STATUS
echo.
echo [��Ϣ] Windows ȫ��ӳ�䣨���� localbackup Ӧӳ�䣩
git config --global --get-regexp ^url\..*\.insteadof$ 2>nul | findstr /I localbackup
echo ------------------------------------------------------------
if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="1" (
    echo [��Ϣ] WSL ȫ��ӳ�䣨������ url.*.insteadof=localbackup: ��Ŀ��
    wsl --cd ~ -e git config --global --get-regexp url\..*\.insteadof
  ) else (
    echo [��ʾ] δ�����Ѱ�װ�� WSL ���а棬�޷� WSL ӳ���顣
  )
  echo ------------------------------------------------------------
)
pushd "%PROJECT_DIR%" >nul 2>nul
if errorlevel 1 (
  echo [����] �޷�������ĿĿ¼��%PROJECT_DIR%
  goto :AFTER_ACTION
)
echo [��Ϣ] ��Ŀ remotes��
git remote -v
popd >nul
goto :AFTER_ACTION

REM =================
REM  9) ����ӳ��
REM =================
:RESET_SCHEME_A
echo.
echo [ִ��] �������ã�����A��ӳ�䣨Windows+WSL��

REM Windows ӳ�����ã�����������ã�������������
git config --global --unset-all %WIN_URL_KEY% 2>nul
git config --global %WIN_URL_KEY% localbackup:
if errorlevel 1 (
  echo [����] ���� Windows ӳ��ʧ�ܣ����� Git ����Ȩ��
  echo [���] �����ֶ�ִ�У�git config --global %WIN_URL_KEY% localbackup:
) else (
  echo [���] Windows ӳ��������ɣ�%WIN_URL_KEY% = localbackup:
)

REM WSL ��������
if "%WSL_AVAILABLE%"=="1" (
  if "%WSL_HAS_DISTRO%"=="1" (
    REM �ȳ�����������ã����Դ�����Ϊ���ܲ����ڣ�
    wsl --cd ~ -e git config --global --unset-all %WSL_URL_KEY% 2>nul
    REM ���������ò������
    wsl --cd ~ -e git config --global %WSL_URL_KEY% localbackup:
    if errorlevel 1 (
      echo [����] ���� WSL ӳ��ʧ�ܣ��뵽 WSL ���ֶ�ִ�У�
      echo         git config --global --unset-all %WSL_URL_KEY%
      echo         git config --global %WSL_URL_KEY% localbackup:
      echo [���] ������ WSL �� Git ����Ȩ������� Git δ��ȷ��װ
    ) else (
      echo [���] WSL ӳ��������ɣ�%WSL_URL_KEY% = localbackup:
    )
  ) else (
    echo [��ʾ] δ�����Ѱ�װ�� WSL ���а棬���� WSL ӳ�����á�
  )
)
goto :AFTER_ACTION

REM =================
REM  12) �������
REM =================
:DO_DIAGNOSE
echo.
echo [ִ��] ������ϼ��
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