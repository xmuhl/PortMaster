# PortMaster 全量源码审查报告

- 日期：2025-09-16
- 审查范围：仓库内全部 `.cpp`、`.h`、`.rc` 源文件（含 Common、Protocol、Transport、UI、测试及资源模块）
- 审查方式：逐文件分段阅读并结合静态分析，记录高风险/中风险缺陷与结构性隐患。

## 总体结论

代码量大且模块化清晰，但仍存在若干关键实现错误与大量未完成的功能占位（TODO）。部分缺陷会直接导致主要功能无法使用，必须优先修复；其余则影响稳定性、可维护性或扩展性。

## 主要问题

1. **[高] TCP 传输读取路径丢失数据** — `Transport/TcpTransport.cpp:715-744` 在异步回调中把收到的数据写入局部静态 `s_internalBuffer`，却从未同步回 `m_readBuffer`；`Read()` 仍从空的 `m_readBuffer` 读取，导致上层永远读不到数据且静态缓冲无限增长。
2. **[高] 协议检测会吞掉真实业务数据** — `Protocol/ProtocolManager.cpp:246-269` 直接调用 `transport->Read(buffer, 256)` 判定协议类型，却不把读出的字节放回缓冲，首次探测后业务层将永久丢失这批数据。
3. **[高] 串口配置字段映射错误** — `Protocol/ProtocolManager.cpp:340-346` 创建串口传输时只设置 `TransportConfig.portName`，而 `SerialTransport::Open()` 实际使用 `config.ipAddress` 作为串口名，默认值“127.0.0.1”会导致串口始终打开失败。
4. **[高] 默认串口配置未清空 IP 字段** — `Common/ConfigManager.cpp:482-499` 返回串口默认配置时未把 `ipAddress` 置空，仍保留构造函数默认值“127.0.0.1”，进一步放大上一问题，在任何未显式配置情况下串口均不可用。
5. **[中] JSON 读取失败后丢失所有默认项** — `Common/ConfigManager.cpp:277-305` 在解析失败时把 `m_config` 置为空对象，却不重新调用 `SetDefaultValues()`，使得首次运行或损坏的配置文件落入“无默认值”状态，随后保存会生成空配置。

## 次要 / 结构性问题

- `Transport/TcpTransport.cpp` 中的静态缓冲还会无限累积，占用内存。
- `Common/TransportManager.cpp` 仅支持串口与环回类型，其他输运类（TCP/UDP/LPT/USB）未接入 UI 与回调链路，存在大量 TODO。
- 多数模块（IOWorker、ReliableChannel、ProtocolManager 等）保留了异常处理占位或未实现的测试钩子，需要补齐以保证稳定性。
- UI 相关代码（`PortMasterDlg.*`）包含大量未实现的交互（窗口自适应、日志滚动），应与功能开发同步。

## 建议

1. 修复上述高风险问题后，编写覆盖串口/TCP 基线流程的自动化测试（可放在 `tests/` 目录）。
2. 完善 `ConfigManager` 的错误恢复逻辑，确保解析失败时恢复默认配置；同时把串口相关字段拆分为真正的串口名字段。
3. 统一 `ProtocolManager` 与各传输层的配置映射，明确 DeviceInfo -> TransportConfig 的转换逻辑。
4. 梳理并分阶段消化仓库内的 TODO/TBD 项，避免关键路径长期未实现。
5. 引入静态分析与持续集成（如 `cppcheck`/`clang-tidy`）辅助回归检查。

