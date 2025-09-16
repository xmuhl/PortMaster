#pragma execution_character_set("utf-8")
#include "pch.h"
#include "ConnectionManager.h"
#include "../PortMasterDlg.h"
#include "../Transport/ITransport.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/TcpTransport.h"
#include "../Transport/UdpTransport.h"
#include "../Transport/LptSpoolerTransport.h"
#include "../Transport/UsbPrinterTransport.h"
#include "../Transport/LoopbackTransport.h"
#include "../Protocol/ReliableChannel.h"
#include "ConfigManager.h"
#include "TransportManager.h"
#include "StateManager.h"

extern void WriteDebugLog(const char* message);

ConnectionManager::ConnectionManager(CPortMasterDlg* dialog)
    : m_dialog(dialog)
    , m_transport(nullptr)
    , m_reliableChannel(nullptr)
    , m_bConnected(false)
    , m_bReliableMode(false)
{
    WriteDebugLog("[DEBUG] ConnectionManager::ConnectionManager: 连接管理器构造完成");
}

bool ConnectionManager::EstablishConnection(int transportIndex, const TransportConfig& config)
{
    WriteDebugLog("[DEBUG] ConnectionManager::EstablishConnection: 开始建立连接");

    if (transportIndex < 0)
    {
        WriteDebugLog("[ERROR] ConnectionManager::EstablishConnection: 无效的传输类型索引");
        if (m_dialog) {
            m_dialog->AppendLog(L"请选择传输类型");
        }
        return false;
    }

    // 使用工厂模式创建传输实例 (SOLID-O: 开闭原则)
    std::shared_ptr<ITransport> newTransport = CreateTransportFromIndex(transportIndex, config);
    if (!newTransport)
    {
        WriteDebugLog("[ERROR] ConnectionManager::EstablishConnection: 创建传输对象失败");
        if (m_dialog) {
            m_dialog->AppendLog(L"不支持的传输类型");
        }
        return false;
    }

    // 尝试打开传输连接
    if (!newTransport->Open(config))
    {
        std::string error = newTransport->GetLastError();
        CString statusMsg = TransportManager::GetConnectionStatusMessage(TRANSPORT_ERROR, error);

        WriteDebugLog(CT2A(CString("[ERROR] ConnectionManager::EstablishConnection: 连接失败 - ") + statusMsg));

        if (m_dialog) {
            // SOLID-S: 单一职责 - 提供针对性的错误建议
            CString detailedError = TransportManager::GetDetailedErrorSuggestion(transportIndex, error);
            m_dialog->AppendLog(L"连接失败: " + statusMsg);
            if (!detailedError.IsEmpty())
            {
                m_dialog->AppendLog(L"建议: " + detailedError);
            }

            // 更新状态显示
            m_dialog->UpdateStatusDisplay(statusMsg, L"空闲", L"状态: 连接失败", L"", CPortMasterDlg::StatusPriority::CRITICAL);
        }
        return false;
    }

    // 连接成功，更新传输对象
    m_transport = newTransport;
    WriteDebugLog("[DEBUG] ConnectionManager::EstablishConnection: 传输连接建立成功");

    // 配置可靠通道
    ConfigureReliableChannel(m_transport);

    // 设置传输回调
    SetupTransportCallbacks();

    // 启动可靠通道
    if (m_reliableChannel && m_reliableChannel->Start())
    {
        m_bConnected = true;
        WriteDebugLog("[DEBUG] ConnectionManager::EstablishConnection: 可靠通道启动成功");

        if (m_dialog) {
            m_dialog->UpdateButtonStatesLegacy();

            // 更新连接状态显示
            std::string transportType = m_transport->GetTransportType();
            std::string endpoint = GetConnectionEndpoint(config, transportType);
            UpdateConnectionDisplay(true, transportType, endpoint);
        }
    }
    else
    {
        std::string error = m_reliableChannel ? m_reliableChannel->GetLastError() : "可靠通道启动失败";
        CString statusMsg = TransportManager::GetConnectionStatusMessage(TRANSPORT_ERROR, error);

        WriteDebugLog(CT2A(CString("[ERROR] ConnectionManager::EstablishConnection: 可靠通道启动失败 - ") + statusMsg));

        if (m_dialog) {
            m_dialog->AppendLog(L"可靠通道启动失败: " + statusMsg);
            m_dialog->UpdateStatusDisplay(statusMsg, L"失败", L"状态: 通道启动失败", L"", CPortMasterDlg::StatusPriority::CRITICAL);
        }

        // 清理资源
        m_transport.reset();
        m_reliableChannel.reset();
        return false;
    }

    WriteDebugLog("[DEBUG] ConnectionManager::EstablishConnection: 连接建立完成");
    return true;
}

bool ConnectionManager::DisconnectTransport()
{
    WriteDebugLog("[DEBUG] ConnectionManager::DisconnectTransport: 开始断开连接");

    if (!m_bConnected)
    {
        WriteDebugLog("[DEBUG] ConnectionManager::DisconnectTransport: 当前未连接，无需断开");
        return true;
    }

    // 停止可靠通道
    if (m_reliableChannel)
    {
        m_reliableChannel->Stop();
        WriteDebugLog("[DEBUG] ConnectionManager::DisconnectTransport: 可靠通道已停止");
    }

    // 关闭传输连接
    if (m_transport)
    {
        m_transport->Close();
        WriteDebugLog("[DEBUG] ConnectionManager::DisconnectTransport: 传输连接已关闭");
    }

    // 清理资源
    m_transport.reset();
    m_reliableChannel.reset();
    m_bConnected = false;

    if (m_dialog) {
        m_dialog->UpdateButtonStatesLegacy();
        m_dialog->AppendLog(L"连接已断开");
        m_dialog->UpdateStatusDisplay(L"未连接", L"空闲", L"状态: 已断开", L"", CPortMasterDlg::StatusPriority::NORMAL);
    }

    WriteDebugLog("[DEBUG] ConnectionManager::DisconnectTransport: 断开连接完成");
    return true;
}

std::shared_ptr<ITransport> ConnectionManager::CreateTransportFromIndex(int transportIndex, const TransportConfig& config)
{
    CString debugMsg;
    debugMsg.Format(L"[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建传输对象，索引=%d", transportIndex);
    WriteDebugLog(CT2A(debugMsg));

    // SOLID-O: 开放封闭原则 - 工厂模式支持扩展新传输类型
    switch (transportIndex)
    {
    case 0: // Serial (COM)
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建串口传输");
        return std::make_shared<SerialTransport>();

    case 1: // LPT (并口)
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建并口传输");
        return std::make_shared<LptSpoolerTransport>();

    case 2: // USB打印机
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建USB传输");
        return std::make_shared<UsbPrinterTransport>();

    case 3: // TCP网络
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建TCP传输");
        return std::make_shared<TcpTransport>();

    case 4: // UDP网络
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建UDP传输");
        return std::make_shared<UdpTransport>();

    case 5: // 本地回路
        WriteDebugLog("[DEBUG] ConnectionManager::CreateTransportFromIndex: 创建本地回路传输");
        return std::make_shared<LoopbackTransport>();

    default:
        CString errorMsg;
        errorMsg.Format(L"[ERROR] ConnectionManager::CreateTransportFromIndex: 不支持的传输类型索引=%d", transportIndex);
        WriteDebugLog(CT2A(errorMsg));
        return nullptr;
    }
}

void ConnectionManager::ConfigureReliableChannel(std::shared_ptr<ITransport> transport)
{
    WriteDebugLog("[DEBUG] ConnectionManager::ConfigureReliableChannel: 开始配置可靠通道");

    m_reliableChannel = std::make_shared<ReliableChannel>(transport);

    // SOLID-S: 单一职责 - 配置协议参数 (DRY: 统一配置管理)
    // 性能优化：本地回路跳过重配置，使用默认值提升连接速度
    if (std::dynamic_pointer_cast<LoopbackTransport>(transport)) {
        WriteDebugLog("[DEBUG] ConnectionManager::ConfigureReliableChannel: 本地回路使用默认配置");
        if (m_dialog) {
            m_dialog->ConfigureReliableChannelForLoopback();
        }
    } else {
        WriteDebugLog("[DEBUG] ConnectionManager::ConfigureReliableChannel: 从配置文件加载参数");
        if (m_dialog) {
            m_dialog->ConfigureReliableChannelFromConfig();
        }
    }

    WriteDebugLog("[DEBUG] ConnectionManager::ConfigureReliableChannel: 可靠通道配置完成");
}

void ConnectionManager::SetupTransportCallbacks()
{
    WriteDebugLog("[DEBUG] ConnectionManager::SetupTransportCallbacks: 开始设置传输回调");

    if (!m_transport || !m_dialog) {
        WriteDebugLog("[ERROR] ConnectionManager::SetupTransportCallbacks: 传输对象或对话框指针为空");
        return;
    }

    // 设置直接传输模式的数据接收回调
    m_transport->SetDataReceivedCallback([this](const std::vector<uint8_t>& data) {
        // 复制数据到堆内存用于线程间传递
        std::vector<uint8_t>* dataPtr = new std::vector<uint8_t>(data);

        // 使用SafePostMessage发送到UI线程处理
        if (!m_dialog->SafePostMessage(WM_DISPLAY_RECEIVED_DATA, 0, reinterpret_cast<LPARAM>(dataPtr)))
        {
            // SafePostMessage失败，清理分配的内存
            delete dataPtr;
            WriteDebugLog("[WARNING] ConnectionManager: 直接传输数据接收回调SafePostMessage失败");
        }
    });

    if (m_reliableChannel) {
        // 设置进度回调 - 使用SafePostMessage实现线程安全
        m_reliableChannel->SetProgressCallback([this](const TransferStats& stats) {
            // 频率限制机制 - 防止UI消息队列饱和
            static std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);

            if (elapsed.count() < 50) { // 最小50ms间隔
                return; // 跳过过于频繁的回调
            }
            lastUpdate = now;

            // 线程安全的进度更新
            if (stats.totalBytes > 0 && m_dialog && ::IsWindow(m_dialog->GetSafeHwnd()))
            {
                int progress = static_cast<int>((stats.transferredBytes * 100) / stats.totalBytes);
                CString* statusText = new CString();
                statusText->Format(L"状态: 传输中 (%.1f%%, %zu/%zu 字节)",
                    stats.GetProgress() * 100, stats.transferredBytes, stats.totalBytes);

                if (!m_dialog->SafePostMessage(WM_UPDATE_PROGRESS, progress, reinterpret_cast<LPARAM>(statusText)))
                {
                    delete statusText;
                    WriteDebugLog("[WARNING] ConnectionManager: 进度回调SafePostMessage失败");
                }
            }
        });

        // 设置完成回调
        m_reliableChannel->SetCompletionCallback([this](bool success, const std::string& message) {
            // 验证窗口句柄有效性后再进行UI操作
            if (m_dialog && ::IsWindow(m_dialog->GetSafeHwnd()))
            {
                CString* msgData = new CString(CA2W(message.c_str(), CP_UTF8));

                if (!m_dialog->SafePostMessage(WM_UPDATE_COMPLETION, success ? 1 : 0, reinterpret_cast<LPARAM>(msgData)))
                {
                    delete msgData;
                    WriteDebugLog("[WARNING] ConnectionManager: 完成回调SafePostMessage失败");
                }
            }
        });

        // 设置文件接收回调
        m_reliableChannel->SetFileReceivedCallback([this](const std::string& filename, const std::vector<uint8_t>& data) {
            // 验证窗口句柄有效性后再进行UI操作
            if (m_dialog && ::IsWindow(m_dialog->GetSafeHwnd()))
            {
                // 创建数据包用于传递到UI线程
                struct FileReceivedData {
                    CString filename;
                    std::vector<uint8_t> data;
                };

                FileReceivedData* receivedData = new FileReceivedData{CA2W(filename.c_str()), data};

                if (!m_dialog->SafePostMessage(WM_UPDATE_FILE_RECEIVED, 0, reinterpret_cast<LPARAM>(receivedData)))
                {
                    delete receivedData;
                    WriteDebugLog("[WARNING] ConnectionManager: 文件接收回调SafePostMessage失败");
                }
            }
        });

        // 启用接收功能
        m_reliableChannel->EnableReceiving(true);
    }

    WriteDebugLog("[DEBUG] ConnectionManager::SetupTransportCallbacks: 传输回调设置完成");
}

void ConnectionManager::UpdateConnectionDisplay(bool connected, const std::string& transportType, const std::string& endpoint)
{
    if (!m_dialog) return;

    if (connected) {
        // 格式化连接信息
        CString transportInfo = TransportManager::FormatTransportInfo(transportType, endpoint);
        CString statusMsg = TransportManager::GetConnectionStatusMessage(TRANSPORT_OPEN);

        m_dialog->AppendLog(L"连接成功 - " + transportInfo);
        m_dialog->UpdateStatusDisplay(statusMsg, L"空闲", L"状态: 已连接", L"", CPortMasterDlg::StatusPriority::HIGH);
    }
}

std::string ConnectionManager::GetNetworkConnectionInfo(const std::string& transportType)
{
    // 获取实际的网络连接信息
    if (m_transport) {
        // 这里可以扩展获取实际连接信息的逻辑
        return "";
    }
    return "";
}

std::string ConnectionManager::GetConnectionEndpoint(const TransportConfig& config, const std::string& transportType)
{
    if (transportType == "Serial") {
        return config.portName;
    }
    else if (transportType == "TCP" || transportType == "UDP") {
        // 获取实际的网络连接信息
        std::string actualEndpoint = GetNetworkConnectionInfo(transportType);
        return actualEndpoint.empty() ? (config.ipAddress + ":" + std::to_string(config.port)) : actualEndpoint;
    }
    else if (transportType == "LPT" || transportType == "USB") {
        return config.portName;
    }
    return "Unknown";
}