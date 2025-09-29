# PortMaster 开发计划

## 1. 项目概述

- **项目定位**：面向串口、并口、USB 打印、网络打印、回路测试的多通道数据收发与可靠传输工具。
- **交付目标**：单一静态链接 EXE、完整设计文档、核心源码、资源文件、构建脚本、可靠协议说明、测试指引、风险清单。
- **开发准则**：全部中文内容、文件编码 UTF-8（含 BOM）、遵循 CLAUDE.md 工作流、首选 `autobuild_x86_debug.bat` 并确保 0 error / 0 warning。

## 2. 总体架构

| 层级 | 职责 |
| ---- | ---- |
| UI 表现层 | MFC 主界面、参数配置、状态展示、交互操作 |
| 应用服务层 | 配置管理、日志记录、任务调度、启动画面控制 |
| 通信核心层 | Transport 抽象、可靠信道、协议状态机、缓冲与统计 |
| 系统支撑层 | Win32 API 封装、线程与同步、资源加载、JSON 序列化 |
| 构建与工具 | 自动构建脚本、日志验证、配置迁移工具 |

## 3. 功能模块划分

- `AppBootstrap`：程序入口、Splash、全局异常、单实例。
- `MainFrame & Views`：主框架、停靠面板、菜单与工具栏。
- `PortSelector`：端口枚举与占用检测。
- `SerialTransport` / `ParallelTransport` / `UsbPrintTransport` / `NetworkPrintTransport`：各类端口实现，遵循 `ITransport` 接口。
- `LoopbackTester`：回路测试模式与统计。
- `ReliableChannel`：可靠协议会话、窗口与重传管理。
- `PayloadProcessor`：文本/十六进制转换、分包重组。
- `TaskOrchestrator`：发送任务队列、暂停/恢复/中断控制。
- `LogCenter`：实时日志、过滤、导出。
- `ConfigStore`：JSON 配置读写、默认值与容错。
- `ResourceManager`：ICO/PNG 资源嵌入与加载。
- `TestingToolkit`：手动测试脚本模板、日志收集助手。

## 4. 核心接口设计

### 4.1 ITransport

```cpp
class ITransport {
public:
    virtual bool Open(const PortConfig& cfg, TransportContext& ctx) = 0;
    virtual void Close() = 0;
    virtual bool Send(const BufferView& data) = 0;
    virtual bool Receive(Buffer& out, DWORD timeout) = 0;
    virtual TransportStatus QueryStatus() = 0;
    virtual ~ITransport() = default;
};
```

### 4.2 ReliableChannel

- `AttachTransport(std::shared_ptr<ITransport>)`
- `Configure(const ReliableConfig&)`
- `StartSession(const SessionMeta&)`
- `ReliableEventStep Pump(const BufferView& payload)`
- `ReliableStats GetStats()`
- 状态机：`Idle → Handshake → Transfer → Drain → Complete/Abort`。

### 4.3 TaskOrchestrator / ConfigStore / LogCenter

- `TaskOrchestrator`：维护任务队列、调度线程、事件回调。
- `ConfigStore`：`bool Load(AppConfig&)`、`bool Save(const AppConfig&)`，失败自动回退 `%LOCALAPPDATA%`。
- `LogCenter`：追加日志、导出、级别过滤与上下文关联。

## 5. 可靠协议概要

- **启动 TLV**：`START(0x01)`，包含版本、会话 ID、端口标识、文件摘要、长度、校验模式、窗口大小、重传策略。
- **帧结构**：头部（魔数、类型、序号、长度）+ 负载 + `CRC32`。
- **控制帧**：`ACK`、`NAK`、`HEARTBEAT`、`EOT`。
- **窗口与超时**：默认窗口 4 帧、初始超时 500ms，指数退避至 2s；回路模式可降级为窗口 1。
- **错误处理**：超时重传、快速重传、CRC 失败计数与告警。
- **扩展钩子**：可插入加密 TLV、压缩标记。

## 6. 数据与配置模型

- `AppConfig`：端口列表、网络配置、UI 状态、回路统计、可靠默认值。
- `LogEntry`：时间戳、级别、模块、端口、消息、任务 ID。
- 任务描述：`taskId`、来源（手动/文件/回路）、`transportType`、可靠开关、校验信息。

## 7. UI 结构

- 左侧连接面板：端口类型与参数、占用检测、可靠模式、连接/断开。
- 右上发送区：文本/十六进制视图、文件拖放、发送/停止/清空/文件选择。
- 右下接收区：滚动显示、Hex/文本同步、清除/复制/保存。
- 底部日志：端口状态、速率、回路轮次、校验结果、导出按钮。
- 菜单/工具栏：配置导入导出、测试向导、日志过滤、帮助关于。

## 8. 迭代规划

1. **迭代 0（1 周）**：项目模板、资源接入、构建脚本验证、修订流程落地。
2. **迭代 1（2 周）**：Transport 抽象、串口实现、可靠信道 MVP、任务调度框架。
3. **迭代 2（2 周）**：并口/USB/网络打印、配置管理、日志中心、基础 UI。
4. **迭代 3（2 周）**：Hex 视图、拖放、暂停/恢复、统计面板、回路测试、可靠优化。
5. **迭代 4（1 周）**：性能与稳定性、错误提示、配置容错、发布脚本、文档审校。

## 9. 角色与任务分工

- 项目经理/架构负责人：进度跟踪、风险管理、代码审查。
- 通信核心工程师：Transport、ReliableChannel、任务调度。
- UI/MFC 工程师：界面布局、交互逻辑、资源集成。
- 平台工具工程师：构建脚本、配置存储、日志工具。
- 测试工程师：测试计划、场景执行、日志核验、构建验证。
- 文档工程师：设计文档、修订记录、用户手册、测试报告。

## 10. 工作基线与检查点

- 需求冻结：本计划为基线，变更需走变更控制。
- 周例会：同步进度、风险、阻塞，更新 `修订工作记录`。
- 编码规范：中文注释、PascalCase/CamelCase、最少必要注释。
- 代码审查：PR 需双人确认（架构负责人+模块负责人）。
- 构建门禁：合并前执行首选脚本并核对 `msbuild_*.log`。
- 质量门槛：可靠传输通过、各端口连通、Win7/Win10 兼容验证。

## 11. 测试策略

- 手动矩阵：串口直通/可靠、并口、USB、网络打印（RAW9100/LPR/IPP）、回路直通/可靠。
- 回归检查：配置保存、窗口状态恢复、日志导出、拖放大文件、异常提示。
- 性能测试：串口 115200bps 长文件、网络打印延迟模拟。
- 异常场景：端口占用、设备拔出、网络中断、CRC 失败、配置损坏重建。
- 日志核验：首尾记录、异常捕获、可靠统计。
- 验收标准：全部测试通过、日志零异常、构建零告警、UI 符合设计。

## 12. 风险与应对

- 多端口 API 兼容：抽象封装、列出 Win7/Win10 差异。
- 可靠协议复杂：先交付小窗口 MVP，保留详尽调试日志。
- 资源编码：统一 UTF-8（BOM），设置 VS 自动转换。
- 并口/USB 权限：管理员提示、失败回退策略。
- 网络协议差异：分层实现 RAW/LPR/IPP，提供适配接口。
- 测试环境不足：提前预约设备，补充虚拟回路工具。

## 13. 文档与交付

- 设计说明书：记录架构、接口、协议、UI、边界。
- 接口参考：生成 API 摘要（Doxygen/Markdown）。
- 配置手册：JSON 字段说明与示例。
- 测试指引：步骤、判定标准、记录模板。
- 风险日志：迭代更新，纳入收尾报告。
- 交付包：源码、资源、构建日志、测试报告、用户手册、修订记录。

## 14. 后续扩展建议

- 脚本化回路测试，支持批量执行与统计导出。
- Transport 插件机制，引入第三方扩展。
- 可选数据加密与压缩，满足跨网段安全需求。
- 多线程/批量传输增强，提升吞吐。
- 远程监控与告警面板，实时推送设备状态。
