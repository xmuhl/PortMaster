#pragma once
#pragma execution_character_set("utf-8")

#include "ITransport.h"
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// 网络打印协议类型
enum class NetworkPrintProtocol
{
    RAW = 0,      // RAW 9100 协议
    LPR = 1,      // LPR/LPD 协议
    IPP = 2       // IPP-over-HTTP(S) 协议
};

// 网络打印认证类型
enum class NetworkAuthType
{
    None = 0,     // 无认证
    Basic = 1,    // 基本认证
    NTLM = 2,     // NTLM认证
    Certificate = 3 // 证书认证
};

// 网络打印专用配置
struct NetworkPrintConfig : public TransportConfig
{
    std::string hostname = "192.168.1.100";    // 目标主机名或IP
    WORD port = 9100;                          // 端口号
    NetworkPrintProtocol protocol = NetworkPrintProtocol::RAW; // 协议类型
    std::string queueName = "raw";             // 队列名(LPR用)
    std::string jobName = "PortMaster_Job";    // 作业名
    std::string userName;                      // 用户名
    std::string password;                      // 密码
    NetworkAuthType authType = NetworkAuthType::None; // 认证类型
    std::string certificatePath;               // 证书路径
    
    // 连接参数
    DWORD connectTimeout = 5000;               // 连接超时(ms)
    DWORD sendTimeout = 10000;                 // 发送超时(ms)
    DWORD receiveTimeout = 10000;              // 接收超时(ms)
    bool enableKeepAlive = true;               // 启用保活
    DWORD keepAliveTime = 30000;               // 保活时间(ms)
    DWORD keepAliveInterval = 1000;            // 保活间隔(ms)
    
    // 重连参数
    bool enableReconnect = true;               // 启用重连
    int maxReconnectAttempts = 3;              // 最大重连次数
    DWORD reconnectInterval = 2000;            // 重连间隔(ms)
    
    // SSL/TLS参数 (IPP HTTPS用)
    bool enableSSL = false;                    // 启用SSL
    bool verifySSLCert = true;                 // 验证SSL证书
    std::string sslCertPath;                   // SSL证书路径
    
    // HTTP参数 (IPP用)
    std::string httpPath = "/ipp/print";       // HTTP路径
    std::string userAgent = "PortMaster/1.0";  // User-Agent
    std::string contentType = "application/ipp"; // Content-Type
    
    NetworkPrintConfig() 
    {
        portName = hostname + ":" + std::to_string(port);
        readTimeout = receiveTimeout;
        writeTimeout = sendTimeout;
        bufferSize = 8192;                     // 网络传输缓冲区较大
        asyncMode = true;                      // 默认异步模式
    }
};

// 网络连接状态
enum class NetworkConnectionState
{
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Authenticating = 3,
    Authenticated = 4,
    Sending = 5,
    Receiving = 6,
    Error = 7
};

// LPR作业状态
enum class LPRJobStatus
{
    Unknown = 0,
    Queued = 1,
    Printing = 2,
    Completed = 3,
    Error = 4,
    Cancelled = 5
};

// 网络打印传输实现类
class NetworkPrintTransport : public ITransport
{
public:
    NetworkPrintTransport();
    virtual ~NetworkPrintTransport();
    
    // ITransport接口实现
    virtual TransportError Open(const TransportConfig& config) override;
    virtual TransportError Close() override;
    virtual TransportError Write(const void* data, size_t size, size_t* written = nullptr) override;
    virtual TransportError Read(void* buffer, size_t size, size_t* read, DWORD timeout = INFINITE) override;
    virtual TransportError WriteAsync(const void* data, size_t size) override;
    virtual TransportError StartAsyncRead() override;
    virtual TransportError StopAsyncRead() override;
    virtual TransportState GetState() const override;
    virtual bool IsOpen() const override;
    virtual TransportStats GetStats() const override;
    virtual void ResetStats() override;
    virtual std::string GetPortName() const override;
    virtual void SetDataReceivedCallback(DataReceivedCallback callback) override;
    virtual void SetStateChangedCallback(StateChangedCallback callback) override;
    virtual void SetErrorOccurredCallback(ErrorOccurredCallback callback) override;
    virtual TransportError FlushBuffers() override;
    virtual size_t GetAvailableBytes() const override;
    
    // 网络打印专用方法
    NetworkConnectionState GetConnectionState() const;
    std::string GetRemoteAddress() const;
    WORD GetRemotePort() const;
    NetworkPrintProtocol GetProtocol() const;
    TransportError SendJob(const std::vector<uint8_t>& data, const std::string& jobName = "");
    TransportError CancelJob(const std::string& jobId);
    LPRJobStatus GetJobStatus(const std::string& jobId) const;
    std::vector<std::string> GetQueueStatus() const;
    
    // 静态辅助方法
    static bool ResolveHostname(const std::string& hostname, std::string& ipAddress);
    static bool IsValidIPAddress(const std::string& ip);
    static bool IsPortOpen(const std::string& hostname, WORD port, DWORD timeout = 3000);
    static std::vector<std::string> DiscoverNetworkPrinters(const std::string& subnet = "");
    static std::string GetProtocolName(NetworkPrintProtocol protocol);
    
private:
    // 内部状态
    mutable std::mutex m_mutex;
    std::atomic<TransportState> m_state;
    std::atomic<NetworkConnectionState> m_connectionState;
    SOCKET m_socket;
    NetworkPrintConfig m_config;
    TransportStats m_stats;
    
    // 连接信息
    std::string m_resolvedIP;
    struct sockaddr_in m_serverAddr;
    std::string m_currentJobId;
    std::string m_authenticationHeader;
    
    // 回调函数
    DataReceivedCallback m_dataReceivedCallback;
    StateChangedCallback m_stateChangedCallback;
    ErrorOccurredCallback m_errorOccurredCallback;
    
    // 异步操作支持
    std::atomic<bool> m_asyncReadRunning;
    std::thread m_asyncReadThread;
    std::queue<std::vector<uint8_t>> m_writeQueue;
    std::mutex m_writeQueueMutex;
    std::thread m_asyncWriteThread;
    std::atomic<bool> m_asyncWriteRunning;
    
    // 重连支持
    std::thread m_reconnectThread;
    std::atomic<bool> m_reconnectThreadRunning;
    std::atomic<int> m_reconnectAttempts;
    
    // Winsock初始化
    static std::atomic<int> s_wsaInitCount;
    static std::mutex s_wsaMutex;
    
    // 内部方法
    void SetState(TransportState newState);
    void SetConnectionState(NetworkConnectionState newState);
    void NotifyError(TransportError error, const std::string& message);
    void UpdateStats(uint64_t bytesSent, uint64_t bytesReceived);
    
    // 网络操作
    TransportError InitializeWinsock();
    void CleanupWinsock();
    TransportError CreateSocket();
    void CloseSocket();
    TransportError ConnectToHost();
    TransportError SendData(const void* data, size_t size, size_t* sent);
    TransportError ReceiveData(void* buffer, size_t size, size_t* received, DWORD timeout);
    
    // 协议实现
    TransportError SendRAWData(const void* data, size_t size);
    TransportError SendLPRJob(const void* data, size_t size, const std::string& jobName);
    TransportError SendIPPJob(const void* data, size_t size, const std::string& jobName);
    
    // LPR协议辅助
    TransportError SendLPRCommand(const std::string& command);
    TransportError ReceiveLPRResponse(std::string& response);
    std::string GenerateLPRJobId() const;
    std::string FormatLPRControlFile(const std::string& jobName, const std::string& userName, size_t dataSize) const;
    
    // IPP协议辅助
    TransportError SendHTTPRequest(const std::string& method, const std::string& path, 
                                  const std::vector<uint8_t>& data, const std::string& contentType);
    TransportError ReceiveHTTPResponse(std::vector<uint8_t>& response);
    std::string BuildHTTPHeaders(const std::string& method, const std::string& path, 
                                size_t contentLength, const std::string& contentType) const;
    std::vector<uint8_t> BuildIPPRequest(const std::vector<uint8_t>& data, const std::string& jobName) const;
    
    // 认证处理
    TransportError Authenticate();
    TransportError BasicAuthenticate();
    TransportError NTLMAuthenticate();
    TransportError CertificateAuthenticate();
    std::string BuildBasicAuthHeader() const;
    std::string Base64Encode(const std::string& input) const;
    
    // SSL/TLS支持
    TransportError InitializeSSL();
    void CleanupSSL();
    TransportError SSLHandshake();
    
    // 异步操作线程
    void AsyncReadThread();
    void AsyncWriteThread();
    void ReconnectThread();
    
    // 错误处理
    TransportError GetSocketError();
    std::string GetSocketErrorMessage(int errorCode) const;
    
    // 配置验证
    bool ValidateConfig(const NetworkPrintConfig& config) const;
    TransportError ResolveHostAddress();
    
    // 超时设置
    TransportError SetSocketTimeouts();
    TransportError SetSocketOptions();
    
    // 保活处理
    TransportError EnableKeepAlive();
    void KeepAliveMonitor();
    
    // 禁用复制构造和赋值
    NetworkPrintTransport(const NetworkPrintTransport&) = delete;
    NetworkPrintTransport& operator=(const NetworkPrintTransport&) = delete;
};

// 网络打印专用错误码
enum class NetworkPrintError
{
    Success = 0,
    WinsockInitFailed,
    SocketCreateFailed,
    HostResolveFailed,
    ConnectionFailed,
    AuthenticationFailed,
    ProtocolError,
    HTTPError,
    SSLError,
    TimeoutError,
    NetworkError,
    ServerError,
    JobError,
    QueueError,
    ConfigurationError
};

// 网络打印错误码转换
class NetworkPrintErrorConverter
{
public:
    static TransportError ConvertToTransportError(NetworkPrintError error);
    static std::string GetNetworkPrintErrorString(NetworkPrintError error);
    static NetworkPrintError ConvertFromSocketError(int socketError);
};

// 网络发现辅助类
class NetworkPrinterDiscovery
{
public:
    struct PrinterInfo
    {
        std::string ipAddress;
        std::string hostname;
        WORD port;
        NetworkPrintProtocol protocol;
        std::string queueName;
        std::string description;
        bool isOnline;
    };
    
    static std::vector<PrinterInfo> DiscoverPrinters(const std::string& subnet, DWORD timeout = 5000);
    static bool ProbePrinter(const std::string& ip, WORD port, NetworkPrintProtocol protocol, DWORD timeout = 3000);
    static std::string GetSubnetFromIP(const std::string& ip);
    static std::vector<std::string> GenerateIPRange(const std::string& subnet);
};