<# =====================================================================
 GitLocalBackupToolkit.ps1
 一体化：本地裸仓库创建 + 项目仓库初始化 + 首次推送 + 自动推送钩子 +
        CLI 成功后自动备份 + 存档标签 + 克隆验证 + 一键回溯
 适配：Windows 10/11 + Git for Windows + VS/VS Code/WDK/MFC 项目
 作者：starthl 专用定制版
===================================================================== #>

[CmdletBinding()]
param(
  [ValidateSet('init','install-hook','remove-hook','run','savepoint','clone-test','restore','diagnose')]
  [string]$Action = 'init',

  # ——必填：项目根目录（含 .sln 的那一层）
  [Parameter(Mandatory = $true)]
  [string]$ProjectDir,

  # ——可调：本地"远程"裸仓库根目录、远程名、file:// 协议
  [string]$BackupsRoot = $(if ($env:WSL_DISTRO_NAME -or [System.Environment]::OSVersion.Platform -eq "Unix") { "/mnt/d/GitBackups" } else { "D:\GitBackups" }),
  [string]$RemoteName = "backup",
  [switch]$UseFileProtocol,

  # ——仅 run 动作使用
  [string]$Command,
  [string]$Message,

  # ——clone-test 动作使用
  [string]$CloneDest,

  # ——restore 动作使用
  [string]$Ref,
  [switch]$NewBranch
)

# ========== 诊断函数 ==========
function Test-GitBackupEnvironment {
  Write-Host "=== Git Backup 环境诊断 ===" -ForegroundColor Cyan
  $env = Get-CurrentEnvironment
  Write-Host "当前环境：$env" -ForegroundColor Green

  # Git 版本检查
  try {
    $gitVersion = (git --version 2>$null)
    Write-Host "✅ Git 可用：$gitVersion" -ForegroundColor Green
  } catch {
    Write-Host "❌ Git 未安装或不可用" -ForegroundColor Red
    return $false
  }

  # 路径兼容性检查
  Write-Host "路径配置：BackupsRoot = $BackupsRoot" -ForegroundColor Yellow
  if (Test-Path $BackupsRoot) {
    Write-Host "✅ 备份根目录存在：$BackupsRoot" -ForegroundColor Green
  } else {
    Write-Host "⚠️  备份根目录不存在，将自动创建：$BackupsRoot" -ForegroundColor Yellow
  }

  # 环境特定检查
  if ($env -eq "WSL") {
    Write-Host "WSL 环境特定检查：" -ForegroundColor Yellow
    Write-Host "  - Windows 路径挂载：$(Test-Path '/mnt/c' && '✅ 可用' || '❌ 不可用')" -ForegroundColor White
    Write-Host "  - PowerShell 可用：$(Get-Command pwsh -ErrorAction SilentlyContinue && '✅ 可用' || '❌ 不可用')" -ForegroundColor White
  } else {
    Write-Host "Windows 环境特定检查：" -ForegroundColor Yellow
    Write-Host "  - WSL 可用：$(Get-Command wsl -ErrorAction SilentlyContinue && '✅ 可用' || '❌ 不可用')" -ForegroundColor White
    Write-Host "  - Git Bash 可用：$(Get-Command bash -ErrorAction SilentlyContinue && '✅ 可用' || '❌ 不可用')" -ForegroundColor White
  }

  return $true
}

# ========== 公用函数 ==========
function Get-CurrentEnvironment {
  if ($env:WSL_DISTRO_NAME -or (Test-Path "/proc/version" -ErrorAction SilentlyContinue)) {
    return "WSL"
  } elseif ($env:OS -eq "Windows_NT") {
    return "Windows"
  } else {
    return "Unknown"
  }
}

function Ensure-Git {
  if (-not (git --version 2>$null)) {
    $env = Get-CurrentEnvironment
    if ($env -eq "WSL") {
      throw "未检测到 git，请在 WSL 中安装 Git 或确保 Windows Git 可访问。"
    } else {
      throw "未检测到 git，请先安装 Git for Windows。"
    }
  }
}

function Resolve-Strict([string]$p) {
  try { return (Resolve-Path $p).Path } catch { throw "路径不存在：$p" }
}

function Ensure-Dir([string]$p) {
  if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

# 修正版：写入 UTF-8（无 BOM）且强制 LF 行尾
function Write-LF([string]$Path,[string]$Content) {
    # 将所有换行规范化为 LF，避免 Bash 钩子因 CRLF 报错
    $lf = $Content -replace "`r?`n", "`n"
    # 以 UTF-8 无 BOM 编码写入（VS/记事本查看正常，Git Bash 执行不受 BOM 影响）
    $enc = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
    [System.IO.File]::WriteAllText($Path, $lf, $enc)
}

function In-Project { param([string]$proj) Set-Location $proj }

function Ensure-BareRepo([string]$bareDir) {
  if (-not (Test-Path $bareDir)) {
    Ensure-Dir (Split-Path $bareDir -Parent)
    New-Item -ItemType Directory -Path $bareDir | Out-Null
    git init --bare "$bareDir" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "创建裸仓库失败：$bareDir" }
    Write-Host "[OK] 已创建裸仓库：$bareDir" -ForegroundColor Green
  } else {
    Write-Host "[i] 已存在裸仓库：$bareDir"
  }
}

function Ensure-ProjectRepo {
  if (-not (Test-Path ".git")) {
    git init -b main | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "初始化项目仓库失败：$(Get-Location)" }
    Write-Host "[OK] 已初始化项目仓库（main）" -ForegroundColor Green
  } else {
    # 统一分支名为 main（如你不想强制，可注释下一行）
    git branch -M main 2>$null
  }
  git config core.autocrlf true 2>$null
  git config --global --add safe.directory (Get-Location).Path 2>$null
}

function Ensure-GitIgnore {
  if (Test-Path ".gitignore") { Write-Host "[i] 检测到 .gitignore，保持现状"; return }
  @'
# —— IDE —— 
.vscode/
.vs/
*.code-workspace

# —— VS/WDK/MFC 构建产物 —— 
[Bb]in/
[Dd]ebug*/
[Rr]elease*/
x64/
x86/
[Oo]bj/
ipch/
*.obj
*.pdb
*.idb
*.ilk
*.exp
*.lib
*.exe
*.dll
*.iobj
*.ipdb
*.tlog
*.lastbuildstate
*.log
*.sbr

# —— 版本控制/其他 —— 
.svn/
Thumbs.db
.DS_Store
*.tmp
*.bak
*.swp

# —— Node/Web 可选 —— 
node_modules/
dist/
coverage/

# —— Python 可选 —— 
__pycache__/
*.py[cod]
.venv/
venv/
*.egg-info/
'@ | Set-Content -Encoding UTF8 ".gitignore"
  Write-Host "[OK] 已生成 .gitignore" -ForegroundColor Green
}

function First-Commit-IfNeeded {
  git add -A 2>$null
  git commit -m "init: 初始提交" 2>$null
  if ($LASTEXITCODE -eq 0) { Write-Host "[OK] 已完成首次提交" -ForegroundColor Green }
  else { Write-Host "[i] 无需首次提交（可能此前已有提交或无变更）" }
}

function Set-Remote-And-Push([string]$remote,[string]$url) {
  $has = (git remote 2>$null) -split '\r?\n' | Where-Object { $_ -eq $remote }
  if (-not $has) {
    git remote add "$remote" "$url"
    Write-Host "[OK] 已添加远程 $remote -> $url" -ForegroundColor Green
  } else {
    git remote set-url "$remote" "$url"
    Write-Host "[i] 已更新远程 $remote URL -> $url"
  }
  git branch -M main 2>$null
  git push -u "$remote" main
  if ($LASTEXITCODE -ne 0) { throw "推送失败，请检查分支/远程路径/权限。" }
  Write-Host "[DONE] 首次推送完成：$remote/main → $url" -ForegroundColor Green
}

# 修订版：Install-PostCommitHook（自动定位仓库根；UTF-8 无 BOM + LF 写入）
function Install-PostCommitHook {
  # 通过 git 自检仓库根，避免受当前目录影响
  $repoRoot = (& git rev-parse --show-toplevel 2>$null)
  if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    throw "未检测到 Git 仓库。请先在项目根执行：git init（或运行本工具 -Action init）。"
  }

  $gitDir  = Join-Path $repoRoot ".git"
  $hookDir = Join-Path $gitDir "hooks"
  if (-not (Test-Path $hookDir)) {
    New-Item -ItemType Directory -Path $hookDir | Out-Null
  }

  $hook = @'
#!/usr/bin/env bash
# post-commit：提交完成后自动推送到 backup / origin（若存在）
set -e
branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '')"
if [ -z "$branch" ] || [ "$branch" = "HEAD" ]; then exit 0; fi
remote_exists() { git remote | grep -qx "$1"; }
push_one() {
  local r="$1"
  if remote_exists "$r"; then
    if git rev-parse --symbolic-full-name --quiet "@{u}" >/dev/null 2>&1; then
      git push "$r" "$branch" || true
    else
      git push -u "$r" "$branch" || true
    fi
  fi
}
push_one backup
push_one origin
exit 0
'@

  # 使用统一的Write-LF函数，确保UTF-8无BOM+LF写入
  $path = Join-Path $hookDir "post-commit"
  if (Test-Path $path) { Copy-Item $path "$path.bak" -Force }
  Write-LF -Path $path -Content $hook

  # 跨环境兼容的权限设置
  $currentEnv = Get-CurrentEnvironment
  try {
    if ($currentEnv -eq "WSL") {
      bash -lc "chmod +x `"$path`"" 2>$null
    } else {
      # Windows环境下，通过WSL或Git Bash设置权限
      if (Get-Command wsl -ErrorAction SilentlyContinue) {
        wsl chmod +x "`"$path`"" 2>$null
      } elseif (Get-Command bash -ErrorAction SilentlyContinue) {
        bash -lc "chmod +x `"$path`"" 2>$null
      }
    }
  } catch {
    Write-Host "[i] 无法设置钩子执行权限，请手动执行：chmod +x $path" -ForegroundColor Yellow
  }
  Write-Host "[OK] 已安装自动推送钩子：$path" -ForegroundColor Green
  Write-Host "提示：确保已配置远程，如 backup -> $BackupsRoot\<Project>.git" -ForegroundColor Yellow
}

function Remove-PostCommitHook {
  $path = ".git/hooks/post-commit"
  $bak  = ".git/hooks/post-commit.bak"
  if (Test-Path $path) { Remove-Item $path -Force }
  if (Test-Path $bak)  { Move-Item $bak $path -Force }
  Write-Host "[OK] 已卸载/还原 post-commit 钩子" -ForegroundColor Green
}

function Choose-Remote {
  $remotes = (git remote 2>$null) -split '\r?\n'
  if ($remotes -contains $RemoteName) { return $RemoteName }
  if ($remotes -contains 'backup')    { return 'backup' }
  if ($remotes -contains 'origin')    { return 'origin' }
  return $null
}

function Run-And-AutoBackup([string]$cmd,[string]$msg) {
  if ([string]::IsNullOrWhiteSpace($cmd)) { throw "run 动作需要 -Command" }
  Write-Host ">> 执行命令：$cmd" -ForegroundColor Cyan
  cmd /c $cmd
  if ($LASTEXITCODE -ne 0) { throw "命令执行失败（exit=$LASTEXITCODE），取消备份。" }
  git add -A
  if ([string]::IsNullOrWhiteSpace($msg)) {
    $msg = "auto: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') 任务完成自动备份"
  }
  git commit -m $msg 2>$null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "[i] 没有可提交的变更，略过提交。" -ForegroundColor Yellow
  } else {
    Write-Host "[OK] 已提交：$msg" -ForegroundColor Green
  }
  $r = Choose-Remote
  if ($null -ne $r) { git push $r HEAD }
  else { Write-Host "[!] 未配置远程，已跳过推送。" -ForegroundColor Yellow }
}

function Create-Savepoint {
  $tag = "save-" + (Get-Date -Format 'yyyyMMdd-HHmmss')
  git add -A 2>$null
  git commit -m "savepoint: $tag" 2>$null | Out-Null
  git tag $tag
  $r = Choose-Remote
  if ($null -ne $r) {
    git push $r HEAD
    git push $r --tags
    Write-Host "[OK] 已创建并推送存档标签：$tag → $r" -ForegroundColor Green
  } else {
    Write-Host "[i] 已创建本地标签：$tag（未配置远程，未推送）"
  }
}

function Clone-Test([string]$bareUrl,[string]$dest) {
  if ([string]::IsNullOrWhiteSpace($dest)) { throw "clone-test 动作需要 -CloneDest" }
  Ensure-Dir (Split-Path $dest -Parent)
  git clone "$bareUrl" "$dest"
}

function Restore-Ref([string]$ref,[switch]$newBranch) {
  if ([string]::IsNullOrWhiteSpace($ref)) { throw "restore 动作需要 -Ref <提交号/标签/分支>" }
  if ($newBranch) {
    $safe = ($ref -replace '[^\w\.-]','_')
    git switch -c "recovery/$safe" "$ref"
    Write-Host "[OK] 已基于 $ref 创建并切换到分支：recovery/$safe" -ForegroundColor Green
  } else {
    git switch --detach "$ref"
    Write-Host "[OK] 已切换到只读游离状态：$ref（若需修改，建议加 -NewBranch）" -ForegroundColor Green
  }
}

# ========== 主流程 ==========
try {
  Ensure-Git
  $projPath = Resolve-Strict $ProjectDir
  $projName = Split-Path $projPath -Leaf
  $bareDir  = Join-Path $BackupsRoot ($projName + ".git")
  # 跨环境兼容的URL构造
  $currentEnv = Get-CurrentEnvironment
  $bareUrl = if ($UseFileProtocol) {
    if ($currentEnv -eq "WSL") {
      "file://" + $bareDir  # WSL中路径已经是Unix格式
    } else {
      "file:///" + ($bareDir -replace '\\','/')  # Windows中需要转换斜杠
    }
  } else {
    $bareDir
  }

  switch ($Action) {
    'init' {
      Write-Host "=== 初始化并备份：$projName ==="
      Ensure-Dir $BackupsRoot
      Ensure-BareRepo $bareDir
      In-Project $projPath
      Ensure-ProjectRepo
      Ensure-GitIgnore
      First-Commit-IfNeeded
      Set-Remote-And-Push -remote $RemoteName -url $bareUrl
    }
    'install-hook' {
      In-Project $projPath
      Install-PostCommitHook
    }
    'remove-hook' {
      In-Project $projPath
      Remove-PostCommitHook
    }
    'run' {
      In-Project $projPath
      Run-And-AutoBackup -cmd $Command -msg $Message
    }
    'savepoint' {
      In-Project $projPath
      Create-Savepoint
    }
    'clone-test' {
      Ensure-BareRepo $bareDir   # 仅确保存在
      Clone-Test -bareUrl $bareUrl -dest (Resolve-Strict $CloneDest)
    }
    'restore' {
      In-Project $projPath
      Restore-Ref -ref $Ref -newBranch:$NewBranch
    }
    'diagnose' {
      Write-Host "项目路径：$projPath" -ForegroundColor Yellow
      Write-Host "备份目录：$bareDir" -ForegroundColor Yellow
      Test-GitBackupEnvironment
      if (Test-Path $projPath) {
        In-Project $projPath
        Write-Host "`n=== 项目 Git 状态 ===" -ForegroundColor Cyan
        Write-Host "当前分支：$(git rev-parse --abbrev-ref HEAD 2>$null || 'N/A')" -ForegroundColor White
        Write-Host "远程仓库：" -ForegroundColor White
        git remote -v 2>$null || Write-Host "  无远程仓库配置" -ForegroundColor Yellow
        Write-Host "Git 配置映射：" -ForegroundColor White
        git config --get-regexp "^url\..*\.insteadof$" 2>$null || Write-Host "  无 URL 映射配置" -ForegroundColor Yellow
      } else {
        Write-Host "❌ 项目路径不存在：$projPath" -ForegroundColor Red
      }
    }
  }
}
catch {
  Write-Error $_.Exception.Message
  exit 1
}
