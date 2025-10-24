# PortMaster 全自动迭代开发与测试总控提示词（提交给 Claude Code / 其他AI编程CLI）

> 目标：将 PortMaster 项目从“需求→实现→自测→修订→提交”尽量自动化，先行由大模型为**每个功能模块**生成脚本化测试或简易测试程序，**验证通过后**再并入主工程编译。仅在必须接入真实设备时再请求人工介入。

## 0. 行为准则与输入输出

- 你是**CI/Dev 编排代理**（非对话式解释器），按照本提示执行**批处理式**任务。
- **遵循**本仓库规范与《PortMaster 项目开发.md》全部要求（平台、架构、协议、UI、构建与交付）。
- **默认不开启自动编译/推送**；完成变更后输出“编译确认清单”，仅在收到 `ALLOW_AUTO_COMPILE=true` 或显式指令“同意编译”后执行构建与推送。
- **产物与文档必须可落盘**：所有新建代码、测试脚本、报告与日志写入工作目录的 `AI/` 与 `tests/` 子目录；同时更新“修订工作记录”与阶段报告。

## 1. 工作区检测与初始化

1. 自动识别运行环境（WSL/PowerShell），打印以下信息：
   - OS/终端类型/当前用户；
   - 工作目录：`C:\Users\huangl\Desktop\PortMaster` 与 WSL 路径 `/mnt/c/Users/huangl/Desktop/PortMaster`（如路径含空格需转义）；
   - VS2022/Toolset v143 可用性、构建脚本存在性与 Git 远程状态。
2. 若存在 `CLAUDE.md` 的工作流要求，则按要求：拉取最新代码、创建 `修订工作记录YYYYMMDD-HHMMSS.md`、进入任务阶段记录。
3. 创建 `AI/SESSION_YYYYMMDD-HHMMSS/` 会话目录，后续产物全部归档于此。

## 2. 任务分解（依据项目规范）

面向 **六大模块** 建立“实现→自测→集成”三段式流水：

- 传输层适配：`SerialTransport` / `LptPortTransport` / `UsbPortTransport` / `NetworkPrintTransport` / `LoopbackTransport`；
- 协议层：`ReliableChannel`、`FrameCodec`、`CRC32`；
- UI/交互：十六进制显示联动、发送/暂停/继续/停止、日志与状态栏；
- 配置与日志：JSON 配置、重要事件与导出；
- 构建与交付：脚本、日志零错零警告校验；
- 工具链：**自动化测试工具**与**自愈代理**。

> 对每个模块：先生成**独立可执行的自测工具或脚本**（headless/CLI），以模拟/仿真/回路替代真实设备；验证通过后再集成到主工程。

## 3. 自动化测试与工具要求

### 3.1 协议层（可靠传输）自测套件

- 组件：`crc32_test`, `frame_codec_test`, `reliable_smoke`, `reliable_soak`。
- 场景：
  - 帧编解码：包头/尾、类型、序号、长度、CRC、负载上限（默认 1024，可配置）；
  - ACK/NAK/超时重传：逐包 ACK，窗口=1；动态超时估计（基于近端 RTT）；
  - 粘包/半包/失步重同步与**断点续传**；
  - START 元数据（ver/flags/name\_len/name/file\_size/mtime）正确性；
  - 统计输出：RTT/重传/吞吐/错误计数。
- 形式：
  - 可执行（`tests/bin/reliable_*`）+ YAML/JSON 用例（参数化负载、错误注入、随机中断）。
  - 失败生成最小复现场景（保存二进制帧流水与时间线）。

### 3.2 串口（COM）

- 覆盖：端口占用检测、波特率/数据位/校验/停止位/流控与超时组合；OVERLAPPED I/O；长传暂停/继续/中断；吞吐与稳定性。
- 工具：`com_probe.exe`、`com_soak.exe`、`com_reliable_sendrecv.exe`（组合可靠协议与直通模式）。
- 仿真：在无真实串口时，使用**回路测试端口**或命名管道模拟；双进程双端测试（A 发送/B 接收）。

### 3.3 并口（LPT）/ USB 打印

- 覆盖：句柄读写/状态查询/权限提示；典型口号（LPT1/LPT2、USB001/USB002）。
- 工具：`lpt_echo.exe`、`usb_print_check.exe`（失败时输出权限/兼容性建议）。

### 3.4 网络打印（RAW9100 / LPR/LPD / IPP(HTTPS)）

- 覆盖：连接/超时/Keep-Alive/重连/可选认证；直通与可靠校验可选开启。
- 工具：`netprint_raw9100.exe`、`netprint_lpr.exe`、`netprint_ipp.exe`；支持目标/端口/队列/证书等参数。

### 3.5 回路测试端口

- 工具：`loopback_pass.exe`（直通）、`loopback_reliable.exe`（可靠）；
- 统计：轮次、成功/失败计数、吞吐曲线；支持随机错误注入与限速。

### 3.6 UI/交互快速回归（可选）

- 轻量 UI 驱动脚本：切换“十六进制显示”应同步影响收发区；拖放单文件阈值提示；发送/暂停/继续/停止按钮状态机；日志导出。

## 4. 自愈与异常收集

- 提供 `tools/autoheal/`：
  - 监控弹窗/异常消息（标题/文本/错误码），保存快照与触发时上下文；
  - 基于规则生成“修复建议 PR 草案”（定位到模块与可疑函数+拟修改点），**不直接改代码**，而是产出补丁建议与风险评估；
  - 将异常样例回注到对应测试用例集，形成**失败先行**策略。

## 5. 集成策略（先测后并入）

1. **模块内通过**：自测工具 100% 绿灯后，才在 `src/` 并入或替换实现；
2. **主工程构建**：
   - 默认仅生成构建计划与 `msbuild` 命令清单，不立即执行；
   - 收到允许后，按优先脚本 `autobuild_x86_debug.bat` 构建；失败再切换 `autobuild.bat`；
   - 抽取 `msbuild_*.log` 关键片段并校验 0 error / 0 warning；
3. **版本控制**：按 `feat|fix|docs|refactor|test|chore: 概述` 规范提交；可选打 `save-YYYYMMDD-HHMMSS` 标签；
4. **汇报**：输出 commit ID、推送结果、构建要点、遗留问题与后续建议。

## 6. 目录与产物规范

```
AI/
  SESSION_YYYYMMDD-HHMMSS/
    plan.md                  # 本次任务计划&决策记录
    risk.md                  # 风险与回滚预案
    compile_checklist.md     # 编译确认清单（默认待确认）
    reports/
      test_summary.md        # 测试汇总（通过率、失败TopN、样例链接）
      errors/...
    patches/                 # 若生成修复建议补丁或PR草案

tests/
  specs/                     # 用例YAML/JSON（参数、容错注入、期望）
  bin/                       # 各自测可执行/脚本
  logs/                      # 运行日志与帧流水
```

## 7. 执行步骤（你必须按顺序执行）

1. **读取与解析规范**：提取平台/构建脚本/架构分层/端口类型/协议细节/UI交互/测试要求/边界约束；生成 `AI/.../plan.md` 与“任务分解-检查清单”。
2. **生成各模块的自测工具与用例**：落盘代码/脚本与 specs；给出 `run_all_tests.(ps1|sh)` 入口（对环境自适应）。
3. **运行自测（可模拟/回路）**：尽量在无真实设备时完成高覆盖验证；导出 `reports/test_summary.md` 与失败样例。
4. **仅在允许时执行主工程编译**：
   - 用户下达 `ALLOW_AUTO_COMPILE=true` 或 “同意编译”后：调用首选脚本构建；
   - 校验日志 0 error / 0 warning；
5. **版本控制与推送**：按规范提交；汇报commit与标签；
6. **阶段文档更新**：更新“修订工作记录YYYYMMDD-HHMMSS.md”的阶段一/阶段二/阶段三条目；整理交付清单；输出后续建议。

## 8. 成功判据（Acceptance）

- `tests/bin/*` 自测工具均可在本机完成基础/容错/回路测试至少 95% 用例通过；
- 未获授权不触发主工程构建与推送；授权后构建 0 error/0 warning；
- 交付目录完整：设计/协议规格/状态机图（ASCII）/关键源码/`.rc` 资源/配置与日志/测试指引；
- 生成“自动化测试与自愈改进方案”报告，列出未来回归套件拓展与CI接入方案。

## 9. 命令与环境变量

- 允许自动编译：`ALLOW_AUTO_COMPILE=true`（或显式文本指令“同意编译”）。
- 工作根：Windows `C:\Users\huangl\Desktop\PortMaster`；WSL `/mnt/c/Users/huangl/Desktop/PortMaster`（含空格需转义：`Visual\ Studio\ 2022`）。
- 构建脚本：`autobuild_x86_debug.bat` → `autobuild.bat`（逐级回退）。

## 10. 交付与输出格式

- 在 `AI/SESSION_.../` 产出所有计划/报告/清单；
- 在 `tests/` 产出全部工具、用例与日志；
- 在顶层生成 `PortMaster_自动化测试与集成报告_YYYYMMDD.md`；
- 控制台**最终仅输出**：
  1. 本次通过率与失败用例数量；
  2. 是否获得编译授权与执行结果；
  3. `git` 提交与推送摘要；
  4. 后续人工接入清单（需要真实设备的测试项目与接线说明）。

---

**执行现在开始**：

1. 环境检测→拉取代码→创建本轮“修订工作记录”。
2. 解析规范→生成任务分解与测试计划。
3. 为六大模块生成自测工具与用例→运行→汇总报告。
4. 等待编译授权；若授权则构建、校验并推送；否则仅提交变更与文档。
5. 输出最终汇报与后续建议。

