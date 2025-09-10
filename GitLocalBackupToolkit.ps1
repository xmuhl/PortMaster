<# =====================================================================
 GitLocalBackupToolkit.ps1
 一体化：本地裸仓库创建 + 项目仓库初始化 + 首次推送 + 自动推送钩子 +
        CLI 成功后自动备份 + 存档标签 + 克隆验证 + 一键回溯
 适配：Windows 10/11 + Git for Windows + VS/VS Code/WDK/MFC 项目
 作者：starthl 专用定制版
===================================================================== #>

[CmdletBinding()]
param(
  [ValidateSet('init','install-hook','remove-hook','run','savepoint','clone-test','restore')]
  [string]$Action = 'init',

  # ——必填：项目根目录（含 .sln 的那一层）
  [Parameter(Mandatory = $true)]
  [string]$ProjectDir,

  # ——可调：本地“远程”裸仓库根目录、远程名、file:// 协议
  [string]$BackupsRoot = "D:\GitBackups",
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

# ========== 公用函数 ==========
function Ensure-Git {
  if (-not (git --version 2>$null)) {
    throw "未检测到 git，请先安装 Git for Windows。"
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

function Install-PostCommitHook {
  $hookDir = ".git/hooks"
  if (-not (Test-Path $hookDir)) { throw "未检测到 .git/hooks，请先 git init 并提交一次。" }
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
  $path = Join-Path $hookDir "post-commit"
  if (Test-Path $path) { Copy-Item $path "$path.bak" -Force }
  Write-LF -Path $path -Content $hook
  try { bash -lc "chmod +x `"$path`"" 2>$null } catch {}
  Write-Host "[OK] 已安装自动推送钩子：$path" -ForegroundColor Green
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
  $bareUrl  = $( if ($UseFileProtocol) { "file:///" + ($bareDir -replace '\\','/') } else { $bareDir } )

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
  }
}
catch {
  Write-Error $_.Exception.Message
  exit 1
}
