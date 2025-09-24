# PortMaster 项目现状评估报告（2025-09-24）

## 1. 概要
- 本报告依据《PortMaster 项目开发.md》与《PortMaster 开发计划整理.md》要求，对当前仓库源码（提交快照：2025-09-24 11:04）进行静态检查，未执行构建与运行验证。
- 分析范围覆盖 `src/`、`Transport/`、`Protocol/`、`Common/` 目录及相关文档，重点关注可靠传输、各端口驱动、配置存储与 UI 驱动逻辑。
- 本次评估以发现实现缺口、逻辑错误与安全隐患为主，未覆盖性能压测与外部接口联调。

## 2. 需求完成度检查
- **传输抽象**：`ITransport` 及串口/并口/USB/网络实现文件已经存在，但网络打印协议与高级特性仍大量占位（详见第3节）。
- **可靠通道**：`Protocol/ReliableChannel.*` 提供了发送/接收框架，但缺少文档要求的状态机（`Idle → Handshake → Transfer → Drain → Complete/Abort`，参见 `PortMaster 开发计划整理.md:57`），握手与 Drain 逻辑未实现。
- **UI 层**：`src/PortMasterDlg.cpp` 已包含主要控件绑定及与 `ReliableChannel` 的调用，但不存在文档所列 `TaskOrchestrator`、`LogCenter`、`PayloadProcessor` 等关键模块（文档出处 `PortMaster 开发计划整理.md:28-61`），功能尚未闭环。
- **工具与测试**：仓库缺少自动化测试、构建验证结果与日志采集脚本的落地实现，文档要求的 ASCII 类图、协议规格、测试指引等交付物未在仓库找到。

## 3. 主要问题清单

### 3.1 逻辑缺陷
- `Transport/SerialTransport.cpp:25` & `Transport/SerialTransport.cpp:226-248`：`m_readEvent` 使用手动复位事件，但异步读取前未调用 `ResetEvent`；一旦事件保持有信号，后续 `WaitForSingleObject` 将立即返回，造成忙轮询与假阳性读完成。
- `Protocol/ReliableChannel.cpp:1176-1288` & `Protocol/ReliableChannel.cpp:1529-1547`：发送窗口未检测槽位是否仍在使用，新序号会覆盖尚未 ACK 的帧，导致重传关联丢失并触发数据错乱。
- `Protocol/ReliableChannel.cpp:1351-1366`：可靠文件传输在发送 START/END 帧后仍复用前述有缺陷的窗口管理，无法保证完整可靠的文件边界与顺序。
- `Transport/NetworkPrintTransport.cpp:837-843` & `Transport/NetworkPrintTransport.cpp:985-993`：IPP 作业构建函数仅返回空向量，当前实现无法真正按照 RFC 2910 发送打印任务。
- `PortMaster 开发计划整理.md:57` vs `Protocol/ReliableChannel.h:41-108`：可靠通道缺失文档描述的状态机、握手与 Drain 阶段，实现与设计严重错位。

### 3.2 安全隐患
- `Transport/NetworkPrintTransport.cpp:1005-1037`：`BasicAuthenticate`/`NTLMAuthenticate`/`CertificateAuthenticate` 均为空实现却返回成功，`BuildBasicAuthHeader` 直接暴露 `user:password`，未做 Base64/TLS 保护，存在凭据被明文发送的风险。
- `Transport/NetworkPrintTransport.h:74-94` 与 `.cpp`：虽然声明了 `enableSSL/verifySSLCert` 等字段，但源码未提供 `InitializeSSL`/`SSLHandshake` 等实现，启用 HTTPS 配置不会生效，误导用户以为链路已加密。
- `Common/ConfigStore.cpp:782-833` & `Common/ConfigStore.cpp:877-965`：自研 JSON 解析通过字符串查找/截取实现，无法正确处理转义、嵌套或恶意输入，存在配置被突破或写回畸形 JSON 的风险；同时 `StringToInt` 等直接 `std::stoi`，在异常时抛出并被吞掉，可能回退到默认值而未经提示。

### 3.3 质量与交付风险
- 文档要求的 `TaskOrchestrator`、`LogCenter`、`PayloadProcessor`、`LoopbackTester`、`ResourceManager` 等核心模块在仓库中缺失（仅见于 `PortMaster 开发计划整理.md:28-61`）。
- `Protocol/ReliableChannel.cpp:11-18` 与 `src/PortMasterDlg.cpp:60-83` 将调试日志写入工作目录 `PortMaster_debug.log`，缺少滚动/大小控制，也未区分发布模式，可能泄露敏感数据并影响性能。
- `Transport/NetworkPrintTransport.cpp` 多处 TODO（IPP 构建、LPR 队列探测、重连监控）尚未落实，整体网络打印能力处于样板阶段。

## 4. 风险等级与修复建议
- **高优先级**：修复可靠通道窗口管理与事件同步问题、补齐认证/TLS 实现或临时禁用相关入口、替换 ConfigStore 手工 JSON 解析为成熟库（如 nlohmann/json）并增加异常处理。
- **中优先级**：补全 IPP/LPR 具体协议封装、实现文档规定的可靠状态机流程、增加凭据与敏感日志的保护策略。
- **低优先级**：完善日志轮换、实现文档列出的工具链与测试脚本、根据 CLAUDE 流程补全构建记录与交付文档。

## 5. 额外说明
- 本次评估未执行 `autobuild_x86_debug.bat`，建议在关键缺陷修复后进行全量构建与手动测试。
- 若需进一步验证网络打印与可靠协议，建议编写最小化自动化测试或回路对端模拟，以便快速回归。
