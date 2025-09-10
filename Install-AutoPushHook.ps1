# Install-AutoPushHook.ps1
# 功能：安装 post-commit 钩子，实现每次提交后自动 push 到 backup / origin
# 用法（在项目根目录执行）：powershell -ExecutionPolicy Bypass -File .\Install-AutoPushHook.ps1

function Write-FileWithLF {
    param([string]$Path,[string]$Content)
    $lf = $Content -replace "`r?`n", "`n"
    [System.IO.File]::WriteAllText($Path, $lf, New-Object System.Text.UTF8Encoding($false))
}

# 1) 基础检测
$git = (git --version 2>$null)
if (-not $git) { Write-Error "未检测到 git，请先安装 Git for Windows。"; exit 1 }

if (-not (Test-Path ".git")) { Write-Error "当前目录不是 Git 仓库根目录（缺少 .git）：$(Get-Location)"; exit 1 }

$hookDir = Join-Path ".git" "hooks"
if (-not (Test-Path $hookDir)) { New-Item -ItemType Directory -Path $hookDir | Out-Null }

# 2) 生成 post-commit 内容（Bash）
$hookContent = @'
#!/usr/bin/env bash
# post-commit：提交完成后自动推送到 backup / origin（若存在）
set -e

# 当前分支（若无提交或处于分离头指针，则直接退出）
branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '')"
if [ -z "$branch" ] || [ "$branch" = "HEAD" ]; then
  exit 0
fi

remote_exists() { git remote | grep -qx "$1"; }

push_one() {
  local r="$1"
  if remote_exists "$r"; then
    # 若尚无上游，先用 -u 绑定；否则直接推送
    if git rev-parse --symbolic-full-name --quiet "@{u}" >/dev/null 2>&1; then
      git push "$r" "$branch" || true
    else
      git push -u "$r" "$branch" || true
    fi
  fi
}

# 优先推送到 backup（你的本地裸仓库），然后 origin（如配置了 GitHub）
push_one backup
push_one origin

exit 0
'@

# 3) 备份已有钩子并写入新钩子（LF 行尾）
$hookPath = Join-Path $hookDir "post-commit"
if (Test-Path $hookPath) {
    Copy-Item $hookPath "$hookPath.bak" -Force
}
Write-FileWithLF -Path $hookPath -Content $hookContent

# 4) 在 Windows 下无需 chmod，但保持一致性（若用户在类 Unix 上也能用）
try { bash -lc "chmod +x `"$hookPath`"" 2>$null } catch {}

Write-Host "[OK] 已安装提交后自动推送钩子：$hookPath" -ForegroundColor Green
Write-Host "提示：确保已配置至少一个远程，如 backup -> D:\GitBackups\<Project>.git" -ForegroundColor Yellow
