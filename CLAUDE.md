# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Primary Build Process

- **Primary build command**: `autobuild_x86_debug.bat` - Optimized for Win32 Debug configuration with zero errors and zero warnings enforcement
- **Fallback build command**: `autobuild.bat` - Full configuration build (Win32/x64, Debug/Release) when primary script is unavailable
- **Build requirements**: Visual Studio 2022 (Community/Professional/Enterprise) with v143 toolset
- **Build outputs**: Static-linked MFC application, single EXE deployment target
- **Script priority**: Always use `autobuild_x86_debug.bat` first, fallback to `autobuild.bat` if not found

### Build Verification

- All builds must achieve **zero errors and zero warnings** before proceeding
- Build logs are generated as `msbuild_*.log` files for detailed analysis
- The build script enforces `/p:TreatWarningsAsErrors=true` for quality assurance

## Architecture Overview

### Core Design Pattern

PortMaster follows a **layered architecture** with clear separation between transport, protocol, and application layers:

```
Application Layer (MFC UI)
    ↓
Protocol Layer (Reliable Channel + Frame Codec)
    ↓  
Transport Layer (ITransport implementations)
    ↓
Hardware/OS Layer (Serial/LPT/Network/USB)
```

### Key Architectural Components

**Transport Layer (`Transport/`)**

- `ITransport` - Unified interface for all communication channels
- Multiple concrete implementations: Serial (COM), LPT (via Windows print spooler), USB Printers, TCP/UDP networking, Loopback testing
- Each transport handles its own connection management, buffering, and error handling
- Async I/O support via callback mechanisms

**Protocol Layer (`Protocol/`)**

- `ReliableChannel` - Implements custom reliable transmission protocol over any ITransport
- `FrameCodec` - Handles frame encoding/decoding with format: `0xAA55 + Type + Sequence + Length + CRC32 + Data + 0x55AA`
- `CRC32` - IEEE 802.3 standard implementation (polynomial 0xEDB88320)
- Frame types: START (with file metadata), DATA, END, ACK, NAK
- State machine: Sender (Idle→Starting→Sending→Ending→Done), Receiver (Idle→Ready→Receiving→Done)

**Common Utilities (`Common/`)**

- `ConfigManager` - JSON-based configuration persistence (program directory first, fallback to %LOCALAPPDATA%)
- `RingBuffer` - Auto-expanding circular buffer for data streaming
- `IOWorker` - OVERLAPPED I/O completion port worker for Windows async operations

### State Management Patterns

- Transport states: `TRANSPORT_CLOSED`, `TRANSPORT_OPENING`, `TRANSPORT_OPEN`, `TRANSPORT_CLOSING`, `TRANSPORT_ERROR`
- Protocol states: `RELIABLE_IDLE`, `RELIABLE_STARTING`, `RELIABLE_SENDING`, `RELIABLE_ENDING`, `RELIABLE_READY`, `RELIABLE_RECEIVING`, `RELIABLE_DONE`, `RELIABLE_FAILED`
- Frame types: `FRAME_START`, `FRAME_DATA`, `FRAME_END`, `FRAME_ACK`, `FRAME_NAK`

### Threading Model

- Each transport may spawn its own background threads for I/O operations
- Protocol layer runs state machine in dedicated thread with condition variable synchronization
- UI thread remains responsive through callback-based event delivery
- Thread-safe operations using std::atomic, std::mutex, and proper RAII patterns

### Error Handling Strategy

- Layered error propagation: Transport → Protocol → Application
- CRC validation at protocol layer with automatic NAK/retransmission
- Timeout-based recovery mechanisms with configurable retry limits
- Graceful degradation: protocol layer can be disabled for direct transport access

### Configuration Architecture

- Multi-tier storage: Transport-specific configs, protocol parameters, UI state, application settings
- Automatic persistence on shutdown with corruption recovery
- Default value fallbacks for all configuration parameters
- Support for per-transport-type parameter sets

## Development Notes

### Code Standards

- **Language**: All code, comments, and resources in Simplified Chinese
- **Encoding**: UTF-8 with BOM, `#pragma execution_character_set("utf-8")` where needed
- **Compatibility**: Windows 7-11 support, both x86 and x64 architectures
- **Deployment**: Single EXE with static MFC linking (`/MT` runtime)

### Protocol Implementation Details

- Frame size limit: 1024 bytes payload (configurable via `FrameCodec::SetMaxPayloadSize`)
- Sliding window size: 1 (stop-and-wait ARQ for simplicity)
- File transfer includes metadata: filename (UTF-8), file size, modification time
- Received files auto-saved with collision resolution (numeric suffixes)

### Transport Layer Extension Points

To add new transport types:

1. Implement `ITransport` interface
2. Handle state transitions consistently
3. Implement async callback patterns
4. Add configuration parameters to `TransportConfig`
5. Register with `ConfigManager` for persistence

### UI Integration Patterns

- Transport selection via combo box with dynamic port enumeration
- Dual-view data display (hexadecimal and text representations)
- File drag-drop support for transmission
- Real-time progress reporting via callback mechanisms
- Splash screen integration during application startup

## 跨环境自动执行工作流程

### 目标设定

- **工作目录**：`C:\Users\huangl\Desktop\PortMaster` (Windows) 或 `/mnt/c/Users/huangl/Desktop/PortMaster` (WSL)
- **核心目标**：修改代码并确保编译 0 error / 0 warning，之后自动版本化与远程推送
- **环境特点**：支持Windows PowerShell和WSL2环境，自动检测并适配当前运行环境

### 工作流程步骤（必须按顺序执行）

#### 0. 环境自动检测与初始化

**环境检测脚本（自动适配）：**

```bash
# 检测当前运行环境
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

echo "检测到环境: $CURRENT_ENV"
echo "工作目录: $WORK_DIR"
```

#### 1. 创建修订工作记录文件

**每轮修订工作正式开始前的必要准备：**

```bash
# 生成修订记录文件名（基于当前时间戳，跨环境兼容）
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    REVISION_FILE="修订工作记录$(date +%Y%m%d-%H%M%S).md"
else
    # PowerShell环境
    TIMESTAMP=$(powershell.exe -Command "Get-Date -Format 'yyyyMMdd-HHmmss'")
    REVISION_FILE="修订工作记录${TIMESTAMP}.md"
fi
echo "创建修订记录文件: $REVISION_FILE"

# 创建修订工作记录文件模板
cat > "$REVISION_FILE" << 'EOF'
# 修订工作记录

## 修订概述
- **开始时间**: $(date '+%Y-%m-%d %H:%M:%S')
- **修订目标**: [在此详细描述本轮修订要解决的具体问题]
- **预期成果**: [明确列出期望达成的技术目标]

## 问题详细分析
### 问题描述
[详细描述发现的问题现象、触发条件、影响范围]

### 根本原因分析
[深入分析问题的技术根源、代码层面的具体原因]

### 解决方案设计
[制定具体的修复策略、涉及的文件和函数、修改方案]

## 修订计划安排
### 阶段一：代码分析与定位
- [ ] 任务1: [具体分析任务]
- [ ] 任务2: [具体分析任务]

### 阶段二：代码修改实施
- [ ] 任务1: [具体修改任务]
- [ ] 任务2: [具体修改任务]

### 阶段三：测试验证
- [ ] 编译验证: 确保0 error 0 warning
- [ ] 功能测试: 验证修复效果
- [ ] 回归测试: 确保无新问题引入

## 修订执行记录
[在修订过程中实时更新，记录每个步骤的执行情况和结果]

## 技术总结
[修订完成后填写，总结技术要点、经验教训、改进建议]
EOF

echo "✅ 修订工作记录文件已创建: $REVISION_FILE"
echo "📝 请在正式开始修订前完善问题分析和计划安排部分"
```

**重要说明：**

- 🔴 **强制要求**: 每轮修订必须先创建并完善此记录文件
- 📋 **内容要求**: 详细填写问题分析、解决方案和计划安排
- 🎯 **目的**: 确保修订工作有明确目标和系统性规划
- 📁 **文件管理**: 记录文件将成为项目技术档案的重要组成部分

#### 2. 环境准备与同步

```bash
# 切换到工作目录（跨环境兼容）
cd "$WORK_DIR"

# 同步远程变更（跨环境兼容）
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # WSL环境使用标准Git命令
    git pull --rebase 2>/dev/null || echo "同步完成或无需同步"
else
    # PowerShell环境
    git pull --rebase 2>$null || echo "同步完成或无需同步"
fi
```

#### 3. 进度文档更新（优化策略）

**文档更新时机调整（基于第八轮修订经验）：**

```bash
# 阶段一：代码修改前 - 记录任务开始
# 格式：- ⏳ **HH:MM**: 开始[具体任务描述]

# 阶段二：代码修改完成后 - 记录修改内容和编译结果
# 格式：- ✅ **HH:MM**: 完成[任务描述] - [关键修改点]

# 阶段三：版本控制前 - 最终文档整理
# 包含：详细修改记录、编译验证结果、技术总结
```

**文档内容边界（重要）：**

- ✅ **应记录**: 问题详细分析、代码修改内容、编译验证结果、技术总结
- ❌ **不应记录**: Git提交ID、推送状态、标签详情、版本控制操作过程
- 📝 **目的**: 避免文档变更后再次推送的循环问题
- 🔄 **优化原则**: 分阶段更新文档，最后统一版本控制，减少推送次数

#### 4. 代码修改规范

- 仅修改必要文件，保持最小变更原则
- **严禁提交以下目录**：`.vs/`、`bin/`、`obj/`、`Debug/`、`Release/` 等被 `.gitignore` 忽略的目录
- 遵循现有代码标准：UTF-8 编码、中文注释、MFC 静态链接

**编码格式要求：**
- **新建代码文件**：必须设置为 UTF-8 with BOM 编码格式
- **现有文件修改**：不允许改变现有文件的编码格式，保持原有编码不变
- **编码验证**：使用 `#pragma execution_character_set("utf-8")` 确保编译时正确处理中文字符
- **文件创建**：所有新创建的 .cpp、.h、.rc 文件都必须采用 UTF-8 with BOM 格式

#### 5. 智能编译验证流程（跨环境自适应 + 自动进程关闭）

**编译检查前置步骤（重要）：**

```bash
# 检查变更文件类型，判断是否需要编译（跨环境兼容）
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    CHANGED_FILES=$(git status --porcelain | awk '{print $2}')
else
    # PowerShell环境使用不同的命令
    CHANGED_FILES=$(git status --porcelain | ForEach-Object { $_.Split()[1] })
fi
echo "变更文件: $CHANGED_FILES"

# 源码文件扩展名（需要编译）: .cpp .h .rc .vcxproj .sln 等
# 文档文件扩展名（无需编译）: .md .txt .log 等
```

**编译执行规则：**

- ✅ **源码文件变更时** - 执行智能编译验证
- 🚫 **仅文档文件变更时** - 跳过编译验证
- 🤖 **自动化处理** - 检测并自动关闭占用进程
- ⚡ **效率优化** - 避免不必要的编译操作

**智能编译命令（跨环境自适应 + 自动进程处理）：**

```bash
# 跨环境智能编译系统
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # WSL环境：使用智能编译脚本
    echo "WSL环境：使用智能编译脚本（自动处理进程占用）"
    cd "$WORK_DIR"
    
    # 优先使用智能编译脚本
    if [[ -f "smart_build_wsl.sh" ]]; then
        ./smart_build_wsl.sh
        BUILD_RESULT=$?
    else
        # 备用：使用传统编译方式
        echo "智能编译脚本不存在，使用传统编译方式"
        cmd.exe /c "autobuild_x86_debug.bat" 2>&1 | tail -20
        BUILD_RESULT=$?
        
        # 如果失败且可能是进程占用，手动处理
        if [[ $BUILD_RESULT -ne 0 ]]; then
            echo "编译失败，尝试关闭可能的进程占用..."
            cmd.exe /c "taskkill /F /IM PortMaster.exe" 2>/dev/null
            sleep 2
            echo "重新编译..."
            cmd.exe /c "autobuild_x86_debug.bat" 2>&1 | tail -20
            BUILD_RESULT=$?
        fi
    fi
else
    # PowerShell环境：使用Windows智能编译脚本
    echo "PowerShell环境：使用智能编译脚本"
    cd "$WORK_DIR"
    
    if (Test-Path "smart_build.bat") {
        .\smart_build.bat
        $BUILD_RESULT = $LASTEXITCODE
    } elseif (Test-Path "autobuild_x86_debug.bat") {
        # 备用：使用传统编译方式
        .\autobuild_x86_debug.bat
        $BUILD_RESULT = $LASTEXITCODE
        
        # 如果失败，尝试关闭进程并重编译
        if ($BUILD_RESULT -ne 0) {
            Write-Host "编译失败，尝试关闭可能的进程占用..."
            taskkill /F /IM PortMaster.exe 2>$null
            Start-Sleep -Seconds 2
            Write-Host "重新编译..."
            .\autobuild_x86_debug.bat
            $BUILD_RESULT = $LASTEXITCODE
        }
    } else {
        Write-Error "未找到编译脚本"
        $BUILD_RESULT = 1
    }
fi
```

**智能编译系统特性：**

- 🎯 **自动检测进程占用**：识别LNK1104错误并自动关闭PortMaster.exe进程
- 🔄 **自动重试机制**：进程关闭后自动重新编译
- 📊 **详细状态报告**：显示编译过程和错误处理步骤
- 🛡️ **故障保护**：如果自动处理失败，显示完整错误信息
- 📝 **日志记录**：保存编译日志用于问题诊断

**编译质量要求：**

- 必须达到 **0 error 0 warning** 标准
- 智能编译脚本自动验证编译质量
- 自动处理常见的编译阻塞问题（进程占用、文件锁定等）
- 需在聊天中展示关键编译日志片段（包含 "0 error、0 warning" 确认信息）
- 编译成功后立即更新进度文档

#### 6. 版本控制与推送（跨环境自适应，优化流程）

**检查变更状态（跨环境兼容）：**

```bash
# 检查是否有变更
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    CHANGES=$(git status --porcelain)
else
    # PowerShell环境
    CHANGES=$(git status --porcelain)
fi

if [[ -z "$CHANGES" ]]; then
    echo "无变更，无需提交"
    exit 0
fi
```

**统一提交策略（代码+文档一次性提交，跨环境兼容）：**

```bash
# 暂存所有变更（包括代码文件和文档文件）
git add -A

# 智能生成提交信息（优先级：代码变更 > 文档变更）
# 格式：类型: 简述修改内容（不超过50字符）
# 代码变更示例：fix: 修复可靠传输模式按钮状态异常问题
# 文档变更示例：docs: 更新第八轮修订工作记录和流程优化
# 混合变更示例：fix: 修复传输状态判断逻辑并更新技术文档
git commit -m "类型: <根据实际修改内容自动生成的简练中文描述>"

# 推送到远程仓库（跨环境自适应，利用localbackup映射）
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # WSL环境：推送到PortMaster远程（通过localbackup映射自动适配路径）
    git push PortMaster HEAD
  
    # 推送到backup远程（工具脚本配置的备份仓库）
    git remote | grep -q backup && git push backup HEAD
else
    # PowerShell环境：检查并推送到可用的远程仓库
    $remotes = git remote
    if ($remotes -contains "PortMaster") {
        git push PortMaster HEAD
    }
    if ($remotes -contains "backup") {
        git push backup HEAD
    }
fi

# 注：如已安装GitBackup工具的post-commit钩子，上述推送可能自动执行
```

**提交信息规范：**

- **格式**：`类型: 简述修改内容`
- **类型枚举**：`feat`（新功能）、`fix`（修复）、`docs`（文档更新）、`refactor`（重构）、`test`（测试）、`chore`（维护）
- **描述要求**：简体中文，不超过50字符，准确概括本次变更核心内容
- **自动生成**：基于 `git diff --name-only` 和 `git status` 分析文件变更，智能生成描述

#### 7. 存档标签生成（推荐）

```bash
# 生成时间戳标签（推送到主要远程）
tag="save-$(date +%Y%m%d-%H%M%S)"
git tag "$tag"
git push PortMaster --tags
```

#### 8. 工作汇报

每次完成工作流程后，必须提供：

- 最新 commit ID 和说明
- 远程推送结果（backup/origin 状态）
- 编译成功的关键日志段落
- 如有未能自动修复的问题，列出详细清单与建议修复方案

### 跨环境配置与适配

#### 路径处理规范（跨环境兼容）

**WSL环境：**

```bash
# WSL工作目录
WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"

# Windows路径（用于cmd.exe调用）
WIN_PATH="C:\Users\huangl\Desktop\PortMaster"

# Git远程仓库路径（已适配WSL）
BACKUP_REPO="/mnt/d/GitBackups/PortMaster.git"
```

**PowerShell环境：**

```powershell
# PowerShell工作目录
$WORK_DIR = "C:\Users\huangl\Desktop\PortMaster"

# Git远程仓库路径
$BACKUP_REPO = "D:\GitBackups\PortMaster.git"
```

#### 跨平台工具使用策略

**WSL环境工具选择：**

```bash
# Windows命令执行（编译脚本）
cmd.exe /c "cd /d \"$WIN_PATH\" && command.bat"

# Linux命令使用（Git、文本处理）
git status
grep pattern file
sed 's/old/new/' file
```

**PowerShell环境工具选择：**

```powershell
# 直接调用Windows工具
& "C:\path\to\tool.exe" args

# PowerShell内置命令
Get-Content file | Select-String pattern
(Get-Content file) -replace 'old','new' | Set-Content file
```

### 异常处理策略（跨环境自适应）

#### 编译失败处理（跨环境兼容）

```bash
# 编译失败时的诊断步骤（跨环境适配）
echo "编译失败，开始诊断..."

if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # WSL环境诊断
    echo "WSL环境诊断："
  
    # 检查Visual Studio环境
    cmd.exe /c "where devenv.exe" 2>/dev/null || echo "Visual Studio未找到"
  
    # 检查项目文件完整性
    ls -la *.sln *.vcxproj 2>/dev/null || echo "项目文件缺失"
  
    # 检查路径权限
    chmod +x *.bat 2>/dev/null || echo "权限设置完成"
else
    # PowerShell环境诊断
    echo "PowerShell环境诊断："
  
    # 检查Visual Studio环境
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
    if (!(Test-Path $vsPath)) {
        Write-Warning "Visual Studio 2022未找到，检查其他版本..."
        Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" -ErrorAction SilentlyContinue
    }
  
    # 检查项目文件完整性
    if (!(Test-Path "*.sln") -or !(Test-Path "*.vcxproj")) {
        Write-Warning "项目文件可能缺失"
    }
fi

# 输出详细错误信息
echo "请检查编译输出中的具体错误信息"
```

**编译失败处理原则：**

- **不允许提交或推送** 编译失败的代码
- 返回详细错误信息，包含环境特有的路径转换错误
- 尝试自动修复常见问题（路径分隔符、编码问题）
- 如无法自动修复，提供手动修复建议

#### 合并冲突处理（跨环境兼容）

```bash
# 检测合并冲突（跨环境适配）
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # WSL环境冲突检测
    git status | grep -q "both modified" && {
        echo "检测到合并冲突，需要手动解决"
        git status --porcelain | grep "^UU"
        echo "请解决冲突后重新提交"
        exit 1
    }
else
    # PowerShell环境冲突检测
    $conflictFiles = git status --porcelain | Where-Object { $_ -match "^UU" }
    if ($conflictFiles) {
        Write-Warning "检测到合并冲突，需要手动解决："
        $conflictFiles
        Write-Host "请解决冲突后重新提交"
        exit 1
    }
fi
```

**合并冲突处理原则：**

- 立即停止工作流程
- 明确指出冲突文件位置
- 提供解决方案：
  - 执行 `git merge --abort` 回退
  - 或提供手动解决冲突的具体步骤

#### 环境特有问题处理

**WSL特有问题处理：**

```bash
# WSL环境特殊处理
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    # 路径权限问题
    chmod +x *.bat 2>/dev/null || echo "权限设置完成"
  
    # Windows/Linux换行符问题
    dos2unix *.cpp *.h 2>/dev/null || echo "换行符转换完成"
  
    # 文件系统同步问题
    sync && echo "文件系统同步完成"
  
    # 路径转换验证
    echo "WSL路径: $WORK_DIR"
    echo "Windows路径: $WIN_PATH"
fi
```

**PowerShell特有问题处理：**

```powershell
# PowerShell环境特殊处理
if ($env:CURRENT_ENV -eq "PowerShell") {
    # 执行策略问题
    $policy = Get-ExecutionPolicy
    if ($policy -eq "Restricted") {
        Write-Warning "执行策略受限，可能影响脚本执行"
        Write-Host "建议运行：Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser"
    }
  
    # 路径长度问题
    $currentPath = Get-Location
    if ($currentPath.Path.Length -gt 200) {
        Write-Warning "当前路径过长，可能导致编译问题"
    }
  
    # 编码问题检查
    Write-Host "检查文件编码..."
    Get-ChildItem -Filter "*.cpp" | ForEach-Object {
        $content = Get-Content $_.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        if (!$content) {
            Write-Warning "文件 $($_.Name) 可能存在编码问题"
        }
    }
}
```

**环境问题处理要点：**

- **路径转换错误**：自动检测并转换Windows/WSL路径格式
- **文件权限问题**：使用 `chmod +x`修复执行权限（WSL）
- **编码问题**：确保UTF-8编码在不同环境间正确转换
- **Git远程路径**：验证挂载路径可访问性
- **执行策略**：检查PowerShell执行策略限制

### 质量保证原则

- **零容忍政策**：绝不允许带有编译错误或警告的代码进入版本库
- **最小变更原则**：仅修改实现目标所必需的文件
- **完整验证**：每次变更都必须通过完整的编译验证流程
- **进度同步**：代码变更与文档更新保持同步，确保项目状态可追踪

### 环境验证清单（跨环境自适应）

#### 跨环境检查脚本

```bash
# 跨环境验证脚本
echo "=== 跨环境验证开始 ==="

# 环境检测
if [[ "$CURRENT_ENV" == "WSL" ]]; then
    echo "当前环境: WSL (Windows Subsystem for Linux)"
  
    # WSL特有验证
    echo "--- WSL环境验证 ---"
  
    # 1. 工作目录验证
    echo "工作目录: $(pwd)"
    ls -la PortMaster.sln 2>/dev/null && echo "✅ 项目文件存在" || echo "❌ 项目文件缺失"
  
    # 2. Git配置验证
    git remote -v && echo "✅ Git远程仓库配置正常" || echo "❌ Git配置异常"
  
    # 3. Windows工具链验证
    cmd.exe /c "where devenv.exe" 2>/dev/null && echo "✅ Visual Studio可访问" || echo "❌ Visual Studio不可访问"
  
    # 4. 编译脚本验证
    ls -la autobuild*.bat 2>/dev/null && echo "✅ 编译脚本存在" || echo "❌ 编译脚本缺失"
  
    # 5. 文件权限验证
    [[ -x autobuild_x86_debug.bat ]] && echo "✅ 编译脚本可执行" || echo "❌ 编译脚本权限不足"
  
    # 6. 路径转换验证
    WIN_PATH=$(wslpath -w "$(pwd)")
    echo "WSL路径: $(pwd)"
    echo "Windows路径: $WIN_PATH"
  
else
    echo "当前环境: PowerShell (Windows Native)"
  
    # PowerShell特有验证
    echo "--- PowerShell环境验证 ---"
  
    # 1. 工作目录验证
    $currentDir = Get-Location
    Write-Host "工作目录: $currentDir"
    if (Test-Path "PortMaster.sln") {
        Write-Host "✅ 项目文件存在" -ForegroundColor Green
    } else {
        Write-Host "❌ 项目文件缺失" -ForegroundColor Red
    }
  
    # 2. Git配置验证
    try {
        $remotes = git remote -v
        if ($remotes) {
            Write-Host "✅ Git远程仓库配置正常" -ForegroundColor Green
        }
    } catch {
        Write-Host "❌ Git配置异常" -ForegroundColor Red
    }
  
    # 3. Visual Studio验证
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
    if (Test-Path $vsPath) {
        Write-Host "✅ Visual Studio 2022可访问" -ForegroundColor Green
    } else {
        Write-Host "❌ Visual Studio 2022不可访问，检查其他版本..." -ForegroundColor Yellow
        $vsInstalls = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" -ErrorAction SilentlyContinue
        if ($vsInstalls) {
            Write-Host "发现Visual Studio安装: $($vsInstalls.Name -join ', ')" -ForegroundColor Yellow
        }
    }
  
    # 4. 编译脚本验证
    $buildScripts = Get-ChildItem "autobuild*.bat" -ErrorAction SilentlyContinue
    if ($buildScripts) {
        Write-Host "✅ 编译脚本存在: $($buildScripts.Name -join ', ')" -ForegroundColor Green
    } else {
        Write-Host "❌ 编译脚本缺失" -ForegroundColor Red
    }
  
    # 5. 执行策略验证
    $policy = Get-ExecutionPolicy
    if ($policy -ne "Restricted") {
        Write-Host "✅ PowerShell执行策略允许脚本运行: $policy" -ForegroundColor Green
    } else {
        Write-Host "❌ PowerShell执行策略受限: $policy" -ForegroundColor Red
        Write-Host "建议运行: Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser" -ForegroundColor Yellow
    }
fi

echo "=== 验证完成 ==="
```

#### 环境要求清单

执行工作流程前，必须确认以下环境要求：

**WSL环境要求：**

- [X] WSL2环境正常工作
- [X] `/mnt/c/Users/huangl/Desktop/PortMaster` 路径可访问
- [X] `cmd.exe` 可正常调用
- [X] Git远程仓库 `/mnt/d/GitBackups/PortMaster.git` 可访问
- [X] 编译脚本 `autobuild_x86_debug.bat` 和 `autobuild.bat` 存在

**PowerShell环境要求：**

- [X] Windows 7-11 系统
- [X] `C:\Users\huangl\Desktop\PortMaster` 路径可访问
- [X] Visual Studio 2022 (Community/Professional/Enterprise) 已安装
- [X] PowerShell执行策略允许脚本运行
- [X] Git远程仓库 `D:\GitBackups\PortMaster.git` 可访问

## 修订记录管理

### 修订记录文件组织

具体的修订记录已迁移到独立文件中，便于管理和查阅：

- `第六轮修订工作记录.md` - 传输状态管理优化
- `第七轮修订工作记录.md` - 数据显示与传输控制综合修订
- `第八轮修订工作记录.md` - 工作流程优化
- `第九轮修订工作记录.md` - 可靠传输模式切换状态重置修复

### 技术调试经验总结

#### 状态管理函数调试方法

1. **定位症状函数**: 从UI异常行为反推到状态判断函数
2. **状态枚举分析**: 检查所有状态枚举值的处理逻辑
3. **边界状态测试**: 重点关注完成、失败等边界状态处理
4. **日志验证**: 通过编译运行验证状态转换正确性

#### 多层状态同步模式

- **UI层状态**: `TransmissionState::TRANSMITTING/PAUSED`
- **协议层状态**: `RELIABLE_STARTING/SENDING/ENDING/RECEIVING/DONE/FAILED`
- **同步策略**: 任一层面活跃即认为传输活跃，但完成状态强制非活跃

### 持续改进建议

#### 预防性修复检查点

- 每次修改状态管理相关函数后，检查所有状态枚举值的处理逻辑
- 重点验证边界状态（完成、失败、超时）的UI同步机制
- 确保状态转换的原子性和一致性

#### 文档同步强化

- 修订文档与代码变更保持实时同步
- 建立标准的问题分析→修复→验证→记录工作循环
- 每轮修订后更新独立的修订记录文件

```

```
