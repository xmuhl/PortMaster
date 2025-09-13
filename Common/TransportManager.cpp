#pragma execution_character_set("utf-8")
#include "pch.h"
#include "TransportManager.h"
#include "DeviceManager.h"
#include "../Protocol/ProtocolManager.h"
#include "../Transport/ITransport.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/LoopbackTransport.h"
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
    WriteDebugLog("[DEBUG] TransportManager构造完成");
}

TransportManager::~TransportManager()
{
    try {
        WriteDebugLog("[DEBUG] TransportManager析构完成");
    }
    catch (const std::exception& e) {
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
        
        // 使用ITransport接口的Open方法
        TransportConfig config;
        if (!m_transport->Open(config)) {
            ReportError("连接传输", "传输连接失败");
            SetTransmissionState(TransportOperationState::ERROR);
            return false;
        }
        
        m_connected = true;
        SetTransmissionState(TransportOperationState::CONNECTED);
        
        WriteDebugLog("[DEBUG] TransportManager::Connect: 连接成功");
        return true;
    }
    catch (const std::exception& e) {
        ReportError("连接传输", e.what());
        SetTransmissionState(TransportOperationState::ERROR);
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
            SetTransmissionState(TransportOperationState::ERROR);
        }
        
        m_transmitting = false;
        return success;
    }
    catch (const std::exception& e) {
        m_transmitting = false;
        ReportError("发送数据", e.what());
        SetTransmissionState(TransportOperationState::ERROR);
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

// TransportManagerFactory 实现

std::unique_ptr<TransportManager> TransportManagerFactory::Create(
    std::shared_ptr<DeviceManager> deviceManager,
    std::shared_ptr<ProtocolManager> protocolManager)
{
    return std::make_unique<TransportManager>(deviceManager, protocolManager);
}