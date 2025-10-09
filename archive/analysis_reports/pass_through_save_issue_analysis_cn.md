# 直通模式保存文件体积异常分析

## 现象
- 日志 `build\\Debug\\PortMaster_debug.log` 显示 `SaveBinaryDataToFile` 在 12:58:34 仅写入 987136 字节，而前序 `ThreadSafeAppendReceiveData` 已累计到 1113432 字节，导致最终保存文件明显偏小。
- 传输线程继续接收数据时多次写入 4096 字节块（日志中多次出现全 0x30 的数据），但保存时使用的内存数据仍停留在 987136 字节的快照。

## 关键代码路径
- `OnBnClickedButtonSaveAll` (`src\\PortMasterDlg.cpp:2550` 起) 在弹出文件对话框之前就调用 `ReadAllDataFromTempCacheUnlocked()` 读取临时缓存，并将结果拷贝到局部变量 `binaryData`。
- 与此同时，`OnTransportDataReceived` (`src\\PortMasterDlg.cpp:3376`) 仍在接收数据并调用 `ThreadSafeAppendReceiveData` (`src\\PortMasterDlg.cpp:2249`)，持续向临时文件和 `m_receiveDataCache` 追加新内容。
- 当用户在对话框中选择保存路径后再次执行 `SaveBinaryDataToFile` (`src\\PortMasterDlg.cpp:2875`)，传入的 `binaryData` 仍是早先的快照，未包含后续写入的数据，生成的文件因而被截断。

## 根因分析
- 保存流程读取数据与弹出对话框操作耦合在一起，导致“读取临时文件→等待用户选择→写入磁盘”之间存在时间窗口。
- 直通模式下 `UpdateSaveButtonStatus` (`src\\PortMasterDlg.cpp:3349`) 会直接启用保存按钮，即使传输线程仍在活跃。用户只要在传输尚未完全落盘时点选保存，就会触发上述时间窗口问题。
- 虽然保存前检查了 `m_isTransmitting`，但该标志在直通模式下不会随着被动接收自动更新，从而无法阻止用户过早保存。

## 修改建议
1. **延后读取最新数据**：将 `ReadAllDataFromTempCacheUnlocked()` 的调用移动到用户点击“保存”确认之后，并重新读取一遍缓存（或在确认后对比 `binaryData.size()` 与 `m_totalReceivedBytes`，不一致时重新抓取），确保写盘数据与实际接收总量一致。
2. **补充就绪判断**：在直通模式下仅当 `m_totalReceivedBytes` 在短时间内保持稳定或传输线程明确结束时才启用保存按钮，避免用户在传输未收尾时触发保存流程。
3. **保存前再次比对**：在 `SaveBinaryDataToFile` 之前添加一次轻量校验，例如复读临时文件并确认长度，若发现 `binaryData` 落后于最新 `m_totalReceivedBytes`，提示用户稍后再试或自动刷新数据。

落实以上改动后，可消除直通模式下保存文件尺寸偏小的问题，确保保存结果覆盖全部已接收的数据。
