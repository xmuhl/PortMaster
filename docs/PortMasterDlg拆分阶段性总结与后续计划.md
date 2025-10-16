# PortMasterDlg 模块拆分阶段性总结与后续计划（2025-10-16）

## 1. 当前进度概览
| 阶段 | 执行方案要求 | 当前状态 | 备注 |
| --- | --- | --- | --- |
| 阶段0 基线确认 | 编译、行数记录、核心功能验证、基线存档 | ✅ 已完成 | `baseline_portmasterdlg_20251016-1523.txt` 记录 0 error / 0 warning、行数 2911、功能验证结论 |
| 阶段1 控件初始化与布局抽离 | 行数目标 ≥250 行；控件管理逻辑迁移至 `DialogUiController` | ⏳ 部分完成 | `DialogUiController` 已接管控件初始化、节流、状态更新；坐标行数仅下降 26 行，仍需继续消减原函数主体及直接控件访问 |
| 阶段2 端口配置与枚举拆分 | 行数目标 ≥300 行；解除 UI 与 Transport 实现耦合 | ⏳ 部分完成 | `PortConfigPresenter` 已创建并负责端口配置；行数累计下降 84 行；需继续迁移残留的 UI 控件操作代码 |
| 阶段3 事件调度集中化 | 创建 `PortMasterDialogEvents`，迁移所有 UI 事件处理 | 未开始 | 初次尝试时发现需处理大量私有成员与日志逻辑耦合，暂未提交修改 |
| 阶段4 状态展示与日志统一管理 | 创建 `StatusDisplayManager` | 未开始 | 待阶段1、2 收敛后执行 |
| 阶段5 状态数据容器抽离 | 创建 `DialogStateSnapshot` | 未开始 | |
| 阶段6 余留清理与收尾 | 清理 include / 成员、单元测试、回归验证、文档更新 | 未开始 | |

## 2. 现有问题与影响
1. **阶段1未达行数目标**  
   - `LogMessage`、部分控件直接操作仍保留在 `CPortMasterDlg` 内；大量按钮事件仍直接访问控件。  
   - 对话框成员中 `m_binaryDataDetected` 等状态字段仍散布多个函数，影响职责集中。

2. **阶段2未完全解耦**  
   - 尽管已移除 Transport 具体实现头文件，`OnBnClickedButtonConnect/Disconnect`、`OnDropFiles`、`OnBnClickedButtonCopyAll/SaveAll` 等函数仍包含大量业务逻辑，导致在阶段3迁移事件时碰到大块代码需要同时迁移。  
   - `LoadDataFromFile` 等文件操作函数仍位于 `CPortMasterDlg`，在事件拆分时成为阻碍。

3. **阶段3初次尝试遇到的障碍**  
   - 多个事件函数涉及缓存、日志、状态更新，需要同步迁移或先行抽象，否则 `PortMasterDialogEvents` 无法直接引用。  
   - 保存/复制函数包含可靠模式校验、临时文件处理、用户交互，迁移门槛高，需要预先拆解为可重用接口。

## 3. 调整后的执行策略
为保证后续阶段顺利实施，先补强阶段1、阶段2 的拆分，降低事件迁移难度。调整后的执行顺序如下：

### 阶段1 追加任务（优先完成）
1. **日志输出一体化**  
   - 将 `CPortMasterDlg::LogMessage` 的残余逻辑完全迁移至 `DialogUiController::LogMessage`（已完成）并统一调用点（确认无遗漏）。
   - 将所有状态栏与按钮文本更新统统改为调用 `DialogUiController`，消除 `CPortMasterDlg` 内对控件的直接引用。
2. **定时器与节流逻辑清理**  
   - 确认 `ThrottledUpdateReceiveDisplay` 等函数只与 `DialogUiController` 交互，不再访问内部节流成员。
3. **控件访问整理**  
   - 对 `OnBnClickedButtonStop` 等仍直接操作控件的函数，抽取接口至 `DialogUiController` 或交由即将创建的事件调度器调用。
4. **行数目标复验**  
   - 完成上述优化后再次统计 `wc -l src/PortMasterDlg.cpp`，确保相对于基线累计减少 ≥250 行。若仍不足，继续清理 `OnBnClickedButtonClear*` 等函数内的直接操作。

### 阶段2 追加任务
1. **端口事件转发**  
   - 将 `OnBnClickedButtonConnect/Disconnect` 的 UI 控件操作（按钮启停、状态文本等）提取到 `DialogUiController`，保留业务委托给 `PortSessionController`。
2. **文件处理抽象**  
   - 将 `LoadDataFromFile`、`SaveDataToFile`、`OnDropFiles` 相关逻辑拆分为独立方法或委托给即将建立的事件调度器，以减少阶段3的迁移难度。
3. **行数复验**  
   - 确保阶段1、2 完成后总行数下降满足阶段目标（基线 3021 行 → 阶段1+2 后至少降低 600 行）。

### 阶段3 重新实施计划
1. 在阶段1、2 完整落地后，再次创建 `PortMasterDialogEvents`，迁移按钮、复选框、拖拽等事件函数。
2. 事件函数内仅做调度，实际操作通过已有服务或新抽象方法执行，保持 UI 层纯粹。
3. 每迁移一批事件即编译、执行核心功能测试，避免功能回归。

### 阶段4-6 按原方案执行
在阶段3 稳定的前提下，继续创建 `StatusDisplayManager`、`DialogStateSnapshot`，最终完成余留清理与文档更新。

## 4. 后续执行计划（待确认后实施）
1. 按上述“阶段1追加任务”“阶段2追加任务”补齐工作，达到行数和职责目标。
2. 复测并更新修订记录、评估文档中的阶段完成状态。
3. 进入阶段3，采用更细粒度的迁移步骤（如先迁移清空/复制/保存，再处理发送/停止等）。
4. 阶段4-6 保持原执行方案不变。

请确认本调整方案后，我将据此继续实施后续修订。*** End Patch
