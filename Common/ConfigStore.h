#pragma once
#pragma execution_character_set("utf-8")

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

// JSON处理库 - 使用简单的字符串操作实现
// 避免引入外部依赖，保持项目的轻量级特性

// 应用程序配置
struct AppConfig
{
    std::string version = "1.0.0";              // 应用版本
    std::string language = "zh-CN";             // 界面语言
    bool enableLogging = true;                  // 启用日志
    int logLevel = 2;                          // 日志级别 (0=Error, 1=Warning, 2=Info, 3=Debug)
    bool autoSave = true;                      // 自动保存配置
    int autoSaveInterval = 30;                 // 自动保存间隔(秒)
    
    AppConfig() = default;
};

// 串口配置
struct SerialPortConfig
{
    std::string portName = "COM1";             // 端口名
    DWORD baudRate = 9600;                     // 波特率
    BYTE dataBits = 8;                         // 数据位
    BYTE parity = NOPARITY;                    // 校验位
    BYTE stopBits = ONESTOPBIT;                // 停止位
    DWORD flowControl = 0;                     // 流控制
    DWORD readTimeout = 1000;                  // 读超时
    DWORD writeTimeout = 1000;                 // 写超时
    bool reliableMode = false;                 // 可靠模式
    
    SerialPortConfig() = default;
};

// 并口配置
struct ParallelPortConfig
{
    std::string portName = "LPT1";             // 端口名
    DWORD accessMode = 0;                      // 访问模式
    DWORD readTimeout = 2000;                  // 读超时
    DWORD writeTimeout = 2000;                 // 写超时
    bool reliableMode = false;                 // 可靠模式
    
    ParallelPortConfig() = default;
};

// USB打印配置
struct UsbPrintConfig
{
    std::string deviceName = "USB001";         // 设备名
    std::string deviceId;                      // 设备ID
    DWORD accessMode = 0;                      // 访问模式
    DWORD readTimeout = 2000;                  // 读超时
    DWORD writeTimeout = 2000;                 // 写超时
    bool reliableMode = false;                 // 可靠模式
    
    UsbPrintConfig() = default;
};

// 网络打印配置
struct NetworkPrintConfig
{
    std::string hostName = "192.168.1.100";   // 主机名/IP
    std::string protocol = "RAW";              // 协议类型 (RAW/LPR/IPP)
    DWORD port = 9100;                         // 端口号
    std::string queueName;                     // 队列名(LPR用)
    std::string userName;                      // 用户名
    std::string password;                      // 密码
    DWORD connectTimeout = 5000;               // 连接超时
    DWORD readTimeout = 10000;                 // 读超时
    DWORD writeTimeout = 10000;                // 写超时
    bool enableKeepAlive = true;               // 启用保活
    bool reliableMode = false;                 // 可靠模式
    
    NetworkPrintConfig() = default;
};

// 回路测试配置
struct LoopbackTestConfig
{
    DWORD delayMs = 10;                        // 延迟时间(ms)
    DWORD errorRate = 0;                       // 错误率(0-100%)
    DWORD packetLossRate = 0;                  // 丢包率(0-100%)
    bool enableJitter = false;                 // 启用抖动
    DWORD jitterMaxMs = 5;                     // 最大抖动(ms)
    DWORD maxQueueSize = 1000;                 // 最大队列大小
    bool autoTest = false;                     // 自动测试
    bool reliableMode = true;                  // 可靠模式
    
    LoopbackTestConfig() = default;
};

// 可靠传输协议配置
struct ReliableProtocolConfig
{
    BYTE version = 1;                          // 协议版本
    WORD windowSize = 4;                       // 滑动窗口大小
    WORD maxRetries = 3;                       // 最大重试次数
    DWORD timeoutBase = 500;                   // 基础超时时间(ms)
    DWORD timeoutMax = 2000;                   // 最大超时时间(ms)
    DWORD heartbeatInterval = 1000;            // 心跳间隔(ms)
    DWORD maxPayloadSize = 1024;               // 最大负载大小
    bool enableCompression = false;            // 启用压缩
    bool enableEncryption = false;             // 启用加密
    std::string encryptionKey;                 // 加密密钥
    
    ReliableProtocolConfig() = default;
};

// UI界面配置
struct UIConfig
{
    // 窗口状态
    int windowX = CW_USEDEFAULT;               // 窗口X坐标
    int windowY = CW_USEDEFAULT;               // 窗口Y坐标
    int windowWidth = 1000;                    // 窗口宽度
    int windowHeight = 700;                    // 窗口高度
    bool maximized = false;                    // 最大化状态
    
    // 控件状态
    bool hexDisplay = false;                   // 十六进制显示
    bool autoScroll = true;                    // 自动滚动
    bool wordWrap = true;                      // 自动换行
    std::string lastPortType = "串口";          // 最后使用的端口类型
    std::string lastPortName = "COM1";         // 最后使用的端口名
    
    // 最近文件列表
    std::vector<std::string> recentFiles;      // 最近打开的文件
    int maxRecentFiles = 10;                   // 最大最近文件数
    
    UIConfig() = default;
};

// 完整配置结构
struct PortMasterConfig
{
    AppConfig app;                             // 应用程序配置
    SerialPortConfig serial;                   // 串口配置
    ParallelPortConfig parallel;               // 并口配置
    UsbPrintConfig usb;                        // USB打印配置
    NetworkPrintConfig network;                // 网络打印配置
    LoopbackTestConfig loopback;               // 回路测试配置
    ReliableProtocolConfig protocol;           // 可靠协议配置
    UIConfig ui;                               // UI配置
    
    PortMasterConfig() = default;
};

// 配置存储管理器
class ConfigStore
{
public:
    // 构造函数和析构函数
    ConfigStore();
    ~ConfigStore();
    
    // 单例模式
    static ConfigStore& GetInstance();
    
    // 配置加载和保存
    bool LoadConfig();
    bool SaveConfig();
    bool SaveConfigAs(const std::string& filePath);
    
    // 配置访问
    const PortMasterConfig& GetConfig() const;
    void SetConfig(const PortMasterConfig& config);
    
    // 分项配置访问
    const AppConfig& GetAppConfig() const;
    const SerialPortConfig& GetSerialConfig() const;
    const ParallelPortConfig& GetParallelConfig() const;
    const UsbPrintConfig& GetUsbConfig() const;
    const NetworkPrintConfig& GetNetworkConfig() const;
    const LoopbackTestConfig& GetLoopbackConfig() const;
    const ReliableProtocolConfig& GetProtocolConfig() const;
    const UIConfig& GetUIConfig() const;
    
    void SetAppConfig(const AppConfig& config);
    void SetSerialConfig(const SerialPortConfig& config);
    void SetParallelConfig(const ParallelPortConfig& config);
    void SetUsbConfig(const UsbPrintConfig& config);
    void SetNetworkConfig(const NetworkPrintConfig& config);
    void SetLoopbackConfig(const LoopbackTestConfig& config);
    void SetProtocolConfig(const ReliableProtocolConfig& config);
    void SetUIConfig(const UIConfig& config);
    
    // 路径管理
    std::string GetConfigFilePath() const;
    std::string GetConfigDirectory() const;
    std::string GetBackupFilePath() const;
    
    // 配置验证和修复
    bool ValidateConfig();
    bool RepairConfig();
    void ResetToDefaults();
    
    // 最近文件管理
    void AddRecentFile(const std::string& filePath);
    void RemoveRecentFile(const std::string& filePath);
    void ClearRecentFiles();
    std::vector<std::string> GetRecentFiles() const;
    
    // 配置导入导出
    bool ExportConfig(const std::string& filePath) const;
    bool ImportConfig(const std::string& filePath);
    
    // 自动保存管理
    void EnableAutoSave(bool enable);
    bool IsAutoSaveEnabled() const;
    void SetAutoSaveInterval(int seconds);
    int GetAutoSaveInterval() const;
    
    // 配置变更通知
    typedef std::function<void(const std::string&)> ConfigChangedCallback;
    void SetConfigChangedCallback(ConfigChangedCallback callback);
    
private:
    // 内部成员
    PortMasterConfig m_config;                 // 配置数据
    mutable std::mutex m_mutex;                // 线程安全锁
    std::string m_configFilePath;              // 配置文件路径
    std::string m_backupFilePath;              // 备份文件路径
    bool m_autoSaveEnabled;                    // 自动保存开关
    int m_autoSaveInterval;                    // 自动保存间隔
    HANDLE m_autoSaveTimer;                    // 自动保存定时器
    ConfigChangedCallback m_configChangedCallback; // 配置变更回调
    
    // 内部方法
    bool LoadConfigFromFile(const std::string& filePath);
    bool SaveConfigToFile(const std::string& filePath) const;
    std::string FindConfigPath() const;
    bool CreateConfigDirectory(const std::string& path) const;
    bool BackupConfig() const;
    bool RestoreFromBackup();
    
    // JSON序列化
    std::string SerializeToJson() const;
    bool DeserializeFromJson(const std::string& jsonStr);
    
    // JSON处理辅助函数
    std::string EscapeJsonString(const std::string& str) const;
    std::string UnescapeJsonString(const std::string& str) const;
    std::string GetJsonValue(const std::string& json, const std::string& key) const;
    std::string GetJsonObject(const std::string& json, const std::string& key) const;
    std::vector<std::string> GetJsonArray(const std::string& json, const std::string& key) const;
    
    // 数据类型转换
    std::string BoolToString(bool value) const;
    bool StringToBool(const std::string& str) const;
    std::string IntToString(int value) const;
    int StringToInt(const std::string& str) const;
    std::string DwordToString(DWORD value) const;
    DWORD StringToDword(const std::string& str) const;
    
    // 配置验证辅助
    bool ValidatePortName(const std::string& portName, const std::string& type) const;
    bool ValidateIPAddress(const std::string& ip) const;
    bool ValidateRange(int value, int min, int max) const;
    
    // 自动保存相关
    static void CALLBACK AutoSaveTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    void TriggerAutoSave();
    
    // 禁用拷贝构造和赋值
    ConfigStore(const ConfigStore&) = delete;
    ConfigStore& operator=(const ConfigStore&) = delete;
    
    // 静态实例
    static std::unique_ptr<ConfigStore> s_instance;
    static std::mutex s_instanceMutex;
};