# PortMaster 可靠模式保存缺陷分析与处置方案

## 1. 问题摘要
- **场景**：可靠传输模式完成大文件回路测试后，点击“保存全部”按钮。
- **现象**：弹出“数据完整性校验异常”告警；选择继续保存后，生成文件仅 `974,848` 字节，明显小于原始 `1,113,432` 字节。
- **日志证据**：`build/Debug/PortMaster_debug.log` 中 18:20:28～18:21:19 的记录显示：
  - 18:20:28 `ReadDataFromTempCache` 首次读取仅 `974,848` 字节，随后传输线程仍持续写入临时文件直到 `1,113,432` 字节。
  - 18:21:14 用户确认继续保存后，程序直接使用**早先缓存的 974,848 字节**写入新文件，未重新获取最新数据。

## 2. 根因分析
1. `OnBnClickedButtonSaveAll` 在提示框出现前就把 `ReadAllDataFromTempCache()` 返回值保存到局部变量 `cachedData`，此时临时文件尺寸仍在增长（可靠模式存在握手/重传尾块）。
2. 若校验发现 `cachedData.size()` 与 `m_totalReceivedBytes`/`m_bytesReceived` 不符，会弹出“是否仍要保存”对话框。但 **即便用户稍后确认继续保存，代码也不会重新读取临时文件**，仍旧用最初的 `cachedData` 写文件，导致输出文件被永久截断。
3. 可靠模式由于窗口和 ACK 往返，相比直通模式更容易出现“保存时仍在落盘”的状态，因此问题只在可靠模式下暴露。

## 3. 推荐修复方案（单一方案）
### 3.1 保存路径改造
在 `OnBnClickedButtonSaveAll` 中做如下调整：
1. **锁定接收文件互斥**：沿用 `m_receiveFileMutex`，在进入保存流程前持锁，确保没有新的数据正在写入。
2. **增加“完整性自愈”循环**：
   - 循环最多 N 次（建议 5 次，每次 100~200ms）重新执行 `ReadAllDataFromTempCache()`，每次读取后即时校验：
     - `cachedData.size() == m_totalReceivedBytes`
     - `m_bytesReceived == m_totalReceivedBytes`
   - 只要条件满足即可跳出循环；若一直不满足，说明接收仍在进行，应提示用户稍后再保存并直接返回，不生成文件。
3. **用户确认后重新读取**：即使弹出警告并获得用户确认，也要重新调用 `ReadAllDataFromTempCache()` 取得最新数据，严禁复用旧缓存。
4. **可靠模式的按钮管控**：在 `ReliableChannel::ReceiveThread` 完成文件尾帧处理后，通过回调通知 UI，才允许启用“保存全部”按钮，避免用户在传输未完全收尾时触发保存。

### 3.2 代码落点参考
- `src/PortMasterDlg.cpp`
  - `OnBnClickedButtonSaveAll`：添加循环重读逻辑与“不可保存时直接返回”。
  - `ReadAllDataFromTempCache`：保留现有互斥保护，可配合新增的等待逻辑使用。
  - `OnReliableComplete` or 新增 `OnReliableReceiveFinished` 回调：在 reliable 模式确认 `m_fileTransferActive == false` 后启用保存按钮。
- 如需暴露可靠通道状态，可在 `Protocol/ReliableChannel.cpp` 增加只读接口（例如 `IsFileTransferActive()`），供 UI 判断保存按钮启用条件。

## 4. 验证建议
1. 使用 `verify_reliable_protocol.py` 验证可靠握手流程仍完整。
2. 编写一个“小文件-大文件”组合脚本：同时运行可靠、直通模式，在传输过程中多次尝试点击保存，确认：
   - 可靠模式不再生成截断文件；
   - 未完成接收时，保存按钮不可用或提示“请等待完成”。
3. 对比传输完成后的 `m_bytesReceived`、`m_totalReceivedBytes` 与 `ReadAllDataFromTempCache()` 结果，保证三者一致。

## 5. 风险与注意事项
- 循环等待时务必设置超时时间，防止因底层卡死造成 UI 永久阻塞。
- 若在 CI 中加入自动化保存测试，需确保脚本运行环境可访问临时目录（Windows 临时路径）。
- 当可靠通道因重传导致的重复片段到达时，应继续依赖可靠栈自身的去重逻辑，保存侧只负责确保“读取时数据已落盘且计数一致”。

落实以上步骤后，可靠模式的保存流程将与直通模式保持相同的完整性保障，不会再因为早期读取缓存而写出残缺文件。
