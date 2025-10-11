# Repository Guidelines

> 本文件整合 `CLAUDE.md` 的工作流、编码与交付规范，所有代理与贡献者必须在每次操作前自动对照执行。

## 核心原则
- 使用分层架构：`UI (src/) → Protocol (Protocol/) → Transport (Transport/)`，公共工具置于 `Common/`。
- 中文界面与日志必须保持正确编码与内容准确，传输流程需保证字节不丢失、无重复。
- 任何提交前都要通过零错误、零警告的构建验证。

## 文件编码与命名约定
- 源码一律采用 **UTF-8 with BOM**；含中文常量时添加 `#pragma execution_character_set("utf-8")`。
- 注释、日志、UI 文本统一使用简体中文，强调技术要点、避免空泛描述。
- 类/结构体使用 PascalCase，成员变量以 `m_` 开头，函数名和局部变量使用 lowerCamelCase。
- 禁止提交 `.vs/、bin/、obj/、Debug/、Release/` 等生成目录或临时产物。

## 环境检测与初始化（Step 0）
1. 运行以下脚本自动判定环境与工作目录：
   ```bash
   if [[ "$WSL_DISTRO_NAME" != "" ]] || [[ "$(uname -r)" == *microsoft* ]]; then
       CURRENT_ENV="WSL"
       WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"
       WIN_PATH="C:\\Users\\huangl\\Desktop\\PortMaster"
       DATE_CMD="date"
       BACKUP_REPO="/mnt/d/GitBackups/PortMaster.git"
   else
       CURRENT_ENV="PowerShell"
       WORK_DIR="C:\\Users\\huangl\\Desktop\\PortMaster"
       WIN_PATH="C:\\Users\\huangl\\Desktop\\PortMaster"
       DATE_CMD="Get-Date -Format"
       BACKUP_REPO="D:\\GitBackups\\PortMaster.git"
   fi
   echo "环境: $CURRENT_ENV"
   echo "工作目录: $WORK_DIR"
   ```
2. 后续所有命令均在 `WORK_DIR` 下执行，禁止跳过初始化。

## 标准工作流步骤
### Step 1 – 修订记录（强制）
- 在 `WORK_DIR` 生成 `修订工作记录YYYYMMDD-HHMMSS.md`，内容至少包括：修订目标、预期成果、问题描述、根因分析、解决方案设计、分阶段任务与测试清单。
- 修订过程中的关键节点要实时更新“修订执行记录”与“技术总结”。

### Step 2 – 同步与冲突检查
- 执行 `git pull --rebase` 同步远端；如有冲突立即停止，按 `CLAUDE.md` 的冲突处理指引解决后再继续。

### Step 3 – 进度文档节奏
- 三阶段记录法：
  - ⏳ 开始前记录时间与任务范围。
  - ✅ 完成后记录关键改动与验证结果。
  - 提交前补充总结，不在文档中写入 git 命令或提交号。

### Step 4 – 代码修改规范
- 遵循最小变更原则，优先复用 `Common/`、`Transmission*` 等现有辅助函数。
- 出现复杂逻辑时添加简洁技术注释，禁止冗长无信息量的描述。
- 影响传输可靠性的变更需同步更新 `AutoTest/` 与 `TestReliable/` 脚本或记录验证步骤。

### Step 5 – 编译触发判定
- 检查 `git status --porcelain`；如包含 `.cpp/.h/.rc/.vcxproj/.sln` 等源码改动，必须编译。纯文档更新可跳过。

### Step 6 – 智能编译执行
- **WSL**：优先运行 `./smart_build_wsl.sh`；若不存在则 `cmd.exe /c "autobuild_x86_debug.bat"`，失败时自动结束 `PortMaster.exe` 后重试。
- **PowerShell**：优先 `smart_build.bat`；其次 `autobuild_x86_debug.bat`。如失败执行 `taskkill /F /IM PortMaster.exe`，等待 2 秒后重试。
- 保留含 “0 error(s), 0 warning(s)” 的日志片段，在最终汇报中展示。

### Step 7 – 版本控制与推送
- `git add -A`，使用 `type: 简述修改内容` 的中文提交信息，类型限定为 `feat|fix|docs|refactor|test|chore`。
- 推送顺序：`git push PortMaster HEAD`，若存在 `backup` 远程则再推送一次。
- 仅在明确要求时创建 `save-YYYYMMDD-HHMMSS` 标签。

### Step 8 – 汇报输出
- 汇报必须包含：最新 commit ID、推送结果、关键编译日志、测试验证步骤/结果、遗留问题或后续操作建议。
- 所有描述需与修订记录及提交内容保持一致。

## 构建与测试命令清单
- `autobuild_x86_debug.bat`：标准 Win32 Debug 构建，警告视为错误。
- `autobuild.bat`：完整 Win32/x64 Debug/Release 构建备用脚本。
- `smart_build_wsl.sh` / `smart_build.bat`：环境自适应构建脚本，自动处理进程占用。
- 使用 VS2022（v143）打开 `PortMaster.sln` 进行调试或增量编译。
- 传输链路改动需验证实际文件一致性，例如比较 `StocksInfo.ini` 与 `ReceiveData.ini` 的字节数。

## 故障处理与环境注意事项
- 编译失败或测试未通过不得提交；应记录错误信息、临时处理方案及未解决问题。
- **WSL**：注意执行权限（`chmod +x *.bat`）、换行符（`dos2unix`）和文件同步（`sync`）。
- **PowerShell**：若执行策略为 `Restricted`，建议运行 `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser`；同时关注路径长度与文件编码异常。

> 违反流程会导致审核阻塞或质量风险。请在接手任何任务前自检是否满足以上要求。
