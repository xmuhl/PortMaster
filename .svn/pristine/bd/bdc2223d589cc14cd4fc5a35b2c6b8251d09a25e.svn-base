#include "pch.h"
#include "TcpTransport.h"

bool TcpTransport::s_wsaInitialized = false;
int TcpTransport::s_wsaRefCount = 0;

TcpTransport::TcpTransport()
    : m_socket(INVALID_SOCKET)
    , m_listenSocket(INVALID_SOCKET)
    , m_stopThreads(false)
    , m_stopAccept(false)
    , m_continuousReading(false)
    , m_ioWorker(std::make_shared<IOWorker>())
{
    m_state = TRANSPORT_CLOSED;
    m_readBuffer.resize(4096);
}

TcpTransport::~TcpTransport()
{
    Close();
}

bool TcpTransport::Open(const TransportConfig& config)
{
    if (m_state != TRANSPORT_CLOSED)
    {
        SetLastError("TCP连接已打开或正在操作中");
        return false;
    }
    
    m_config = config;
    
    NotifyStateChanged(TRANSPORT_OPENING, config.isServer ? "启动TCP服务器" : "连接TCP服务器");
    
    // 初始化Winsock
    if (!InitializeWinsock())
    {
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 创建套接字
    if (!CreateSocket())
    {
        CleanupWinsock();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 根据配置启动客户端或服务端
    bool success = false;
    if (config.isServer)
    {
        success = StartServer();
    }
    else
    {
        success = ConnectClient();
    }
    
    if (success)
    {
        // 启动IOWorker
        if (!m_ioWorker->Start())
        {
            SetLastError("启动IOWorker失败");
            if (m_socket != INVALID_SOCKET)
            {
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
            }
            CleanupWinsock();
            NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
            return false;
        }
        
        // 开始连续异步读取
        StartContinuousReading();
        
        NotifyStateChanged(TRANSPORT_OPEN, config.isServer ? "TCP服务器已启动" : "TCP已连接");
        return true;
    }
    else
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        if (m_listenSocket != INVALID_SOCKET)
        {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
        CleanupWinsock();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
}

void TcpTransport::Close()
{
    if (m_state == TRANSPORT_CLOSED)
        return;
    
    NotifyStateChanged(TRANSPORT_CLOSING, "正在关闭TCP连接");
    
    // 停止连续异步读取
    StopContinuousReading();
    
    // 停止并清理IOWorker
    if (m_ioWorker)
    {
        m_ioWorker->Stop();
    }
    
    // 停止线程
    m_stopThreads = true;
    m_stopAccept = true;
    
    // 等待接受线程结束（仅服务端模式）
    if (m_acceptThread.joinable())
    {
        m_acceptThread.join();
    }
    
    // 关闭套接字
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    
    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    
    // 清理Winsock
    CleanupWinsock();
    
    // 清空缓冲区
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_readBuffer.clear();
    }
    
    m_remoteEndpoint.clear();
    
    NotifyStateChanged(TRANSPORT_CLOSED, "TCP连接已关闭");
}

bool TcpTransport::IsOpen() const
{
    return m_state == TRANSPORT_OPEN;
}

TransportState TcpTransport::GetState() const
{
    return m_state;
}

bool TcpTransport::Configure(const TransportConfig& config)
{
    m_config = config;
    return true;
}

TransportConfig TcpTransport::GetConfiguration() const
{
    return m_config;
}

size_t TcpTransport::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

size_t TcpTransport::Write(const uint8_t* data, size_t length)
{
    if (!IsOpen() || !data || length == 0 || m_socket == INVALID_SOCKET)
    {
        return 0;
    }
    
    size_t totalSent = 0;
    const uint8_t* currentData = data;
    size_t remainingLength = length;
    
    while (remainingLength > 0 && !m_stopThreads)
    {
        // 使用select检查套接字是否可写（带超时）
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(m_socket, &writeSet);
        
        timeval timeout;
        timeout.tv_sec = 5;  // 5秒写入超时
        timeout.tv_usec = 0;
        
        int result = select(0, nullptr, &writeSet, nullptr, &timeout);
        if (result == SOCKET_ERROR)
        {
            SetLastError("写入时select失败：" + GetSocketErrorString(WSAGetLastError()));
            break;
        }
        else if (result == 0)
        {
            SetLastError("写入超时");
            break;
        }
        else if (FD_ISSET(m_socket, &writeSet))
        {
            // 套接字可写，发送数据
            int bytesSent = send(m_socket, reinterpret_cast<const char*>(currentData), 
                               static_cast<int>(remainingLength), 0);
            
            if (bytesSent > 0)
            {
                // 成功发送部分或全部数据
                totalSent += bytesSent;
                currentData += bytesSent;
                remainingLength -= bytesSent;
            }
            else if (bytesSent == 0)
            {
                // 连接已关闭
                SetLastError("连接已关闭");
                break;
            }
            else
            {
                // 发生错误
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                {
                    // 非阻塞套接字暂时无法发送，继续尝试
                    continue;
                }
                else if (error == WSAECONNRESET || error == WSAECONNABORTED)
                {
                    SetLastError("连接被重置");
                    break;
                }
                else
                {
                    SetLastError("发送数据失败：" + GetSocketErrorString(error));
                    break;
                }
            }
        }
    }
    
    return totalSent;
}

size_t TcpTransport::Read(std::vector<uint8_t>& data, size_t maxLength)
{
    data.clear();
    
    if (!IsOpen())
    {
        return 0;
    }
    
    // 如果未指定最大长度，设置默认值
    if (maxLength == 0)
    {
        maxLength = 4096;
    }
    
    size_t bytesRead = 0;
    
    // 线程安全地访问读取缓冲区
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_readBuffer.empty())
        {
            // 确定要读取的字节数
            bytesRead = std::min(maxLength, m_readBuffer.size());
            
            // 复制数据
            data.assign(m_readBuffer.begin(), m_readBuffer.begin() + bytesRead);
            
            // 从缓冲区中移除已读取的数据
            m_readBuffer.erase(m_readBuffer.begin(), m_readBuffer.begin() + bytesRead);
        }
    }
    
    return bytesRead;
}

size_t TcpTransport::Available() const
{
    if (!IsOpen())
    {
        return 0;
    }
    
    // 线程安全地获取缓冲区大小
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_readBuffer.size();
}

std::string TcpTransport::GetLastError() const
{
    return m_lastError;
}

std::string TcpTransport::GetPortName() const
{
    return m_config.ipAddress + ":" + std::to_string(m_config.port);
}

std::string TcpTransport::GetTransportType() const
{
    return m_config.isServer ? "TCP Server" : "TCP Client";
}

void TcpTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataCallback = callback;
}

void TcpTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateCallback = callback;
}

bool TcpTransport::Flush()
{
    return true;
}

bool TcpTransport::ClearBuffers()
{
    return true;
}

std::string TcpTransport::GetRemoteEndpoint() const
{
    return m_remoteEndpoint;
}

std::string TcpTransport::GetLocalEndpoint() const
{
    return "127.0.0.1:" + std::to_string(m_config.port);
}

bool TcpTransport::InitializeWinsock()
{
    if (!s_wsaInitialized)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            SetLastError("Winsock初始化失败：" + GetSocketErrorString(result));
            return false;
        }
        
        // 验证Winsock版本
        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
        {
            WSACleanup();
            SetLastError("不支持的Winsock版本");
            return false;
        }
        
        s_wsaInitialized = true;
    }
    
    s_wsaRefCount++;
    return true;
}

void TcpTransport::CleanupWinsock()
{
    if (s_wsaInitialized && s_wsaRefCount > 0)
    {
        s_wsaRefCount--;
        
        // 当引用计数为0时才真正清理Winsock
        if (s_wsaRefCount == 0)
        {
            WSACleanup();
            s_wsaInitialized = false;
        }
    }
}

bool TcpTransport::CreateSocket()
{
    // 创建TCP套接字
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
        SetLastError("创建TCP套接字失败：" + GetSocketErrorString(WSAGetLastError()));
        return false;
    }
    
    // 设置套接字选项
    BOOL reuseAddr = TRUE;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr)) == SOCKET_ERROR)
    {
        SetLastError("设置套接字选项SO_REUSEADDR失败：" + GetSocketErrorString(WSAGetLastError()));
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
    
    // 设置套接字为非阻塞模式（可选，取决于需求）
    u_long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
    {
        SetLastError("设置非阻塞模式失败：" + GetSocketErrorString(WSAGetLastError()));
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
    
    return true;
}

bool TcpTransport::ConnectClient()
{
    // 构建服务器地址结构
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(m_config.port));
    
    // 解析IP地址
    if (inet_pton(AF_INET, m_config.ipAddress.c_str(), &serverAddr.sin_addr) != 1)
    {
        SetLastError("无效的IP地址：" + m_config.ipAddress);
        return false;
    }
    
    // 发起连接（非阻塞）
    int result = connect(m_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (result == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            SetLastError("连接失败：" + GetSocketErrorString(error));
            return false;
        }
        
        // 等待连接完成
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(m_socket, &writeSet);
        
        timeval timeout;
        timeout.tv_sec = 10;  // 10秒连接超时
        timeout.tv_usec = 0;
        
        result = select(0, nullptr, &writeSet, nullptr, &timeout);
        if (result == SOCKET_ERROR)
        {
            SetLastError("等待连接完成失败：" + GetSocketErrorString(WSAGetLastError()));
            return false;
        }
        else if (result == 0)
        {
            SetLastError("连接超时");
            return false;
        }
        
        // 检查连接结果
        int connectError = 0;
        int optLen = sizeof(connectError);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, 
                       reinterpret_cast<char*>(&connectError), &optLen) == SOCKET_ERROR)
        {
            SetLastError("获取连接状态失败：" + GetSocketErrorString(WSAGetLastError()));
            return false;
        }
        
        if (connectError != 0)
        {
            SetLastError("连接被拒绝：" + GetSocketErrorString(connectError));
            return false;
        }
    }
    
    // 记录远程端点信息
    m_remoteEndpoint = m_config.ipAddress + ":" + std::to_string(m_config.port);
    
    return true;
}

bool TcpTransport::StartServer()
{
    // 创建监听套接字
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
    {
        SetLastError("创建监听套接字失败：" + GetSocketErrorString(WSAGetLastError()));
        return false;
    }
    
    // 设置套接字选项
    BOOL reuseAddr = TRUE;
    if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr)) == SOCKET_ERROR)
    {
        SetLastError("设置监听套接字选项失败：" + GetSocketErrorString(WSAGetLastError()));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    
    // 构建监听地址结构
    sockaddr_in listenAddr = {0};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(static_cast<u_short>(m_config.port));
    
    // 如果指定了IP地址则绑定到该地址，否则绑定到任意地址
    if (!m_config.ipAddress.empty() && m_config.ipAddress != "0.0.0.0")
    {
        if (inet_pton(AF_INET, m_config.ipAddress.c_str(), &listenAddr.sin_addr) != 1)
        {
            SetLastError("无效的监听IP地址：" + m_config.ipAddress);
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            return false;
        }
    }
    else
    {
        listenAddr.sin_addr.s_addr = INADDR_ANY;
    }
    
    // 绑定套接字
    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&listenAddr), sizeof(listenAddr)) == SOCKET_ERROR)
    {
        SetLastError("绑定端口失败：" + GetSocketErrorString(WSAGetLastError()));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    
    // 开始监听
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        SetLastError("开始监听失败：" + GetSocketErrorString(WSAGetLastError()));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    
    // 启动接受连接线程
    m_stopThreads = false;
    m_stopAccept = false;
    m_acceptThread = std::thread(&TcpTransport::AcceptThreadFunc, this);
    
    return true;
}

void TcpTransport::ReadThreadFunc()
{
    // 注意：此函数已被弃用，已迁移到异步I/O模式
    // 现在所有读取操作都通过IOWorker异步完成
    // 参见：StartContinuousReading() 和 OnReadCompleted()
    
    // 如果仍有代码调用此函数，记录警告并直接返回
    SetLastError("ReadThreadFunc已弃用，请使用异步I/O模式");
}

void TcpTransport::AcceptThreadFunc()
{
    while (!m_stopAccept && m_listenSocket != INVALID_SOCKET)
    {
        // 使用select检查是否有连接到来（带超时）
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_listenSocket, &readSet);
        
        timeval timeout;
        timeout.tv_sec = 1;  // 1秒超时，用于检查停止标志
        timeout.tv_usec = 0;
        
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        if (result == SOCKET_ERROR)
        {
            if (!m_stopAccept)
            {
                SetLastError("等待连接时select失败：" + GetSocketErrorString(WSAGetLastError()));
            }
            break;
        }
        else if (result == 0)
        {
            // 超时，继续循环检查停止标志
            continue;
        }
        
        // 有连接到来，接受连接
        sockaddr_in clientAddr = {0};
        int addrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSocket == INVALID_SOCKET)
        {
            if (!m_stopAccept)
            {
                SetLastError("接受连接失败：" + GetSocketErrorString(WSAGetLastError()));
            }
            continue;  // 继续等待下一个连接
        }
        
        // 设置客户端套接字为非阻塞模式
        u_long nonBlocking = 1;
        if (ioctlsocket(clientSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
        {
            SetLastError("设置客户端套接字为非阻塞模式失败：" + GetSocketErrorString(WSAGetLastError()));
            closesocket(clientSocket);
            continue;
        }
        
        // 如果已经有连接，关闭旧连接（简单的单连接模式）
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
        }
        
        // 设置新的通信套接字
        m_socket = clientSocket;
        
        // 记录客户端信息
        char clientIP[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN))
        {
            m_remoteEndpoint = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        }
        else
        {
            m_remoteEndpoint = "Unknown";
        }
        
        // 对于简单的单连接模式，接受一个连接后就退出循环
        // 复杂的多连接模式需要更多的管理逻辑
        break;
    }
}

void TcpTransport::SetLastError(const std::string& error)
{
    m_lastError = error;
}

void TcpTransport::NotifyDataReceived(const std::vector<uint8_t>& data)
{
    if (m_dataCallback)
    {
        // SOLID-D: 依赖抽象接口，添加异常边界保护 (修复回调异常保护缺失)
        try {
            m_dataCallback(data);
        }
        catch (const std::exception& e) {
            SetLastError("回调异常: " + std::string(e.what()));
        }
        catch (...) {
            SetLastError("回调发生未知异常");
        }
    }
}

void TcpTransport::NotifyStateChanged(TransportState state, const std::string& message)
{
    m_state = state;
    if (m_stateCallback)
    {
        m_stateCallback(state, message);
    }
}

void TcpTransport::StartContinuousReading()
{
    if (!m_ioWorker || !IsOpen() || m_socket == INVALID_SOCKET)
        return;
        
    m_continuousReading = true;
    
    // 使用IOWorker的实际接口启动异步读取
    m_readBuffer.resize(4096); // 确保缓冲区大小
    if (m_ioWorker->AsyncRead(reinterpret_cast<HANDLE>(m_socket), m_readBuffer,
        [this](const IOResult& result) {
            OnReadCompleted(result);
        }))
    {
        // 异步读取启动成功
    }
    else
    {
        SetLastError("启动异步读取失败");
        m_continuousReading = false;
    }
}

void TcpTransport::StopContinuousReading()
{
    m_continuousReading = false;
    
    // IOWorker不提供CancelIO方法，通过关闭句柄来取消I/O操作
    // 这将在Close()方法中处理
}

void TcpTransport::OnReadCompleted(const IOResult& result)
{
    if (!m_continuousReading || m_state != TRANSPORT_OPEN)
        return;
        
    if (result.success && result.bytesTransferred > 0)
    {
        // IOResult.data中包含接收到的数据
        std::vector<uint8_t> receivedData = result.data;
        receivedData.resize(result.bytesTransferred); // 确保大小正确
        
        // 线程安全地添加到缓冲区
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // 保留原有m_readBuffer作为内部缓冲区，新建一个存储用的vector
            static std::vector<uint8_t> s_internalBuffer;
            s_internalBuffer.insert(s_internalBuffer.end(), 
                                  receivedData.begin(), receivedData.end());
        }
        
        // 通知数据接收回调
        NotifyDataReceived(receivedData);
        
        // 继续异步读取
        if (m_continuousReading && m_ioWorker && IsOpen())
        {
            std::vector<uint8_t> nextBuffer(4096);
            m_ioWorker->AsyncRead(reinterpret_cast<HANDLE>(m_socket), nextBuffer,
                [this](const IOResult& result) {
                    OnReadCompleted(result);
                });
        }
    }
    else if (!result.success)
    {
        // 读取失败或连接断开
        if (result.errorCode != ERROR_OPERATION_ABORTED) // 非用户取消
        {
            SetLastError("异步读取失败: " + std::to_string(result.errorCode));
            NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        }
    }
}

void TcpTransport::OnWriteCompleted(const IOResult& result)
{
    // TCP写入完成处理（目前暂时简单处理）
    if (!result.success)
    {
        SetLastError("异步写入失败: " + std::to_string(result.errorCode));
    }
}

std::string TcpTransport::GetSocketErrorString(int error) const
{
    switch (error)
    {
        case WSAEWOULDBLOCK:
            return "操作将阻塞 (WSAEWOULDBLOCK)";
        case WSAECONNRESET:
            return "连接被对方重置 (WSAECONNRESET)";
        case WSAECONNABORTED:
            return "连接被中断 (WSAECONNABORTED)";
        case WSAECONNREFUSED:
            return "连接被拒绝 (WSAECONNREFUSED)";
        case WSAETIMEDOUT:
            return "连接超时 (WSAETIMEDOUT)";
        case WSAEINVAL:
            return "无效参数 (WSAEINVAL)";
        case WSAEADDRINUSE:
            return "地址已在使用 (WSAEADDRINUSE)";
        case WSAEADDRNOTAVAIL:
            return "地址不可用 (WSAEADDRNOTAVAIL)";
        case WSAENETUNREACH:
            return "网络不可达 (WSAENETUNREACH)";
        case WSAEHOSTUNREACH:
            return "主机不可达 (WSAEHOSTUNREACH)";
        case WSAEHOSTDOWN:
            return "主机已关闭 (WSAEHOSTDOWN)";
        case WSAENETDOWN:
            return "网络已关闭 (WSAENETDOWN)";
        case WSAEFAULT:
            return "地址错误 (WSAEFAULT)";
        case WSAEINTR:
            return "操作被中断 (WSAEINTR)";
        case WSAEINPROGRESS:
            return "操作正在进行 (WSAEINPROGRESS)";
        case WSAEALREADY:
            return "操作已在进行 (WSAEALREADY)";
        case WSAENOTSOCK:
            return "非套接字操作 (WSAENOTSOCK)";
        case WSAEDESTADDRREQ:
            return "需要目标地址 (WSAEDESTADDRREQ)";
        case WSAEMSGSIZE:
            return "消息过长 (WSAEMSGSIZE)";
        case WSAEPROTONOSUPPORT:
            return "不支持的协议 (WSAEPROTONOSUPPORT)";
        case WSAEPROTOTYPE:
            return "协议类型错误 (WSAEPROTOTYPE)";
        case WSAENOPROTOOPT:
            return "协议选项无效 (WSAENOPROTOOPT)";
        case WSAEOPNOTSUPP:
            return "操作不支持 (WSAEOPNOTSUPP)";
        case WSAEPFNOSUPPORT:
            return "协议族不支持 (WSAEPFNOSUPPORT)";
        case WSAEAFNOSUPPORT:
            return "地址族不支持 (WSAEAFNOSUPPORT)";
        case WSAEACCES:
            return "访问被拒绝 (WSAEACCES)";
        case WSAENOTCONN:
            return "套接字未连接 (WSAENOTCONN)";
        case WSAESHUTDOWN:
            return "套接字已关闭 (WSAESHUTDOWN)";
        case WSAEISCONN:
            return "套接字已连接 (WSAEISCONN)";
        case WSANOTINITIALISED:
            return "WSAStartup未初始化 (WSANOTINITIALISED)";
        case WSASYSNOTREADY:
            return "网络子系统不可用 (WSASYSNOTREADY)";
        case WSAVERNOTSUPPORTED:
            return "Winsock版本不支持 (WSAVERNOTSUPPORTED)";
        case WSAEDISCON:
            return "正常断开连接 (WSAEDISCON)";
        case WSATYPE_NOT_FOUND:
            return "类型未找到 (WSATYPE_NOT_FOUND)";
        case WSAHOST_NOT_FOUND:
            return "主机未找到 (WSAHOST_NOT_FOUND)";
        case WSATRY_AGAIN:
            return "临时失败，请重试 (WSATRY_AGAIN)";
        case WSANO_RECOVERY:
            return "不可恢复的错误 (WSANO_RECOVERY)";
        case WSANO_DATA:
            return "无数据记录 (WSANO_DATA)";
        case WSA_INVALID_HANDLE:
            return "无效句柄 (WSA_INVALID_HANDLE)";
        case WSA_INVALID_PARAMETER:
            return "无效参数 (WSA_INVALID_PARAMETER)";
        case WSA_IO_INCOMPLETE:
            return "I/O未完成 (WSA_IO_INCOMPLETE)";
        case WSA_IO_PENDING:
            return "I/O操作挂起 (WSA_IO_PENDING)";
        case WSA_NOT_ENOUGH_MEMORY:
            return "内存不足 (WSA_NOT_ENOUGH_MEMORY)";
        case WSA_OPERATION_ABORTED:
            return "操作被取消 (WSA_OPERATION_ABORTED)";
        default:
            return "未知套接字错误 (错误代码: " + std::to_string(error) + ")";
    }
}