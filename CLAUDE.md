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

## 自动执行工作流程

### 目标设定
工作目录：C:\Users\huangl\Desktop\PortMaster
核心目标：修改代码并确保编译 0 error / 0 warning，之后自动版本化与远程推送

### 工作流程步骤（必须按顺序执行）

#### 1. 环境准备与同步
```powershell
cd C:\Users\huangl\Desktop\PortMaster
git pull --rebase 2>$null
```

#### 2. 代码修改规范
- 仅修改必要文件，保持最小变更原则
- **严禁提交以下目录**：`.vs/`、`bin/`、`obj/`、`Debug/`、`Release/` 等被 `.gitignore` 忽略的目录
- 遵循现有代码标准：UTF-8 编码、中文注释、MFC 静态链接

#### 3. 编译验证流程
**首选编译命令：**
```batch
.\autobuild_x86_debug.bat
```

**备用编译命令：**
```batch
.\autobuild.bat
```

**编译要求：**
- 必须达到 **0 error 0 warning** 标准
- 如出现任何 error 或 warning，必须逐一修复后重新编译
- 需在聊天中展示关键编译日志片段（包含 "0 error、0 warning" 确认信息）

#### 4. 版本控制与推送
**检查变更状态：**
- 若 `git status` 显示无变更，回复 "无变更，无需提交" 并结束流程

**提交变更（有文件变更时）：**
```powershell
git add -A

# 自动生成简练的简体中文提交信息，避免弹出编辑器
# 格式：类型: 简述修改内容（不超过50字符）
# 示例：feat: 添加串口通信模块, fix: 修复CRC校验错误, docs: 更新README文档
git commit -m "类型: <根据实际修改内容自动生成的简练中文描述>"

# 推送到备份仓库
git push backup HEAD

# 推送到主仓库（如果存在）
git remote | findstr origin && git push origin HEAD
```

**提交信息规范：**
- **格式**：`类型: 简述修改内容`
- **类型枚举**：`feat`（新功能）、`fix`（修复）、`docs`（文档）、`refactor`（重构）、`test`（测试）、`chore`（维护）
- **描述要求**：简体中文，不超过50字符，准确概括本次变更核心内容
- **自动生成**：基于 `git diff --name-only` 和 `git status` 分析文件变更，智能生成描述

#### 5. 存档标签生成（推荐）
```powershell
$tag = "save-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
git tag $tag
git push --tags
```

#### 6. 工作汇报
每次完成工作流程后，必须提供：
- 最新 commit ID 和说明
- 远程推送结果（backup/origin 状态）
- 编译成功的关键日志段落
- 如有未能自动修复的问题，列出详细清单与建议修复方案

### 异常处理策略

#### 编译失败处理
- **不允许提交或推送** 编译失败的代码
- 返回详细错误信息
- 尝试自动修复常见问题
- 如无法自动修复，提供手动修复建议

#### 合并冲突处理
- 立即停止工作流程
- 明确指出冲突文件位置
- 提供解决方案：
  - 执行 `git merge --abort` 回退
  - 或提供手动解决冲突的具体步骤

### 质量保证原则
- **零容忍政策**：绝不允许带有编译错误或警告的代码进入版本库
- **最小变更原则**：仅修改实现目标所必需的文件
- **完整验证**：每次变更都必须通过完整的编译验证流程