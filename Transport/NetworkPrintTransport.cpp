#pragma execution_character_set("utf-8")

#include "pch.h"
#include "NetworkPrintTransport.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <random>

// 静态成员初始化
std::atomic<int> NetworkPrintTransport::s_wsaInitCount(0);
std::mutex NetworkPrintTransport::s_wsaMutex;

// 构造函数
NetworkPrintTransport::NetworkPrintTransport()
    : m_state(TransportState::Closed)
    , m_connectionState(NetworkConnectionState::Disconnected)
    , m_socket(INVALID_SOCKET)
    , m_asyncReadRunning(false)
    , m_asyncWriteRunning(false)
    , m_reconnectThreadRunning(false)
    , m_reconnectAttempts(0)
{
    // 初始化统计信息
    memset(&m_stats, 0, sizeof(m_stats));
    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
}

// 析构函数
NetworkPrintTransport::~NetworkPrintTransport()
{
    Close();
}

// 打开传输通道
TransportError NetworkPrintTransport::Open(const TransportConfig& baseConfig)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != TransportState::Closed)
    {
        return TransportError::AlreadyOpen;
    }
    
    // 转换配置类型
    const NetworkPrintConfig* config = dynamic_cast<const NetworkPrintConfig*>(&baseConfig);
    if (!config)
    {
        // 如果不是网络打印配置，创建默认配置
        NetworkPrintConfig defaultConfig;
        defaultConfig.portName = baseConfig.portName;
        defaultConfig.readTimeout = baseConfig.readTimeout;
        defaultConfig.writeTimeout = baseConfig.writeTimeout;
        defaultConfig.bufferSize = baseConfig.bufferSize;
        defaultConfig.asyncMode = baseConfig.asyncMode;
        
        // 尝试从portName解析主机名和端口
        size_t colonPos = baseConfig.portName.find(':');
        if (colonPos != std::string::npos)
        {
            defaultConfig.hostname = baseConfig.portName.substr(0, colonPos);
            std::string portStr = baseConfig.portName.substr(colonPos + 1);
            defaultConfig.port = static_cast<WORD>(std::atoi(portStr.c_str()));
        }
        
        m_config = defaultConfig;
    }
    else
    {
        m_config = *config;
    }
    
    // 验证配置
    if (!ValidateConfig(m_config))
    {
        return TransportError::InvalidConfig;
    }
    
    SetState(TransportState::Opening);
    SetConnectionState(NetworkConnectionState::Connecting);
    
    // 初始化Winsock
    TransportError result = InitializeWinsock();
    if (result != TransportError::Success)
    {
        SetState(TransportState::Error);
        return result;
    }
    
    // 解析主机地址
    result = ResolveHostAddress();
    if (result != TransportError::Success)
    {
        SetState(TransportState::Error);
        return result;
    }
    
    // 创建套接字
    result = CreateSocket();
    if (result != TransportError::Success)
    {
        SetState(TransportState::Error);
        return result;
    }
    
    // 设置套接字选项
    result = SetSocketOptions();
    if (result != TransportError::Success)
    {
        CloseSocket();
        SetState(TransportState::Error);
        return result;
    }
    
    // 连接到主机
    result = ConnectToHost();
    if (result != TransportError::Success)
    {
        CloseSocket();
        SetState(TransportState::Error);
        return result;
    }
    
    SetConnectionState(NetworkConnectionState::Connected);
    
    // 执行认证
    if (m_config.authType != NetworkAuthType::None)
    {
        SetConnectionState(NetworkConnectionState::Authenticating);
        result = Authenticate();
        if (result != TransportError::Success)
        {
            CloseSocket();
            SetState(TransportState::Error);
            return result;
        }
        SetConnectionState(NetworkConnectionState::Authenticated);
    }
    
    // 启动异步读取
    if (m_config.asyncMode)
    {
        result = StartAsyncRead();
        if (result != TransportError::Success)
        {
            CloseSocket();
            SetState(TransportState::Error);
            return result;
        }
    }
    
    // 启动重连监控
    if (m_config.enableReconnect)
    {
        m_reconnectThreadRunning = true;
        m_reconnectThread = std::thread(&NetworkPrintTransport::ReconnectThread, this);
    }
    
    SetState(TransportState::Open);
    return TransportError::Success;
}

// 关闭传输通道
TransportError NetworkPrintTransport::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == TransportState::Closed)
    {
        return TransportError::Success;
    }
    
    SetState(TransportState::Closing);
    SetConnectionState(NetworkConnectionState::Disconnected);
    
    // 停止异步操作
    StopAsyncRead();
    
    // 停止重连线程
    if (m_reconnectThreadRunning)
    {
        m_reconnectThreadRunning = false;
        if (m_reconnectThread.joinable())
        {
            m_reconnectThread.join();
        }
    }
    
    // 停止异步写入
    if (m_asyncWriteRunning)
    {
        m_asyncWriteRunning = false;
        if (m_asyncWriteThread.joinable())
        {
            m_asyncWriteThread.join();
        }
    }
    
    // 关闭套接字
    CloseSocket();
    
    // 清理Winsock
    CleanupWinsock();
    
    SetState(TransportState::Closed);
    return TransportError::Success;
}

// 同步写入数据
TransportError NetworkPrintTransport::Write(const void* data, size_t size, size_t* written)
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (!data || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    SetConnectionState(NetworkConnectionState::Sending);
    
    TransportError result;
    switch (m_config.protocol)
    {
        case NetworkPrintProtocol::RAW:
            result = SendRAWData(data, size);
            break;
            
        case NetworkPrintProtocol::LPR:
            result = SendLPRJob(data, size, m_config.jobName);
            break;
            
        case NetworkPrintProtocol::IPP:
            result = SendIPPJob(data, size, m_config.jobName);
            break;
            
        default:
            result = TransportError::InvalidParameter;
            break;
    }
    
    if (written)
    {
        *written = (result == TransportError::Success) ? size : 0;
    }
    
    SetConnectionState(NetworkConnectionState::Connected);
    return result;
}

// 同步读取数据
TransportError NetworkPrintTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (!buffer || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    SetConnectionState(NetworkConnectionState::Receiving);
    TransportError result = ReceiveData(buffer, size, read, timeout);
    SetConnectionState(NetworkConnectionState::Connected);
    
    return result;
}

// 异步写入数据
TransportError NetworkPrintTransport::WriteAsync(const void* data, size_t size)
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (!data || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    // 将数据加入写入队列
    std::vector<uint8_t> buffer(static_cast<const uint8_t*>(data), 
                                static_cast<const uint8_t*>(data) + size);
    
    {
        std::lock_guard<std::mutex> lock(m_writeQueueMutex);
        m_writeQueue.push(std::move(buffer));
    }
    
    // 启动异步写入线程
    if (!m_asyncWriteRunning)
    {
        m_asyncWriteRunning = true;
        m_asyncWriteThread = std::thread(&NetworkPrintTransport::AsyncWriteThread, this);
    }
    
    return TransportError::Success;
}

// 异步读取启动
TransportError NetworkPrintTransport::StartAsyncRead()
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (m_asyncReadRunning)
    {
        return TransportError::Success;
    }
    
    m_asyncReadRunning = true;
    m_asyncReadThread = std::thread(&NetworkPrintTransport::AsyncReadThread, this);
    
    return TransportError::Success;
}

// 停止异步读取
TransportError NetworkPrintTransport::StopAsyncRead()
{
    if (!m_asyncReadRunning)
    {
        return TransportError::Success;
    }
    
    m_asyncReadRunning = false;
    
    if (m_asyncReadThread.joinable())
    {
        m_asyncReadThread.join();
    }
    
    return TransportError::Success;
}

// 查询传输状态
TransportState NetworkPrintTransport::GetState() const
{
    return m_state;
}

// 检查是否已打开
bool NetworkPrintTransport::IsOpen() const
{
    return m_state == TransportState::Open && m_socket != INVALID_SOCKET;
}

// 获取统计信息
TransportStats NetworkPrintTransport::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

// 重置统计信息
void NetworkPrintTransport::ResetStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

// 获取端口名称
std::string NetworkPrintTransport::GetPortName() const
{
    return m_config.portName;
}

// 设置数据接收回调
void NetworkPrintTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataReceivedCallback = callback;
}

// 设置状态变化回调
void NetworkPrintTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateChangedCallback = callback;
}

// 设置错误发生回调
void NetworkPrintTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback)
{
    m_errorOccurredCallback = callback;
}

// 清空缓冲区
TransportError NetworkPrintTransport::FlushBuffers()
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    // TCP套接字自动管理缓冲区，这里可以发送保活包确保数据发送
    char keepalive = 0;
    size_t sent = 0;
    return SendData(&keepalive, 0, &sent);
}

// 获取可用字节数
size_t NetworkPrintTransport::GetAvailableBytes() const
{
    if (!IsOpen())
    {
        return 0;
    }
    
    u_long available = 0;
    if (ioctlsocket(m_socket, FIONREAD, &available) == SOCKET_ERROR)
    {
        return 0;
    }
    
    return static_cast<size_t>(available);
}

// 获取连接状态
NetworkConnectionState NetworkPrintTransport::GetConnectionState() const
{
    return m_connectionState;
}

// 获取远程地址
std::string NetworkPrintTransport::GetRemoteAddress() const
{
    return m_resolvedIP;
}

// 获取远程端口
WORD NetworkPrintTransport::GetRemotePort() const
{
    return m_config.port;
}

// 获取协议
NetworkPrintProtocol NetworkPrintTransport::GetProtocol() const
{
    return m_config.protocol;
}

// 发送作业
TransportError NetworkPrintTransport::SendJob(const std::vector<uint8_t>& data, const std::string& jobName)
{
    return Write(data.data(), data.size());
}

// 取消作业
TransportError NetworkPrintTransport::CancelJob(const std::string& jobId)
{
    if (m_config.protocol == NetworkPrintProtocol::LPR)
    {
        std::string command = "\x01" + m_config.queueName + " " + jobId + "\n";
        return SendLPRCommand(command);
    }
    
    return TransportError::Success;
}

// 获取作业状态
LPRJobStatus NetworkPrintTransport::GetJobStatus(const std::string& jobId) const
{
    // 实际实现需要查询服务器状态
    return LPRJobStatus::Unknown;
}

// 获取队列状态
std::vector<std::string> NetworkPrintTransport::GetQueueStatus() const
{
    std::vector<std::string> status;
    
    if (m_config.protocol == NetworkPrintProtocol::LPR)
    {
        // LPR队列状态查询
        std::string command = "\x03" + m_config.queueName + "\n";
        // 实际实现需要发送命令并解析响应
    }
    
    return status;
}

// 解析主机名
bool NetworkPrintTransport::ResolveHostname(const std::string& hostname, std::string& ipAddress)
{
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (ret != 0)
    {
        return false;
    }
    
    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr_in->sin_addr), ip, INET_ADDRSTRLEN);
    ipAddress = ip;
    
    freeaddrinfo(result);
    return true;
}

// 验证IP地址
bool NetworkPrintTransport::IsValidIPAddress(const std::string& ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

// 检查端口是否开放
bool NetworkPrintTransport::IsPortOpen(const std::string& hostname, WORD port, DWORD timeout)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        return false;
    }
    
    // 设置非阻塞模式
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    std::string ip;
    if (!ResolveHostname(hostname, ip))
    {
        closesocket(sock);
        return false;
    }
    
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);
    
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    
    bool connected = (select(0, nullptr, &writeSet, nullptr, &tv) > 0);
    
    closesocket(sock);
    return connected;
}

// 发现网络打印机
std::vector<std::string> NetworkPrintTransport::DiscoverNetworkPrinters(const std::string& subnet)
{
    std::vector<std::string> printers;
    
    // 实际实现需要扫描网络段
    // 这里提供基本框架
    
    return printers;
}

// 获取协议名称
std::string NetworkPrintTransport::GetProtocolName(NetworkPrintProtocol protocol)
{
    switch (protocol)
    {
        case NetworkPrintProtocol::RAW:
            return "RAW";
        case NetworkPrintProtocol::LPR:
            return "LPR/LPD";
        case NetworkPrintProtocol::IPP:
            return "IPP";
        default:
            return "Unknown";
    }
}

// 设置状态
void NetworkPrintTransport::SetState(TransportState newState)
{
    TransportState oldState = m_state;
    m_state = newState;
    
    if (oldState != newState && m_stateChangedCallback)
    {
        m_stateChangedCallback(newState);
    }
}

// 设置连接状态
void NetworkPrintTransport::SetConnectionState(NetworkConnectionState newState)
{
    m_connectionState = newState;
}

// 通知错误
void NetworkPrintTransport::NotifyError(TransportError error, const std::string& message)
{
    if (m_errorOccurredCallback)
    {
        m_errorOccurredCallback(error, message);
    }
}

// 更新统计信息
void NetworkPrintTransport::UpdateStats(uint64_t bytesSent, uint64_t bytesReceived)
{
    m_stats.bytesSent += bytesSent;
    m_stats.bytesReceived += bytesReceived;
    m_stats.packetsTotal++;
    
    // 计算吞吐率
    static auto lastTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
    
    if (elapsed.count() > 1000) // 每秒更新一次
    {
        m_stats.throughputBps = (bytesSent + bytesReceived) * 1000.0 / elapsed.count();
        lastTime = currentTime;
    }
}

// 初始化Winsock
TransportError NetworkPrintTransport::InitializeWinsock()
{
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    
    if (s_wsaInitCount == 0)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            return TransportError::OpenFailed;
        }
    }
    
    s_wsaInitCount++;
    return TransportError::Success;
}

// 清理Winsock
void NetworkPrintTransport::CleanupWinsock()
{
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    
    s_wsaInitCount--;
    if (s_wsaInitCount == 0)
    {
        WSACleanup();
    }
}

// 创建套接字
TransportError NetworkPrintTransport::CreateSocket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
        return GetSocketError();
    }
    
    return TransportError::Success;
}

// 关闭套接字
void NetworkPrintTransport::CloseSocket()
{
    if (m_socket != INVALID_SOCKET)
    {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

// 连接到主机
TransportError NetworkPrintTransport::ConnectToHost()
{
    int result = connect(m_socket, reinterpret_cast<struct sockaddr*>(&m_serverAddr), sizeof(m_serverAddr));
    if (result == SOCKET_ERROR)
    {
        return GetSocketError();
    }
    
    return TransportError::Success;
}

// 发送数据
TransportError NetworkPrintTransport::SendData(const void* data, size_t size, size_t* sent)
{
    if (!data || size == 0)
    {
        if (sent) *sent = 0;
        return TransportError::Success;
    }
    
    int result = send(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0);
    if (result == SOCKET_ERROR)
    {
        if (sent) *sent = 0;
        return GetSocketError();
    }
    
    if (sent) *sent = result;
    UpdateStats(result, 0);
    return TransportError::Success;
}

// 接收数据
TransportError NetworkPrintTransport::ReceiveData(void* buffer, size_t size, size_t* received, DWORD timeout)
{
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_socket, &readSet);
    
    int selectResult = select(0, &readSet, nullptr, nullptr, &tv);
    if (selectResult == 0)
    {
        if (received) *received = 0;
        return TransportError::Timeout;
    }
    else if (selectResult == SOCKET_ERROR)
    {
        if (received) *received = 0;
        return GetSocketError();
    }
    
    int result = recv(m_socket, static_cast<char*>(buffer), static_cast<int>(size), 0);
    if (result == SOCKET_ERROR)
    {
        if (received) *received = 0;
        return GetSocketError();
    }
    else if (result == 0)
    {
        if (received) *received = 0;
        return TransportError::ConnectionClosed;
    }
    
    if (received) *received = result;
    UpdateStats(0, result);
    return TransportError::Success;
}

// 发送RAW数据
TransportError NetworkPrintTransport::SendRAWData(const void* data, size_t size)
{
    size_t sent = 0;
    return SendData(data, size, &sent);
}

// 发送LPR作业
TransportError NetworkPrintTransport::SendLPRJob(const void* data, size_t size, const std::string& jobName)
{
    // LPR协议实现
    std::string jobId = GenerateLPRJobId();
    m_currentJobId = jobId;
    
    // 1. 发送接收作业命令
    std::string command = "\x02" + m_config.queueName + "\n";
    TransportError result = SendLPRCommand(command);
    if (result != TransportError::Success)
    {
        return result;
    }
    
    // 2. 接收响应
    std::string response;
    result = ReceiveLPRResponse(response);
    if (result != TransportError::Success || response[0] != '\0')
    {
        return TransportError::WriteFailed;
    }
    
    // 3. 发送控制文件
    std::string controlFile = FormatLPRControlFile(jobName, m_config.userName, size);
    std::string controlHeader = "\x02" + std::to_string(controlFile.length()) + " cfA" + jobId + m_config.userName + "\n";
    
    result = SendLPRCommand(controlHeader);
    if (result != TransportError::Success) return result;
    
    result = ReceiveLPRResponse(response);
    if (result != TransportError::Success || response[0] != '\0') return TransportError::WriteFailed;
    
    size_t sent = 0;
    result = SendData(controlFile.c_str(), controlFile.length(), &sent);
    if (result != TransportError::Success) return result;
    
    char ack = '\0';
    result = SendData(&ack, 1, &sent);
    if (result != TransportError::Success) return result;
    
    result = ReceiveLPRResponse(response);
    if (result != TransportError::Success || response[0] != '\0') return TransportError::WriteFailed;
    
    // 4. 发送数据文件
    std::string dataHeader = "\x03" + std::to_string(size) + " dfA" + jobId + m_config.userName + "\n";
    
    result = SendLPRCommand(dataHeader);
    if (result != TransportError::Success) return result;
    
    result = ReceiveLPRResponse(response);
    if (result != TransportError::Success || response[0] != '\0') return TransportError::WriteFailed;
    
    result = SendData(data, size, &sent);
    if (result != TransportError::Success) return result;
    
    result = SendData(&ack, 1, &sent);
    if (result != TransportError::Success) return result;
    
    result = ReceiveLPRResponse(response);
    if (result != TransportError::Success || response[0] != '\0') return TransportError::WriteFailed;
    
    return TransportError::Success;
}

// 发送IPP作业
TransportError NetworkPrintTransport::SendIPPJob(const void* data, size_t size, const std::string& jobName)
{
    // 构建IPP请求
    std::vector<uint8_t> jobData(static_cast<const uint8_t*>(data), 
                                 static_cast<const uint8_t*>(data) + size);
    std::vector<uint8_t> ippRequest = BuildIPPRequest(jobData, jobName);
    
    // 发送HTTP请求
    return SendHTTPRequest("POST", m_config.httpPath, ippRequest, m_config.contentType);
}

// 发送LPR命令
TransportError NetworkPrintTransport::SendLPRCommand(const std::string& command)
{
    size_t sent = 0;
    return SendData(command.c_str(), command.length(), &sent);
}

// 接收LPR响应
TransportError NetworkPrintTransport::ReceiveLPRResponse(std::string& response)
{
    char buffer[256];
    size_t received = 0;
    TransportError result = ReceiveData(buffer, sizeof(buffer) - 1, &received, m_config.receiveTimeout);
    
    if (result == TransportError::Success && received > 0)
    {
        buffer[received] = '\0';
        response = buffer;
    }
    
    return result;
}

// 生成LPR作业ID
std::string NetworkPrintTransport::GenerateLPRJobId() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << (time_t % 1000);
    return oss.str();
}

// 格式化LPR控制文件
std::string NetworkPrintTransport::FormatLPRControlFile(const std::string& jobName, const std::string& userName, size_t dataSize) const
{
    std::ostringstream oss;
    oss << "H" << m_config.hostname << "\n";         // 主机名
    oss << "P" << userName << "\n";                  // 用户名
    oss << "J" << jobName << "\n";                   // 作业名
    oss << "L" << userName << "\n";                  // 横幅页用户名
    oss << "fdfA" << m_currentJobId << userName << "\n"; // 数据文件名
    return oss.str();
}

// 发送HTTP请求
TransportError NetworkPrintTransport::SendHTTPRequest(const std::string& method, const std::string& path, 
                                                      const std::vector<uint8_t>& data, const std::string& contentType)
{
    std::string headers = BuildHTTPHeaders(method, path, data.size(), contentType);
    
    // 发送HTTP头
    size_t sent = 0;
    TransportError result = SendData(headers.c_str(), headers.length(), &sent);
    if (result != TransportError::Success)
    {
        return result;
    }
    
    // 发送数据
    if (!data.empty())
    {
        result = SendData(data.data(), data.size(), &sent);
        if (result != TransportError::Success)
        {
            return result;
        }
    }
    
    return TransportError::Success;
}

// 接收HTTP响应
TransportError NetworkPrintTransport::ReceiveHTTPResponse(std::vector<uint8_t>& response)
{
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    response.clear();
    
    // 接收HTTP响应头
    bool headerComplete = false;
    std::string headers;
    
    while (!headerComplete)
    {
        size_t received = 0;
        TransportError result = ReceiveData(buffer, bufferSize, &received, m_config.receiveTimeout);
        if (result != TransportError::Success)
        {
            return result;
        }
        
        headers.append(buffer, received);
        
        // 检查是否收到完整的HTTP头
        size_t headerEnd = headers.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
        {
            headerComplete = true;
            
            // 如果还有数据，添加到响应中
            size_t bodyStart = headerEnd + 4;
            if (bodyStart < headers.length())
            {
                response.assign(headers.begin() + bodyStart, headers.end());
            }
        }
    }
    
    // 根据Content-Length接收剩余数据
    // 这里简化处理，实际需要解析HTTP头获取内容长度
    
    return TransportError::Success;
}

// 构建HTTP头
std::string NetworkPrintTransport::BuildHTTPHeaders(const std::string& method, const std::string& path, 
                                                    size_t contentLength, const std::string& contentType) const
{
    std::ostringstream oss;
    oss << method << " " << path << " HTTP/1.1\r\n";
    oss << "Host: " << m_config.hostname << ":" << m_config.port << "\r\n";
    oss << "User-Agent: " << m_config.userAgent << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << contentLength << "\r\n";
    
    if (m_config.authType == NetworkAuthType::Basic)
    {
        oss << "Authorization: " << BuildBasicAuthHeader() << "\r\n";
    }
    
    oss << "Connection: close\r\n";
    oss << "\r\n";
    
    return oss.str();
}

// 构建IPP请求
std::vector<uint8_t> NetworkPrintTransport::BuildIPPRequest(const std::vector<uint8_t>& data, const std::string& jobName) const
{
    std::vector<uint8_t> request;
    
    // IPP请求格式较复杂，这里提供基本框架
    // 实际实现需要按照RFC 2910标准构建IPP消息
    
    return request;
}

// 执行认证
TransportError NetworkPrintTransport::Authenticate()
{
    switch (m_config.authType)
    {
        case NetworkAuthType::Basic:
            return BasicAuthenticate();
        case NetworkAuthType::NTLM:
            return NTLMAuthenticate();
        case NetworkAuthType::Certificate:
            return CertificateAuthenticate();
        default:
            return TransportError::Success;
    }
}

// 基本认证
TransportError NetworkPrintTransport::BasicAuthenticate()
{
    // 基本认证通常在HTTP头中完成
    return TransportError::Success;
}

// NTLM认证
TransportError NetworkPrintTransport::NTLMAuthenticate()
{
    // NTLM认证实现复杂，这里提供框架
    return TransportError::Success;
}

// 证书认证
TransportError NetworkPrintTransport::CertificateAuthenticate()
{
    // 证书认证需要SSL/TLS支持
    return TransportError::Success;
}

// 构建基本认证头
std::string NetworkPrintTransport::BuildBasicAuthHeader() const
{
    std::string credentials = m_config.userName + ":" + m_config.password;
    // 这里需要Base64编码，简化处理
    return "Basic " + credentials;
}

// 异步读取线程
void NetworkPrintTransport::AsyncReadThread()
{
    const size_t bufferSize = m_config.bufferSize;
    std::vector<uint8_t> buffer(bufferSize);
    
    while (m_asyncReadRunning && IsOpen())
    {
        size_t bytesRead = 0;
        TransportError result = ReceiveData(buffer.data(), bufferSize, &bytesRead, m_config.readTimeout);
        
        if (result == TransportError::Success && bytesRead > 0)
        {
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
            
            if (m_dataReceivedCallback)
            {
                m_dataReceivedCallback(data);
            }
        }
        else if (result != TransportError::Timeout)
        {
            NotifyError(result, "异步读取失败");
            break;
        }
    }
}

// 异步写入线程
void NetworkPrintTransport::AsyncWriteThread()
{
    while (m_asyncWriteRunning)
    {
        std::vector<uint8_t> data;
        
        // 从队列中获取数据
        {
            std::lock_guard<std::mutex> lock(m_writeQueueMutex);
            if (m_writeQueue.empty())
            {
                Sleep(10);
                continue;
            }
            
            data = std::move(m_writeQueue.front());
            m_writeQueue.pop();
        }
        
        // 写入数据
        size_t written = 0;
        TransportError result = Write(data.data(), data.size(), &written);
        
        if (result != TransportError::Success)
        {
            NotifyError(result, "异步写入失败");
        }
    }
}

// 重连线程
void NetworkPrintTransport::ReconnectThread()
{
    while (m_reconnectThreadRunning)
    {
        Sleep(m_config.reconnectInterval);
        
        if (!IsOpen() && m_reconnectAttempts < m_config.maxReconnectAttempts)
        {
            m_reconnectAttempts++;
            
            // 尝试重连
            TransportError result = ConnectToHost();
            if (result == TransportError::Success)
            {
                SetState(TransportState::Open);
                SetConnectionState(NetworkConnectionState::Connected);
                m_reconnectAttempts = 0;
            }
        }
    }
}

// 获取套接字错误
TransportError NetworkPrintTransport::GetSocketError()
{
    int error = WSAGetLastError();
    m_stats.lastErrorCode = error;
    
    switch (error)
    {
        case WSAECONNREFUSED:
            return TransportError::ConnectionClosed;
        case WSAETIMEDOUT:
            return TransportError::Timeout;
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:
            return TransportError::OpenFailed;
        case WSAEADDRINUSE:
            return TransportError::Busy;
        case WSAEINVAL:
            return TransportError::InvalidParameter;
        default:
            return TransportError::WriteFailed;
    }
}

// 获取套接字错误消息
std::string NetworkPrintTransport::GetSocketErrorMessage(int errorCode) const
{
    // Windows套接字错误消息转换
    switch (errorCode)
    {
        case WSAECONNREFUSED:
            return "连接被拒绝";
        case WSAETIMEDOUT:
            return "连接超时";
        case WSAENETUNREACH:
            return "网络不可达";
        case WSAEHOSTUNREACH:
            return "主机不可达";
        case WSAEADDRINUSE:
            return "地址已被使用";
        default:
            return "网络错误: " + std::to_string(errorCode);
    }
}

// 验证配置
bool NetworkPrintTransport::ValidateConfig(const NetworkPrintConfig& config) const
{
    if (config.hostname.empty())
    {
        return false;
    }
    
    if (config.port == 0 || config.port > 65535)
    {
        return false;
    }
    
    if (config.connectTimeout == 0 || config.sendTimeout == 0 || config.receiveTimeout == 0)
    {
        return false;
    }
    
    return true;
}

// 解析主机地址
TransportError NetworkPrintTransport::ResolveHostAddress()
{
    if (!ResolveHostname(m_config.hostname, m_resolvedIP))
    {
        return TransportError::OpenFailed;
    }
    
    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(m_config.port);
    inet_pton(AF_INET, m_resolvedIP.c_str(), &m_serverAddr.sin_addr);
    
    return TransportError::Success;
}

// 设置套接字超时
TransportError NetworkPrintTransport::SetSocketTimeouts()
{
    DWORD sendTimeout = m_config.sendTimeout;
    DWORD receiveTimeout = m_config.receiveTimeout;
    
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, 
                   reinterpret_cast<const char*>(&sendTimeout), sizeof(sendTimeout)) == SOCKET_ERROR)
    {
        return GetSocketError();
    }
    
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout)) == SOCKET_ERROR)
    {
        return GetSocketError();
    }
    
    return TransportError::Success;
}

// 设置套接字选项
TransportError NetworkPrintTransport::SetSocketOptions()
{
    // 设置超时
    TransportError result = SetSocketTimeouts();
    if (result != TransportError::Success)
    {
        return result;
    }
    
    // 启用保活
    if (m_config.enableKeepAlive)
    {
        result = EnableKeepAlive();
        if (result != TransportError::Success)
        {
            return result;
        }
    }
    
    return TransportError::Success;
}

// 启用保活
TransportError NetworkPrintTransport::EnableKeepAlive()
{
    BOOL keepalive = TRUE;
    if (setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, 
                   reinterpret_cast<const char*>(&keepalive), sizeof(keepalive)) == SOCKET_ERROR)
    {
        return GetSocketError();
    }
    
    return TransportError::Success;
}

// 网络打印错误码转换实现
TransportError NetworkPrintErrorConverter::ConvertToTransportError(NetworkPrintError error)
{
    switch (error)
    {
        case NetworkPrintError::Success:
            return TransportError::Success;
        case NetworkPrintError::WinsockInitFailed:
        case NetworkPrintError::SocketCreateFailed:
            return TransportError::OpenFailed;
        case NetworkPrintError::HostResolveFailed:
        case NetworkPrintError::ConnectionFailed:
            return TransportError::ConnectionClosed;
        case NetworkPrintError::AuthenticationFailed:
            return TransportError::OpenFailed;
        case NetworkPrintError::TimeoutError:
            return TransportError::Timeout;
        case NetworkPrintError::NetworkError:
            return TransportError::WriteFailed;
        default:
            return TransportError::WriteFailed;
    }
}

std::string NetworkPrintErrorConverter::GetNetworkPrintErrorString(NetworkPrintError error)
{
    switch (error)
    {
        case NetworkPrintError::Success:
            return "成功";
        case NetworkPrintError::WinsockInitFailed:
            return "Winsock初始化失败";
        case NetworkPrintError::SocketCreateFailed:
            return "套接字创建失败";
        case NetworkPrintError::HostResolveFailed:
            return "主机名解析失败";
        case NetworkPrintError::ConnectionFailed:
            return "连接失败";
        case NetworkPrintError::AuthenticationFailed:
            return "认证失败";
        case NetworkPrintError::ProtocolError:
            return "协议错误";
        case NetworkPrintError::HTTPError:
            return "HTTP错误";
        case NetworkPrintError::SSLError:
            return "SSL错误";
        case NetworkPrintError::TimeoutError:
            return "超时错误";
        case NetworkPrintError::NetworkError:
            return "网络错误";
        default:
            return "未知错误";
    }
}