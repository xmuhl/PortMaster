# PortMasterDlg.cpp 模块拆分优化执行方案（强制执行版）

> **规范要求**：以下步骤为唯一执行方案，必须严格按顺序逐项完成，不允许增删或并行执行其他操作。每步结束后需立即记录结果，如未达成预期须回滚至本步开始前的状态重试。

## 0. 基线确认
1. 运行 `autobuild_x86_debug.bat`，确认 0 error / 0 warning，记录日志片段。
2. 记录基线行数：`wc -l src/PortMasterDlg.cpp`（当前应为 3021 行），将结果写入修订记录。
3. 手工执行以下功能并记录截图/日志：连接与断开、直通发送、可靠发送、文件接收保存、十六进制显示切换、配置读写。
4. 在 `docs/修订工作记录xxxx.md` 中标记“基线确认完成”，附上述证据。

## 1. 控件初始化与布局抽离
1. 新建 `src/DialogUiController.h`、`src/DialogUiController.cpp`。
2. 在 `DialogUiController` 中实现以下职责：控件指针缓存、初始值设置、节流定时器管理、按钮状态更新、状态栏文本更新。
3. `CPortMasterDlg::OnInitDialog` 内仅保留框架代码，将原有控件初始化、状态更新、定时器初始化全部改为调用 `DialogUiController`。
4. 删除 `CPortMasterDlg` 中与上述职责重复的成员变量和函数（如 `UpdateButtonStates`、`UpdateConnectionStatus`、`UpdateSaveButtonStatus` 等）。
5. 编译验证并重新执行基线功能测试，记录行数变化（目标：相对基线减少 ≥ 250 行）。
6. 更新修订记录与评估文档，标注“控件初始化抽离完成”。

## 2. 端口配置与枚举拆分
1. 新建 `src/PortConfigPresenter.h`、`src/PortConfigPresenter.cpp`。
2. 将端口类型、端口号、波特率、数据位、校验位、停止位、流控相关逻辑迁移至 `PortConfigPresenter`，包括枚举、UI 同步和事件响应。
3. `PortConfigPresenter` 对 Transport 枚举使用接口或 ConfigStore 提供的数据，移除 `PortMasterDlg.cpp` 对 `SerialTransport/ParallelTransport/...` 头文件的直接包含。
4. 在 `CPortMasterDlg` 中保留对 `PortConfigPresenter` 的唯一成员引用，所有端口相关事件转发给该类处理。
5. 编译验证并执行基线功能测试，记录行数变化（目标：相对上一阶段减少 ≥ 300 行）。
6. 更新修订记录与评估文档，标注“端口配置拆分完成”。

## 3. 事件调度集中化
1. 新建 `src/PortMasterDialogEvents.h`、`src/PortMasterDialogEvents.cpp`。
2. 将所有按钮/复选框/下拉框/拖拽事件逻辑迁移至 `PortMasterDialogEvents`，包括 `OnBnClickedButtonSend/Stop/File/Clear/Copy/Save`、`OnBnClickedCheckHex`、`OnDropFiles` 等。
3. 事件处理函数内仅保留对调度器的调用，不得直接访问服务层或 UI 控件。
4. 调度器内部调用现有服务（`TransmissionCoordinator`、`PortSessionController`、`ReceiveCacheService`）完成业务操作。
5. 编译验证并执行基线功能测试，记录行数变化（目标：相对上一阶段减少 ≥ 400 行）。
6. 更新修订记录与评估文档，标注“事件调度拆分完成”。

## 4. 状态展示与日志统一管理
1. 新建 `src/StatusDisplayManager.h`、`src/StatusDisplayManager.cpp`。
2. 将 `UpdateStatistics`、`LogMessage`、`SetProgressPercent`、`ThrottledUpdateReceiveDisplay`、二进制提示、节流相关逻辑迁移至该管理器。
3. 管理器提供统一接口：`UpdateStatistics`、`ShowLogMessage`、`UpdateProgress`、`ShowBinaryHint` 等，对 UI 控件的直接操作封装在内部。
4. `CPortMasterDlg` 调用管理器方法，不再直接操作状态文本或日志控件。
5. 编译验证并执行基线功能测试，记录行数变化（目标：相对上一阶段减少 ≥ 350 行）。
6. 更新修订记录与评估文档，标注“状态展示管理完成”。

## 5. 状态数据容器抽离
1. 新建 `src/DialogStateSnapshot.h`、`src/DialogStateSnapshot.cpp`。
2. 将对话框中的布尔/计数/节流等状态成员整合到 `DialogStateSnapshot`，统一由该类维护和提供访问。
3. 所有调用处通过 `DialogStateSnapshot` 读写状态，`CPortMasterDlg` 不直接维护散布的状态成员。
4. 配合 `DialogUiController`、`StatusDisplayManager` 更新接口，确保状态一致性。
5. 编译验证并执行基线功能测试，记录行数变化（目标：相对上一阶段减少 ≥ 150 行）。
6. 更新修订记录与评估文档，标注“状态容器抽离完成”。

## 6. 余留清理与收尾
1. 全面检查 `src/PortMasterDlg.h/.cpp`，移除未使用的 include、成员声明、宏定义；确认仅保留接口层头文件依赖。
2. 确认 `UIStateManager` 等旧管理器若无引用则删除相关文件与引用；如仍需保留，确保其职责在上述新模块中替代后再移除。
3. 对新建模块补充最小必要单元测试或示例（如可行），确保编译通过。
4. 执行一次完整回归测试：连接、发送、接收、保存、配置、拖拽、可靠模式等，记录全部结果。
5. 汇总行数变化、模块职责、测试结果，写入 `docs/PortMasterDlg拆分落差复盘报告.md` 与 `docs/PortMasterDlg模块拆分评估.md`。
6. 更新 `AGENTS.md`、`CLAUDE.md`、`GEMINI.md` 如有任何流程或结构变化，并在修订记录中说明收尾状态。

> **验收标准**：完成以上步骤后，`src/PortMasterDlg.cpp` 需显著短于初始 3021 行，职责边界清晰，所有回归测试通过，文档同步准确。未满足任一条件视为未完成。
