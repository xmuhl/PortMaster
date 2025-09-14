#pragma execution_character_set("utf-8")
#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString

// 前置声明
class ITransport;
class ReliableChannel;
class DeviceManager;
class ProtocolManager;
struct TransportConfig;

/**
 * @brief 传输模式枚举
 */
enum class TransportMode
{
    DIRECT = 0,      // 直接传输模式
    RELIABLE         // 可靠传输模式
};

/**
 * @brief 传输状态枚举
 * SOLID-O: 开放封闭原则 - 易于扩展新的状态类型
 */
enum class TransportOperationState
{
    IDLE = 0,        // 空闲状态
    CONNECTING,      // 连接中
    CONNECTED,       // 已连接
    TRANSMITTING,    // 传输中
    PAUSED,          // 传输暂停
    DISCONNECTING,   // 断开连接中
    TRANSPORT_ERROR, // 错误状态
    COMPLETED        // 传输完成
};

/**
 * @brief 传输配置结构
 * SOLID-S: 单一职责原则 - 专门管理传输配置
 */
struct TransportConfiguration
{
    int transportType;           // 传输类型索引
    std::string endpoint;        // 连接端点（端口/地址等）
    int baudRate;               // 波特率（串口）
    int dataBits;               // 数据位（串口）
    int stopBits;               // 停止位（串口）
    int parity;                 // 校验位（串口）
    TransportMode mode;         // 传输模式
    bool autoRetry;             // 自动重试
    int maxRetries;             // 最大重试次数
    
    // 构造函数：初始化默认值
    TransportConfiguration()
        : transportType(0)
        , baudRate(9600)
        , dataBits(8)
        , stopBits(1)
        , parity(0)
        , mode(TransportMode::DIRECT)
        , autoRetry(false)
        , maxRetries(3)
    {
    }
};

/**
 * @brief 传输事件回调接口
 * SOLID-I: 接口隔离原则 - 分离不同类型的事件回调
 */
class ITransportEventCallback
{
public:
    virtual ~ITransportEventCallback() = default;
    
    /**
     * @brief 连接状态变化回调
     * @param connected 是否已连接
     * @param errorMsg 错误消息（如有）
     */
    virtual void OnConnectionStateChanged(bool connected, const std::string& errorMsg = "") = 0;
    
    /**
     * @brief 数据接收回调
     * @param data 接收到的数据
     */
    virtual void OnDataReceived(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief 传输进度回调
     * @param bytesTransmitted 已传输字节数
     * @param totalBytes 总字节数
     */
    virtual void OnTransmissionProgress(size_t bytesTransmitted, size_t totalBytes) = 0;
    
    /**
     * @brief 传输完成回调
     * @param success 是否成功
     * @param errorMsg 错误消息（如有）
     */
    virtual void OnTransmissionComplete(bool success, const std::string& errorMsg = "") = 0;
    
    /**
     * @brief 状态变化回调
     * @param state 新的状态
     * @param message 状态描述消息
     */
    virtual void OnStateChanged(TransportOperationState state, const std::string& message = "") = 0;
};

/**
 * @brief 统一传输管理器
 * SOLID-S: 单一职责原则 - 专门负责传输层管理
 * SOLID-D: 依赖倒置原则 - 依赖于传输接口抽象
 */
class TransportManager
{
public:
    /**
     * @brief 构造函数
     * @param deviceManager 设备管理器
     * @param protocolManager 协议管理器
     */
    TransportManager(std::shared_ptr<DeviceManager> deviceManager,
                     std::shared_ptr<ProtocolManager> protocolManager);
    
    /**
     * @brief 析构函数
     */
    ~TransportManager();

    // 禁用拷贝构造和拷贝赋值 - RAII原则
    TransportManager(const TransportManager&) = delete;
    TransportManager& operator=(const TransportManager&) = delete;
    
    /**
     * @brief 设置事件回调
     * @param callback 事件回调接口
     */
    void SetEventCallback(std::shared_ptr<ITransportEventCallback> callback);
    
    /**
     * @brief 配置传输参数
     * @param config 传输配置
     * @return 配置是否成功
     */
    bool ConfigureTransport(const TransportConfiguration& config);
    
    /**
     * @brief 连接传输
     * @return 连接是否成功
     */
    bool Connect();
    
    /**
     * @brief 断开传输连接
     */
    void Disconnect();
    
    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 发送是否成功
     */
    bool SendData(const std::vector<uint8_t>& data);
    
    /**
     * @brief 发送文件
     * @param filePath 文件路径
     * @return 发送是否成功
     */
    bool SendFile(const std::string& filePath);
    
    /**
     * @brief 停止当前传输
     */
    void StopTransmission();
    
    /**
     * @brief 暂停/恢复传输
     * @param pause true为暂停，false为恢复
     */
    void PauseResumeTransmission(bool pause);
    
    /**
     * @brief 获取连接状态
     * @return 是否已连接
     */
    bool IsConnected() const;
    
    /**
     * @brief 获取传输状态
     * @return 当前传输状态
     */
    TransportOperationState GetTransmissionState() const;
    
    /**
     * @brief 获取当前配置
     * @return 当前传输配置
     */
    TransportConfiguration GetCurrentConfig() const;
    
    /**
     * @brief 获取传输统计信息
     * @param bytesTransmitted 已传输字节数
     * @param totalBytes 总字节数
     * @param transferRate 传输速率（字节/秒）
     */
    void GetTransmissionStats(size_t& bytesTransmitted, size_t& totalBytes, double& transferRate) const;
    
    /**
     * @brief 获取连接信息字符串
     * @return 连接信息描述
     */
    std::string GetConnectionInfo() const;
    
    /**
     * @brief 获取错误信息
     * @return 最后的错误信息
     */
    std::string GetLastError() const;
    
    /**
     * @brief 重置传输状态
     */
    void Reset();

private:
    // 核心组件
    std::shared_ptr<DeviceManager> m_deviceManager;      // 设备管理器
    std::shared_ptr<ProtocolManager> m_protocolManager;  // 协议管理器
    std::shared_ptr<ITransport> m_transport;             // 当前传输对象
    std::shared_ptr<ReliableChannel> m_reliableChannel;  // 可靠传输通道
    std::shared_ptr<ITransportEventCallback> m_callback; // 事件回调
    
    // 配置和状态
    TransportConfiguration m_config;                     // 当前配置
    std::atomic<TransportOperationState> m_state;       // 当前状态
    std::atomic<bool> m_connected;                       // 连接状态
    std::atomic<bool> m_transmitting;                    // 传输状态
    std::atomic<bool> m_paused;                          // 暂停状态
    
    // 传输统计
    std::atomic<size_t> m_bytesTransmitted;              // 已传输字节数
    std::atomic<size_t> m_totalBytes;                    // 总字节数
    std::chrono::steady_clock::time_point m_transferStartTime; // 传输开始时间
    
    // 错误处理
    mutable std::mutex m_errorMutex;                     // 错误信息互斥锁
    std::string m_lastError;                             // 最后错误信息
    
    // 线程安全保护
    mutable std::mutex m_stateMutex;                     // 状态访问互斥锁
    mutable std::mutex m_configMutex;                    // 配置访问互斥锁
    
    // 私有辅助方法
    TransportConfig ConvertToTransportConfig(const TransportConfiguration& config);
    std::shared_ptr<ITransport> CreateTransportInstance(const TransportConfiguration& config);
    void SetupTransportCallbacks();
    void SetupReliableChannelCallbacks();
    void SetTransmissionState(TransportOperationState newState);
    void ReportError(const std::string& operation, const std::string& error);
    void UpdateTransferStats(size_t bytesTransferred);
    void CleanupTransportObjects();
    
    // 私有辅助方法 - 从UI控件获取配置
    std::shared_ptr<ITransport> CreateTransportFromIndex(int transportIndex);
    TransportConfiguration GetTransportConfigFromControls(int transportIndex, 
                                                         const std::string& portName,
                                                         const std::string& baudRate = "",
                                                         const std::string& dataBits = "",
                                                         int parityIndex = -1,
                                                         int stopBitsIndex = -1,
                                                         const std::string& endpoint = "");

public:
    // 🔑 架构重构：从PortMasterDlg转移的传输工厂方法
    /**
     * @brief 从UI控件创建传输对象（从PortMasterDlg::CreateTransportFromUI转移）
     * @param transportIndex 传输类型索引
     * @return 传输对象智能指针
     */
    std::shared_ptr<ITransport> CreateTransportFromUI(int transportIndex);
    
    /**
     * @brief 从UI控件获取传输配置（从PortMasterDlg::GetTransportConfigFromUI转移）
     * @param transportIndex 传输类型索引
     * @param portName 端口名称
     * @param baudRate 波特率字符串
     * @param dataBits 数据位字符串
     * @param parityIndex 校验位索引
     * @param stopBitsIndex 停止位索引
     * @param endpoint 网络端点
     * @return TransportConfig配置对象
     */
    TransportConfig GetTransportConfigFromUI(int transportIndex,
                                            const std::string& portName,
                                            const std::string& baudRate = "",
                                            const std::string& dataBits = "",
                                            int parityIndex = -1,
                                            int stopBitsIndex = -1,
                                            const std::string& endpoint = "");

private:
    
    // 回调处理方法
    void HandleTransportCallback(const std::vector<uint8_t>& data);
    void HandleReliableChannelCallback(const std::vector<uint8_t>& data);
};

/**
 * @brief 传输管理器工厂类
 * SOLID-O: 开放封闭原则 - 支持扩展不同的管理器类型
 */
class TransportManagerFactory
{
public:
    /**
     * @brief 创建默认的传输管理器
     * @param deviceManager 设备管理器
     * @param protocolManager 协议管理器
     * @return 传输管理器实例
     */
    static std::unique_ptr<TransportManager> Create(
        std::shared_ptr<DeviceManager> deviceManager,
        std::shared_ptr<ProtocolManager> protocolManager);
};