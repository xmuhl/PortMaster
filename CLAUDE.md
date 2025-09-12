# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Primary Build Process
- **Build command**: `autobuild.bat` - Automatically detects VS2022 installation, builds all configurations (Win32/x64, Debug/Release) with zero errors and zero warnings enforcement
- **Build requirements**: Visual Studio 2022 (Community/Professional/Enterprise) with v143 toolset
- **Build outputs**: Static-linked MFC application, single EXE deployment target

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

## WSL环境自动执行工作流程

### 目标设定
- **工作目录**：`/mnt/c/Users/huangl/Desktop/PortMaster`
- **核心目标**：修改代码并确保编译 0 error / 0 warning，之后自动版本化与远程推送
- **环境特点**：WSL2环境下操作，兼容Windows文件系统，支持跨平台工具

### 工作流程步骤（必须按顺序执行）

#### 1. 环境准备与同步
```bash
# 切换到工作目录
cd "/mnt/c/Users/huangl/Desktop/PortMaster"

# 同步远程变更（使用Git bash语法）
git pull --rebase 2>/dev/null || echo "同步完成或无需同步"
```

#### 2. 进度文档更新（新增步骤）
**在每次代码修改前，必须更新本地记录文档：**
```bash
# 更新Code_Revision_Progress.md中的执行日志
# 记录当前任务开始时间和目标
# 格式：- ⏳ **HH:MM**: 开始[具体任务描述]
```

**文档更新要求：**
- 每个任务开始前更新执行日志
- 任务完成后标记为完成状态 ✅
- 记录编译结果和关键问题
- 保持文档与实际进度同步

#### 3. 代码修改规范
- 仅修改必要文件，保持最小变更原则
- **严禁提交以下目录**：`.vs/`、`bin/`、`obj/`、`Debug/`、`Release/` 等被 `.gitignore` 忽略的目录
- 遵循现有代码标准：UTF-8 编码、中文注释、MFC 静态链接

#### 4. 编译验证流程（WSL适配）
**首选编译命令（WSL环境）：**
```bash
# 使用cmd.exe执行Windows批处理脚本
cd "/mnt/c/Users/huangl/Desktop/PortMaster" && cmd.exe /c "autobuild_x86_debug.bat" 2>&1 | tail -20
```

**备用编译命令（WSL环境）：**
```bash
# 备用编译脚本
cd "/mnt/c/Users/huangl/Desktop/PortMaster" && cmd.exe /c "autobuild.bat" 2>&1 | tail -20
```

**编译要求：**
- 必须达到 **0 error 0 warning** 标准
- 如出现任何 error 或 warning，必须逐一修复后重新编译
- 需在聊天中展示关键编译日志片段（包含 "0 error、0 warning" 确认信息）
- 编译成功后立即更新进度文档中的编译验证历史表格

#### 5. 版本控制与推送（WSL适配）
**检查变更状态：**
```bash
git status --porcelain
```
- 若输出为空，回复 "无变更，无需提交" 并结束流程

**提交变更（有文件变更时）：**
```bash
# 暂存所有变更
git add -A

# 自动生成简练的简体中文提交信息
# 格式：类型: 简述修改内容（不超过50字符）
# 示例：feat: 添加串口通信模块, fix: 修复CRC校验错误, docs: 更新README文档
git commit -m "类型: <根据实际修改内容自动生成的简练中文描述>"

# 推送到备份仓库（WSL路径已适配）
git push backup HEAD

# 推送到主仓库（如果存在）
git remote | grep -q origin && git push origin HEAD
```

**提交信息规范：**
- **格式**：`类型: 简述修改内容`
- **类型枚举**：`feat`（新功能）、`fix`（修复）、`docs`（文档更新）、`refactor`（重构）、`test`（测试）、`chore`（维护）
- **描述要求**：简体中文，不超过50字符，准确概括本次变更核心内容
- **自动生成**：基于 `git diff --name-only` 和 `git status` 分析文件变更，智能生成描述

#### 6. 存档标签生成（推荐）
```bash
# 生成时间戳标签
tag="save-$(date +%Y%m%d-%H%M%S)"
git tag "$tag"
git push --tags
```

#### 7. 工作汇报
每次完成工作流程后，必须提供：
- 最新 commit ID 和说明
- 远程推送结果（backup/origin 状态）
- 编译成功的关键日志段落
- 进度文档更新确认
- 如有未能自动修复的问题，列出详细清单与建议修复方案

### WSL环境特殊配置

#### 路径处理规范
```bash
# WSL工作目录
WORK_DIR="/mnt/c/Users/huangl/Desktop/PortMaster"

# Windows路径（用于cmd.exe调用）
WIN_PATH="C:\Users\huangl\Desktop\PortMaster"

# Git远程仓库路径（已适配WSL）
BACKUP_REPO="/mnt/d/GitBackups/PortMaster.git"
```

#### 跨平台工具使用
```bash
# Windows命令执行（编译脚本）
cmd.exe /c "cd /d \"$WIN_PATH\" && command.bat"

# Linux命令使用（Git、文本处理）
git status
grep pattern file
sed 's/old/new/' file
```

### 异常处理策略

#### 编译失败处理
- **不允许提交或推送** 编译失败的代码
- 返回详细错误信息，包含WSL环境特有的路径转换错误
- 尝试自动修复常见问题（路径分隔符、编码问题）
- 如无法自动修复，提供手动修复建议

#### 合并冲突处理
- 立即停止工作流程
- 明确指出冲突文件位置
- 提供解决方案：
  - 执行 `git merge --abort` 回退
  - 或提供手动解决冲突的具体步骤

#### WSL特有问题处理
- **路径转换错误**：自动检测并转换Windows/WSL路径格式
- **文件权限问题**：使用`chmod +x`修复执行权限
- **编码问题**：确保UTF-8编码在WSL和Windows间正确转换
- **Git远程路径**：验证WSL挂载路径可访问性

### 质量保证原则
- **零容忍政策**：绝不允许带有编译错误或警告的代码进入版本库
- **最小变更原则**：仅修改实现目标所必需的文件
- **完整验证**：每次变更都必须通过完整的编译验证流程
- **进度同步**：代码变更与文档更新保持同步，确保项目状态可追踪

### 环境验证清单
执行工作流程前，必须确认以下环境要求：
- [x] WSL2环境正常工作
- [x] `/mnt/c/Users/huangl/Desktop/PortMaster` 路径可访问
- [x] `cmd.exe` 可正常调用
- [x] Git远程仓库 `/mnt/d/GitBackups/PortMaster.git` 可访问
- [x] 编译脚本 `autobuild_x86_debug.bat` 和 `autobuild.bat` 存在

## 第六轮修订技术总结与工作流程优化 

### 关键技术发现

#### 可靠传输状态管理模式缺陷
**问题特征：** `IsTransmissionActive()` 函数未正确处理 `RELIABLE_DONE` 状态
- **表现症状：** 传输完成后按钮仍显示"停止"状态，点击弹出"是否停止传输"对话框
- **根本原因：** 函数逻辑中 `RELIABLE_DONE` 状态被当作活跃状态处理
- **标准修复：** 明确将 `RELIABLE_DONE` 和 `RELIABLE_FAILED` 状态返回 `false`（非活跃）

```cpp
// 🔑 标准修复模式：完成状态特殊处理
if (reliableState == RELIABLE_DONE || reliableState == RELIABLE_FAILED)
{
    return false;  // 强制返回非活跃状态
}
```

#### Git远程仓库配置优化
**发现问题：** `backup` 远程仓库路径格式错误 (`/mnt/__DRIVE:~0,1:1/...`)
- **解决方案：** 使用 `PortMaster` 远程仓库 (`/mnt/d/GitBackups/PortMaster.git`)
- **推送策略：** `git push PortMaster HEAD` 替代 `git push backup HEAD`
- **验证方法：** `git remote -v` 检查远程仓库配置正确性

### 工作流程改进记录

#### 修订文档化标准
- **创建专用修订文档** (`第N轮修订工作记录.md`) 记录详细问题分析和修复过程
- **任务分解原则：** 每个修订轮次包含8个标准任务
- **进度跟踪要求：** 实时更新执行日志，记录开始/完成时间和关键结果

#### 编译验证流程优化
- **编译命令：** `cmd.exe /c "autobuild_x86_debug.bat"` (WSL环境)
- **标准要求：** 必须达到 **0 error 0 warning**
- **验证展示：** 在聊天中展示关键编译成功日志段落

#### 版本控制流程固化
- **提交规范：** `类型: 简述修改内容` (简体中文，不超过50字符)
- **推送策略：** 使用 `PortMaster` 远程仓库而非 `backup`
- **标签生成：** `save-YYYYMMDD-HHMMSS` 格式的时间戳标签
- **推送验证：** 确认commit ID和远程推送状态

### 技术调试经验总结

#### 状态管理函数调试方法
1. **定位症状函数：** 从UI异常行为反推到状态判断函数
2. **状态枚举分析：** 检查所有状态枚举值的处理逻辑
3. **边界状态测试：** 重点关注完成、失败等边界状态处理
4. **日志验证：** 通过编译运行验证状态转换正确性

#### 多层状态同步模式
- **UI层状态：** `TransmissionState::TRANSMITTING/PAUSED`
- **协议层状态：** `RELIABLE_STARTING/SENDING/ENDING/RECEIVING/DONE/FAILED`
- **同步策略：** 任一层面活跃即认为传输活跃，但完成状态强制非活跃

### 持续改进建议

#### 预防性修复检查点
- 每次修改状态管理相关函数后，检查所有状态枚举值的处理逻辑
- 重点验证边界状态（完成、失败、超时）的UI同步机制
- 确保状态转换的原子性和一致性

#### 文档同步强化
- 修订文档与代码变更保持实时同步
- 建立标准的问题分析→修复→验证→记录工作循环
- 每轮修订后更新 `Code_Revision_Progress.md` 累积经验