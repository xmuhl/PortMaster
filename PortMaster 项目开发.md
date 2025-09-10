
你是一名资深 Windows C++/MFC 工程师，请严格按照以下完整规格，开发一个名为 **PortMaster（端口大师）** 的桌面应用程序，并按交付要求输出全部设计与源码。

---

### 0. 平台与编译/编码要求

- **操作系统兼容**：Windows 7 ~ Windows 11，支持 x86 与 x64。
    
- **编译环境**：Visual Studio 2022 中文版，Toolset v143。
    
- **目标产物**：单一 EXE 绿色免安装版本，静态链接运行库（/MT 或 /MTd）与静态链接 MFC。
    
- **编码规范**：
    
    - 源码、注释、资源、文档全部使用**简体中文**。
        
    - 源码文件编码统一为 **Unicode 或 UTF-8（带 BOM）**，必要处添加：
        
        ```cpp
        #pragma execution_character_set("utf-8")
        ```
        
    - .rc 资源文件与字符串表同样使用 UTF-8（BOM）。
        
- **配置存储**：优先放在程序同目录，失败时回退到 `%LOCALAPPDATA%`。
    

---

### 0.1 自动编译脚本闭环

- 每次需要验证修改是否正确时，**自动调用项目根目录的 `autobuild.bat`** 编译，并**读取编译输出日志**检查。
    
- 必须将**所有错误和警告修正为 0**（Zero Errors & Zero Warnings）后，方可进入下一阶段开发。
    

---

### 1. 项目名称与品牌

- **项目名称**：PortMaster（端口大师）
    
- **定位**：多端口（串口/LPT/USB 打印机/网络）数据读写与可靠传输工具。
    
- **品牌特征**：跨端口通信、可靠传输协议、单文件绿色版、Win7~Win11 全兼容。
    

---

### 1.1 视觉资源（必须集成）

1. **项目主图标（多分辨率 ICO）**
    
    - 文件：[PortMaster_Icon.ico](sandbox:/mnt/data/PortMaster_Icon.ico)
        
    - 用作应用主图标（MFC 工程 `IDI_MAIN_ICON`），包含 16、32、48、64、128、256 像素分辨率。
        
2. **PNG 图标源文件**
    
    - 文件：[PortMaster_Icon.png](sandbox:/mnt/data/PortMaster_Icon.png)
        
3. **启动画面（Splash Screen）**
    
    - 文件：[PortMaster_Splash.png](sandbox:/mnt/data/PortMaster_Splash.png)
        
    - 程序启动时居中显示 2~3 秒，或在加载初始化模块时显示。
        
4. **集成要求**
    
    - 所有资源文件需打包进 EXE（在 .rc 文件中声明 ICON、BITMAP 或 RCDATA 资源）。
        
    - 启动画面必须在 Win7~Win11 显示正常（避免使用仅 Win8+ API）。
        

---

### 2. 功能要求

#### 2.1 支持端口类型

1. **串口 COM**
    
    - API：`CreateFile / SetCommState / OVERLAPPED I/O`
        
    - 支持参数配置（波特率、数据位、校验位、停止位等）
        
2. **并口 LPT（仅微软标准 API 路径）**
    
    - API：`OpenPrinter / StartDocPrinter / WritePrinter / EndDocPrinter`（RAW 作业方式写出数据到 LPT 或映射打印机）。
        
    - 提供打印机/队列状态查询作为“读”的替代。
        
    - **边界**：不做寄存器级原始 I/O（用户态无微软标准 API 可用）。
        
3. **USB 打印机**
    
    - 同 LPT 实现方式（Winspool RAW）。
        
4. **网络**
    
    - TCP 客户端/服务端 + UDP。
        
5. **本地回环**
    
    - LoopbackTransport，用于协议自测。
        

#### 2.2 工作模式

- **直通模式**：直接对端口进行数据读写（对 LPT 为“写 + 状态查询”）。
    
- **可靠传输模式**：使用本项目自研协议（见 3 节），可随时开/关，不影响直通。
    

---

### 3. 可靠数据传输协议

#### 3.1 帧格式（小端）

```
包头(0xAA55,2B) + 类型(1B) + 序号(2B) + 长度(2B) +
CRC32(4B, 多项式0xEDB88320, init 0xFFFFFFFF, final xor 0xFFFFFFFF, 覆盖“类型|序号|长度|数据”) +
数据(0..N) + 包尾(0x55AA,2B)
```

- 类型：`0x01=START, 0x02=DATA, 0x03=END, 0x10=ACK, 0x11=NAK`
    
- 默认负载上限：1024 字节（可配置）
    

#### 3.2 流程

- **发送端**：START → 等 ACK → DATA(N) 逐包 ACK/NAK/超时重传 → END → 等 ACK → 完成
    
- **接收端**：收到 START 回 ACK；DATA 序号&CRC 正确 → 保存并 ACK；否则 NAK；收到 END 回 ACK
    

#### 3.3 错误处理

- CRC 失败 → 发送 NAK
    
- 超时 → 重传
    
- 超过重试次数 → 失败
    

#### 3.4 状态机

- Sender：Idle → Starting → Sending → Ending → Done/Failed
    
- Receiver：Idle → Ready → Receiving → Done/Failed
    

#### 3.5 流式容错

- 支持粘包/半包/失步重同步
    
- 滑动窗口=1（逐包 ACK）
    

#### 3.6 START 元数据（源文件名自动保存）

- 结构：
    

```
ver(1B)=1, flags(1B), name_len(2B), name(UTF-8), file_size(8B,LE)[, mtime(8B)]
```

- 接收端按源文件名保存（重名加后缀）
    
- 默认保存目录：用户可选，或 `%LOCALAPPDATA%\PortIO\Recv`
    

---

### 4. 架构与代码组织

- **ITransport 接口**：Open / Close / Read / Write / IsOpen / Configure
    
- **实现类**：SerialTransport、LptSpoolerTransport、UsbPrinterTransport、TcpTransport、UdpTransport、LoopbackTransport
    
- **协议层**：ReliableChannel
    
- **工具类**：FrameCodec、CRC32
    
- **I/O**：IOWorker（OVERLAPPED I/O）、RingBuffer（自动扩展）
    
- **MFC 对话框 UI**：
    
    - 端口选择下拉框
        
    - 参数配置面板
        
    - 十六进制 / 文本双视图（输入与接收）
        
    - 拖放文件（读取内容）与文件夹（枚举清单）
        
    - 协议状态视图
        
    - 日志窗口、进度条
        
    - **启动画面显示 PortMaster_Splash.png**
        

---

### 5. 配置与测试

- JSON 格式配置文件，存储所有端口参数与协议参数。
    
- 内置测试向导：
    
    - **阶段 1**：本地连接测试（发送“1234”验证）
        
    - **阶段 2**：串口设备测试（大文件传输 + 干扰恢复）
        
- 每次测试和阶段性开发后，执行 `autobuild.bat` → 检查日志 → 修正错误与警告为 0。
    

---

### 6. 非功能性与边界

- 仅使用微软标准库，CRC32 自研实现。
    
- LPT 原始 I/O 不可行（需驱动），仅 RAW 作业写出 + 状态查询。
    
- Win7 兼容性：避免使用 Win8+ API，或提供条件编译替代。
    
- 安全：默认主动连接，监听需用户确认。
    

---

### 7. 交付内容（按顺序）

1. 总体设计说明（中文）+ ASCII 类图/组件图
    
2. 协议规格表（含 START 元数据 TLV）
    
3. 发送端/接收端状态机图（ASCII）
    
4. 关键 C++ 源码（含资源加载与启动画面显示）
    
5. `.rc` 资源文件（包含 PortMaster_Icon.ico 与 PortMaster_Splash.png）
    
6. 配置保存/加载（JSON 示例与代码）
    
7. 内置测试向导实现
    
8. 构建指南（含资源添加方法）
    
9. 边界与风控清单
    
10. 后续扩展建议  
