#include "pch.h"
#include "ReliableChannel.h"
#include "../Common/CommonTypes.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <array>

void ReliableChannel::WriteVerbose(const std::string &message)
{
    if (!m_verboseLoggingEnabled)
    {
        return;
    }
    WriteLog(message);
}

void ReliableChannel::SetVerboseLoggingEnabled(bool enabled)
{
    m_verboseLoggingEnabled = enabled;
}

bool ReliableChannel::IsVerboseLoggingEnabled() const
{
    return m_verboseLoggingEnabled;
}

int64_t ReliableChannel::GetCurrentFileSize() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_currentFileSize;
}

int64_t ReliableChannel::GetCurrentFileProgress() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_currentFileProgress;
}

std::string ReliableChannel::GetCurrentFileName() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_currentFileName;
}

std::vector<uint8_t> ReliableChannel::GetCompletedFileBuffer() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_completedFileBuffer;
}

bool ReliableChannel::HasCompletedFile() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_hasCompletedFile && !m_completedFileBuffer.empty();
}

void ReliableChannel::ClearCompletedFileBuffer()
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    m_completedFileBuffer.clear();
    m_hasCompletedFile = false;
}


// 日志写入函数
void ReliableChannel::WriteLog(const std::string &message)
{
    if (!m_verboseLoggingEnabled)
    {
        static const std::array<std::string, 14> noisyPrefixes = {
            "ProcessThread",
            "SendThread",
            "ReceiveThread",
            "HeartbeatThread",
            "ProcessIncomingFrame",
            "ProcessDataFrame",
            "ProcessAckFrame",
            "ProcessNakFrame",
            "ProcessStartFrame",
            "ProcessEndFrame",
            "AdvanceSendWindow",
            "AllocateSequence",
            "RetransmitPacket",
            "EnsureSessionStarted"
        };

        const bool hasSeverity =
            message.find("ERROR") != std::string::npos ||
            message.find("失败") != std::string::npos ||
            message.find("❌") != std::string::npos ||
            message.find("⚠") != std::string::npos ||
            message.find("invalid") != std::string::npos;

        if (!hasSeverity)
        {
            for (const auto& prefix : noisyPrefixes)
            {
                if (message.rfind(prefix, 0) == 0)
                {
                    return;
                }
            }
        }
    }

    try
    {
        // 打开日志文件（追加模式）
        std::ofstream logFile("PortMaster_debug.log", std::ios::app);
        if (logFile.is_open())
        {
            logFile << message << std::endl;
            logFile.close();
        }
    }
    catch (...)
    {
        // 忽略日志写入错误
    }
}

// 构造函数
ReliableChannel::ReliableChannel()
    : m_initialized(false), m_connected(false), m_shutdown(false), m_retransmitting(false), m_sendBase(0), m_sendNext(0), m_receiveBase(0), m_receiveNext(0), m_heartbeatSequence(0), m_currentFileName(), m_currentFileSize(0), m_currentFileProgress(0), m_fileTransferActive(false), m_transferStartTime(std::chrono::steady_clock::now()), m_sendBytesAcked(0), m_sendTotalBytes(0), m_handshakeCompleted(false), m_handshakeSequence(0), m_sessionId(0), m_rttMs(100), m_timeoutMs(500), m_frameCodec(std::make_unique<FrameCodec>())
{
    m_lastActivity = std::chrono::steady_clock::now();
    WriteLog("ReliableChannel constructor called");
}

// 析构函数
ReliableChannel::~ReliableChannel()
{
    Shutdown();
}

// 初始化
bool ReliableChannel::Initialize(std::shared_ptr<ITransport> transport, const ReliableConfig &config)
{
    WriteLog("ReliableChannel::Initialize called with windowSize=" + std::to_string(config.windowSize) +
             ", maxPayloadSize=" + std::to_string(config.maxPayloadSize));

    if (!transport)
    {
        WriteLog("ReliableChannel::Initialize failed: transport is null");
        return false;
    }

    // 验证配置参数
    if (config.windowSize == 0)
    {
        WriteLog("ReliableChannel::Initialize failed: windowSize is 0");
        ReportError("窗口大小不能为0");
        return false;
    }

    if (config.windowSize > 256)
    {
        WriteLog("ReliableChannel::Initialize failed: windowSize > 256 (" + std::to_string(config.windowSize) + ")");
        ReportError("窗口大小不能超过256");
        return false;
    }

    if (config.maxPayloadSize == 0)
    {
        WriteLog("ReliableChannel::Initialize failed: maxPayloadSize is 0");
        ReportError("最大负载大小不能为0");
        return false;
    }

    if (config.maxPayloadSize > 65536)
    {
        WriteLog("ReliableChannel::Initialize failed: maxPayloadSize > 65536 (" + std::to_string(config.maxPayloadSize) + ")");
        ReportError("最大负载大小不能超过65536");
        return false;
    }

    WriteLog("ReliableChannel::Initialize validation passed, initializing windows...");

    std::lock_guard<std::mutex> lock(m_windowMutex);

    m_transport = transport;
    m_config = config;
    m_frameCodec->SetMaxPayloadSize(config.maxPayloadSize);

    // 初始化窗口
    WriteLog("Resizing sendWindow to " + std::to_string(config.windowSize) +
             ", receiveWindow to " + std::to_string(config.windowSize));
    m_sendWindow.resize(config.windowSize);
    m_receiveWindow.resize(config.windowSize);

    // 重置序列号
    m_sendBase = 0;
    m_sendNext = 0;
    m_receiveBase = 0;
    m_receiveNext = 0;
    m_heartbeatSequence = 0;  // 重置心跳序列号

    // 重置统计
    ResetStats();

    WriteLog("ReliableChannel::Initialize completed successfully");
    m_initialized = true;
    return true;
}

// 关闭
void ReliableChannel::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_shutdown = true;

    // 通知所有等待的线程
    m_sendCondition.notify_all();
    m_receiveCondition.notify_all();
    m_windowCondition.notify_all();

    // 停止所有线程
    if (m_processThread.joinable())
    {
        m_processThread.join();
    }
    if (m_sendThread.joinable())
    {
        m_sendThread.join();
    }
    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }
    if (m_heartbeatThread.joinable())
    {
        m_heartbeatThread.join();
    }

    // 清理队列
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        while (!m_sendQueue.empty())
        {
            m_sendQueue.pop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        while (!m_receiveQueue.empty())
        {
            m_receiveQueue.pop();
        }
    }

    // 清理窗口
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        for (auto &slot : m_sendWindow)
        {
            slot.inUse = false;
            slot.packet.reset();
        }
        for (auto &slot : m_receiveWindow)
        {
            slot.inUse = false;
            slot.packet.reset();
        }
    }
    m_windowCondition.notify_all();

    m_transport.reset();
    m_initialized = false;
}

// 连接
bool ReliableChannel::Connect()
{
    if (!m_initialized || !m_transport)
    {
        return false;
    }

    if (m_connected)
    {
        return true;
    }

    // 启动线程
    m_shutdown = false;
    m_processThread = std::thread(&ReliableChannel::ProcessThread, this);
    m_sendThread = std::thread(&ReliableChannel::SendThread, this);
    m_receiveThread = std::thread(&ReliableChannel::ReceiveThread, this);
    m_heartbeatThread = std::thread(&ReliableChannel::HeartbeatThread, this);

    m_connected = true;
    UpdateState(true);

    return true;
}

// 断开连接
bool ReliableChannel::Disconnect()
{
    if (!m_connected)
    {
        return true;
    }

    m_connected = false;
    UpdateState(false);

    // 通知所有等待的线程
    m_sendCondition.notify_all();
    m_receiveCondition.notify_all();
    m_windowCondition.notify_all();

    return true;
}

// 是否已连接
bool ReliableChannel::IsConnected() const
{
    return m_connected && m_initialized;
}

// 发送数据
bool ReliableChannel::Send(const std::vector<uint8_t> &data)
{
    if (!IsConnected())
    {
        return false;
    }

    // 【P0修复】确保会话已启动，如果未启动则自动执行握手
    if (!EnsureSessionStarted())
    {
        WriteLog("Send: ERROR - failed to ensure session started, cannot send data");
        return false;
    }

    std::unique_lock<std::mutex> lock(m_sendMutex);

    // 【修复】队列满时阻塞等待，而不是拒绝数据
    size_t maxQueueSize = m_config.windowSize * 10;
    while (m_sendQueue.size() >= maxQueueSize)
    {
        // 检查通道状态
        if (m_shutdown || !IsConnected())
        {
            WriteLog("Send: 通道已关闭，无法发送数据");
            return false;
        }

        // 等待队列空间（超时1秒）
        WriteLog("Send: 队列已满(size=" + std::to_string(m_sendQueue.size()) +
                 ", max=" + std::to_string(maxQueueSize) + ")，等待空间...");

        auto status = m_sendCondition.wait_for(
            lock,
            std::chrono::milliseconds(1000),
            [this, maxQueueSize] {
                return m_sendQueue.size() < maxQueueSize ||
                       m_shutdown ||
                       !IsConnected();
            });

        if (!status)
        {
            // 超时，继续等待（除非通道关闭）
            if (m_shutdown || !IsConnected())
            {
                WriteLog("Send: 等待期间通道关闭");
                return false;
            }
            WriteLog("Send: 等待超时，继续等待...");
        }
    }

    // 队列有空间，添加数据
    m_sendQueue.push(data);
    m_sendCondition.notify_one();

    WriteLog("Send: data queued successfully, queue size=" + std::to_string(m_sendQueue.size()));
    return true;
}

bool ReliableChannel::Send(const void *data, size_t size)
{
    if (!data || size == 0)
    {
        return false;
    }

    std::vector<uint8_t> buffer(size);
    std::memcpy(buffer.data(), data, size);
    return Send(buffer);
}

// 接收数据
bool ReliableChannel::Receive(std::vector<uint8_t> &data, uint32_t timeout)
{
    if (!IsConnected())
    {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_receiveMutex);

    // 等待数据可用
    if (timeout > 0)
    {
        auto result = m_receiveCondition.wait_for(lock, std::chrono::milliseconds(timeout),
                                                  [this]
                                                  { return !m_receiveQueue.empty() || !m_connected || m_shutdown; });

        if (!result)
        {
            return false; // 超时
        }
    }
    else
    {
        m_receiveCondition.wait(lock, [this]
                                { return !m_receiveQueue.empty() || !m_connected || m_shutdown; });
    }

    if ((!m_connected || m_shutdown) && m_receiveQueue.empty())
    {
        return false; // 连接已断开或正在关闭
    }

    if (!m_receiveQueue.empty())
    {
        data = m_receiveQueue.front();
        m_receiveQueue.pop();
        return true;
    }

    return false;
}

size_t ReliableChannel::Receive(void *buffer, size_t size, uint32_t timeout)
{
    if (!buffer || size == 0)
    {
        return 0;
    }

    std::vector<uint8_t> data;
    if (!Receive(data, timeout))
    {
        return 0;
    }

    size_t copySize = (std::min)(size, data.size());
    std::memcpy(buffer, data.data(), copySize);
    return copySize;
}

// 发送文件
bool ReliableChannel::SendFile(const std::string &filePath, std::function<void(int64_t, int64_t)> progressCallback)
{
    if (!IsConnected())
    {
        return false;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        ReportError("无法打开文件: " + filePath);
        return false;
    }

    // 获取文件信息
    file.seekg(0, std::ios::end);
    int64_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // 设置文件传输状态
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_currentFileName = filePath;
        m_currentFileSize = fileSize;
        m_currentFileProgress = 0;
        m_fileTransferActive = true;
        m_transferStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间

        // 【P1优化】初始化发送进度跟踪
        m_sendBytesAcked = 0;
        m_sendTotalBytes = fileSize;
    }

    // 发送开始帧
    auto modifyTime = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    WriteLog("SendFile: sending START frame for handshake");
    if (!SendStart(filePath, fileSize, modifyTime))
    {
        file.close();
        m_fileTransferActive = false;
        WriteLog("SendFile: ERROR - SendStart failed");
        return false;
    }

    // 【关键修复】等待握手真正完成 - 等待接收端 ACK 响应
    WriteLog("SendFile: waiting for handshake ACK response");
    uint32_t handshakeTimeoutMs = m_config.timeoutMax * 2; // 握手超时时间(ms)
    WriteLog("SendFile: handshake timeout set to " + std::to_string(handshakeTimeoutMs) + "ms");

    // 使用条件变量和超时等待握手完成
    bool handshakeCompleted = WaitForHandshakeCompletion(handshakeTimeoutMs);

    if (!handshakeCompleted)
    {
        WriteLog("SendFile: ERROR - handshake timeout after " + std::to_string(handshakeTimeoutMs) + "ms");
        ReportError("握手超时，接收端未响应START帧");
        file.close();
        m_fileTransferActive = false;
        return false;
    }

    if (!m_connected)
    {
        WriteLog("SendFile: ERROR - connection lost during handshake");
        file.close();
        m_fileTransferActive = false;
        return false;
    }

    WriteLog("SendFile: handshake completed, starting data transmission");

    // 发送文件数据
    std::vector<uint8_t> buffer(m_config.maxPayloadSize);
    int64_t bytesSent = 0;

    while (!file.eof() && m_connected)
    {
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        size_t bytesRead = static_cast<size_t>(file.gcount());

        if (bytesRead > 0)
        {
            buffer.resize(bytesRead);
            if (!SendPacket(AllocateSequence(), buffer))
            {
                ReportError("发送文件数据失败");
                file.close();
                m_fileTransferActive = false;
                return false;
            }

            bytesSent += bytesRead;
            UpdateProgress(bytesSent, fileSize);

            if (progressCallback)
            {
                progressCallback(bytesSent, fileSize);
            }
        }
    }

    file.close();

    // 发送结束帧
    if (m_connected && !SendEnd())
    {
        ReportError("发送文件结束帧失败");
        m_fileTransferActive = false;
        return false;
    }

    m_fileTransferActive = false;
    return m_connected;
}

// 接收文件
bool ReliableChannel::ReceiveFile(const std::string &filePath, std::function<void(int64_t, int64_t)> progressCallback)
{
    if (!IsConnected())
    {
        return false;
    }

    // 设置文件传输状态
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_currentFileName = filePath;
        m_currentFileSize = 0;
        m_currentFileProgress = 0;
        m_fileTransferActive = true;
        m_transferStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        ReportError("无法创建文件: " + filePath);
        m_fileTransferActive = false;
        return false;
    }

    // 等待文件传输完成
    while (m_fileTransferActive && m_connected)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (progressCallback)
        {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            progressCallback(m_currentFileProgress, m_currentFileSize);
        }
    }

    file.close();
    return !m_fileTransferActive && m_connected;
}

// 设置配置
void ReliableChannel::SetConfig(const ReliableConfig &config)
{
    // 验证配置参数
    if (config.windowSize == 0)
    {
        ReportError("窗口大小不能为0");
        return;
    }

    if (config.windowSize > 256)
    {
        ReportError("窗口大小不能超过256");
        return;
    }

    if (config.maxPayloadSize == 0)
    {
        ReportError("最大负载大小不能为0");
        return;
    }

    if (config.maxPayloadSize > 65536)
    {
        ReportError("最大负载大小不能超过65536");
        return;
    }

    std::lock_guard<std::mutex> lock(m_windowMutex);
    m_config = config;
    m_frameCodec->SetMaxPayloadSize(config.maxPayloadSize);

    // 调整窗口大小
    if (m_sendWindow.size() != config.windowSize)
    {
        m_sendWindow.resize(config.windowSize);
        m_receiveWindow.resize(config.windowSize);
    }
}

// 获取配置
ReliableConfig ReliableChannel::GetConfig() const
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    return m_config;
}

// 获取统计信息
ReliableStats ReliableChannel::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// 重置统计信息
void ReliableChannel::ResetStats()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ReliableStats();
}

// 设置回调函数
void ReliableChannel::SetDataReceivedCallback(std::function<void(const std::vector<uint8_t> &)> callback)
{
    m_dataReceivedCallback = callback;
}

void ReliableChannel::SetStateChangedCallback(std::function<void(bool)> callback)
{
    m_stateChangedCallback = callback;
}

void ReliableChannel::SetErrorCallback(std::function<void(const std::string &)> callback)
{
    m_errorCallback = callback;
}

void ReliableChannel::SetProgressCallback(std::function<void(int64_t, int64_t)> callback)
{
    m_progressCallback = callback;
}

// 【新增】设置传输完成回调
void ReliableChannel::SetCompleteCallback(std::function<void(bool)> callback)
{
    m_completeCallback = callback;
}

// 获取本地序列号
uint16_t ReliableChannel::GetLocalSequence() const
{
    return m_sendNext;
}

// 获取远端序列号
uint16_t ReliableChannel::GetRemoteSequence() const
{
    return m_receiveNext;
}

// 获取发送队列大小
size_t ReliableChannel::GetSendQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    return m_sendQueue.size();
}

// 获取接收队列大小
size_t ReliableChannel::GetReceiveQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);
    return m_receiveQueue.size();
}

// 【可靠模式按钮管控】检查文件传输是否活跃
bool ReliableChannel::IsFileTransferActive() const
{
    if (!m_fileTransferActive) return false;

    // 检查传输超时（需要非const访问来修改状态）
    if (const_cast<ReliableChannel*>(this)->CheckTransferTimeout()) {
        return false;
    }

    // 返回当前文件传输的活跃状态，用于UI层判断保存按钮启用时机
    return m_fileTransferActive;
}

// 处理线程
void ReliableChannel::ProcessThread()
{
    WriteVerbose("ProcessThread started");

    std::vector<uint8_t> buffer(4096);

    while (!m_shutdown)
    {
        WriteVerbose("ProcessThread: checking connection status...");

        if (!m_connected)
        {
            WriteVerbose("ProcessThread: not connected, sleeping...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        WriteVerbose("ProcessThread: reading from transport...");

        // 接收数据
        size_t bytesReceived = 0;
        TransportError error = m_transport->Read(buffer.data(), buffer.size(), &bytesReceived, 100);
        WriteVerbose("ProcessThread: transport read result: error=" + std::to_string(static_cast<int>(error)) +
                 ", bytesReceived=" + std::to_string(bytesReceived));

        if (error == TransportError::Success && bytesReceived > 0)
        {
            WriteVerbose("ProcessThread: received " + std::to_string(bytesReceived) + " bytes, processing...");

            // 添加到帧编解码器
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesReceived);
            WriteVerbose("ProcessThread: appending data to frame codec...");
            m_frameCodec->AppendData(data);

            // 处理可用的帧
            Frame frame;
            int frameCount = 0;
            while (m_frameCodec->TryGetFrame(frame))
            {
                frameCount++;
                WriteVerbose("ProcessThread: processing frame " + std::to_string(frameCount) +
                         ", type=" + std::to_string(static_cast<int>(frame.type)) +
                         ", sequence=" + std::to_string(frame.sequence));
                ProcessIncomingFrame(frame);
            }

            WriteVerbose("ProcessThread: processed " + std::to_string(frameCount) + " frames");

            // 更新统计
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.bytesReceived += bytesReceived;
                WriteVerbose("ProcessThread: updated stats, total bytesReceived=" + std::to_string(m_stats.bytesReceived));
            }
        }
        else if (error != TransportError::Success)
        {
            WriteVerbose("ProcessThread: transport read error: " + std::to_string(static_cast<int>(error)));
        }

        WriteVerbose("ProcessThread: checking for retransmissions...");

        // 处理重传
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            auto now = std::chrono::steady_clock::now();

            int retransmitCount = 0;
            for (auto &slot : m_sendWindow)
            {
                if (slot.inUse && slot.packet && !slot.packet->acknowledged)
                {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       now - slot.packet->timestamp)
                                       .count();

                    WriteVerbose("ProcessThread: checking slot sequence=" + std::to_string(slot.packet->sequence) +
                             ", elapsed=" + std::to_string(elapsed) + "ms, retryCount=" + std::to_string(slot.packet->retryCount));

                    if (elapsed > CalculateTimeout())
                    {
                        if (slot.packet->retryCount < m_config.maxRetries)
                        {
                            WriteVerbose("ProcessThread: retransmitting packet sequence=" + std::to_string(slot.packet->sequence) +
                                     ", attempt " + std::to_string(slot.packet->retryCount + 1) + "/" + std::to_string(m_config.maxRetries));
                            m_retransmitting = true; // 设置重传标志
                            RetransmitPacketInternal(slot.packet->sequence); // 使用内部版本，已持有锁
                            m_retransmitting = false; // 清除重传标志
                            retransmitCount++;
                        }
                        else
                        {
                            // 【关键修复】超过最大重试次数，标记包为失败并清理
                            uint16_t failedSequence = slot.packet->sequence; // 保存序列号用于后续处理
                            WriteVerbose("ProcessThread: packet sequence=" + std::to_string(failedSequence) +
                                     " exceeded max retries (" + std::to_string(m_config.maxRetries) + "), marking as failed");

                            // 更新统计
                            {
                                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                                m_stats.timeouts++;
                            }

                            // 报告传输错误（在清理之前）
                            ReportError("数据包重传失败，序列号: " + std::to_string(failedSequence));

                            // 检查是否需要推进发送窗口（在清理之前）
                            bool shouldAdvanceWindow = (failedSequence == m_sendBase);

                            // 清理失败的包
                            slot.inUse = false;
                            slot.packet.reset();
                            m_windowCondition.notify_all();

                            // 推进发送窗口（如果这个包是窗口基）
                            if (shouldAdvanceWindow)
                            {
                                WriteVerbose("ProcessThread: advancing send window due to failed packet at base sequence " + std::to_string(failedSequence));
                                AdvanceSendWindow();
                            }
                        }
                    }
                }
            }

            WriteVerbose("ProcessThread: checked retransmissions, count=" + std::to_string(retransmitCount));
        }
    }

    WriteVerbose("ProcessThread exiting");
}

// 发送线程
void ReliableChannel::SendThread()
{
    WriteVerbose("SendThread started");

    while (!m_shutdown)
    {
        WriteVerbose("SendThread: waiting for data or shutdown...");

        std::vector<uint8_t> data;

        // 获取要发送的数据（原子操作）
        {
            std::unique_lock<std::mutex> lock(m_sendMutex);

            // 等待发送数据
            WriteVerbose("SendThread: waiting on condition variable...");
            m_sendCondition.wait(lock, [this]
                                 { return !m_sendQueue.empty() || !m_connected || m_shutdown; });

            WriteVerbose("SendThread: condition variable triggered");

            if (m_shutdown)
            {
                WriteVerbose("SendThread: shutdown detected, exiting");
                break;
            }

            if (!m_connected)
            {
                WriteVerbose("SendThread: not connected, continuing");
                continue;
            }

            if (m_sendQueue.empty())
            {
                WriteVerbose("SendThread: queue is empty, continuing");
                continue;
            }

            // 获取数据并从队列移除
            WriteVerbose("SendThread: getting data from queue, queue size: " + std::to_string(m_sendQueue.size()));
            data = m_sendQueue.front();
            m_sendQueue.pop();
            WriteVerbose("SendThread: data extracted, size: " + std::to_string(data.size()) + " bytes");

            // 【修复】唤醒可能等待队列空间的Send()调用
            m_sendCondition.notify_all();
        }

        // 在没有锁的情况下处理发送
        try
        {
            WriteVerbose("SendThread: allocating sequence number...");
            // 分配序列号并发送
            uint16_t sequence = AllocateSequence();
            WriteVerbose("SendThread: allocated sequence " + std::to_string(sequence) + ", sending packet...");
            if (!SendPacket(sequence, data))
            {
                WriteVerbose("SendThread: SendPacket failed");
                ReportError("发送数据包失败");
            }
            else
            {
                WriteVerbose("SendThread: SendPacket succeeded");
            }
        }
        catch (const std::exception &e)
        {
            WriteVerbose("SendThread: exception caught: " + std::string(e.what()));
            ReportError("发送线程异常: " + std::string(e.what()));
        }
        catch (...)
        {
            WriteVerbose("SendThread: unknown exception caught");
            ReportError("发送线程未知异常");
        }
    }

    WriteVerbose("SendThread exiting");
}

// 接收线程
void ReliableChannel::ReceiveThread()
{
    WriteVerbose("ReceiveThread started");

    while (!m_shutdown)
    {
        WriteVerbose("ReceiveThread: checking connection status...");

        if (!m_connected)
        {
            WriteVerbose("ReceiveThread: not connected, sleeping...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        WriteVerbose("ReceiveThread: checking receive window...");

        // 检查接收窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);

            WriteVerbose("ReceiveThread: locked window mutex, checking slots...");

            while (true)
            {
                uint16_t expected = m_receiveBase;
                bool found = false;

                WriteVerbose("ReceiveThread: looking for sequence " + std::to_string(expected));

                for (auto &slot : m_receiveWindow)
                {
                    if (slot.inUse && slot.packet && slot.packet->sequence == expected)
                    {
                        WriteVerbose("ReceiveThread: found matching packet at slot, sequence=" + std::to_string(slot.packet->sequence));

                        // 将数据放入接收队列
                        int64_t updatedProgress = -1;
                        int64_t progressTotal = 0;
                        size_t chunkSize = slot.packet->data.size();

                        {
                            std::lock_guard<std::mutex> receiveLock(m_receiveMutex);
                            WriteVerbose("ReceiveThread: pushing data to receive queue, size=" + std::to_string(chunkSize));
                            m_receiveQueue.push(slot.packet->data);

                            if (m_fileTransferActive)
                            {
                                m_completedFileBuffer.insert(
                                    m_completedFileBuffer.end(),
                                    slot.packet->data.begin(),
                                    slot.packet->data.end());

                                m_currentFileProgress += static_cast<int64_t>(chunkSize);
                                if (m_currentFileSize > 0 && m_currentFileProgress > m_currentFileSize)
                                {
                                    m_currentFileProgress = m_currentFileSize;
                                }

                                updatedProgress = m_currentFileProgress;
                                progressTotal = (m_currentFileSize > 0) ? m_currentFileSize : m_currentFileProgress;
                            }

                            m_receiveCondition.notify_one();
                            WriteVerbose("ReceiveThread: data pushed, notifying condition");
                        }

                        if (updatedProgress >= 0)
                        {
                            UpdateProgress(updatedProgress, progressTotal);
                        }

                        // 更新接收窗口
                        WriteVerbose("ReceiveThread: updating receive window, old base=" + std::to_string(m_receiveBase));
                        slot.inUse = false;
                        slot.packet.reset();
                        m_receiveBase = (m_receiveBase + 1) % 65536;
                        WriteVerbose("ReceiveThread: new base=" + std::to_string(m_receiveBase));
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    WriteVerbose("ReceiveThread: no matching packet found, breaking loop");
                    break;
                }
            }
        }

        WriteVerbose("ReceiveThread: sleeping for 10ms...");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WriteVerbose("ReceiveThread exiting");
}

// 心跳线程
void ReliableChannel::HeartbeatThread()
{
    WriteVerbose("HeartbeatThread started");

    while (!m_shutdown)
    {
        WriteVerbose("HeartbeatThread: checking connection status...");

        if (m_connected)
        {
            // 在重传期间跳过心跳发送，避免序列冲突
            if (m_retransmitting)
            {
                WriteVerbose("HeartbeatThread: skipping heartbeat during retransmission");
            }
            else
            {
                WriteVerbose("HeartbeatThread: sending heartbeat...");
                SendHeartbeat();
            }

            // 检查连接超时
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - m_lastActivity)
                               .count();

            WriteVerbose("HeartbeatThread: elapsed time since last activity: " + std::to_string(elapsed) + "ms");

            if (elapsed > m_config.timeoutMax * 3)
            {
                WriteVerbose("HeartbeatThread: connection timeout detected, elapsed=" + std::to_string(elapsed) +
                         ", timeoutMax=" + std::to_string(m_config.timeoutMax));
                ReportError("连接超时");
                Disconnect();
            }
            else
            {
                WriteVerbose("HeartbeatThread: connection is healthy");
            }
        }
        else
        {
            WriteVerbose("HeartbeatThread: not connected");
        }

        WriteVerbose("HeartbeatThread: sleeping for " + std::to_string(m_config.heartbeatInterval) + "ms...");
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.heartbeatInterval));
    }

    WriteVerbose("HeartbeatThread exiting");
}

// 处理入站帧
void ReliableChannel::ProcessIncomingFrame(const Frame &frame)
{
    WriteVerbose("ProcessIncomingFrame called: type=" + std::to_string(static_cast<int>(frame.type)) +
             ", sequence=" + std::to_string(frame.sequence) +
             ", payload.size()=" + std::to_string(frame.payload.size()) +
             ", valid=" + std::to_string(frame.valid));

    if (!frame.valid)
    {
        WriteVerbose("ProcessIncomingFrame: frame is invalid, incrementing invalid packet count");

        // 记录无效帧统计
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.packetsInvalid++;
            WriteVerbose("ProcessIncomingFrame: invalid packet count now " + std::to_string(m_stats.packetsInvalid));
        }
        return;
    }

    WriteVerbose("ProcessIncomingFrame: frame is valid, updating activity time");

    // 更新活动时间
    m_lastActivity = std::chrono::steady_clock::now();

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsReceived++;
        WriteVerbose("ProcessIncomingFrame: received packet count now " + std::to_string(m_stats.packetsReceived));
    }

    WriteVerbose("ProcessIncomingFrame: processing frame type " + std::to_string(static_cast<int>(frame.type)));

    // 根据帧类型处理
    switch (frame.type)
    {
    case FrameType::FRAME_DATA:
        WriteVerbose("ProcessIncomingFrame: calling ProcessDataFrame");
        ProcessDataFrame(frame);
        WriteVerbose("ProcessIncomingFrame: ProcessDataFrame completed");
        break;

    case FrameType::FRAME_ACK:
        WriteVerbose("ProcessIncomingFrame: calling ProcessAckFrame with sequence " + std::to_string(frame.sequence));
        ProcessAckFrame(frame.sequence);
        WriteVerbose("ProcessIncomingFrame: ProcessAckFrame completed");
        break;

    case FrameType::FRAME_NAK:
        WriteVerbose("ProcessIncomingFrame: calling ProcessNakFrame with sequence " + std::to_string(frame.sequence));
        ProcessNakFrame(frame.sequence);
        WriteVerbose("ProcessIncomingFrame: ProcessNakFrame completed");
        break;

    case FrameType::FRAME_START:
        WriteVerbose("ProcessIncomingFrame: calling ProcessStartFrame");
        ProcessStartFrame(frame);
        WriteVerbose("ProcessIncomingFrame: ProcessStartFrame completed");
        break;

    case FrameType::FRAME_END:
        WriteVerbose("ProcessIncomingFrame: calling ProcessEndFrame");
        ProcessEndFrame(frame);
        WriteVerbose("ProcessIncomingFrame: ProcessEndFrame completed");
        break;

    case FrameType::FRAME_HEARTBEAT:
        WriteVerbose("ProcessIncomingFrame: calling ProcessHeartbeatFrame");
        ProcessHeartbeatFrame(frame);
        WriteVerbose("ProcessIncomingFrame: ProcessHeartbeatFrame completed");
        break;

    default:
        WriteLog("ProcessIncomingFrame: unknown frame type " + std::to_string(static_cast<int>(frame.type)));
        // 未知帧类型，记录错误
        ReportError("未知帧类型: " + std::to_string(static_cast<int>(frame.type)));
        break;
    }

    WriteVerbose("ProcessIncomingFrame: completed processing frame");
}

// 处理数据帧
void ReliableChannel::ProcessDataFrame(const Frame &frame)
{
    WriteVerbose("ProcessDataFrame called: sequence=" + std::to_string(frame.sequence) +
             ", payload.size()=" + std::to_string(frame.payload.size()) +
             ", receiveBase=" + std::to_string(m_receiveBase) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    // 检查序列号是否在接收窗口内
    bool inWindow = IsSequenceInWindow(frame.sequence, m_receiveBase, m_config.windowSize);
    WriteVerbose("ProcessDataFrame: sequence in window check: " + std::to_string(inWindow) +
             ", window range=[" + std::to_string(m_receiveBase) + "," + std::to_string(m_receiveBase + m_config.windowSize) + ")");

    if (!inWindow)
    {
        uint16_t expected = m_receiveBase;
        uint16_t lastAck = (expected == 0) ? 65535 : static_cast<uint16_t>(expected - 1);

        WriteVerbose("ProcessDataFrame: sequence outside window, expected=" + std::to_string(expected) +
                 ", received=" + std::to_string(frame.sequence) + ", sending duplicate ACK");

        // 再次确认最后一次成功的序列，提示对端重发缺失的数据
        SendAck(lastAck);
        return;
    }

    WriteVerbose("ProcessDataFrame: sequence in window, processing...");

    // 将数据放入接收窗口
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        WriteVerbose("ProcessDataFrame: locked window mutex");

        // 确保窗口大小有效
        if (m_config.windowSize == 0 || m_receiveWindow.empty())
        {
            WriteLog("ProcessDataFrame: ERROR - receive window not initialized or size is 0");
            ReportError("接收窗口未初始化或大小为0");
            return;
        }

        uint16_t index = frame.sequence % m_config.windowSize;
        WriteVerbose("ProcessDataFrame: calculated index=" + std::to_string(index) +
                 ", receiveWindow.size()=" + std::to_string(m_receiveWindow.size()));

        // 边界检查
        if (index >= m_receiveWindow.size())
        {
            WriteLog("ProcessDataFrame: ERROR - receive window index out of bounds: " + std::to_string(index) +
                     " >= " + std::to_string(m_receiveWindow.size()));
            ReportError("接收窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_receiveWindow.size()));
            return;
        }

        // 【P0修复】检查窗口slot是否已有该序列号的数据（去重保护）
        if (m_receiveWindow[index].inUse &&
            m_receiveWindow[index].packet &&
            m_receiveWindow[index].packet->sequence == frame.sequence)
        {
            // 这是重传数据，已经处理过，只需重新发送ACK
            WriteVerbose("ProcessDataFrame: 检测到重传数据seq=" + std::to_string(frame.sequence) +
                     "，跳过重复处理，仅发送ACK");
            SendAck(frame.sequence);
            return;
        }

        // 【P0修复】只有新数据才更新slot和进度
        WriteVerbose("ProcessDataFrame: setting window slot " + std::to_string(index) + " to inUse=true");
        m_receiveWindow[index].inUse = true;

        if (!m_receiveWindow[index].packet)
        {
            WriteVerbose("ProcessDataFrame: creating new packet for slot " + std::to_string(index));
            m_receiveWindow[index].packet = std::make_shared<Packet>();
        }

        m_receiveWindow[index].packet->sequence = frame.sequence;
        m_receiveWindow[index].packet->data = frame.payload;

        // ✅ 只有首次接收才更新进度
        m_currentFileProgress += frame.payload.size();
        if (m_currentFileSize > 0 && m_currentFileProgress > m_currentFileSize)
        {
            m_currentFileProgress = m_currentFileSize;
        }

        WriteVerbose("ProcessDataFrame: 新数据seq=" + std::to_string(frame.sequence) +
                 ", size=" + std::to_string(frame.payload.size()) +
                 ", progress=" + std::to_string(m_currentFileProgress) + "/" +
                 std::to_string(m_currentFileSize));
    }

    WriteVerbose("ProcessDataFrame: window update completed, sending ACK");

    // 发送ACK
    SendAck(frame.sequence);

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.bytesReceived += frame.payload.size();
        WriteVerbose("ProcessDataFrame: stats updated, bytesReceived=" + std::to_string(m_stats.bytesReceived));
    }

    WriteVerbose("ProcessDataFrame: completed successfully");
}

// 处理ACK帧
void ReliableChannel::ProcessAckFrame(uint16_t sequence)
{
    WriteVerbose("ProcessAckFrame called: sequence=" + std::to_string(sequence) +
             ", sendBase=" + std::to_string(m_sendBase) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    std::lock_guard<std::mutex> lock(m_windowMutex);
    WriteVerbose("ProcessAckFrame: locked window mutex");

    // 确保窗口大小有效
    if (m_config.windowSize == 0 || m_sendWindow.empty())
    {
        WriteLog("ProcessAckFrame: ERROR - send window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return;
    }

    // 在发送窗口中查找对应的包
    uint16_t index = sequence % m_config.windowSize;
    WriteVerbose("ProcessAckFrame: calculated index=" + std::to_string(index) +
             ", sendWindow.size()=" + std::to_string(m_sendWindow.size()));

    // 边界检查
    if (index >= m_sendWindow.size())
    {
        WriteLog("ProcessAckFrame: ERROR - send window index out of bounds: " + std::to_string(index) +
                 " >= " + std::to_string(m_sendWindow.size()));
        ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
        return;
    }

    WriteVerbose("ProcessAckFrame: checking slot " + std::to_string(index) +
             ", inUse=" + std::to_string(m_sendWindow[index].inUse));

    if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
        m_sendWindow[index].packet->sequence == sequence && !m_sendWindow[index].packet->acknowledged)
    {
        WriteVerbose("ProcessAckFrame: processing ACK for packet " + std::to_string(sequence));

        // 标记为已确认
        m_sendWindow[index].packet->acknowledged = true;
        WriteVerbose("ProcessAckFrame: packet marked as acknowledged");

        // 更新RTT
        auto now = std::chrono::steady_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - m_sendWindow[index].packet->timestamp)
                       .count();
        WriteVerbose("ProcessAckFrame: calculated RTT=" + std::to_string(rtt) + "ms");
        UpdateRTT(static_cast<uint32_t>(rtt));

        // 【P1优化】更新发送进度（基于ACK）
        size_t dataSize = m_sendWindow[index].packet->data.size();
        int64_t ackedBytes = m_sendBytesAcked.fetch_add(dataSize) + dataSize;

        // 回调进度更新（仅在文件传输活跃时）
        if (m_fileTransferActive && m_progressCallback && m_sendTotalBytes > 0)
        {
            UpdateProgress(ackedBytes, m_sendTotalBytes);
            WriteVerbose("ProcessAckFrame: 发送进度=" +
                     std::to_string(ackedBytes) + "/" +
                     std::to_string(m_sendTotalBytes) +
                     " (" + std::to_string((ackedBytes * 100) / m_sendTotalBytes) + "%)");
        }

        // 【关键修复4】检查是否为握手帧的ACK
        uint16_t handshakeSeq = m_handshakeSequence.load();
        if (sequence == handshakeSeq && !m_handshakeCompleted.load())
        {
            WriteLog("ProcessAckFrame: received ACK for handshake START frame (sequence=" + std::to_string(sequence) + ")");

            // 设置握手完成标志
            m_handshakeCompleted.store(true);

            // 通知等待握手完成的线程
            {
                std::lock_guard<std::mutex> handshakeLock(m_handshakeMutex);
                m_handshakeCondition.notify_all();
            }

            WriteLog("ProcessAckFrame: handshake completed, sessionId=" + std::to_string(m_sessionId.load()));
        }

        // 推进发送窗口
        WriteVerbose("ProcessAckFrame: calling AdvanceSendWindow");
        AdvanceSendWindow();
        WriteVerbose("ProcessAckFrame: AdvanceSendWindow completed");
    }
    else
    {
        WriteVerbose("ProcessAckFrame: ACK not processed - inUse=" + std::to_string(m_sendWindow[index].inUse) +
                 ", hasPacket=" + std::to_string(m_sendWindow[index].packet != nullptr) +
                 ", sequenceMatch=" + std::to_string(m_sendWindow[index].packet && m_sendWindow[index].packet->sequence == sequence) +
                 ", alreadyAcked=" + std::to_string(m_sendWindow[index].packet && m_sendWindow[index].packet->acknowledged));
        if (m_sendWindow[index].packet)
        {
            WriteVerbose("ProcessAckFrame: slot sequence=" + std::to_string(m_sendWindow[index].packet->sequence) +
                     ", expected=" + std::to_string(sequence));
        }
    }

    WriteVerbose("ProcessAckFrame: completed");
}

// 处理NAK帧
void ReliableChannel::ProcessNakFrame(uint16_t sequence)
{
    WriteLog("ProcessNakFrame called: sequence=" + std::to_string(sequence) +
             ", sendBase=" + std::to_string(m_sendBase) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    std::lock_guard<std::mutex> lock(m_windowMutex);
    WriteLog("ProcessNakFrame: locked window mutex");

    // 确保窗口大小有效
    if (m_config.windowSize == 0 || m_sendWindow.empty())
    {
        WriteLog("ProcessNakFrame: ERROR - send window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return;
    }

    // 重传对应的包
    uint16_t index = sequence % m_config.windowSize;
    WriteLog("ProcessNakFrame: calculated index=" + std::to_string(index) +
             ", sendWindow.size()=" + std::to_string(m_sendWindow.size()));

    // 边界检查
    if (index >= m_sendWindow.size())
    {
        WriteLog("ProcessNakFrame: ERROR - send window index out of bounds: " + std::to_string(index) +
                 " >= " + std::to_string(m_sendWindow.size()));
        ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
        return;
    }

    WriteLog("ProcessNakFrame: checking slot " + std::to_string(index) +
             ", inUse=" + std::to_string(m_sendWindow[index].inUse));

    if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
        m_sendWindow[index].packet->sequence == sequence)
    {
        WriteLog("ProcessNakFrame: retransmitting packet " + std::to_string(sequence));
        m_retransmitting = true; // 设置重传标志
        RetransmitPacketInternal(sequence); // 使用内部版本，已持有锁
        m_retransmitting = false; // 清除重传标志
        WriteLog("ProcessNakFrame: retransmission completed");
    }
    else
    {
        WriteLog("ProcessNakFrame: NAK not processed - inUse=" + std::to_string(m_sendWindow[index].inUse) +
                 ", hasPacket=" + std::to_string(m_sendWindow[index].packet != nullptr) +
                 ", sequenceMatch=" + std::to_string(m_sendWindow[index].packet && m_sendWindow[index].packet->sequence == sequence));
    }

    WriteLog("ProcessNakFrame: completed");
}

// 处理开始帧
void ReliableChannel::ProcessStartFrame(const Frame &frame)
{
    WriteLog("ProcessStartFrame called: sequence=" + std::to_string(frame.sequence) +
             ", payload.size()=" + std::to_string(frame.payload.size()));

    StartMetadata metadata;
    if (m_frameCodec->DecodeStartMetadata(frame.payload, metadata))
    {
        WriteLog("ProcessStartFrame: metadata decoded successfully - fileName=" + metadata.fileName +
                 ", fileSize=" + std::to_string(metadata.fileSize) +
                 ", version=" + std::to_string(metadata.version));

        // 设置文件传输信息
        {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            m_currentFileName = metadata.fileName;
            m_currentFileSize = metadata.fileSize;
            m_currentFileProgress = 0;
            m_fileTransferActive = true; // 激活文件传输状态
            m_transferStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间
            m_completedFileBuffer.clear();
            m_hasCompletedFile = false;
        }

        // 更新进度显示
        UpdateProgress(0, metadata.fileSize);

        // 【关键修复】发送 ACK 响应，建立握手闭环
        WriteLog("ProcessStartFrame: sending ACK response to establish handshake");
        if (SendAck(frame.sequence))
        {
            WriteLog("ProcessStartFrame: ACK sent successfully, handshake established");

            // 【新增】同步接收窗口基准到START帧序列号 - 解决NAK风暴问题
            {
                std::lock_guard<std::mutex> lock(m_windowMutex);
                WriteLog("ProcessStartFrame: synchronizing receive window base from " + 
                         std::to_string(m_receiveBase) + " to " + std::to_string(frame.sequence + 1));
                
                // 将接收基准设置为START帧的下一个序列号，确保与发送方一致
                uint16_t newBase = (frame.sequence + 1) % 65536;
                m_receiveBase = newBase;
                m_receiveNext = newBase;
                
                WriteLog("ProcessStartFrame: receive window synchronized, new base=" + 
                         std::to_string(m_receiveBase) + ", new next=" + std::to_string(m_receiveNext));
            }

            // 推动接收端状态机 - 准备接收数据
            WriteLog("ProcessStartFrame: receiver ready for data transmission");
        }
        else
        {
            WriteLog("ProcessStartFrame: ERROR - failed to send ACK response");
            ReportError("START帧ACK响应发送失败");
        }
    }
    else
    {
        WriteLog("ProcessStartFrame: ERROR - failed to decode START metadata");
        ReportError("START帧元数据解析失败");

        // 发送 NAK 表示协商失败
        SendNak(frame.sequence);
    }
}

// 处理结束帧
void ReliableChannel::ProcessEndFrame(const Frame &frame)
{
    WriteLog("ProcessEndFrame: 收到END帧，验证传输完整性...");

    // 检查是否有预期的文件大小信息
    if (m_currentFileSize > 0)
    {
        // 【修复】计算字节差异
        int64_t byteDifference = static_cast<int64_t>(m_currentFileSize) - static_cast<int64_t>(m_currentFileProgress);

        // 【修复】±1KB 容错范围
        const int64_t TOLERANCE_BYTES = 1024; // 1KB 容错
        bool withinTolerance = (byteDifference >= -TOLERANCE_BYTES && byteDifference <= TOLERANCE_BYTES);

        WriteLog("ProcessEndFrame: 字节差异 = " + std::to_string(byteDifference) +
                 " 字节，容错范围 ±" + std::to_string(TOLERANCE_BYTES) + " 字节");

        // 验证实际接收字节数与预期文件大小是否匹配（带容错）
        if (withinTolerance)
        {
            WriteLog("ProcessEndFrame: 传输完整性验证通过（容错范围内） - " +
                     std::to_string(m_currentFileProgress) + "/" +
                     std::to_string(m_currentFileSize) + " 字节");

            // 清除传输活跃状态
            m_fileTransferActive = false;

            // 更新进度到100%
            UpdateProgress(m_currentFileSize, m_currentFileSize);
            m_hasCompletedFile = true;

            WriteLog("ProcessEndFrame: 文件传输正常完成");

            // 【新增】触发传输完成回调
            if (m_completeCallback)
            {
                WriteLog("ProcessEndFrame: 调用完成回调，成功=true");
                m_completeCallback(true);
            }
        }
        else if (byteDifference > TOLERANCE_BYTES)
        {
            // 数据不足，启动短超时机制
            if (!m_shortTimeoutActive)
            {
                WriteLog("ProcessEndFrame: 传输不完整（差 " + std::to_string(byteDifference) +
                        " 字节），启动30秒短超时等待更多数据");
                m_shortTimeoutActive = true;
                m_shortTimeoutStart = std::chrono::steady_clock::now();
            }
            else
            {
                // 检查短超时是否到期
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_shortTimeoutStart);

                if (elapsed.count() >= m_shortTimeoutDuration)
                {
                    WriteLog("ProcessEndFrame: 短超时到期（" + std::to_string(elapsed.count()) +
                            "秒），强制结束传输，数据差异 " + std::to_string(byteDifference) + " 字节");

                    // 强制结束传输
                    m_fileTransferActive = false;
                    m_shortTimeoutActive = false;
                    m_hasCompletedFile = true;  // 标记为有数据（尽管不完整）

                    ReportError("文件传输不完整（短超时），预期 " + std::to_string(m_currentFileSize) +
                               " 字节，实际接收 " + std::to_string(m_currentFileProgress) + " 字节");

                    // 【新增】触发传输完成回调（失败）
                    if (m_completeCallback)
                    {
                        WriteLog("ProcessEndFrame: 调用完成回调，成功=false（短超时）");
                        m_completeCallback(false);
                    }
                }
                else
                {
                    WriteLog("ProcessEndFrame: 短超时进行中（" + std::to_string(elapsed.count()) +
                            "/" + std::to_string(m_shortTimeoutDuration) + "秒），继续等待数据");
                }
            }
        }
        else
        {
            // 数据超过预期（负差异超过容错）
            WriteLog("ProcessEndFrame: 接收数据超过预期（多 " + std::to_string(-byteDifference) +
                    " 字节），仍认为传输完成");

            // 清除传输活跃状态
            m_fileTransferActive = false;
            m_shortTimeoutActive = false;

            // 更新进度
            UpdateProgress(m_currentFileProgress, m_currentFileProgress);
            m_hasCompletedFile = true;

            WriteLog("ProcessEndFrame: 文件传输完成（数据超额）");

            // 【新增】触发传输完成回调
            if (m_completeCallback)
            {
                WriteLog("ProcessEndFrame: 调用完成回调，成功=true（数据超额）");
                m_completeCallback(true);
            }
        }
    }
    else
    {
        WriteLog("ProcessEndFrame: 无文件大小信息，使用传统逻辑结束传输");
        m_fileTransferActive = false;
        m_shortTimeoutActive = false;
        UpdateProgress(m_currentFileProgress, m_currentFileProgress);
        {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            m_hasCompletedFile = !m_completedFileBuffer.empty();
        }
        WriteLog("ProcessEndFrame: 传统模式传输结束");

        // 【新增】触发传输完成回调
        if (m_completeCallback)
        {
            WriteLog("ProcessEndFrame: 调用完成回调，成功=true（传统模式）");
            m_completeCallback(true);
        }
    }

    WriteLog("ProcessEndFrame: 处理完成，当前传输状态 = " + std::string(m_fileTransferActive ? "活跃" : "已结束"));
}

// 处理心跳帧
void ReliableChannel::ProcessHeartbeatFrame(const Frame &frame)
{
    // 心跳帧不需要特殊处理，只需要更新活动时间
}

// 发送数据包
bool ReliableChannel::SendPacket(uint16_t sequence, const std::vector<uint8_t> &data, FrameType type)
{
    WriteVerbose("SendPacket called: sequence=" + std::to_string(sequence) +
             ", data.size()=" + std::to_string(data.size()) +
             ", type=" + std::to_string(static_cast<int>(type)));

    // 压缩数据（如果启用）
    std::vector<uint8_t> payload = data;
    if (m_config.enableCompression)
    {
        WriteVerbose("SendPacket: compressing data...");
        payload = CompressData(data);
        WriteVerbose("SendPacket: compression done, payload.size()=" + std::to_string(payload.size()));
    }

    // 加密数据（如果启用）
    if (m_config.enableEncryption)
    {
        WriteVerbose("SendPacket: encrypting data...");
        payload = EncryptData(payload);
        WriteVerbose("SendPacket: encryption done, payload.size()=" + std::to_string(payload.size()));
    }

    // 编码帧
    WriteVerbose("SendPacket: encoding frame...");
    std::vector<uint8_t> frameData;
    switch (type)
    {
    case FrameType::FRAME_DATA:
        frameData = m_frameCodec->EncodeDataFrame(sequence, payload);
        break;
    case FrameType::FRAME_START:
        frameData = m_frameCodec->EncodeFrame(type, sequence, payload);
        break;
    case FrameType::FRAME_END:
        frameData = m_frameCodec->EncodeEndFrame(sequence);
        break;
    case FrameType::FRAME_HEARTBEAT:
        frameData = m_frameCodec->EncodeHeartbeatFrame(sequence);
        break;
    default:
        WriteLog("SendPacket: ERROR - unknown frame type " + std::to_string(static_cast<int>(type)));
        return false;
    }

    WriteVerbose("SendPacket: frame encoded, frameData.size()=" + std::to_string(frameData.size()));

    // 发送数据
    WriteVerbose("SendPacket: writing to transport...");
    size_t written = 0;
    if (m_transport->Write(frameData.data(), frameData.size(), &written) != TransportError::Success || written != frameData.size())
    {
        WriteLog("SendPacket: ERROR - transport write failed or incomplete");
        return false;
    }

    WriteVerbose("SendPacket: transport write succeeded, " + std::to_string(written) + " bytes written");

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsSent++;
        m_stats.bytesSent += frameData.size();
        WriteVerbose("SendPacket: stats updated - packetsSent=" + std::to_string(m_stats.packetsSent) +
                 ", bytesSent=" + std::to_string(m_stats.bytesSent));
    }

    // 对于数据帧，需要保存到发送窗口
    if (type == FrameType::FRAME_DATA)
    {
        WriteVerbose("SendPacket: saving to send window...");
        std::lock_guard<std::mutex> lock(m_windowMutex);

        // 确保窗口大小有效
        if (m_config.windowSize == 0 || m_sendWindow.empty())
        {
            WriteLog("SendPacket: ERROR - window not initialized or size is 0");
            ReportError("发送窗口未初始化或大小为0");
            return false;
        }

        uint16_t index = sequence % m_config.windowSize;
        WriteVerbose("SendPacket: calculated index=" + std::to_string(index) +
                 ", windowSize=" + std::to_string(m_config.windowSize));

        // 边界检查
        if (index >= m_sendWindow.size())
        {
            WriteLog("SendPacket: ERROR - index out of bounds: " + std::to_string(index) +
                     " >= " + std::to_string(m_sendWindow.size()));
            ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
            return false;
        }

        WriteVerbose("SendPacket: setting window slot " + std::to_string(index) + " to inUse=true");
        m_sendWindow[index].inUse = true;

        if (!m_sendWindow[index].packet)
        {
            WriteVerbose("SendPacket: creating new packet for slot " + std::to_string(index));
            m_sendWindow[index].packet = std::make_shared<Packet>();
        }

        m_sendWindow[index].packet->sequence = sequence;
        m_sendWindow[index].packet->data = data; // 保存原始数据
        m_sendWindow[index].packet->timestamp = std::chrono::steady_clock::now();
        m_sendWindow[index].packet->retryCount = 0;
        m_sendWindow[index].packet->acknowledged = false;

        WriteVerbose("SendPacket: window slot " + std::to_string(index) + " updated successfully");
    }

    WriteVerbose("SendPacket: completed successfully");
    return true;
}

// 发送ACK
bool ReliableChannel::SendAck(uint16_t sequence)
{
    WriteVerbose("SendAck called: sequence=" + std::to_string(sequence));

    std::vector<uint8_t> frameData = m_frameCodec->EncodeAckFrame(sequence);
    WriteVerbose("SendAck: frame encoded, size=" + std::to_string(frameData.size()));

    size_t written = 0;
    TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);
    bool success = (error == TransportError::Success && written == frameData.size());

    WriteVerbose("SendAck: transport write result: error=" + std::to_string(static_cast<int>(error)) +
             ", written=" + std::to_string(written) + ", success=" + std::to_string(success));

    return success;
}

// 发送NAK
bool ReliableChannel::SendNak(uint16_t sequence)
{
    WriteVerbose("SendNak called: sequence=" + std::to_string(sequence));

    std::vector<uint8_t> frameData = m_frameCodec->EncodeNakFrame(sequence);
    WriteVerbose("SendNak: frame encoded, size=" + std::to_string(frameData.size()));

    size_t written = 0;
    TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);
    bool success = (error == TransportError::Success && written == frameData.size());

    WriteVerbose("SendNak: transport write result: error=" + std::to_string(static_cast<int>(error)) +
             ", written=" + std::to_string(written) + ", success=" + std::to_string(success));

    return success;
}

// 发送心跳
bool ReliableChannel::SendHeartbeat()
{
    // 使用独立的心跳序列号，不占用数据传输序列号
    uint16_t sequence = m_heartbeatSequence++;
    WriteVerbose("SendHeartbeat: using independent heartbeat sequence " + std::to_string(sequence));

    std::vector<uint8_t> frameData = m_frameCodec->EncodeHeartbeatFrame(sequence);
    size_t written = 0;
    return m_transport->Write(frameData.data(), frameData.size(), &written) == TransportError::Success && written == frameData.size();
}

// 发送开始帧
bool ReliableChannel::SendStart(const std::string &fileName, uint64_t fileSize, uint64_t modifyTime)
{
    WriteLog("SendStart called: fileName=" + fileName +
             ", fileSize=" + std::to_string(fileSize) +
             ", modifyTime=" + std::to_string(modifyTime));

    // 【关键修复1】动态生成会话ID，不再写死为0
    uint16_t sessionId = GenerateSessionId();
    WriteLog("SendStart: generated sessionId=" + std::to_string(sessionId));

    // 【关键修复2】重置握手状态，准备新的握手过程
    m_handshakeCompleted.store(false);
    WriteLog("SendStart: handshake state reset");

    StartMetadata metadata;
    metadata.version = m_config.version;
    metadata.flags = 0;
    metadata.fileName = fileName;
    metadata.fileSize = fileSize;
    metadata.modifyTime = modifyTime;
    metadata.sessionId = sessionId;

    // 分配序列号用于START控制帧
    uint16_t sequence = AllocateSequence();
    m_handshakeSequence.store(sequence); // 保存握手序列号
    WriteLog("SendStart: allocated sequence=" + std::to_string(sequence) + " for START frame");

    // 编码开始帧
    WriteLog("SendStart: encoding START frame...");
    std::vector<uint8_t> frameData = m_frameCodec->EncodeStartFrame(sequence, metadata);
    WriteLog("SendStart: frame encoded, size=" + std::to_string(frameData.size()));

    // 【关键修复3】将START帧也存储到发送窗口中以支持ACK匹配
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        uint16_t index = sequence % m_config.windowSize;

        if (index < m_sendWindow.size())
        {
            // 创建START控制帧的数据包
            auto packet = std::make_shared<Packet>(sequence, frameData);
            packet->acknowledged = false;

            m_sendWindow[index].packet = packet;
            m_sendWindow[index].inUse = true;

            WriteLog("SendStart: START frame stored in send window at index=" + std::to_string(index));
        }
        else
        {
            WriteLog("SendStart: ERROR - window index out of bounds: " + std::to_string(index));
            ReportError("发送窗口索引越界");
            return false;
        }
    }

    // 发送帧数据
    WriteLog("SendStart: sending START frame to transport...");
    size_t written = 0;
    TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);
    bool success = (error == TransportError::Success && written == frameData.size());

    if (success)
    {
        WriteLog("SendStart: START frame sent successfully, " + std::to_string(written) + " bytes written");

        // 更新统计
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.packetsSent++;
            m_stats.bytesSent += frameData.size();
        }

        WriteLog("SendStart: START control frame stored in send window for ACK tracking");
    }
    else
    {
        WriteLog("SendStart: ERROR - failed to send START frame, error=" +
                 std::to_string(static_cast<int>(error)) + ", written=" + std::to_string(written));

        // 发送失败时清理发送窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            uint16_t index = sequence % m_config.windowSize;
            if (index < m_sendWindow.size())
            {
                m_sendWindow[index].inUse = false;
                m_sendWindow[index].packet.reset();
                m_windowCondition.notify_all();
            }
        }

        ReportError("START帧发送失败");
    }

    return success;
}

// 发送结束帧
bool ReliableChannel::SendEnd()
{
    uint16_t sequence = AllocateSequence();
    return SendPacket(sequence, {}, FrameType::FRAME_END);
}

// 重传数据包（内部版本，假设已持有窗口锁）
void ReliableChannel::RetransmitPacketInternal(uint16_t sequence)
{
    WriteLog("RetransmitPacketInternal called: sequence=" + std::to_string(sequence));

    // 确保窗口大小有效
    if (m_config.windowSize == 0 || m_sendWindow.empty())
    {
        WriteLog("RetransmitPacketInternal: ERROR - send window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return;
    }

    uint16_t index = sequence % m_config.windowSize;
    WriteLog("RetransmitPacketInternal: calculated index=" + std::to_string(index) +
             ", sendWindow.size()=" + std::to_string(m_sendWindow.size()));

    // 边界检查
    if (index >= m_sendWindow.size())
    {
        WriteLog("RetransmitPacketInternal: ERROR - send window index out of bounds: " + std::to_string(index) +
                 " >= " + std::to_string(m_sendWindow.size()));
        ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
        return;
    }

    WriteLog("RetransmitPacketInternal: checking slot " + std::to_string(index) +
             ", inUse=" + std::to_string(m_sendWindow[index].inUse));

    if (m_sendWindow[index].inUse && m_sendWindow[index].packet)
    {
        WriteLog("RetransmitPacketInternal: incrementing retry count from " + std::to_string(m_sendWindow[index].packet->retryCount));
        m_sendWindow[index].packet->retryCount++;
        m_sendWindow[index].packet->timestamp = std::chrono::steady_clock::now();
        WriteLog("RetransmitPacketInternal: new retry count=" + std::to_string(m_sendWindow[index].packet->retryCount));

        // 重新发送数据包
        WriteLog("RetransmitPacketInternal: encoding data frame for retransmission");
        try
        {
            std::vector<uint8_t> frameData = m_frameCodec->EncodeDataFrame(
                sequence, m_sendWindow[index].packet->data);
            WriteLog("RetransmitPacketInternal: frame encoded, size=" + std::to_string(frameData.size()));

            size_t written = 0;
            WriteLog("RetransmitPacketInternal: writing to transport...");
            TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);
            WriteLog("RetransmitPacketInternal: transport write completed, written=" + std::to_string(written) +
                     ", error=" + std::to_string(static_cast<int>(error)));

            if (error != TransportError::Success)
            {
                WriteLog("RetransmitPacketInternal: ERROR - transport write failed with error: " + std::to_string(static_cast<int>(error)));
                ReportError("重传数据包失败，传输错误: " + std::to_string(static_cast<int>(error)));
                return;
            }

            // 更新统计
            {
                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                m_stats.packetsRetransmitted++;
                WriteLog("RetransmitPacketInternal: stats updated, packetsRetransmitted=" + std::to_string(m_stats.packetsRetransmitted));
            }

            WriteLog("RetransmitPacketInternal: retransmission completed successfully");
        }
        catch (const std::exception &e)
        {
            WriteLog("RetransmitPacketInternal: EXCEPTION during retransmission: " + std::string(e.what()));
            ReportError("重传数据包异常: " + std::string(e.what()));
        }
        catch (...)
        {
            WriteLog("RetransmitPacketInternal: UNKNOWN EXCEPTION during retransmission");
            ReportError("重传数据包未知异常");
        }
    }
    else
    {
        WriteLog("RetransmitPacketInternal: ERROR - slot not in use or no packet: inUse=" + std::to_string(m_sendWindow[index].inUse) +
                 ", hasPacket=" + std::to_string(m_sendWindow[index].packet != nullptr));
    }

    WriteLog("RetransmitPacketInternal: completed");
}

// 重传数据包（外部版本，负责获取锁）
void ReliableChannel::RetransmitPacket(uint16_t sequence)
{
    WriteLog("RetransmitPacket called: sequence=" + std::to_string(sequence));

    std::lock_guard<std::mutex> lock(m_windowMutex);
    WriteLog("RetransmitPacket: locked window mutex");
    
    RetransmitPacketInternal(sequence);
    
    WriteLog("RetransmitPacket: completed");
}

// 推进发送窗口
void ReliableChannel::AdvanceSendWindow()
{
    WriteLog("AdvanceSendWindow called");

    int advanceCount = 0;
    while (true)
    {
        WriteLog("AdvanceSendWindow: checking window, sendBase=" + std::to_string(m_sendBase) +
                 ", windowSize=" + std::to_string(m_config.windowSize));

        // 确保窗口大小有效
        if (m_config.windowSize == 0 || m_sendWindow.empty())
        {
            WriteLog("AdvanceSendWindow: ERROR - window not initialized or size is 0");
            ReportError("发送窗口未初始化或大小为0");
            break;
        }

        uint16_t index = m_sendBase % m_config.windowSize;
        WriteLog("AdvanceSendWindow: calculated index=" + std::to_string(index) +
                 ", sendWindow.size()=" + std::to_string(m_sendWindow.size()));

        // 边界检查
        if (index >= m_sendWindow.size())
        {
            WriteLog("AdvanceSendWindow: ERROR - index out of bounds: " + std::to_string(index) +
                     " >= " + std::to_string(m_sendWindow.size()));
            ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
            break;
        }

        WriteLog("AdvanceSendWindow: checking slot " + std::to_string(index) +
                 ", inUse=" + std::to_string(m_sendWindow[index].inUse));

        if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
            m_sendWindow[index].packet->acknowledged)
        {
            WriteLog("AdvanceSendWindow: advancing window, old sendBase=" + std::to_string(m_sendBase));
            m_sendWindow[index].inUse = false;
            m_sendWindow[index].packet.reset();
            m_sendBase = (m_sendBase + 1) % 65536;
            advanceCount++;
            WriteLog("AdvanceSendWindow: new sendBase=" + std::to_string(m_sendBase) + ", advanceCount=" + std::to_string(advanceCount));
        }
        else
        {
            WriteLog("AdvanceSendWindow: no more slots to advance, breaking");
            break;
        }
    }

    WriteLog("AdvanceSendWindow: completed, advanced " + std::to_string(advanceCount) + " slots");

    if (advanceCount > 0)
    {
        m_windowCondition.notify_all();
    }
}

// 更新接收窗口
void ReliableChannel::UpdateReceiveWindow(uint16_t sequence)
{
    // 这里可以实现更复杂的接收窗口管理逻辑
}

// 分配序列号
bool ReliableChannel::IsSendWindowFullLocked() const
{
    if (m_config.windowSize == 0 || m_sendWindow.empty())
    {
        return false;
    }

    uint16_t outstanding = GetWindowDistance(m_sendBase, m_sendNext);
    if (outstanding < m_config.windowSize)
    {
        return false;
    }

    uint16_t index = m_sendNext % m_config.windowSize;
    if (index >= m_sendWindow.size())
    {
        return true;
    }

    const auto &slot = m_sendWindow[index];
    bool slotBusy = slot.inUse && (!slot.packet || !slot.packet->acknowledged);
    return slotBusy;
}

uint16_t ReliableChannel::AllocateSequence()
{
    WriteLog("AllocateSequence called");

    std::unique_lock<std::mutex> lock(m_windowMutex);

    WriteLog("AllocateSequence: sendWindow.size()=" + std::to_string(m_sendWindow.size()) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    // 确保窗口已初始化
    if (m_sendWindow.empty() || m_config.windowSize == 0)
    {
        WriteLog("AllocateSequence: ERROR - window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return 0;
    }

    while (!m_shutdown.load() && m_connected.load() && IsSendWindowFullLocked())
    {
        WriteLog("AllocateSequence: send window full (sendBase=" + std::to_string(m_sendBase) +
                 ", sendNext=" + std::to_string(m_sendNext) + "), waiting for availability");

        m_windowCondition.wait(lock, [this]() {
            return m_shutdown.load() || !m_connected.load() || !IsSendWindowFullLocked();
        });

        WriteLog("AllocateSequence: wake, current sendBase=" + std::to_string(m_sendBase) +
                 ", sendNext=" + std::to_string(m_sendNext));
    }

    uint16_t sequence = m_sendNext;
    WriteLog("AllocateSequence: returning sequence " + std::to_string(sequence) +
             ", next will be " + std::to_string((m_sendNext + 1) % 65536));
    m_sendNext = (m_sendNext + 1) % 65536;
    return sequence;
}

// 检查序列号是否在窗口内
bool ReliableChannel::IsSequenceInWindow(uint16_t sequence, uint16_t base, uint16_t windowSize) const
{
    bool result;
    if (sequence >= base)
    {
        uint16_t diff = sequence - base;
        result = diff < windowSize;
    }
    else
    {
        uint16_t diff = 65536 - base + sequence;
        result = diff < windowSize;
    }
    return result;
}

// 获取窗口距离
uint16_t ReliableChannel::GetWindowDistance(uint16_t from, uint16_t to) const
{
    uint16_t result;
    if (to >= from)
    {
        result = to - from;
    }
    else
    {
        result = 65536 - from + to;
    }
    return result;
}

// 计算超时时间
uint32_t ReliableChannel::CalculateTimeout() const
{
    uint32_t result = (std::min)(m_timeoutMs, m_config.timeoutMax);
    return result;
}

// 更新RTT
void ReliableChannel::UpdateRTT(uint32_t rttMs)
{
    // 使用指数加权移动平均
    m_rttMs = (m_rttMs * 7 + rttMs) / 8;
    m_timeoutMs = m_rttMs * 2; // 超时时间是RTT的两倍

    // 限制超时时间范围
    m_timeoutMs = (std::max)(m_timeoutMs, m_config.timeoutBase);
    m_timeoutMs = (std::min)(m_timeoutMs, m_config.timeoutMax);
}

// 压缩数据
std::vector<uint8_t> ReliableChannel::CompressData(const std::vector<uint8_t> &data) const
{
    // TODO: 实现数据压缩
    return data;
}

// 解压缩数据
std::vector<uint8_t> ReliableChannel::DecompressData(const std::vector<uint8_t> &data) const
{
    // TODO: 实现数据解压缩
    return data;
}

// 加密数据
std::vector<uint8_t> ReliableChannel::EncryptData(const std::vector<uint8_t> &data) const
{
    // TODO: 实现数据加密
    return data;
}

// 解密数据
std::vector<uint8_t> ReliableChannel::DecryptData(const std::vector<uint8_t> &data) const
{
    // TODO: 实现数据解密
    return data;
}

// 报告错误
void ReliableChannel::ReportError(const std::string &error)
{
    WriteLog("ReportError called: " + error);

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.errors++;
        WriteVerbose("ReportError: error count incremented to " + std::to_string(m_stats.errors));
    }

    if (m_errorCallback)
    {
        WriteVerbose("ReportError: calling error callback");
        m_errorCallback(error);
    }
    else
    {
        WriteVerbose("ReportError: no error callback set");
    }
}

// 更新状态
void ReliableChannel::UpdateState(bool connected)
{
    if (m_stateChangedCallback)
    {
        m_stateChangedCallback(connected);
    }
}

// 更新进度
void ReliableChannel::UpdateProgress(int64_t current, int64_t total)
{
    if (m_progressCallback)
    {
        m_progressCallback(current, total);
    }
}

// 检查传输超时
bool ReliableChannel::CheckTransferTimeout()
{
    if (!m_fileTransferActive) return false;

    auto now = std::chrono::steady_clock::now();
    auto transferDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_transferStartTime);

    // 【新增】日志节流机制：静态变量跟踪上次日志时间，防止日志文件爆炸式增长
    static std::chrono::steady_clock::time_point lastLogTime;
    static const auto LOG_THROTTLE_DURATION = std::chrono::seconds(30);

    // 计算合理的传输超时时间
    // 基本策略：大文件给更多时间，但设置合理上限
    int64_t maxExpectedSeconds;
    if (m_currentFileSize > 0)
    {
        // 按1KB/s的最低速度计算，但至少60秒，最多30分钟
        int64_t minSeconds = 60LL;
        int64_t maxSeconds = 1800LL;
        int64_t calculatedSeconds = m_currentFileSize / 1024;
        maxExpectedSeconds = (calculatedSeconds > minSeconds) ?
                            ((calculatedSeconds < maxSeconds) ? calculatedSeconds : maxSeconds) :
                            minSeconds;
    }
    else
    {
        // 无文件大小信息时，默认5分钟超时
        maxExpectedSeconds = 300;
    }

    if (transferDuration.count() > maxExpectedSeconds)
    {
        // 【修复】日志节流：仅在超过节流间隔时记录详细日志（防止每2秒写一次日志导致日志文件爆炸）
        bool shouldLog = (now - lastLogTime) >= LOG_THROTTLE_DURATION;

        if (shouldLog)
        {
            WriteLog("CheckTransferTimeout: 传输超时，强制结束");
            WriteLog("CheckTransferTimeout: 预期最大时间 " + std::to_string(maxExpectedSeconds) +
                    " 秒，实际用时 " + std::to_string(transferDuration.count()) + " 秒");
            WriteLog("CheckTransferTimeout: 文件大小 " + std::to_string(m_currentFileSize) +
                    " 字节，当前进度 " + std::to_string(m_currentFileProgress) + " 字节");
            lastLogTime = now; // 更新最后日志时间
        }

        m_fileTransferActive = false;

        // 【保留】错误报告始终执行（不受节流限制），确保UI能及时得到通知
        ReportError("文件传输超时：传输时间 " + std::to_string(transferDuration.count()) +
                   " 秒超过预期 " + std::to_string(maxExpectedSeconds) + " 秒");
        return true;
    }

    return false;
}

// 生成会话ID
uint16_t ReliableChannel::GenerateSessionId()
{
    // 使用当前时间戳的低16位作为会话ID，确保每次握手都有不同的ID
    auto now = std::chrono::steady_clock::now();
    auto timestamp = now.time_since_epoch().count();
    uint16_t sessionId = static_cast<uint16_t>(timestamp & 0xFFFF);

    // 确保会话ID不为0（0表示无效会话）
    if (sessionId == 0)
    {
        sessionId = 1;
    }

    m_sessionId.store(sessionId);
    WriteLog("GenerateSessionId: generated sessionId=" + std::to_string(sessionId));
    return sessionId;
}

// 等待握手完成
bool ReliableChannel::WaitForHandshakeCompletion(uint32_t timeoutMs)
{
    WriteLog("WaitForHandshakeCompletion: waiting for handshake, timeout=" + std::to_string(timeoutMs) + "ms");

    std::unique_lock<std::mutex> lock(m_handshakeMutex);

    // 使用条件变量等待握手完成，带超时
    bool completed = m_handshakeCondition.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this]() { return m_handshakeCompleted.load(); }
    );

    if (completed)
    {
        WriteLog("WaitForHandshakeCompletion: handshake completed successfully");
    }
    else
    {
        WriteLog("WaitForHandshakeCompletion: handshake timeout after " + std::to_string(timeoutMs) + "ms");
    }

    return completed;
}

// 【P0修复】确保会话已启动，如果未启动则自动执行握手
bool ReliableChannel::EnsureSessionStarted()
{
    WriteLog("EnsureSessionStarted: checking session status");

    // 检查是否已经连接
    if (!IsConnected())
    {
        WriteLog("EnsureSessionStarted: ERROR - not connected");
        return false;
    }

    // 检查握手是否已完成
    if (m_handshakeCompleted.load())
    {
        WriteLog("EnsureSessionStarted: handshake already completed, sessionId=" + std::to_string(m_sessionId.load()));
        return true;
    }

    WriteLog("EnsureSessionStarted: handshake not completed, initiating new handshake");

    // 生成会话ID
    uint16_t sessionId = GenerateSessionId();
    WriteLog("EnsureSessionStarted: generated sessionId=" + std::to_string(sessionId));

    // 重置握手状态
    m_handshakeCompleted.store(false);
    WriteLog("EnsureSessionStarted: handshake state reset");

    // 构造START帧的元数据（用于握手，不包含文件信息）
    StartMetadata metadata;
    metadata.version = m_config.version;
    metadata.flags = 0;
    metadata.fileName = ""; // 空文件名表示纯握手
    metadata.fileSize = 0;
    metadata.modifyTime = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
    metadata.sessionId = sessionId;

    // 分配序列号用于START控制帧
    uint16_t sequence = AllocateSequence();
    m_handshakeSequence.store(sequence);
    WriteLog("EnsureSessionStarted: allocated sequence=" + std::to_string(sequence) + " for handshake START frame");

    // 编码开始帧
    WriteLog("EnsureSessionStarted: encoding handshake START frame");
    std::vector<uint8_t> frameData = m_frameCodec->EncodeStartFrame(sequence, metadata);
    WriteLog("EnsureSessionStarted: frame encoded, size=" + std::to_string(frameData.size()));

    // 将START帧存储到发送窗口中以支持ACK匹配
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        uint16_t index = sequence % m_config.windowSize;

        if (index < m_sendWindow.size())
        {
            auto packet = std::make_shared<Packet>(sequence, frameData);
            packet->acknowledged = false;

            m_sendWindow[index].packet = packet;
            m_sendWindow[index].inUse = true;

            WriteLog("EnsureSessionStarted: handshake START frame stored in send window at index=" + std::to_string(index));
        }
        else
        {
            WriteLog("EnsureSessionStarted: ERROR - window index out of bounds: " + std::to_string(index));
            ReportError("发送窗口索引越界");
            return false;
        }
    }

    // 发送握手帧
    WriteLog("EnsureSessionStarted: sending handshake START frame to transport");
    size_t written = 0;
    TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);
    bool success = (error == TransportError::Success && written == frameData.size());

    if (!success)
    {
        WriteLog("EnsureSessionStarted: ERROR - failed to send handshake START frame, error=" +
                 std::to_string(static_cast<int>(error)) + ", written=" + std::to_string(written));

        // 发送失败时清理发送窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            uint16_t index = sequence % m_config.windowSize;
            if (index < m_sendWindow.size())
            {
                m_sendWindow[index].inUse = false;
                m_sendWindow[index].packet.reset();
                m_windowCondition.notify_all();
            }
        }
        return false;
    }

    WriteLog("EnsureSessionStarted: handshake START frame sent successfully, " + std::to_string(written) + " bytes written");

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsSent++;
        m_stats.bytesSent += frameData.size();
    }

    // 等待握手完成，使用配置的超时时间
    uint32_t timeoutMs = m_config.timeoutMax; // 使用最大超时时间
    WriteLog("EnsureSessionStarted: waiting for handshake completion, timeout=" + std::to_string(timeoutMs) + "ms");

    bool handshakeSuccess = WaitForHandshakeCompletion(timeoutMs);

    if (handshakeSuccess)
    {
        WriteLog("EnsureSessionStarted: handshake completed successfully, sessionId=" + std::to_string(m_sessionId.load()));
    }
    else
    {
        WriteLog("EnsureSessionStarted: ERROR - handshake timeout or failed");

        // 握手失败时清理发送窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            uint16_t index = sequence % m_config.windowSize;
            if (index < m_sendWindow.size())
            {
                m_sendWindow[index].inUse = false;
                m_sendWindow[index].packet.reset();
                m_windowCondition.notify_all();
            }
        }
    }

    return handshakeSuccess;
}
