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
