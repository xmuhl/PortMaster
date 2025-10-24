# user_rules.md

> 通用型个人工作规范，适用于新建项目的初始化配置与日常协作流程，可在此基础上添加项目专属细节。

## 1. 基本原则

- 所有构建以 **0 error / 0 warning** 为硬性标准。
- 默认工作语言为简体中文，代码注释、界面文本与文档保持一致。
- 源码、脚本与文档统一使用 UTF-8 编码（必要时保留 BOM；C++/MFC 模块需配合 `#pragma execution_character_set("utf-8")`）。
- 任何关键改动必须同步更新规范、流程文档及修订记录，保证可追踪性。

## 2. 编译与构建

### 2.1 首选命令

| 优先级 | 命令/脚本 | 说明 |
| ------ | --------- | ---- |
| 1 | `autobuild_x86_debug.bat` | Win32 Debug 快速验证脚本，强制零警告。 |
| 2 | `autobuild.bat` | Win32/x64、Debug/Release 全量构建脚本，作为兜底方案。 |

- 构建环境要求 Visual Studio 2022（任意版本）及 v143 工具集。
- 运行库建议使用静态链接 MFC（/MT）以交付单一 EXE。
- 构建日志输出为 `msbuild_<Configuration>_<Platform>.log`，用于问题追踪。

### 2.2 构建前自检

- 校验解决方案与脚本是否存在（如 `.sln`、`autobuild*.bat`）。
- 确认 Git 远程可访问，凭证有效。
- 在 Windows / WSL 双环境下验证 Visual Studio、PowerShell 执行策略及 `cmd.exe` 调用可用。
- 记录检测结论，异常时提供修复建议。

## 3. 推荐体系结构指引（可按需裁剪）

- 按“应用/界面层 ↔ 协议或业务层 ↔ 传输或数据层”的分层方式组织代码，保持接口清晰、依赖自上而下。
- 异步/多线程逻辑需通过标准同步原语（`std::mutex`、`std::atomic` 等）保证线程安全。
- 错误处理自底向上冒泡，必要时支持降级运行模式。
- 配置数据分层存储（应用、协议、传输、UI 等），提供默认值与损坏恢复机制。

## 4. 开发流程与修改管控

### 4.1 工作流程

1. **初始化**：克隆仓库 → 运行环境自检 → 同步依赖 → 校验构建脚本可用。
2. **开发循环**：
   - 明确所处层级和接口契约。
   - 对关键路径补充单元或集成测试，必要时追加临时日志。
   - 涉及状态机、线程、协议的改动需更新设计说明。
3. **构建验证**：始终优先执行 `autobuild_x86_debug.bat`，若失败再执行 `autobuild.bat`；任一脚本失败必须立即处理。
4. **提交前检查**：清理临时产物，核对编码与语言规范。
5. **提交与发布**：提交信息使用 `类别: 描述` 格式（`feat` / `fix` / `docs` / `refactor` / `test` / `chore` 等），简洁说明影响范围；必要时打标签归档。

### 4.2 修订记录

- 维护 `docs/修订工作记录YYYYMMDD-HHMMSS.md` 形式的日志，记录目标、计划、执行与总结。
- 每次关键操作及时填写，退出前补充结果及后续动作。

### 4.3 文件与编码要求

- 新增文件使用 UTF-8 编码并保持行结束符一致。
- 禁止提交 `.vs/`、`bin/`、`obj/`、`Debug/`、`Release/` 等临时目录。
- 对脚本授予正确执行权限（WSL 下 `chmod +x`）。

## 5. 跨环境自动化示例

### 5.1 环境识别脚本

```bash
if grep -qi microsoft /proc/version 2>/dev/null; then
    export CURRENT_ENV="WSL"
else
    export CURRENT_ENV="Windows"
fi
```

```powershell
if ($env:WSL_INTEROP) { $env:CURRENT_ENV = "WSL" } else { $env:CURRENT_ENV = "Windows" }
```

### 5.2 修改记录模板生成

```bash
REVISION_DIR="docs"
REVISION_FILE="修订工作记录$(date +%Y%m%d-%H%M%S).md"
mkdir -p "$REVISION_DIR"
cat <<'EOF' > "$REVISION_DIR/$REVISION_FILE"
# 修订工作记录

## 本次修订任务
- [ ] 明确目标
- [ ] 所需输入
- [ ] 风险评估

## 修订计划
- 阶段一：
- 阶段二：
- 阶段三：

## 执行记录
- 如：HH:MM 开始 XX
- 如：HH:MM 完成 XX → 结果

## 汇总
- 成果概述：
- 难点与解决：
- 后续事项：
EOF
```

### 5.3 环境同步

```bash
cd "$WORK_DIR"
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    git pull --rebase 2>/dev/null || echo "提示：同步失败需人工处理"
else
    git pull --rebase 2>$null || Write-Host "提示：同步失败需人工处理"
fi
```

### 5.4 构建与验证

```bash
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    cmd.exe /c "cd /d %WIN_PATH% && autobuild_x86_debug.bat" || \
    cmd.exe /c "cd /d %WIN_PATH% && autobuild.bat"
else
    if (Test-Path "autobuild_x86_debug.bat") {
        .\autobuild_x86_debug.bat
    } elseif (Test-Path "autobuild.bat") {
        .\autobuild.bat
    } else {
        & "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" *.sln /build Debug
    }
fi
```

### 5.5 提交与推送

```bash
git add -A
git commit -m "fix: 精简构建脚本输出"
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # 推送到PortMaster远程（通过localbackup映射自动适配路径）
    git push PortMaster HEAD
    # 推送到backup远程（工具脚本配置的备份仓库）
    git remote | grep -q backup && git push backup HEAD
else
    $remotes = git remote
    if ($remotes -contains "PortMaster") { git push PortMaster HEAD }
    if ($remotes -contains "backup") { git push backup HEAD }
fi
# 注：如已安装GitBackup工具的post-commit钩子，上述推送可能自动执行
```

## 6. 目录与工具路径（示例，可按需修改）

**WSL 环境**

```bash
WORK_DIR="/mnt/c/Users/huangl/Desktop/Project"
WIN_PATH="C:\\Users\\huangl\\Desktop\\Project"
BACKUP_REPO="/mnt/d/GitBackups/Project.git"
```

**Windows 环境**

```powershell
$WORK_DIR = "C:\\Users\\huangl\\Desktop\\Project"
$BACKUP_REPO = "D:\\GitBackups\\Project.git"
```

## 7. 异常处理

### 7.1 构建失败

```bash
echo "构建失败，开始排查"
cmd.exe /c "where devenv.exe" 2>/dev/null || echo "提示：未检测到 Visual Studio"
ls -la *.sln *.vcxproj 2>/dev/null || echo "提示：缺少项目文件"
echo "请参考构建日志定位具体错误"
```

### 7.2 合并冲突

```bash
if git status | grep -q "both modified"; then
    echo "检测到合并冲突，请手动解决"
    git status --porcelain | grep "^UU"
    exit 1
fi
```

### 7.3 环境异常

- WSL：`chmod +x *.bat` 修正脚本权限，`dos2unix` 统一换行，`sync` 刷新磁盘。
- PowerShell：检查执行策略 `Get-ExecutionPolicy`，必要时 `Set-ExecutionPolicy RemoteSigned`；关注路径长度与文件编码可读性。

## 8. 构建验证脚本示例

```bash
echo "=== 环境验证开始 ==="
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    echo "当前目录: $(pwd)"
    ls -la *.sln 2>/dev/null && echo "✔ 发现解决方案" || echo "✘ 缺少解决方案"
    git remote -v && echo "✔ Git 远端可访问" || echo "✘ Git 配置异常"
    cmd.exe /c "where devenv.exe" 2>/dev/null && echo "✔ Visual Studio 可用" || echo "✘ 未找到 VS"
    ls -la autobuild*.bat 2>/dev/null && echo "✔ 构建脚本齐备" || echo "✘ 构建脚本缺失"
else
    Write-Host "当前目录: $(Get-Location)"
    if (Test-Path *.sln) { Write-Host "✔ 发现解决方案" -ForegroundColor Green } else { Write-Host "✘ 缺少解决方案" -ForegroundColor Red }
    try { git remote -v | Out-Null; Write-Host "✔ Git 远端可访问" -ForegroundColor Green } catch { Write-Host "✘ Git 配置异常" -ForegroundColor Red }
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
    if (Test-Path $vsPath) { Write-Host "✔ Visual Studio 可用" -ForegroundColor Green }
    $scripts = Get-ChildItem "autobuild*.bat" -ErrorAction SilentlyContinue
    if ($scripts) { Write-Host "✔ 构建脚本: $($scripts.Name -join ', ')" -ForegroundColor Green }
    $policy = Get-ExecutionPolicy
    if ($policy -ne "Restricted") { Write-Host "✔ 执行策略: $policy" -ForegroundColor Green }
fi
echo "=== 验证结束 ==="
```

## 9. 文档与版本管理

- 重要变更后同步更新规范文档、设计说明与操作手册。
- 修订记录按时间归档，确保审计可追溯。
- 提交信息遵循统一格式，必要时附带标签与构建结果摘要。

---

本规范可直接用于新项目；如需扩展，请在此文件增补章节并在项目 README 中引用，使团队成员能够及时获得最新要求。
