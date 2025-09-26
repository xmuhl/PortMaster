# PortMaster 近期检查报告（遗留问题）

生成时间：2025-09-26 13:15:01

## 1. 可靠传输链路

1. **握手仍未真正闭环**（Protocol/ReliableChannel.cpp:354-404）
   - 发送 START 帧后仅等待固定时长便默认成功，缺少对接收端 ACK 的实时确认，可靠模式依旧存在漏判风险。
2. **会话标识缺失**（Protocol/ReliableChannel.cpp:1464）
   - metadata.sessionId 始终写死为 0，无法区分不同会话或用于双方状态匹配。
3. **控制帧未纳入窗口管理**（Protocol/ReliableChannel.cpp:1478-1503）
   - START 等控制帧未写入发送窗口，后续无法对 ACK 进行对应校验，重试策略仍不完整。

## 2. 并口实现

- **宏重复定义风险**（Transport/ParallelTransport.cpp:639-646）
  - 函数体内重新 #define CTL_CODE / METHOD_BUFFERED 等宏，易与 <winioctl.h> 的标准定义冲突，可能产生编译警告或错误。

## 3. 传输工厂

- **TransportFactory 仍缺落地文件**
  - Transport/ITransport.h:135 只声明工厂接口，SimpleTransportTest.cpp、TestTransportModules.cpp 依旧引用 Transport/TransportFactory.h，目前无法通过相关测试或示例编译。

## 4. 自动化验证体系

1. **自动脚本尚未编写**
   - 可靠协议、配置读写等模块的自动化验证流程仍停留在规划阶段，尚无可直接运行的脚本/小程序。
2. **联机场景缺少配套说明**
   - 并口、USB、网络等需要实机配合的测试尚未提供脚本骨架与操作说明（接线步骤、命令示例、预期输出、人工判定点等）。

## 5. 调试输出与排障

- 虽已新增大量日志，但仍缺少“握手成功/失败、ACK 匹配状态”等明确信息，排查时需要人工从长日志中筛查关键点。

---
整改建议可参考 docs/PortMaster_代码全量分析报告.md 第三、第四节，优先闭合可靠协议握手与自动化验证链路，再逐步解决其他问题。

## 6. 手工测试新增问题

1. **大文件传输完成后接收窗口持续闪烁**（src/PortMasterDlg.cpp:2995、3116、1738）
   - 接收线程在每个 4KB 分片到达时都会 PostMessage(WM_USER+1, ...) 触发整窗重绘，UpdateReceiveDisplayFromCache() 又会重建最后 100 行文本。大文件场景下消息队列积压，传输完成弹框关闭后仍继续处理剩余消息，造成接收框闪烁。
   - **建议**：引入节流或批量处理机制、在弹框前等待消息队列完成，或改为增量追加显示内容，避免整块重绘。

2. **发送完成后再次点击“发送”却提示“没有可发送的数据”**（src/PortMasterDlg.cpp:3310、478、2252）
   - 传输完成回调清空了 m_sendDataCache 并把 m_sendCacheValid 置为 false，但发送编辑框文本仍保留。StartTransmission() 只依据发送缓存判断，从未重新同步编辑框内容，用户立即再次点击发送时就被判定为“无数据”。
   - **建议**：只有当用户点击清空输入框按钮时才允许执行清除输入数据窗口内容和发送缓存，其他操作不能代替执行这部分操作。
