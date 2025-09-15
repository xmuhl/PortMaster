#pragma execution_character_set("utf-8")
#include "pch.h"
#include "TransportManager.h"
#include "DeviceManager.h"
#include "../Protocol/ProtocolManager.h"
#include "../Transport/ITransport.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/LoopbackTransport.h"
#include "../Transport/LptSpoolerTransport.h"
#include "../Transport/UsbPrinterTransport.h"
#include "../Transport/TcpTransport.h"
#include "../Transport/UdpTransport.h"
#include "../Protocol/ReliableChannel.h"
#include <algorithm>
#include <fstream>

extern void WriteDebugLog(const char* message);

// TransportManager 简化实现（第一阶段）

TransportManager::TransportManager(std::shared_ptr<DeviceManager> deviceManager,
                                 std::shared_ptr<ProtocolManager> protocolManager)
    : m_deviceManager(deviceManager)
    , m_protocolManager(protocolManager)
    , m_state(TransportOperationState::IDLE)
    , m_connected(false)
    , m_transmitting(false)
    , m_paused(false)
    , m_bytesTransmitted(0)
    , m_totalBytes(0)
{
    WriteDebugLog("[DEBUG] TransportManager构造开始");
    
    // 🔴 性能优化：移除启动时的重型初始化操作
    // 延迟初始化策略：仅在实际需要时才创建传输对象
    
    WriteDebugLog("[DEBUG] TransportManager构造完成 - 快速启动模式");
}

TransportManager::~TransportManager()
{
    try {
        WriteDebugLog("[DEBUG] TransportManager析构完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] TransportManager析构异常");
    }
}

void TransportManager::SetEventCallback(std::shared_ptr<ITransportEventCallback> callback)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_callback = callback;
    WriteDebugLog("[DEBUG] TransportManager::SetEventCallback: 事件回调已设置");
}

bool TransportManager::ConfigureTransport(const TransportConfiguration& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    try {
        m_config = config;
        
        // 创建基础传输实例
        m_transport = CreateTransportInstance(config);
        if (!m_transport) {
            ReportError("配置传输", "无法创建传输实例");
            return false;
        }
        
        WriteDebugLog("[DEBUG] TransportManager::ConfigureTransport: 传输配置成功");
        return true;
    }
    catch (const std::exception& e) {
        ReportError("配置传输", e.what());
        return false;
    }
}

bool TransportManager::Connect()
{
    if (!m_transport) {
        ReportError("连接传输", "传输对象未配置");
        return false;
    }
    
    try {
        SetTransmissionState(TransportOperationState::CONNECTING);
        
        // 转换配置格式
        TransportConfig config = ConvertToTransportConfig(m_config);
        
        // 设置传输回调
        SetupTransportCallbacks();
        
        // 使用ITransport接口的Open方法
        if (!m_transport->Open(config)) {
            ReportError("连接传输", "传输连接失败");
            SetTransmissionState(TransportOperationState::TRANSPORT_ERROR);
            return false;
        }
        
        m_connected = true;
        SetTransmissionState(TransportOperationState::CONNECTED);
        
        WriteDebugLog("[DEBUG] TransportManager::Connect: 连接成功");
        return true;
    }
    catch (const std::exception& e) {
        ReportError("连接传输", e.what());
        SetTransmissionState(TransportOperationState::TRANSPORT_ERROR);
        return false;
    }
}

void TransportManager::Disconnect()
{
    try {
        SetTransmissionState(TransportOperationState::DISCONNECTING);
        
        if (m_transport) {
            m_transport->Close();  // 使用正确的方法名
        }
        
        m_connected = false;
        SetTransmissionState(TransportOperationState::IDLE);
        
        WriteDebugLog("[DEBUG] TransportManager::Disconnect: 断开连接完成");
    }
    catch (const std::exception& e) {
        ReportError("断开连接", e.what());
    }
}

bool TransportManager::SendData(const std::vector<uint8_t>& data)
{
    if (!m_connected.load()) {
        ReportError("发送数据", "传输未连接");
        return false;
    }
    
    if (data.empty()) {
        WriteDebugLog("[WARNING] TransportManager::SendData: 发送数据为空");
        return true;
    }
    
    try {
        SetTransmissionState(TransportOperationState::TRANSMITTING);
        m_transmitting = true;
        
        // 使用ITransport的Write方法
        size_t written = m_transport->Write(data);
        bool success = (written == data.size());
        
        if (success) {
            SetTransmissionState(TransportOperationState::COMPLETED);
            WriteDebugLog("[DEBUG] TransportManager::SendData: 数据发送成功");
        } else {
            ReportError("发送数据", "传输发送失败");
            SetTransmissionState(TransportOperationState::TRANSPORT_ERROR);
        }
        
        m_transmitting = false;
        return success;
    }
    catch (const std::exception& e) {
        m_transmitting = false;
        ReportError("发送数据", e.what());
        SetTransmissionState(TransportOperationState::TRANSPORT_ERROR);
        return false;
    }
}

bool TransportManager::SendFile(const std::string& filePath)
{
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            ReportError("发送文件", "无法打开文件: " + filePath);
            return false;
        }
        
        std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
        file.close();
        
        if (fileData.empty()) {
            WriteDebugLog("[WARNING] TransportManager::SendFile: 文件为空");
            return true;
        }
        
        WriteDebugLog("[DEBUG] TransportManager::SendFile: 开始发送文件");
        return SendData(fileData);
    }
    catch (const std::exception& e) {
        ReportError("发送文件", e.what());
        return false;
    }
}

void TransportManager::StopTransmission()
{
    try {
        m_transmitting = false;
        m_paused = false;
        
        if (m_connected.load()) {
            SetTransmissionState(TransportOperationState::CONNECTED);
        } else {
            SetTransmissionState(TransportOperationState::IDLE);
        }
        
        WriteDebugLog("[DEBUG] TransportManager::StopTransmission: 传输已停止");
    }
    catch (const std::exception& e) {
        ReportError("停止传输", e.what());
    }
}

void TransportManager::PauseResumeTransmission(bool pause)
{
    m_paused = pause;
    WriteDebugLog(pause ? "[DEBUG] 传输已暂停" : "[DEBUG] 传输已恢复");
}

bool TransportManager::IsConnected() const
{
    return m_connected.load();
}

TransportOperationState TransportManager::GetTransmissionState() const
{
    return m_state.load();
}

TransportConfiguration TransportManager::GetCurrentConfig() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void TransportManager::GetTransmissionStats(size_t& bytesTransmitted, size_t& totalBytes, double& transferRate) const
{
    bytesTransmitted = m_bytesTransmitted.load();
    totalBytes = m_totalBytes.load();
    transferRate = 0.0; // 简化实现
}

std::string TransportManager::GetConnectionInfo() const
{
    return "传输信息"; // 简化实现
}

std::string TransportManager::GetLastError() const
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void TransportManager::Reset()
{
    try {
        if (m_connected.load()) {
            Disconnect();
        }
        
        m_transmitting = false;
        m_paused = false;
        m_bytesTransmitted = 0;
        m_totalBytes = 0;
        
        {
            std::lock_guard<std::mutex> lock(m_errorMutex);
            m_lastError.clear();
        }
        
        SetTransmissionState(TransportOperationState::IDLE);
        WriteDebugLog("[DEBUG] TransportManager::Reset: 重置完成");
    }
    catch (const std::exception& e) {
        ReportError("重置传输管理器", e.what());
    }
}

// 私有方法实现

TransportConfig TransportManager::ConvertToTransportConfig(const TransportConfiguration& config)
{
    TransportConfig transportConfig;
    
    // 设置端点信息
    transportConfig.portName = config.endpoint;
    
    // 设置串口参数
    transportConfig.baudRate = config.baudRate;
    transportConfig.dataBits = config.dataBits;
    transportConfig.stopBits = config.stopBits;
    transportConfig.parity = config.parity;
    
    // 设置网络参数（如果适用）
    // transportConfig.host = config.endpoint;  // 根据需要设置
    // transportConfig.port = 8080;             // 根据需要设置
    
    WriteDebugLog("[DEBUG] TransportManager::ConvertToTransportConfig: 配置转换完成");
    return transportConfig;
}

std::shared_ptr<ITransport> TransportManager::CreateTransportInstance(const TransportConfiguration& config)
{
    try {
        switch (config.transportType) {
        case 0: // 串口
            return std::make_shared<SerialTransport>();
        case 5: // 回环
            return std::make_shared<LoopbackTransport>();
        default:
            WriteDebugLog("[ERROR] TransportManager::CreateTransportInstance: 未知传输类型");
            return nullptr;
        }
    }
    catch (const std::exception& e) {
        ReportError("创建传输实例", e.what());
        return nullptr;
    }
}

void TransportManager::SetupTransportCallbacks()
{
    if (!m_transport) {
        return;
    }
    
    // 使用正确的回调设置方法
    m_transport->SetDataReceivedCallback([this](const std::vector<uint8_t>& data) {
        HandleTransportCallback(data);
    });
}

void TransportManager::SetupReliableChannelCallbacks()
{
    // 简化实现 - 预留接口
}

void TransportManager::SetTransmissionState(TransportOperationState newState)
{
    m_state = newState;
    WriteDebugLog("[DEBUG] TransportManager状态变化");
}

void TransportManager::ReportError(const std::string& operation, const std::string& error)
{
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_lastError = operation + ": " + error;
    }
    
    WriteDebugLog("[ERROR] TransportManager错误");
}

void TransportManager::UpdateTransferStats(size_t bytesTransferred)
{
    m_bytesTransmitted.fetch_add(bytesTransferred);
}

void TransportManager::CleanupTransportObjects()
{
    try {
        if (m_transmitting.load()) {
            StopTransmission();
        }
        
        if (m_connected.load()) {
            Disconnect();
        }
        
        m_transport.reset();
        m_reliableChannel.reset();
        
        WriteDebugLog("[DEBUG] TransportManager::CleanupTransportObjects: 清理完成");
    }
    catch (const std::exception& e) {
        ReportError("清理传输对象", e.what());
    }
}

void TransportManager::HandleTransportCallback(const std::vector<uint8_t>& data)
{
    if (m_callback) {
        m_callback->OnDataReceived(data);
    }
}

void TransportManager::HandleReliableChannelCallback(const std::vector<uint8_t>& data)
{
    if (m_callback) {
        m_callback->OnDataReceived(data);
    }
}

// 🔑 架构重构：从PortMasterDlg转移的传输工厂方法实现

std::shared_ptr<ITransport> TransportManager::CreateTransportFromUI(int transportIndex)
{
    // 直接调用现有的CreateTransportFromIndex方法
    return CreateTransportFromIndex(transportIndex);
}

TransportConfig TransportManager::GetTransportConfigFromUI(int transportIndex,
                                                          const std::string& portName,
                                                          const std::string& baudRate,
                                                          const std::string& dataBits,
                                                          int parityIndex,
                                                          int stopBitsIndex,
                                                          const std::string& endpoint)
{
    TransportConfig config; // 使用默认构造函数提供的基础默认值
    
    // SOLID-S: 单一职责 - 分类型配置采集
    switch (transportIndex)
    {
    case 0: // 串口
        {
            // 端口名称
            config.portName = portName;
            
            // 波特率
            if (!baudRate.empty()) {
                config.baudRate = std::stoi(baudRate);
            }
            
            // 数据位
            if (!dataBits.empty()) {
                config.dataBits = std::stoi(dataBits);
            }
            
            // 校验位
            if (parityIndex != -1) {
                config.parity = parityIndex; // 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
            }
            
            // 停止位
            if (stopBitsIndex != -1) {
                config.stopBits = (stopBitsIndex == 0) ? 1 : ((stopBitsIndex == 1) ? 2 : 2); // 1, 1.5->2, 2
            }
        }
        break;
        
    case 1: // 并口
    case 2: // USB打印机
        {
            // 获取打印机名称
            config.portName = portName;
        }
        break;
        
    case 3: // TCP客户端
        {
            config.isServer = false;
            
            // 从端点解析IP:端口
            if (!endpoint.empty()) {
                size_t colonPos = endpoint.find(':');
                if (colonPos != std::string::npos) {
                    config.ipAddress = endpoint.substr(0, colonPos);
                    config.port = std::stoi(endpoint.substr(colonPos + 1));
                } else {
                    // 默认值
                    config.ipAddress = "127.0.0.1";
                    config.port = 8080;
                }
            } else {
                config.ipAddress = "127.0.0.1";
                config.port = 8080;
            }
        }
        break;
        
    case 4: // TCP服务器
        {
            config.isServer = true;
            
            // 从端点解析端口
            if (!endpoint.empty()) {
                size_t colonPos = endpoint.find(':');
                if (colonPos != std::string::npos) {
                    config.port = std::stoi(endpoint.substr(colonPos + 1));
                } else {
                    config.port = 8080;
                }
            } else {
                config.port = 8080;
            }
            
            // 服务器绑定到所有接口
            config.ipAddress = "0.0.0.0";
        }
        break;
        
    case 5: // UDP
        {
            // 从端点解析端口
            if (!endpoint.empty()) {
                size_t colonPos = endpoint.find(':');
                if (colonPos != std::string::npos) {
                    config.port = std::stoi(endpoint.substr(colonPos + 1));
                } else {
                    config.port = 8080;
                }
            } else {
                config.port = 8080;
            }
            
            // UDP默认配置
            config.ipAddress = "127.0.0.1";
        }
        break;
        
    case 6: // 回环测试
        {
            // 回环测试使用默认配置
            config.portName = "loopback";
        }
        break;
    }
    
    WriteDebugLog("[DEBUG] TransportManager::GetTransportConfigFromUI: 配置获取完成");
    return config;
}

// TransportManagerFactory 实现

// 🔑 架构重构：从UI创建传输工厂逻辑 (从PortMasterDlg转移)
std::shared_ptr<ITransport> TransportManager::CreateTransportFromIndex(int transportIndex)
{
    try {
        // SOLID-O: 开闭原则 - 可扩展的传输类型工厂
        switch (transportIndex)
        {
        case 0: // 串口
            return std::make_shared<SerialTransport>();
            
        case 1: // 并口 
            return std::make_shared<LptSpoolerTransport>();
            
        case 2: // USB打印机
            return std::make_shared<UsbPrinterTransport>();
            
        case 3: // TCP客户端
            {
                auto tcp = std::make_shared<TcpTransport>();
                return tcp;
            }
            
        case 4: // TCP服务器
            {
                auto tcp = std::make_shared<TcpTransport>();
                return tcp;
            }
            
        case 5: // UDP
            return std::make_shared<UdpTransport>();
            
        case 6: // 回环测试
            return std::make_shared<LoopbackTransport>();
            
        default:
            WriteDebugLog("[ERROR] TransportManager::CreateTransportFromIndex: 无效的传输类型索引");
            return nullptr;
        }
    }
    catch (const std::exception& e) {
        ReportError("创建传输实例", e.what());
        return nullptr;
    }
}

TransportConfiguration TransportManager::GetTransportConfigFromControls(int transportIndex, 
                                                                       const std::string& portName,
                                                                       const std::string& baudRate,
                                                                       const std::string& dataBits,
                                                                       int parityIndex,
                                                                       int stopBitsIndex,
                                                                       const std::string& endpoint)
{
    TransportConfiguration config; // 使用默认构造函数提供的基础默认值
    
    try {
        // SOLID-S: 单一职责 - 分类型配置采集
        switch (transportIndex)
        {
        case 0: // 串口
            {
                config.endpoint = portName;
                
                // 波特率
                if (!baudRate.empty()) {
                    config.baudRate = std::stoi(baudRate);
                }
                
                // 数据位
                if (!dataBits.empty()) {
                    config.dataBits = std::stoi(dataBits);
                }
                
                // 校验位
                if (parityIndex >= 0) {
                    config.parity = parityIndex; // 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
                }
                
                // 停止位
                if (stopBitsIndex >= 0) {
                    config.stopBits = (stopBitsIndex == 0) ? 1 : ((stopBitsIndex == 1) ? 2 : 2); // 1, 1.5->2, 2
                }
            }
            break;
            
        case 1: // 并口
        case 2: // USB打印机
            {
                config.endpoint = portName;
            }
            break;
            
        case 3: // TCP客户端
            {
                config.mode = TransportMode::DIRECT;
                
                // 从端点解析IP:端口
                if (!endpoint.empty()) {
                    size_t colonPos = endpoint.find(':');
                    if (colonPos != std::string::npos) {
                        std::string ipStr = endpoint.substr(0, colonPos);
                        std::string portStr = endpoint.substr(colonPos + 1);
                        
                        config.endpoint = ipStr + ":" + portStr;
                    } else {
                        config.endpoint = "127.0.0.1:8080";
                    }
                } else {
                    config.endpoint = "127.0.0.1:8080";
                }
            }
            break;
            
        case 4: // TCP服务器
            {
                config.mode = TransportMode::DIRECT;
                
                // 从端点解析端口
                if (!endpoint.empty()) {
                    size_t colonPos = endpoint.find(':');
                    if (colonPos != std::string::npos) {
                        std::string portStr = endpoint.substr(colonPos + 1);
                        config.endpoint = "0.0.0.0:" + portStr;
                    } else {
                        config.endpoint = "0.0.0.0:8080";
                    }
                } else {
                    config.endpoint = "0.0.0.0:8080";
                }
            }
            break;
            
        case 5: // UDP
            {
                // 从端点解析端口
                if (!endpoint.empty()) {
                    size_t colonPos = endpoint.find(':');
                    if (colonPos != std::string::npos) {
                        std::string portStr = endpoint.substr(colonPos + 1);
                        config.endpoint = "127.0.0.1:" + portStr;
                    } else {
                        config.endpoint = "127.0.0.1:8080";
                    }
                } else {
                    config.endpoint = "127.0.0.1:8080";
                }
            }
            break;
            
        case 6: // 回环测试
            {
                config.endpoint = "loopback";
            }
            break;
        }
        
        WriteDebugLog("[DEBUG] TransportManager::GetTransportConfigFromControls: 配置创建成功");
        return config;
    }
    catch (const std::exception& e) {
        ReportError("获取传输配置", e.what());
        return config; // 返回默认配置
    }
}

// 🔑 架构重构：从PortMasterDlg迁移的工具函数 (SOLID-S: 单一职责)
CString TransportManager::FormatTransportInfo(const std::string& transportType, const std::string& endpoint)
{
    // KISS原则：保持简单的字符串格式化逻辑
    CString typeMsg = CA2W(transportType.c_str(), CP_UTF8);
    
    if (endpoint.empty())
    {
        return typeMsg + L" 连接";
    }
    else
    {
        CString endpointMsg = CA2W(endpoint.c_str(), CP_UTF8);
        return typeMsg + L" (" + endpointMsg + L")";
    }
}

CString TransportManager::GetDetailedErrorSuggestion(int transportIndex, const std::string& error)
{
    // SOLID-S: 单一职责 - 使用静态映射避免UI依赖 (YAGNI: 仅实现必要的传输类型)
    static const wchar_t* transportTypes[] = {
        L"串口", L"TCP客户端", L"TCP服务器", L"UDP", L"并口", L"USB打印机", L"回环测试"
    };
    
    CString transportType = L"";
    if (transportIndex >= 0 && transportIndex < _countof(transportTypes))
    {
        transportType = transportTypes[transportIndex];
    }
    
    CString errorMsg = CA2W(error.c_str(), CP_UTF8);
    errorMsg.MakeLower();
    
    // 串口相关错误建议
    if (transportType == L"串口")
    {
        if (errorMsg.Find(L"access") != -1 || errorMsg.Find(L"占用") != -1)
        {
            return L"串口被其他程序占用，请关闭相关程序后重试";
        }
        else if (errorMsg.Find(L"find") != -1 || errorMsg.Find(L"exist") != -1)
        {
            return L"串口不存在，请检查设备连接并刷新端口列表";
        }
        else if (errorMsg.Find(L"parameter") != -1 || errorMsg.Find(L"baud") != -1)
        {
            return L"串口参数配置错误，请检查波特率、数据位等设置";
        }
        return L"请检查串口连接、权限和参数配置";
    }
    // 网络相关错误建议
    else if (transportType == L"TCP客户端" || transportType == L"TCP服务器")
    {
        if (errorMsg.Find(L"connect") != -1 || errorMsg.Find(L"connection") != -1)
        {
            return L"无法建立TCP连接，请检查IP地址、端口号和网络状况";
        }
        else if (errorMsg.Find(L"bind") != -1 || errorMsg.Find(L"address") != -1)
        {
            return L"TCP端口绑定失败，请检查端口是否被占用或更换端口";
        }
        else if (errorMsg.Find(L"timeout") != -1)
        {
            return L"连接超时，请检查网络连通性和防火墙设置";
        }
        return L"请检查网络配置、防火墙设置和目标设备状态";
    }
    else if (transportType == L"UDP")
    {
        if (errorMsg.Find(L"bind") != -1)
        {
            return L"UDP端口绑定失败，请更换端口或检查权限";
        }
        else if (errorMsg.Find(L"address") != -1)
        {
            return L"UDP地址配置错误，请检查IP地址和端口设置";
        }
        return L"请检查UDP端口配置和网络权限";
    }
    // 打印机相关错误建议
    else if (transportType == L"并口" || transportType == L"USB打印机")
    {
        if (errorMsg.Find(L"printer") != -1 || errorMsg.Find(L"打印") != -1)
        {
            return L"打印机不可用，请检查设备连接和驱动安装";
        }
        else if (errorMsg.Find(L"access") != -1 || errorMsg.Find(L"permission") != -1)
        {
            return L"打印机访问权限不足，请以管理员身份运行程序";
        }
        return L"请检查打印机连接、权限和驱动程序";
    }
    // 回环测试
    else if (transportType == L"回环测试")
    {
        return L"回环测试失败，请检查程序配置和系统资源";
    }
    
    // 通用错误建议
    return L"请检查设备连接和配置参数，或联系技术支持";
}

CString TransportManager::GetConnectionStatusMessage(TransportState state, const std::string& error)
{
    // SOLID-S: 单一职责 - 状态到消息的纯映射函数
    switch (state)
    {
    case TRANSPORT_CLOSED:
        return L"未连接";
    case TRANSPORT_OPENING:
        return L"连接中...";
    case TRANSPORT_OPEN:
        return L"已连接";
    case TRANSPORT_CLOSING:
        return L"断开中...";
    case TRANSPORT_ERROR:
        {
            if (error.empty())
                return L"连接错误";
            CString errorMsg = CA2W(error.c_str(), CP_UTF8);
            return L"错误: " + errorMsg;
        }
    default:
        return L"未知状态";
    }
}

std::unique_ptr<TransportManager> TransportManagerFactory::Create(
    std::shared_ptr<DeviceManager> deviceManager,
    std::shared_ptr<ProtocolManager> protocolManager)
{
    return std::make_unique<TransportManager>(deviceManager, protocolManager);
}