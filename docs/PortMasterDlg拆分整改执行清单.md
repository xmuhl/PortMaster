# PortMasterDlg 模块拆分整改执行清单（必须按序完成）

> 所有任务必须严格遵循下列步骤执行，每个阶段完成后立即核验与文档记录，严禁虚假开发或过度开发。未列出的操作不得擅自执行。行数变动仅作为参考，不作为验收条件，关键考察职责拆分与行为一致性。

## 阶段0：准备与基线确认
1. 使用 `cmd.exe /c autobuild_x86_debug.bat` 编译，确认 0 error / 0 warning。
2. 记录当前 `src/PortMasterDlg.cpp` 行数（`wc -l`），写入补充修订记录。
3. 执行核心功能用例：连接/断开、直通发送、可靠发送、文件接收保存、十六进制显示切换、配置读写。每个用例记录操作与结果。
4. 更新修订记录文件，注明“阶段0完成”，附编译日志和功能验证结果。

## 阶段1：控件管理与日志统一整改
**目标**：`CPortMasterDlg` 不直接操作 UI 控件和状态栏，全部交由 `DialogUiController` 及 `StatusDisplayManager` 管理。

1. 检查 `CPortMasterDlg` 中所有控件操作（按钮文本、状态栏文案、日志控件、进度条等），逐项迁移到 `DialogUiController` 或 `StatusDisplayManager`，不遗漏任何事件函数。
2. 更新 `DialogUiController`，确保内含设置/获取控件文本、更新状态栏、进度条的完整接口。新增接口须在 `CPortMasterDlg` 现有调用处全部替换，无残留旧调用。
3. 更新 `StatusDisplayManager`，确保所有日志、统计、进度相关调用都在该模块内完成。`WriteLog` 等函数仅负责写文件和调用 `StatusDisplayManager`，不得再直接操作 UI。
4. 移除 `CPortMasterDlg` 中所有直接 `SetWindowText`、`GetWindowText`、`EnableWindow` 等控件调用。确认 UI 控件只在控制器/显示管理器内访问。
5. 执行自动编译 `cmd.exe /c autobuild_x86_debug.bat`，确保 0 error / 0 warning，记录日志。
6. 更新修订记录，列出迁移的函数清单与编译结果，注明“阶段1完成”。

## 阶段2：端口流程收尾
**目标**：连接/断开等端口相关逻辑全部通过 `PortConfigPresenter`、`PortSessionController`、控制器/显示管理器协同完成。

1. 检查 `OnBnClickedButtonConnect/Disconnect`、`InitializeTransportConfig` 等函数，确认只调用服务接口，UI 更新交由控制器或显示管理器，不得混杂控件操作。
2. 将 `SetDlgItemText` 等残留操作替换为控制器接口，确保端口状态文本、按钮、保存按钮状态全部通过控制器更新。
3. 再次验证 `PortConfigPresenter` 是否涵盖所有端口类型、参数组合，以及回调接口；如有遗漏立即补全。
4. 执行自动编译 `cmd.exe /c autobuild_x86_debug.bat`，确认 0 error / 0 warning。
5. 更新修订记录，注明阶段2修订点与编译结果，标记“阶段2完成”。

## 阶段3：事件调度集中化修复
**目标**：所有 UI 事件函数仅负责调用 `PortMasterDialogEvents`，事件调度器负责业务逻辑、文件处理、日志输出。

1. 逐一核对事件函数：`OnBnClickedButtonSend/Stop/File/ClearAll/ClearReceive/CopyAll/SaveAll`、`OnBnClickedCheckHex`、`OnDropFiles`、`OnBnClickedButtonConnect/Disconnect` 等，确认全部调用 `m_eventDispatcher` 对应方法，不再含业务代码。
2. 将 `OnBnClickedButtonSaveAll` 中的完整保存流程迁移至 `PortMasterDialogEvents::HandleSaveAll`；确保事件调度器逻辑可直接运行，删除 `CPortMasterDlg` 中冗余流程和重复日志。
3. 将 `OnBnClickedCheckHex` 中的模式切换逻辑迁移到 `HandleToggleHex`，确保控制器返回的文案与日志由 `StatusDisplayManager` 输出。
4. 检查调度器中未使用的函数（如原有 `HandleToggleHex/HandleSaveAll`），确认被调用；若重复或无引用立即删除。
5. 执行自动编译 `cmd.exe /c autobuild_x86_debug.bat`，确认 0 error / 0 warning。
6. 更新修订记录，列出迁移的事件函数列表及编译结果，标记“阶段3完成”。

## 阶段4：状态展示与日志统一回归
**目标**：`StatusDisplayManager` 接管日志、统计、进度条、节流，确保控制器与展示器分工明确。

1. 遍历 `StatusDisplayManager` 暴露的接口，确保在 `CPortMasterDlg` 或事件调度器中均有对应调用；未使用的接口移除或补全调用。
2. 检查 `DialogUiController` 与 `StatusDisplayManager` 的职责划分，避免重复功能；如有交叉，调整为单一模块负责。
3. 确保节流逻辑仅在一个模块内管理。若 `StatusDisplayManager` 提供节流接口，就删除 `DialogUiController` 中相同功能并调整调用。
4. 执行自动编译 `cmd.exe /c autobuild_x86_debug.bat`，确认 0 error / 0 warning。
5. 记录阶段4完成情况，包括控制器与展示器职责划分及编译结果。

## 阶段5：状态数据容器评估（维持跳过策略）
**目标**：确认跳过 DialogStateSnapshot 的决策仍成立，并记录证据。

1. 使用 `rg` 或 IDE 搜索，确认 `UIStateManager` 等旧组件仍无引用，定期验证。
2. 评估 `std::atomic` 状态管理是否满足当前需求；若出现新需求需改动，另行列出设计方案再执行。
3. 更新修订记录，重申跳过理由和验证证据，注明“阶段5保持跳过状态”。

## 阶段6：余留清理与终验
1. 检查源文件中是否仍包含被排除的旧模块（ThreadSafeProgressManager 等），决定归档或删除；若仅排除编译，需在 `docs` 中说明处理策略。
2. 复查工程文件（.vcxproj）引用，确保不再包含已废除的源文件。
3. 执行自动编译 `cmd.exe /c autobuild_x86_debug.bat`，确认 0 error / 0 warning，并执行静态分析 `cmd.exe /c msbuild PortMaster.sln /t:Rebuild /p:Configuration=Debug /p:Platform=Win32 /p:RunCodeAnalysis=true`，保存分析结果。
4. 汇总阶段0-6的执行情况与结果，更新 `docs/PortMasterDlg模块拆分完整任务计划导出.md` 与修订记录，标记每阶段“完成”或“跳过”状态。
5. 编写提交摘要，说明自动编译与静态分析结论，准备交付。
6. 在所有自动验证完成且文档更新后，通知测试团队按标准用例执行人工回归测试，人工测试结果另行记录。
