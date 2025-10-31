# PortMaster 项目概述 (IFLOW上下文)

## 项目类型
这是一个使用Visual Studio开发的Windows桌面应用程序，基于MFC框架和C++语言。项目旨在提供一个多功能的端口通信测试工具，支持串口、并口、USB打印端口、网络打印端口和回路测试等多种传输方式。

## 核心功能
1.  **多端口支持**：提供对串口、并口、USB打印端口、网络打印端口和回路测试的统一接口。
2.  **传输模式**：
    *   **直通模式**：简单的数据透传。
    *   **可靠传输模式**：计划实现基于滑动窗口和确认重传机制的可靠数据传输协议，但当前实现不完整。
3.  **配置管理**：支持保存和加载应用程序、端口和UI的配置，但部分端口和可靠模式配置的持久化未完全实现。
4.  **文件传输**：具备发送和接收文件的功能，尤其在可靠模式下需要保证文件完整性。
5.  **UI交互**：提供图形用户界面用于端口配置、数据发送/接收显示、状态监控等。

## 技术架构
*   **语言**：C++
*   **框架**：MFC (Microsoft Foundation Class)
*   **构建系统**：MSBuild (Visual Studio)
*   **平台**：Windows (Win32)
*   **字符集**：Unicode

### 主要目录结构
*   `Common/`：通用组件，如配置存储(`ConfigStore`)、日志、数据处理服务、工具函数等。
*   `Protocol/`：传输协议相关，如可靠通道(`ReliableChannel`)和帧编解码(`FrameCodec`)。
*   `Transport/`: 各种传输方式的具体实现，如串口(`SerialTransport`)、并口(`ParallelTransport`)、USB(`UsbPrintTransport`)、网络(`NetworkPrintTransport`)和回路测试(`LoopbackTransport`)。
*   `src/`: 核心应用程序源代码，包括主窗口(`PortMasterDlg`)、UI控制器、传输协调器等。
*   `include/`: 公共头文件。
*   `resources/`: 资源文件，如图标、RC文件等。
*   `docs/`: 项目文档，包含详细的设计和修订报告。

### 关键源文件
*   `PortMaster.vcxproj`: Visual Studio项目文件，定义了构建配置、源文件列表和依赖项。
*   `Protocol/ReliableChannel.h/cpp`: 可靠传输通道的实现（当前功能不完整）。
*   `Common/ConfigStore.h/cpp`: 配置管理器，负责读写配置。
*   `Transport/ITransport.h`: 传输层接口定义。
*   `Transport/*Transport.h/cpp`: 各种具体传输方式的实现。
*   `src/PortMasterDlg.h/cpp`: 主对话框，是UI和逻辑的核心。

## 构建与运行
1.  **环境要求**：
    *   Windows操作系统
    *   Visual Studio (推荐2022或更新版本，项目配置为使用v143平台工具集)
    *   Windows SDK (项目配置为10.0)
2.  **构建**：
    *   打开 `PortMaster.sln` 解决方案文件。
    *   选择 `Debug` 或 `Release` 配置以及 `Win32` 平台。
    *   使用Visual Studio的"生成"菜单进行构建。
    *   输出文件将位于 `build/Debug/` 或 `build/Release/` 目录。
3.  **运行**：
    *   直接运行生成的 `PortMaster.exe` 文件。
    *   也可以通过Visual Studio直接调试运行。

## 开发现状与关键问题
根据 `docs/项目核心功能分析与修订报告.md` 的分析以及对源代码的检查，项目存在以下关键问题需要解决：

1.  **可靠传输协议未实现**：`Protocol/ReliableChannel.cpp` 中的核心逻辑（如握手、数据确认、超时重传）是空的，导致可靠模式不可用。
2.  **配置管理不完整**：`Common/ConfigStore.cpp` 未实现对并口、USB、网络端口以及可靠协议配置的完整序列化和反序列化。
3.  **部分端口实现不完整**：
    *   `Transport/ParallelTransport.cpp` 的状态查询返回假数据。
    *   `Transport/UsbPrintTransport.cpp` 的写入操作缺少完善的超时处理。
    *   `Transport/NetworkPrintTransport.cpp` 的网络协议发送逻辑被注释掉。

## 开发约定
*   **编码风格**：使用Unicode字符集，采用C++标准和MFC框架。
*   **配置管理**：使用`ConfigStore`类管理所有配置，配置格式为自定义JSON格式。
*   **日志记录**：通过`Logger`类或直接在对话框中记录日志。
*   **多线程**：在可靠传输等模块中使用了多线程和同步原语（如mutex, condition_variable）。

## 构建命令和工作流

### 主要构建过程
*   **主要构建命令**：`autobuild_x86_debug.bat` - 针对Win32 Debug配置进行了优化，强制执行零错误和零警告
*   **备用构建命令**：`autobuild.bat` - 完整配置构建（Win32/x64, Debug/Release），当主要脚本不可用时使用
*   **构建要求**：Visual Studio 2022（Community/Professional/Enterprise）与v143工具集
*   **构建输出**：静态链接的MFC应用程序，单一EXE部署目标
*   **脚本优先级**：始终优先使用`autobuild_x86_debug.bat`，如果找不到则回退到`autobuild.bat`

### 构建验证
*   所有构建必须在继续之前实现**零错误和零警告**
*   构建日志生成为`msbuild_*.log`文件用于详细分析
*   构建脚本强制执行`/p:TreatWarningsAsErrors=true`以确保质量

## 架构概述

### 核心设计模式
PortMaster遵循具有清晰分层的**分层架构**，在传输、协议和应用层之间有明确的分离：

```
应用层 (MFC UI)
    ↓
协议层 (可靠通道 + 帧编解码)
    ↓  
传输层 (ITransport 实现)
    ↓
硬件/操作系统层 (串口/LPT/网络/USB)
```

### 关键架构组件

**传输层 (`Transport/`)**
*   `ITransport` - 所有通信通道的统一接口
*   多种具体实现：串口（COM）、并口（通过Windows打印后台处理程序）、USB打印机、TCP/UDP网络、回路测试
*   每个传输处理自己的连接管理、缓冲和错误处理
*   通过回调机制支持异步I/O

**协议层 (`Protocol/)**
*   `ReliableChannel` - 在任何ITransport上实现自定义可靠传输协议
*   `FrameCodec` - 处理帧编码/解码，格式为：`0xAA55 + Type + Sequence + Length + CRC32 + Data + 0x55AA`
*   `CRC32` - IEEE 802.3标准实现（多项式0xEDB88320）
*   帧类型：START（带文件元数据）、DATA、END、ACK、NAK
*   状态机：发送方（Idle→Starting→Sending→Ending→Done），接收方（Idle→Ready→Receiving→Done）

**通用工具 (`Common/)**
*   `ConfigManager` - 基于JSON的配置持久性（优先程序目录，回退到%LOCALAPPDATA%）
*   `RingBuffer` - 自动扩展的循环缓冲区用于数据流
*   `IOWorker` - Windows异步操作的OVERLAPPED I/O完成端口工作者

### 状态管理模式
*   传输状态：`TRANSPORT_CLOSED`, `TRANSPORT_OPENING`, `TRANSPORT_OPEN`, `TRANSPORT_CLOSING`, `TRANSPORT_ERROR`
*   协议状态：`RELIABLE_IDLE`, `RELIABLE_STARTING`, `RELIABLE_SENDING`, `RELIABLE_ENDING`, `RELIABLE_READY`, `RELIABLE_RECEIVING`, `RELIABLE_DONE`, `RELIABLE_FAILED`
*   帧类型：`FRAME_START`, `FRAME_DATA`, `FRAME_END`, `FRAME_ACK`, `FRAME_NAK`

### 线程模型
*   每个传输可能生成自己的后台线程用于I/O操作
*   协议层在专用线程中运行状态机，使用条件变量同步
*   UI线程通过基于回调的事件传递保持响应性
*   使用std::atomic、std::mutex和适当的RAII模式实现线程安全操作

### 错误处理策略
*   分层错误传播：传输 → 协议 → 应用
*   协议层的CRC验证与自动NAK/重传
*   基于超时的恢复机制，具有可配置的重试限制
*   优雅降级：协议层可以禁用以直接访问传输

### 配置架构
*   多层存储：传输特定配置、协议参数、UI状态、应用程序设置
*   关机时自动持久化，具有损坏恢复功能
*   所有配置参数的默认值回退
*   支持每传输类型参数集

## 开发注意事项

### 代码标准
*   **语言**：所有代码、注释和资源均使用简体中文
*   **编码**：UTF-8 with BOM，在需要时使用`#pragma execution_character_set("utf-8")`
*   **兼容性**：支持Windows 7-11，x86和x64架构
*   **部署**：使用静态MFC链接的单个EXE（`/MT`运行时）

### 协议实现细节
*   帧大小限制：1024字节有效载荷（可通过`FrameCodec::SetMaxPayloadSize`配置）
*   滑动窗口大小：1（停止等待ARQ以简化）
*   文件传输包括元数据：文件名（UTF-8）、文件大小、修改时间
*   接收的文件使用冲突解决自动保存（数字后缀）

### 传输层扩展点
要添加新的传输类型：
1. 实现`ITransport`接口
2. 一致地处理状态转换
3. 实现异步回调模式
4. 将配置参数添加到`TransportConfig`
5. 注册到`ConfigManager`以实现持久性

### UI集成模式
*   通过组合框进行传输选择，动态端口枚举
*   双视图数据显示（十六进制和文本表示）
*   文件拖放支持传输
*   通过回调机制实时进度报告
*   应用程序启动期间的启动画面集成

## 模块拆分架构（2025-10更新）

### 概述
2025年10月完成了`PortMasterDlg`的模块拆分重构，将原有的~5000行单体类按照分层架构拆分为5个专用服务模块，显著提升了代码可维护性和可测试性。详细的拆分方案和实施记录参见 `docs/PortMasterDlg模块拆分评估.md`。

### 新增服务模块

**1. PortSessionController (`Protocol/PortSessionController.h/.cpp`)**
*   **职责**：Transport层连接管理和接收会话控制
*   **关键功能**：
    - 根据配置创建和管理5种Transport类型（Serial/Parallel/USB/Network/Loopback）
    - ReliableChannel生命周期管理
    - 接收线程管理（启动、停止、线程安全数据回调）
*   **接口**：
    - `Connect(config)` - 建立连接并创建Transport实例
    - `Disconnect()` - 断开连接并清理资源
    - `StartReceiveSession()` - 启动接收线程
    - `StopReceiveSession()` - 优雅关闭接收线程

**2. TransmissionCoordinator (`src/TransmissionCoordinator.h/.cpp`)**
*   **职责**：TransmissionTask生命周期管理和传输控制
*   **关键功能**：
    - 创建并管理传输任务（ReliableTransmissionTask vs RawTransmissionTask）
    - 自动选择可靠/直接传输模式
    - 任务控制（启动、暂停、恢复、取消）
    - 进度和完成事件转发

**3. DataPresentationService (`Common/DataPresentationService.h/.cpp`)**
*   **职责**：数据格式转换和展示（十六进制/文本/二进制检测）
*   **设计模式**：静态工具类（无状态，纯函数）
*   **关键功能**：
    - 带地址偏移和ASCII侧边栏的完整十六进制格式化
    - 解析格式化的十六进制文本
    - 智能二进制数据检测（识别控制字符和非打印字符）

**4. ReceiveCacheService (`Common/ReceiveCacheService.h/.cpp`)**
*   **职责**：线程安全的临时缓存文件操作
*   **关键功能**：
    - 内存+磁盘双保障机制（数据同时写入内存缓存和临时文件）
    - 分块循环读取（64KB块大小，避免大文件内存溢出）
    - 文件完整性验证和自动恢复
    - 并发访问控制（读写互斥、待写入队列）

**5. DialogConfigBinder (`src/DialogConfigBinder.h/.cpp`)**
*   **职责**：UI控件与ConfigStore之间的数据绑定
*   **关键功能**：
    - UI控件 → ConfigStore（保存）
    - ConfigStore → UI控件（加载）
    - 配置有效性验证

## 跨环境自动执行工作流程

### 目标设定
*   **工作目录**：`C:\Users\huangl\Desktop\PortMaster` (Windows) 或 `/mnt/c/Users/huangl/Desktop/PortMaster` (WSL)
*   **核心目标**：修改代码并确保编译 0 error / 0 warning，之后自动版本化与远程推送
*   **环境特点**：支持Windows PowerShell和WSL2环境，自动检测并适配当前运行环境

### 工作流程步骤（必须按顺序执行）

#### 0. 环境自动检测与初始化
环境检测脚本会自动适配当前运行环境，检测WSL或PowerShell环境，并设置相应的工作目录和路径。

#### 1. 创建修订工作记录文件
每轮修订工作正式开始前必须创建修订记录文件，记录修订概述、问题分析、解决方案和计划安排。

#### 2. 环境准备与同步
切换到工作目录并同步远程变更。

#### 3. 进度文档更新（优化策略）
分阶段更新文档，避免文档变更后再次推送的循环问题。

#### 4. 代码修改规范
*   仅修改必要文件，保持最小变更原则
*   **严禁提交以下目录**：`.vs/`、`bin/`、`obj/`、`Debug/`、`Release/` 等被 `.gitignore` 忽略的目录
*   遵循现有代码标准：UTF-8 编码、中文注释、MFC 静态链接

#### 5. 智能编译验证流程
*   **源码文件变更时** - 执行智能编译验证
*   **仅文档文件变更时** - 跳过编译验证
*   **自动化处理** - 检测并自动关闭占用进程
*   **效率优化** - 避免不必要的编译操作

#### 6. 版本控制与推送
检查变更状态并统一提交策略，代码和文档一次性提交。

#### 7. 存档标签生成（推荐）
生成时间戳标签并推送到主要远程仓库。

#### 8. 工作汇报
每次完成工作流程后，必须提供最新commit ID、远程推送结果、编译成功的关键日志段落等信息。

## 质量保证原则
*   **零容忍政策**：绝不允许带有编译错误或警告的代码进入版本库
*   **最小变更原则**：仅修改实现目标所必需的文件
*   **完整验证**：每次变更都必须通过完整的编译验证流程
*   **进度同步**：代码变更与文档更新保持同步，确保项目状态可追踪