# PortMaster 项目完整文件索引清单

生成时间: 2025-10-10

## 项目概述

PortMaster 是一个基于 MFC 的 Windows 串口通信工具，采用分层架构设计，支持多种传输协议和可靠传输机制。

## 架构概览

```
Application Layer (MFC UI)
    ↓
Protocol Layer (Reliable Channel + Frame Codec)
    ↓
Transport Layer (ITransport implementations)
    ↓
Hardware/OS Layer (Serial/LPT/Network/USB)
```

## 核心源代码文件

### 1. 主要应用程序文件 (src/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `src/PortMaster.cpp` | cpp | 主应用程序入口点 |
| `src/PortMaster.h` | h | 主应用程序头文件 |
| `src/PortMasterDlg.cpp` | cpp | 主对话框实现 |
| `src/PortMasterDlg.h` | h | 主对话框头文件 |
| `src/pch.cpp` | cpp | 预编译头文件实现 |
| `src/ButtonStateManager.cpp` | cpp | 按钮状态管理器 |
| `src/ButtonStateManager.h` | h | 按钮状态管理器头文件 |
| `src/ThreadSafeProgressManager.cpp` | cpp | 线程安全进度管理器 |
| `src/ThreadSafeProgressManager.h` | h | 线程安全进度管理器头文件 |
| `src/ThreadSafeUIUpdater.cpp` | cpp | 线程安全UI更新器 |
| `src/ThreadSafeUIUpdater.h` | h | 线程安全UI更新器头文件 |
| `src/TransmissionStateManager.cpp` | cpp | 传输状态管理器 |
| `src/TransmissionStateManager.h` | h | 传输状态管理器头文件 |
| `src/TransmissionTask.cpp` | cpp | 传输任务实现 |
| `src/TransmissionTask.h` | h | 传输任务头文件 |
| `src/UIStateManager.cpp` | cpp | UI状态管理器 |
| `src/UIStateManager.h` | h | UI状态管理器头文件 |

### 2. 传输层实现 (Transport/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `Transport/ITransport.h` | h | 传输接口定义 |
| `Transport/LoopbackTransport.cpp` | cpp | 环回传输实现 |
| `Transport/LoopbackTransport.h` | h | 环回传输头文件 |
| `Transport/NetworkPrintTransport.cpp` | cpp | 网络打印传输实现 |
| `Transport/NetworkPrintTransport.h` | h | 网络打印传输头文件 |
| `Transport/ParallelTransport.cpp` | cpp | 并口传输实现 |
| `Transport/ParallelTransport.h` | h | 并口传输头文件 |
| `Transport/SerialTransport.cpp` | cpp | 串口传输实现 |
| `Transport/SerialTransport.h` | h | 串口传输头文件 |
| `Transport/TransportFactory.cpp` | cpp | 传输工厂类 |
| `Transport/UsbPrintTransport.cpp` | cpp | USB打印传输实现 |
| `Transport/UsbPrintTransport.h` | h | USB打印传输头文件 |

### 3. 协议层实现 (Protocol/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `Protocol/FrameCodec.cpp` | cpp | 帧编解码器实现 |
| `Protocol/FrameCodec.h` | h | 帧编解码器头文件 |
| `Protocol/ReliableChannel.cpp` | cpp | 可靠通道实现 |
| `Protocol/ReliableChannel.h` | h | 可靠通道头文件 |

### 4. 公共组件 (Common/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `Common/CommonTypes.h` | h | 公共类型定义 |
| `Common/ConfigStore.cpp` | cpp | 配置存储实现 |
| `Common/ConfigStore.h` | h | 配置存储头文件 |
| `Common/RingBuffer.h` | h | 环形缓冲区实现 |

### 5. 头文件目录 (include/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `include/framework.h` | h | MFC框架头文件 |
| `include/pch.h` | h | 预编译头文件 |
| `include/Resource.h` | h | 资源定义头文件 |
| `include/targetver.h` | h | 目标版本头文件 |

### 6. 资源文件 (resources/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `resources/PortMaster.aps` | aps | 资源编译脚本 |
| `resources/PortMaster.rc` | rc | 主资源文件 |
| `resources/RCa13648` | - | 资源临时文件 |
| `resources/resource.h` | h | 资源头文件 |
| `resources/res/PortMaster.ico` | ico | 应用程序图标 |
| `resources/res/PortMaster.rc2` | rc2 | 额外资源文件 |
| `resources/image/PortMaster/1758385786138.png` | png | 界面截图 |

### 7. 自动测试文件 (AutoTest/)

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `AutoTest/main.cpp` | cpp | 测试主程序 |
| `AutoTest/DiagnoseFileTransfer.cpp` | cpp | 文件传输诊断测试 |
| `AutoTest/SimpleLoopbackTest.cpp` | cpp | 简单环回测试 |
| `AutoTest/UI_Test_Console.cpp` | cpp | UI测试控制台 |
| `AutoTest/ui_test_main.cpp` | cpp | UI测试主程序 |
| `AutoTest/ErrorRecoveryTests.h` | h | 错误恢复测试头文件 |
| `AutoTest/IntegrationTests.h` | h | 集成测试头文件 |
| `AutoTest/PerformanceTests.h` | h | 性能测试头文件 |
| `AutoTest/ProtocolUnitTests.h` | h | 协议单元测试头文件 |
| `AutoTest/RegressionTestFramework.h` | h | 回归测试框架头文件 |
| `AutoTest/StressTests.h` | h | 压力测试头文件 |
| `AutoTest/TestFramework.h` | h | 测试框架头文件 |
| `AutoTest/TransportUnitTests.h` | h | 传输单元测试头文件 |

## 项目配置文件

### 主要配置文件

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `PortMaster.sln` | sln | Visual Studio解决方案文件 |
| `PortMaster.vcxproj` | vcxproj | Visual Studio项目文件 |
| `autobuild.bat` | bat | 主编译脚本 |
| `autobuild_x86_debug.bat` | bat | x86调试编译脚本 |

### 文档文件

| 文件路径 | 类型 | 功能描述 |
|---------|------|---------|
| `CLAUDE.md` | md | Claude Code项目指南 |
| `USER_MANUAL.md` | md | 用户手册 |
| `AI_DEV_SYSTEM_EVALUATION.md` | md | AI开发系统评估报告 |
| `AUTOMATION_README.md` | md | 自动化说明文档 |
| `device_testing_guide.md` | md | 设备测试指南 |
| `GEMINI.md` | md | Gemini集成文档 |
| `PORTMASTER_CLEANUP_SUMMARY.md` | md | 项目清理总结 |
| `Protocol_Reliability_Fix_Analysis.md` | md | 协议可靠性修复分析 |
| `user_rules.md` | md | 用户规则文档 |
| `修订工作记录20251008-235358.md` | md | 修订工作记录(10月8日) |
| `修订工作记录20251010-110226.md` | md | 修订工作记录(10月10日) |

## 目录结构

### 主要目录

- `src/` - 核心应用程序源代码
- `Transport/` - 传输层实现
- `Protocol/` - 协议层实现
- `Common/` - 公共组件和工具
- `include/` - 公共头文件
- `resources/` - 资源文件
- `AutoTest/` - 自动测试代码
- `build/` - 编译输出目录
- `docs/` - 文档目录
- `scripts/` - 脚本工具目录

### 支持目录

- `archive/` - 归档文件目录
- `TestReliable/` - 可靠性测试目录
- `static_analysis_reports/` - 静态分析报告
- `automation_reports/` - 自动化报告
- `PortMaster-AI-Dev-System/` - AI开发系统

## 技术栈

- **编程语言**: C++
- **UI框架**: MFC (Microsoft Foundation Classes)
- **编译器**: Visual Studio 2022 (v143工具集)
- **目标平台**: Windows 7-11 (x86/x64)
- **编码格式**: UTF-8 with BOM
- **依赖管理**: 静态链接 (/MT运行时)

## 关键特性

1. **分层架构设计**: 清晰的传输层、协议层、应用层分离
2. **多传输方式支持**: 串口、并口、USB、网络等多种传输方式
3. **可靠传输协议**: 自定义可靠传输协议，支持CRC校验和重传机制
4. **线程安全设计**: 多线程环境下的安全操作
5. **模块化组件**: 可插拔的传输层和协议层实现
6. **全面的测试覆盖**: 单元测试、集成测试、性能测试等

## 编译说明

### 主要编译命令

1. **推荐编译**: `autobuild_x86_debug.bat` (x86 Debug配置，0错误0警告)
2. **完整编译**: `autobuild.bat` (全配置编译)
3. **编译要求**: Visual Studio 2022，v143工具集

### 编译目标

- 静态链接MFC应用程序
- 单EXE部署目标
- 零错误零警告质量标准

---

*本文档自动生成，最后更新时间: 2025-10-10*