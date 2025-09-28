## 2025-02-? 可靠模式 21MB 压缩包保存异常分析

- **问题概览**：本地回路可靠模式传输 21MB 压缩包后，保存结果仅有 1 174 528 字节，但 UI 日志仍宣称“保存验证成功”。
- **关键日志**：
  - `[23:48:18.181] 准备保存原始字节数据，大小: 1174528 字节` 与 `[23:48:25.712] ✅ 保存验证成功：1174528 字节`，证明缓存文件尺寸在保存前就已被截断。
  - `ReceiveThread: looking for sequence 1147` 持续出现，而 `ProcessThread` 继续读取更高序号帧（如 `sequence=11462`），收到 `SendNak` 后仍反复超时。
  - `ProcessThread: packet sequence=21466 exceeded max retries (3), marking as failed` 等多条日志表明大量数据帧在最大重传次数后被直接丢弃。
- **根因定位**：
  1. **发送窗口缺乏回压**（`Protocol/ReliableChannel.cpp:724-799`）：`SendThread` 在没有任何窗口检查的情况下持续调用 `AllocateSequence()`/`SendPacket()`，导致 `m_sendNext` 无限制增长。
  2. **窗口槽位被覆盖**（`Protocol/ReliableChannel.cpp:1392-1405`）：`SendPacket` 无条件写入 `m_sendWindow[index]`，即使该槽仍 `inUse`，从而覆盖尚未确认的帧；随后 `ProcessAckFrame`（同文件 `1092-1144`）无法再匹配 ACK，接收端只能发送 NAK。
  3. **重传耗尽后未中止传输**（`Protocol/ReliableChannel.cpp:673-707`）：达到 `maxRetries` 时仅记录日志并清空槽位，没有通知上层终止流程，UI 仍认为文件传输完成并允许保存。
- **解决建议**：
  1. 在发送线程中引入窗口回压（条件变量或自旋等待），确保 `GetWindowDistance(m_sendBase, m_sendNext) < m_config.windowSize` 时才分配新序号，避免覆盖未确认的数据槽。
  2. 在 `SendPacket` 写入窗口前检测 `m_sendWindow[index].inUse`，必要时等待或返回失败，由上层等待逻辑处理，保证老帧不被覆盖。
  3. 当某帧重传耗尽时，通过 `ReportError` 触发 UI，并显式标记本次文件传输失败（例如设置 `m_fileTransferActive = false`、断开可靠通道），阻止用户保存损坏缓存。
  4. （选做）保存按钮启用前复核 `ReliableChannel::GetStats().errors` 或最近错误日志，若存在重传失败，则禁用保存并提示重新传输。


# PortMaster Save Flow Recheck Report

Generated: 2025-09-26 21:55:03

## 1. Changes Observed

- ReadDataFromTempCacheUnlocked() / ReadAllDataFromTempCacheUnlocked() were added (src/PortMasterDlg.cpp:4096-4216), removing the double-lock issue noted previously.
- OnBnClickedButtonSaveAll still guards the critical section with std::lock_guard<std::mutex> saveLock, but it now calls the unlocked reader and catches std::system_error, so the old
esource deadlock exception no longer appears.
- Warning messages are emitted only when all three counters disagree; read failures now short-circuit the flow with an explicit error dialog.

## 2. Current Test Findings

- **Direct mode**: no warning dialog pops up, but ReceiveData.pdf is only **860,160 bytes** versus the original **1,113,432 bytes**.
- **Reliable mode**: warning dialog still appears, yet the saved ReceiveData-1.pdf matches the original file exactly.
- Logs confirm the behaviour:
  - At 19:14:10 the direct-mode save loop reads exactly 860,160 bytes, the same value held by m_totalReceivedBytes and m_bytesReceived, so the loop exits early and writes the truncated snapshot.
  - At 19:14:34 the reliable-mode loop reads multiple growing sizes (1,011,712 → 1,113,432), so the mismatch warning triggers until the numbers converge; the final retry reads the full data set before writing.

## 3. Root Cause Summary

1. **Save triggered too soon** – the loop considers "current read equals current counters" as success. In direct mode the UI counter and file counter move in lockstep even while data is still arriving, so an intermediate partial file is treated as complete.
2. **No stability window** – the loop never checks whether the counters stop changing. Five retries can still pass immediately if every retry sees the same (but growing) counters.
3. **Reliable mode mismatch timing** – m_bytesReceived advances faster than the file writer, so the warning dialog keeps appearing even though the save eventually succeeds.

## 4. Recommended Fix

1. **Wait for a quiet period**: remember the first m_totalReceivedBytes value and time stamp; continue delaying (e.g. 200 ms per retry) while the counters keep increasing. If they never stabilise within N attempts, inform the user and abort the save.
2. **Compare consecutive reads**: only treat the data as complete when *current read = previous read = both counters*. This guarantees that the file snapshot is stable.
3. **Reliable-mode warning guard**: when m_bytesReceived > m_totalReceivedBytes, delay additional retries (e.g. +500 ms) before showing the warning to avoid false alarms caused by write lag.
4. **Post-save verification**: after writing, call GetFileSize and compare it with finalCachedData.size(); log the result for regression tracking.

## 5. Validation Plan

- After implementing the above guard rails, rerun loopback tests in both direct and reliable modes with large files. Confirm that the save button is only enabled after transfer quiets down, that no false warning appears, and that the saved file size matches the source.
- Re-run verify_reliable_protocol.py and verify_config_system.py, plus extend the automation suite with a "save integrity" script to cover both modes and multiple file sizes.

