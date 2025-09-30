#include "pch.h"
#include "ReliableChannel.h"
#include "../Common/CommonTypes.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

// 日志写入函数
void ReliableChannel::WriteLog(const std::string &message)
{
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
    : m_initialized(false), m_connected(false), m_shutdown(false), m_retransmitting(false), m_sendBase(0), m_sendNext(0), m_receiveBase(0), m_receiveNext(0), m_heartbeatSequence(0), m_sendFileSize(0), m_sendFileProgress(0), m_sendFileActive(false), m_recvFileSize(0), m_recvFileProgress(0), m_recvFileActive(false), m_handshakeCompleted(false), m_handshakeSequence(0), m_sessionId(0), m_rttMs(100), m_timeoutMs(500), m_frameCodec(std::make_unique<FrameCodec>())
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

    std::lock_guard<std::mutex> lock(m_sendMutex);
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

    // 设置发送端文件传输状态
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_sendFileName = filePath;
        m_sendFileSize = fileSize;
        m_sendFileProgress = 0;
        m_sendFileActive = true;
        m_sendStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间
    }

    // 发送开始帧
    auto modifyTime = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    WriteLog("SendFile: sending START frame for handshake");
    if (!SendStart(filePath, fileSize, modifyTime))
    {
        file.close();
        m_sendFileActive = false;
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
        m_sendFileActive = false;
        return false;
    }

    if (!m_connected)
    {
        WriteLog("SendFile: ERROR - connection lost during handshake");
        file.close();
        m_sendFileActive = false;
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

            // 【关键修复】在重试循环外分配序列号，确保重试时使用同一序列号
            uint16_t sequence = AllocateSequence();

            // 【流控机制】实现Busy状态重试逻辑
            const int MAX_RETRY_COUNT = 10;        // 最大重试次数
            const int RETRY_DELAY_MS = 50;         // 每次重试间隔（毫秒）

            int retryCount = 0;
            TransportError sendError = TransportError::Success;

            while (retryCount < MAX_RETRY_COUNT)
            {
                sendError = SendPacket(sequence, buffer);  // 使用同一序列号重试

                if (sendError == TransportError::Success)
                {
                    break; // 成功，退出重试循环
                }
                else if (sendError == TransportError::Busy)
                {
                    // 传输层繁忙，等待后重试
                    WriteLog("SendFile: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT) + " for sequence " + std::to_string(sequence));
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    retryCount++;
                }
                else
                {
                    // 其他错误（如Failed、NotOpen等），立即失败
                    WriteLog("SendFile: SendPacket failed with error=" + std::to_string(static_cast<int>(sendError)) + " for sequence " + std::to_string(sequence));
                    break;
                }
            }

            // 检查最终结果
            if (sendError != TransportError::Success)
            {
                if (sendError == TransportError::Busy && retryCount >= MAX_RETRY_COUNT)
                {
                    ReportError("传输层持续繁忙，重试次数已耗尽");
                }
                else
                {
                    ReportError("发送文件数据失败");
                }
                file.close();
                m_sendFileActive = false;
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

    // 【修复】直接发送END帧，不等待ACK
    // 理由：等待ACK的判断条件有缺陷（slot.inUse在ACK后被设为false）
    // ProcessThread会继续处理重传，END帧本身也需要ACK确认
    WriteLog("SendFile: all data packets sent, sending END frame");

    // 发送结束帧
    if (m_connected && !SendEnd())
    {
        ReportError("发送文件结束帧失败");
        m_sendFileActive = false;
        return false;
    }

    m_sendFileActive = false;
    return m_connected;
}

// 接收文件
bool ReliableChannel::ReceiveFile(const std::string &filePath, std::function<void(int64_t, int64_t)> progressCallback)
{
    if (!IsConnected())
    {
        return false;
    }

    // 设置接收端文件传输状态
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_recvFileName = filePath;
        // 不覆盖m_recvFileSize，它将由ProcessStartFrame设置
        // 如果之前没有设置过，初始化为0
        if (m_recvFileSize == 0)
        {
            // 首次调用，等待START帧设置文件大小
        }
        m_recvFileProgress = 0;
        m_recvFileActive = true; // 设置为true，表示准备接收
        m_recvStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        ReportError("无法创建文件: " + filePath);
        m_recvFileActive = false;
        return false;
    }

    WriteLog("ReceiveFile: waiting for file data...");

    // 【修复BUG】从接收队列读取数据并写入文件
    int64_t bytesWritten = 0;

    while (m_connected)
    {
        // 从接收队列获取数据
        std::vector<uint8_t> data;
        bool transferActive = false;
        bool queueEmpty = false;

        {
            std::unique_lock<std::mutex> lock(m_receiveMutex);

            // 等待数据可用或传输结束
            m_receiveCondition.wait_for(
                lock,
                std::chrono::milliseconds(100),
                [this] { return !m_receiveQueue.empty() || !m_recvFileActive || !m_connected; }
            );

            if (!m_connected)
            {
                WriteLog("ReceiveFile: connection lost");
                break;
            }

            transferActive = m_recvFileActive;
            queueEmpty = m_receiveQueue.empty();

            // 【关键修复】即使传输标记为结束，也要继续排空接收队列
            if (!m_receiveQueue.empty())
            {
                data = m_receiveQueue.front();
                m_receiveQueue.pop();
                WriteLog("ReceiveFile: dequeued data chunk, size=" + std::to_string(data.size()) +
                         ", queue remaining=" + std::to_string(m_receiveQueue.size()));
            }
        }

        // 写入数据到文件
        if (!data.empty())
        {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.flush(); // 立即刷新到磁盘
            bytesWritten += data.size();

            WriteLog("ReceiveFile: wrote " + std::to_string(data.size()) + " bytes to file, total=" + std::to_string(bytesWritten));

            // 调用进度回调
            if (progressCallback)
            {
                std::lock_guard<std::mutex> lock(m_receiveMutex);
                progressCallback(m_recvFileProgress, m_recvFileSize);
            }
        }
        else if (!transferActive && queueEmpty)
        {
            // 传输已结束且队列为空，可以退出
            WriteLog("ReceiveFile: transfer completed and queue empty, exiting");
            break;
        }
    }

    file.close();
    WriteLog("ReceiveFile: file closed, total bytes written=" + std::to_string(bytesWritten));

    return !m_recvFileActive && m_connected;
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
    // 检查发送端或接收端是否有活跃传输
    if (!m_sendFileActive && !m_recvFileActive) return false;

    // 检查传输超时（需要非const访问来修改状态）
    if (const_cast<ReliableChannel*>(this)->CheckTransferTimeout()) {
        return false;
    }

    // 返回当前文件传输的活跃状态，用于UI层判断保存按钮启用时机
    return m_sendFileActive || m_recvFileActive;
}

// 处理线程
void ReliableChannel::ProcessThread()
{
    WriteLog("ProcessThread started");

    std::vector<uint8_t> buffer(4096);

    while (!m_shutdown)
    {
        WriteLog("ProcessThread: checking connection status...");

        if (!m_connected)
        {
            WriteLog("ProcessThread: not connected, sleeping...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        WriteLog("ProcessThread: reading from transport...");

        // 接收数据
        size_t bytesReceived = 0;
        TransportError error = m_transport->Read(buffer.data(), buffer.size(), &bytesReceived, 100);
        WriteLog("ProcessThread: transport read result: error=" + std::to_string(static_cast<int>(error)) +
                 ", bytesReceived=" + std::to_string(bytesReceived));

        if (error == TransportError::Success && bytesReceived > 0)
        {
            WriteLog("ProcessThread: received " + std::to_string(bytesReceived) + " bytes, processing...");

            // 添加到帧编解码器
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesReceived);
            WriteLog("ProcessThread: appending data to frame codec...");
            m_frameCodec->AppendData(data);

            // 处理可用的帧
            Frame frame;
            int frameCount = 0;
            while (m_frameCodec->TryGetFrame(frame))
            {
                frameCount++;
                WriteLog("ProcessThread: processing frame " + std::to_string(frameCount) +
                         ", type=" + std::to_string(static_cast<int>(frame.type)) +
                         ", sequence=" + std::to_string(frame.sequence));
                ProcessIncomingFrame(frame);
            }

            WriteLog("ProcessThread: processed " + std::to_string(frameCount) + " frames");

            // 更新统计
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.bytesReceived += bytesReceived;
                WriteLog("ProcessThread: updated stats, total bytesReceived=" + std::to_string(m_stats.bytesReceived));
            }
        }
        else if (error != TransportError::Success)
        {
            WriteLog("ProcessThread: transport read error: " + std::to_string(static_cast<int>(error)));
        }

        WriteLog("ProcessThread: checking for retransmissions...");

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

                    WriteLog("ProcessThread: checking slot sequence=" + std::to_string(slot.packet->sequence) +
                             ", elapsed=" + std::to_string(elapsed) + "ms, retryCount=" + std::to_string(slot.packet->retryCount));

                    if (elapsed > CalculateTimeout())
                    {
                        if (slot.packet->retryCount < m_config.maxRetries)
                        {
                            WriteLog("ProcessThread: retransmitting packet sequence=" + std::to_string(slot.packet->sequence) +
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
                            WriteLog("ProcessThread: packet sequence=" + std::to_string(failedSequence) +
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

                            // 推进发送窗口（如果这个包是窗口基）
                            if (shouldAdvanceWindow)
                            {
                                WriteLog("ProcessThread: advancing send window due to failed packet at base sequence " + std::to_string(failedSequence));
                                AdvanceSendWindow();
                            }
                        }
                    }
                }
            }

            WriteLog("ProcessThread: checked retransmissions, count=" + std::to_string(retransmitCount));
        }
    }

    WriteLog("ProcessThread exiting");
}

// 发送线程
void ReliableChannel::SendThread()
{
    WriteLog("SendThread started");

    while (!m_shutdown)
    {
        WriteLog("SendThread: waiting for data or shutdown...");

        std::vector<uint8_t> data;

        // 获取要发送的数据（原子操作）
        {
            std::unique_lock<std::mutex> lock(m_sendMutex);

            // 等待发送数据
            WriteLog("SendThread: waiting on condition variable...");
            m_sendCondition.wait(lock, [this]
                                 { return !m_sendQueue.empty() || !m_connected || m_shutdown; });

            WriteLog("SendThread: condition variable triggered");

            if (m_shutdown)
            {
                WriteLog("SendThread: shutdown detected, exiting");
                break;
            }

            if (!m_connected)
            {
                WriteLog("SendThread: not connected, continuing");
                continue;
            }

            if (m_sendQueue.empty())
            {
                WriteLog("SendThread: queue is empty, continuing");
                continue;
            }

            // 获取数据并从队列移除
            WriteLog("SendThread: getting data from queue, queue size: " + std::to_string(m_sendQueue.size()));
            data = m_sendQueue.front();
            m_sendQueue.pop();
            WriteLog("SendThread: data extracted, size: " + std::to_string(data.size()) + " bytes");
        }

        // 在没有锁的情况下处理发送
        try
        {
            WriteLog("SendThread: allocating sequence number...");
            // 【关键修复】在重试循环外分配序列号，确保重试时使用同一序列号
            uint16_t sequence = AllocateSequence();
            WriteLog("SendThread: allocated sequence " + std::to_string(sequence) + ", sending packet...");

            // 【流控机制】实现Busy状态重试逻辑
            const int MAX_RETRY_COUNT = 5;   // 队列发送的重试次数较少
            const int RETRY_DELAY_MS = 20;   // 重试间隔较短

            int retryCount = 0;
            TransportError sendError = TransportError::Success;

            while (retryCount < MAX_RETRY_COUNT)
            {
                sendError = SendPacket(sequence, data);  // 使用同一序列号重试

                if (sendError == TransportError::Success)
                {
                    WriteLog("SendThread: SendPacket succeeded for sequence " + std::to_string(sequence));
                    break;
                }
                else if (sendError == TransportError::Busy)
                {
                    WriteLog("SendThread: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT) + " for sequence " + std::to_string(sequence));
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    retryCount++;
                }
                else
                {
                    WriteLog("SendThread: SendPacket failed with error=" + std::to_string(static_cast<int>(sendError)) + " for sequence " + std::to_string(sequence));
                    break;
                }
            }

            if (sendError != TransportError::Success)
            {
                WriteLog("SendThread: SendPacket failed after retries for sequence " + std::to_string(sequence));
                ReportError("发送数据包失败");
            }
        }
        catch (const std::exception &e)
        {
            WriteLog("SendThread: exception caught: " + std::string(e.what()));
            ReportError("发送线程异常: " + std::string(e.what()));
        }
        catch (...)
        {
            WriteLog("SendThread: unknown exception caught");
            ReportError("发送线程未知异常");
        }
    }

    WriteLog("SendThread exiting");
}

// 接收线程
void ReliableChannel::ReceiveThread()
{
    WriteLog("ReceiveThread started");

    while (!m_shutdown)
    {
        WriteLog("ReceiveThread: checking connection status...");

        if (!m_connected)
        {
            WriteLog("ReceiveThread: not connected, sleeping...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        WriteLog("ReceiveThread: checking receive window...");

        // 检查接收窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);

            WriteLog("ReceiveThread: locked window mutex, checking slots...");

            while (true)
            {
                uint16_t expected = m_receiveBase;
                bool found = false;

                WriteLog("ReceiveThread: looking for sequence " + std::to_string(expected));

                for (auto &slot : m_receiveWindow)
                {
                    if (slot.inUse && slot.packet && slot.packet->sequence == expected)
                    {
                        WriteLog("ReceiveThread: found matching packet at slot, sequence=" + std::to_string(slot.packet->sequence));

                        // 将数据放入接收队列
                        {
                            std::lock_guard<std::mutex> receiveLock(m_receiveMutex);
                            WriteLog("ReceiveThread: pushing data to receive queue, size=" + std::to_string(slot.packet->data.size()));
                            m_receiveQueue.push(slot.packet->data);
                            m_receiveCondition.notify_one();
                            WriteLog("ReceiveThread: data pushed, notifying condition");
                            
                            // 【关键修复】更新文件传输进度计数
                            m_recvFileProgress += slot.packet->data.size();
                            WriteLog("ReceiveThread: updated file progress to " + std::to_string(m_recvFileProgress) + " bytes");

                            // 更新进度回调
                            if (m_recvFileSize > 0)
                            {
                                UpdateProgress(m_recvFileProgress, m_recvFileSize);
                            }
                        }

                        // 更新接收窗口
                        WriteLog("ReceiveThread: updating receive window, old base=" + std::to_string(m_receiveBase));
                        slot.inUse = false;
                        slot.packet.reset();
                        m_receiveBase = (m_receiveBase + 1) % 65536;
                        WriteLog("ReceiveThread: new base=" + std::to_string(m_receiveBase) +
                                ", file progress=" + std::to_string(m_recvFileProgress) + "/" + std::to_string(m_recvFileSize));
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    WriteLog("ReceiveThread: no matching packet found, breaking loop");
                    break;
                }
            }
        }

        WriteLog("ReceiveThread: sleeping for 10ms...");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WriteLog("ReceiveThread exiting");
}

// 心跳线程
void ReliableChannel::HeartbeatThread()
{
    WriteLog("HeartbeatThread started");

    while (!m_shutdown)
    {
        WriteLog("HeartbeatThread: checking connection status...");

        if (m_connected)
        {
            // 在重传期间跳过心跳发送，避免序列冲突
            if (m_retransmitting)
            {
                WriteLog("HeartbeatThread: skipping heartbeat during retransmission");
            }
            else
            {
                WriteLog("HeartbeatThread: sending heartbeat...");
                SendHeartbeat();
            }

            // 检查连接超时
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - m_lastActivity)
                               .count();

            WriteLog("HeartbeatThread: elapsed time since last activity: " + std::to_string(elapsed) + "ms");

            if (elapsed > m_config.timeoutMax * 3)
            {
                WriteLog("HeartbeatThread: connection timeout detected, elapsed=" + std::to_string(elapsed) +
                         ", timeoutMax=" + std::to_string(m_config.timeoutMax));
                ReportError("连接超时");
                Disconnect();
            }
            else
            {
                WriteLog("HeartbeatThread: connection is healthy");
            }
        }
        else
        {
            WriteLog("HeartbeatThread: not connected");
        }

        WriteLog("HeartbeatThread: sleeping for " + std::to_string(m_config.heartbeatInterval) + "ms...");
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.heartbeatInterval));
    }

    WriteLog("HeartbeatThread exiting");
}

// 处理入站帧
void ReliableChannel::ProcessIncomingFrame(const Frame &frame)
{
    WriteLog("ProcessIncomingFrame called: type=" + std::to_string(static_cast<int>(frame.type)) +
             ", sequence=" + std::to_string(frame.sequence) +
             ", payload.size()=" + std::to_string(frame.payload.size()) +
             ", valid=" + std::to_string(frame.valid));

    if (!frame.valid)
    {
        WriteLog("ProcessIncomingFrame: frame is invalid, incrementing invalid packet count");

        // 记录无效帧统计
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.packetsInvalid++;
            WriteLog("ProcessIncomingFrame: invalid packet count now " + std::to_string(m_stats.packetsInvalid));
        }
        return;
    }

    WriteLog("ProcessIncomingFrame: frame is valid, updating activity time");

    // 更新活动时间
    m_lastActivity = std::chrono::steady_clock::now();

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsReceived++;
        WriteLog("ProcessIncomingFrame: received packet count now " + std::to_string(m_stats.packetsReceived));
    }

    WriteLog("ProcessIncomingFrame: processing frame type " + std::to_string(static_cast<int>(frame.type)));

    // 根据帧类型处理
    switch (frame.type)
    {
    case FrameType::FRAME_DATA:
        WriteLog("ProcessIncomingFrame: calling ProcessDataFrame");
        ProcessDataFrame(frame);
        WriteLog("ProcessIncomingFrame: ProcessDataFrame completed");
        break;

    case FrameType::FRAME_ACK:
        WriteLog("ProcessIncomingFrame: calling ProcessAckFrame with sequence " + std::to_string(frame.sequence));
        ProcessAckFrame(frame.sequence);
        WriteLog("ProcessIncomingFrame: ProcessAckFrame completed");
        break;

    case FrameType::FRAME_NAK:
        WriteLog("ProcessIncomingFrame: calling ProcessNakFrame with sequence " + std::to_string(frame.sequence));
        ProcessNakFrame(frame.sequence);
        WriteLog("ProcessIncomingFrame: ProcessNakFrame completed");
        break;

    case FrameType::FRAME_START:
        WriteLog("ProcessIncomingFrame: calling ProcessStartFrame");
        ProcessStartFrame(frame);
        WriteLog("ProcessIncomingFrame: ProcessStartFrame completed");
        break;

    case FrameType::FRAME_END:
        WriteLog("ProcessIncomingFrame: calling ProcessEndFrame");
        ProcessEndFrame(frame);
        WriteLog("ProcessIncomingFrame: ProcessEndFrame completed");
        break;

    case FrameType::FRAME_HEARTBEAT:
        WriteLog("ProcessIncomingFrame: calling ProcessHeartbeatFrame");
        ProcessHeartbeatFrame(frame);
        WriteLog("ProcessIncomingFrame: ProcessHeartbeatFrame completed");
        break;

    default:
        WriteLog("ProcessIncomingFrame: unknown frame type " + std::to_string(static_cast<int>(frame.type)));
        // 未知帧类型，记录错误
        ReportError("未知帧类型: " + std::to_string(static_cast<int>(frame.type)));
        break;
    }

    WriteLog("ProcessIncomingFrame: completed processing frame");
}

// 处理数据帧
void ReliableChannel::ProcessDataFrame(const Frame &frame)
{
    WriteLog("ProcessDataFrame called: sequence=" + std::to_string(frame.sequence) +
             ", payload.size()=" + std::to_string(frame.payload.size()) +
             ", receiveBase=" + std::to_string(m_receiveBase) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    // 检查序列号是否在接收窗口内
    bool inWindow = IsSequenceInWindow(frame.sequence, m_receiveBase, m_config.windowSize);
    WriteLog("ProcessDataFrame: sequence in window check: " + std::to_string(inWindow) +
             ", window range=[" + std::to_string(m_receiveBase) + "," + std::to_string(m_receiveBase + m_config.windowSize) + ")");

    if (!inWindow)
    {
        WriteLog("ProcessDataFrame: sequence outside window, sending NAK");
        // 发送NAK
        SendNak(frame.sequence);
        return;
    }

    WriteLog("ProcessDataFrame: sequence in window, processing...");

    // 将数据放入接收窗口
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        WriteLog("ProcessDataFrame: locked window mutex");

        // 确保窗口大小有效
        if (m_config.windowSize == 0 || m_receiveWindow.empty())
        {
            WriteLog("ProcessDataFrame: ERROR - receive window not initialized or size is 0");
            ReportError("接收窗口未初始化或大小为0");
            return;
        }

        uint16_t index = frame.sequence % m_config.windowSize;
        WriteLog("ProcessDataFrame: calculated index=" + std::to_string(index) +
                 ", receiveWindow.size()=" + std::to_string(m_receiveWindow.size()));

        // 边界检查
        if (index >= m_receiveWindow.size())
        {
            WriteLog("ProcessDataFrame: ERROR - receive window index out of bounds: " + std::to_string(index) +
                     " >= " + std::to_string(m_receiveWindow.size()));
            ReportError("接收窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_receiveWindow.size()));
            return;
        }

        // 【增强日志】检查槽位当前状态
        bool slotWasInUse = m_receiveWindow[index].inUse;
        WriteLog("ProcessDataFrame: slot " + std::to_string(index) + " status - wasInUse=" + std::to_string(slotWasInUse) +
                 ", hasPacket=" + std::to_string(m_receiveWindow[index].packet != nullptr));
        
        WriteLog("ProcessDataFrame: setting window slot " + std::to_string(index) + " to inUse=true");
        m_receiveWindow[index].inUse = true;

        if (!m_receiveWindow[index].packet)
        {
            WriteLog("ProcessDataFrame: creating new packet for slot " + std::to_string(index));
            m_receiveWindow[index].packet = std::make_shared<Packet>();
        }
        else if (slotWasInUse)
        {
            WriteLog("ProcessDataFrame: WARNING - overwriting existing packet in slot " + std::to_string(index) +
                     ", old sequence=" + std::to_string(m_receiveWindow[index].packet->sequence));
        }

        m_receiveWindow[index].packet->sequence = frame.sequence;
        m_receiveWindow[index].packet->data = frame.payload;
        WriteLog("ProcessDataFrame: packet data set, sequence=" + std::to_string(m_receiveWindow[index].packet->sequence) +
                 ", data.size()=" + std::to_string(m_receiveWindow[index].packet->data.size()) +
                 ", expected_next=" + std::to_string(m_receiveBase));
    }

    WriteLog("ProcessDataFrame: window update completed, sending ACK");

    // 发送ACK
    SendAck(frame.sequence);

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.bytesReceived += frame.payload.size();
        WriteLog("ProcessDataFrame: stats updated, bytesReceived=" + std::to_string(m_stats.bytesReceived));
    }

    WriteLog("ProcessDataFrame: completed successfully");
}

// 处理ACK帧
void ReliableChannel::ProcessAckFrame(uint16_t sequence)
{
    WriteLog("ProcessAckFrame called: sequence=" + std::to_string(sequence) +
             ", sendBase=" + std::to_string(m_sendBase) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    std::lock_guard<std::mutex> lock(m_windowMutex);
    WriteLog("ProcessAckFrame: locked window mutex");

    // 确保窗口大小有效
    if (m_config.windowSize == 0 || m_sendWindow.empty())
    {
        WriteLog("ProcessAckFrame: ERROR - send window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return;
    }

    // 在发送窗口中查找对应的包
    uint16_t index = sequence % m_config.windowSize;
    WriteLog("ProcessAckFrame: calculated index=" + std::to_string(index) +
             ", sendWindow.size()=" + std::to_string(m_sendWindow.size()));

    // 边界检查
    if (index >= m_sendWindow.size())
    {
        WriteLog("ProcessAckFrame: ERROR - send window index out of bounds: " + std::to_string(index) +
                 " >= " + std::to_string(m_sendWindow.size()));
        ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
        return;
    }

    WriteLog("ProcessAckFrame: checking slot " + std::to_string(index) +
             ", inUse=" + std::to_string(m_sendWindow[index].inUse));

    if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
        m_sendWindow[index].packet->sequence == sequence && !m_sendWindow[index].packet->acknowledged)
    {
        WriteLog("ProcessAckFrame: processing ACK for packet " + std::to_string(sequence));

        // 标记为已确认
        m_sendWindow[index].packet->acknowledged = true;
        m_sendWindow[index].inUse = false;
        WriteLog("ProcessAckFrame: packet marked as acknowledged and slot freed");

        // 更新RTT
        auto now = std::chrono::steady_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - m_sendWindow[index].packet->timestamp)
                       .count();
        WriteLog("ProcessAckFrame: calculated RTT=" + std::to_string(rtt) + "ms");
        UpdateRTT(static_cast<uint32_t>(rtt));

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
        WriteLog("ProcessAckFrame: calling AdvanceSendWindow");
        AdvanceSendWindow();
        WriteLog("ProcessAckFrame: AdvanceSendWindow completed");
    }
    else
    {
        WriteLog("ProcessAckFrame: ACK not processed - inUse=" + std::to_string(m_sendWindow[index].inUse) +
                 ", hasPacket=" + std::to_string(m_sendWindow[index].packet != nullptr) +
                 ", sequenceMatch=" + std::to_string(m_sendWindow[index].packet && m_sendWindow[index].packet->sequence == sequence) +
                 ", alreadyAcked=" + std::to_string(m_sendWindow[index].packet && m_sendWindow[index].packet->acknowledged));
    }

    WriteLog("ProcessAckFrame: completed");
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

        // 设置接收端文件传输信息
        {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            m_recvFileName = metadata.fileName;
            m_recvFileSize = metadata.fileSize;
            m_recvFileProgress = 0;
            m_recvFileActive = true; // 激活文件传输状态
            m_recvStartTime = std::chrono::steady_clock::now(); // 记录传输开始时间
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
    if (m_recvFileSize > 0)
    {
        // 验证实际接收字节数与预期文件大小是否匹配
        if (m_recvFileProgress >= m_recvFileSize)
        {
            WriteLog("ProcessEndFrame: 传输完整性验证通过 - " +
                     std::to_string(m_recvFileProgress) + "/" +
                     std::to_string(m_recvFileSize) + " 字节");
            m_recvFileActive = false;
            UpdateProgress(m_recvFileSize, m_recvFileSize);
            WriteLog("ProcessEndFrame: 文件传输正常完成");
        }
        else
        {
            // 【关键修复】智能完成机制：根据完成度采取不同策略
            float completionRate = (float)m_recvFileProgress / m_recvFileSize;
            WriteLog("ProcessEndFrame: 传输不完整 - 收到 " +
                     std::to_string(m_recvFileProgress) + "/" +
                     std::to_string(m_recvFileSize) + " 字节 (" +
                     std::to_string(completionRate * 100) + "% 完成)");

            if (completionRate >= 0.95) {
                // 95%以上完成：可能是轻微数据丢失，强制完成
                WriteLog("ProcessEndFrame: 传输接近完成(" +
                         std::to_string(completionRate * 100) + "%)，强制结束");
                m_recvFileActive = false;
                UpdateProgress(m_recvFileProgress, m_recvFileSize);
                ReportWarning("文件传输基本完成，可能有轻微数据丢失 (" +
                             std::to_string(completionRate * 100) + "% 完成)");
            }
            else if (completionRate < 0.1) {
                // 10%以下完成：严重问题，立即终止
                WriteLog("ProcessEndFrame: 传输严重不完整(" +
                         std::to_string(completionRate * 100) + "%)，立即终止");
                m_recvFileActive = false;
                ReportError("文件传输严重失败，仅完成 " +
                           std::to_string(completionRate * 100) + "% (" +
                           std::to_string(m_recvFileProgress) + "/" +
                           std::to_string(m_recvFileSize) + " 字节)");
            }
            else {
                // 10%-95%之间：设置短超时，等待可能的补传
                WriteLog("ProcessEndFrame: 传输部分完成(" +
                         std::to_string(completionRate * 100) + "%)，设置短超时等待补传");

                // 设置短超时标记（30秒）
                m_shortTimeoutActive = true;
                m_shortTimeoutStart = std::chrono::steady_clock::now();
                m_shortTimeoutDuration = 30; // 30秒

                ReportWarning("文件传输部分完成(" + std::to_string(completionRate * 100) + "%)，等待补传超时");
                // 暂时不设置 m_recvFileActive = false，但使用短超时机制
            }
        }
    }
    else
    {
        WriteLog("ProcessEndFrame: 无文件大小信息，使用传统逻辑结束传输");
        m_recvFileActive = false;
        UpdateProgress(m_recvFileProgress, m_recvFileProgress);
        WriteLog("ProcessEndFrame: 传统模式传输结束");
    }

    WriteLog("ProcessEndFrame: 处理完成，当前传输状态 = " + std::string(m_recvFileActive ? "活跃" : "已结束"));
}

// 处理心跳帧
void ReliableChannel::ProcessHeartbeatFrame(const Frame &frame)
{
    // 心跳帧不需要特殊处理，只需要更新活动时间
}

// 发送数据包
TransportError ReliableChannel::SendPacket(uint16_t sequence, const std::vector<uint8_t> &data, FrameType type)
{
    WriteLog("SendPacket called: sequence=" + std::to_string(sequence) +
             ", data.size()=" + std::to_string(data.size()) +
             ", type=" + std::to_string(static_cast<int>(type)));

    // 压缩数据（如果启用）
    std::vector<uint8_t> payload = data;
    if (m_config.enableCompression)
    {
        WriteLog("SendPacket: compressing data...");
        payload = CompressData(data);
        WriteLog("SendPacket: compression done, payload.size()=" + std::to_string(payload.size()));
    }

    // 加密数据（如果启用）
    if (m_config.enableEncryption)
    {
        WriteLog("SendPacket: encrypting data...");
        payload = EncryptData(payload);
        WriteLog("SendPacket: encryption done, payload.size()=" + std::to_string(payload.size()));
    }

    // 编码帧
    WriteLog("SendPacket: encoding frame...");
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
        return TransportError::WriteFailed;
    }

    WriteLog("SendPacket: frame encoded, frameData.size()=" + std::to_string(frameData.size()));

    // 发送数据
    WriteLog("SendPacket: writing to transport...");
    size_t written = 0;
    TransportError error = m_transport->Write(frameData.data(), frameData.size(), &written);

    // 检查写入结果
    if (error != TransportError::Success)
    {
        WriteLog("SendPacket: transport write returned error=" + std::to_string(static_cast<int>(error)));
        return error; // 直接返回传输层的错误码（包括Busy状态）
    }

    if (written != frameData.size())
    {
        WriteLog("SendPacket: ERROR - incomplete write: " + std::to_string(written) + "/" + std::to_string(frameData.size()));
        return TransportError::WriteFailed;
    }

    WriteLog("SendPacket: transport write succeeded, " + std::to_string(written) + " bytes written");

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsSent++;
        m_stats.bytesSent += frameData.size();
        WriteLog("SendPacket: stats updated - packetsSent=" + std::to_string(m_stats.packetsSent) +
                 ", bytesSent=" + std::to_string(m_stats.bytesSent));
    }

    // 对于数据帧，需要保存到发送窗口
    if (type == FrameType::FRAME_DATA)
    {
        WriteLog("SendPacket: saving to send window...");
        std::lock_guard<std::mutex> lock(m_windowMutex);

        // 确保窗口大小有效
        if (m_config.windowSize == 0 || m_sendWindow.empty())
        {
            WriteLog("SendPacket: ERROR - window not initialized or size is 0");
            ReportError("发送窗口未初始化或大小为0");
            return TransportError::WriteFailed;
        }

        uint16_t index = sequence % m_config.windowSize;
        WriteLog("SendPacket: calculated index=" + std::to_string(index) +
                 ", windowSize=" + std::to_string(m_config.windowSize));

        // 边界检查
        if (index >= m_sendWindow.size())
        {
            WriteLog("SendPacket: ERROR - index out of bounds: " + std::to_string(index) +
                     " >= " + std::to_string(m_sendWindow.size()));
            ReportError("发送窗口索引越界: " + std::to_string(index) + " >= " + std::to_string(m_sendWindow.size()));
            return TransportError::WriteFailed;
        }

        WriteLog("SendPacket: setting window slot " + std::to_string(index) + " to inUse=true");
        m_sendWindow[index].inUse = true;

        if (!m_sendWindow[index].packet)
        {
            WriteLog("SendPacket: creating new packet for slot " + std::to_string(index));
            m_sendWindow[index].packet = std::make_shared<Packet>();
        }

        m_sendWindow[index].packet->sequence = sequence;
        m_sendWindow[index].packet->data = data; // 保存原始数据
        m_sendWindow[index].packet->timestamp = std::chrono::steady_clock::now();
        m_sendWindow[index].packet->retryCount = 0;
        m_sendWindow[index].packet->acknowledged = false;

        WriteLog("SendPacket: window slot " + std::to_string(index) + " updated successfully");
    }

    WriteLog("SendPacket: completed successfully");
    return TransportError::Success;
}

// 发送ACK
bool ReliableChannel::SendAck(uint16_t sequence)
{
    WriteLog("SendAck called: sequence=" + std::to_string(sequence));

    std::vector<uint8_t> frameData = m_frameCodec->EncodeAckFrame(sequence);
    WriteLog("SendAck: frame encoded, size=" + std::to_string(frameData.size()));

    // 【流控机制】为ACK控制帧添加Busy重试机制
    const int MAX_RETRY_COUNT = 5;
    const int RETRY_DELAY_MS = 20;

    int retryCount = 0;
    TransportError error = TransportError::Success;
    bool success = false;

    while (retryCount < MAX_RETRY_COUNT)
    {
        size_t written = 0;
        error = m_transport->Write(frameData.data(), frameData.size(), &written);

        if (error == TransportError::Success && written == frameData.size())
        {
            success = true;
            break;
        }
        else if (error == TransportError::Busy)
        {
            WriteLog("SendAck: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT));
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            retryCount++;
        }
        else
        {
            break;
        }
    }

    WriteLog("SendAck: transport write result: error=" + std::to_string(static_cast<int>(error)) +
             ", retries=" + std::to_string(retryCount) + ", success=" + std::to_string(success));

    return success;
}

// 发送NAK
bool ReliableChannel::SendNak(uint16_t sequence)
{
    WriteLog("SendNak called: sequence=" + std::to_string(sequence));

    std::vector<uint8_t> frameData = m_frameCodec->EncodeNakFrame(sequence);
    WriteLog("SendNak: frame encoded, size=" + std::to_string(frameData.size()));

    // 【流控机制】为NAK控制帧添加Busy重试机制
    const int MAX_RETRY_COUNT = 5;
    const int RETRY_DELAY_MS = 20;

    int retryCount = 0;
    TransportError error = TransportError::Success;
    bool success = false;

    while (retryCount < MAX_RETRY_COUNT)
    {
        size_t written = 0;
        error = m_transport->Write(frameData.data(), frameData.size(), &written);

        if (error == TransportError::Success && written == frameData.size())
        {
            success = true;
            break;
        }
        else if (error == TransportError::Busy)
        {
            WriteLog("SendNak: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT));
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            retryCount++;
        }
        else
        {
            break;
        }
    }

    WriteLog("SendNak: transport write result: error=" + std::to_string(static_cast<int>(error)) +
             ", retries=" + std::to_string(retryCount) + ", success=" + std::to_string(success));

    return success;
}

// 发送心跳
bool ReliableChannel::SendHeartbeat()
{
    // 使用独立的心跳序列号，不占用数据传输序列号
    uint16_t sequence = m_heartbeatSequence++;
    WriteLog("SendHeartbeat: using independent heartbeat sequence " + std::to_string(sequence));
    
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

    // 【流控机制】为START控制帧添加Busy重试机制
    WriteLog("SendStart: sending START frame with retry mechanism...");
    const int MAX_RETRY_COUNT = 10;
    const int RETRY_DELAY_MS = 50;

    int retryCount = 0;
    TransportError sendError = TransportError::Success;
    bool success = false;

    while (retryCount < MAX_RETRY_COUNT)
    {
        size_t written = 0;
        sendError = m_transport->Write(frameData.data(), frameData.size(), &written);

        if (sendError == TransportError::Success && written == frameData.size())
        {
            WriteLog("SendStart: START frame sent successfully, " + std::to_string(written) + " bytes written, sequence=" + std::to_string(sequence));
            success = true;

            // 更新统计
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.packetsSent++;
                m_stats.bytesSent += frameData.size();
            }

            WriteLog("SendStart: START control frame stored in send window for ACK tracking");
            break;
        }
        else if (sendError == TransportError::Busy)
        {
            // 传输层繁忙，等待后重试
            WriteLog("SendStart: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT) + " for sequence " + std::to_string(sequence));
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            retryCount++;
        }
        else
        {
            // 其他错误，立即失败
            WriteLog("SendStart: ERROR - failed to send START frame, error=" +
                     std::to_string(static_cast<int>(sendError)) + ", written=" + std::to_string(written) +
                     " for sequence " + std::to_string(sequence));
            break;
        }
    }

    // 处理失败情况
    if (!success)
    {
        if (sendError == TransportError::Busy && retryCount >= MAX_RETRY_COUNT)
        {
            WriteLog("SendStart: transport busy, retry exhausted for sequence " + std::to_string(sequence));
            ReportError("传输层持续繁忙，START帧发送失败");
        }
        else
        {
            ReportError("START帧发送失败");
        }

        // 发送失败时清理发送窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            uint16_t index = sequence % m_config.windowSize;
            if (index < m_sendWindow.size())
            {
                m_sendWindow[index].inUse = false;
                m_sendWindow[index].packet.reset();
            }
        }
    }

    return success;
}

// 发送结束帧
bool ReliableChannel::SendEnd()
{
    // 【关键修复】在重试循环外分配序列号
    uint16_t sequence = AllocateSequence();

    // 【流控机制】为END控制帧添加Busy重试机制
    const int MAX_RETRY_COUNT = 10;
    const int RETRY_DELAY_MS = 50;

    int retryCount = 0;
    TransportError sendError = TransportError::Success;

    while (retryCount < MAX_RETRY_COUNT)
    {
        sendError = SendPacket(sequence, {}, FrameType::FRAME_END);

        if (sendError == TransportError::Success)
        {
            WriteLog("SendEnd: END frame sent successfully, sequence=" + std::to_string(sequence));
            return true;
        }
        else if (sendError == TransportError::Busy)
        {
            // 传输层繁忙，等待后重试
            WriteLog("SendEnd: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT) + " for sequence " + std::to_string(sequence));
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            retryCount++;
        }
        else
        {
            // 其他错误，立即失败
            WriteLog("SendEnd: SendPacket failed with error=" + std::to_string(static_cast<int>(sendError)) + " for sequence " + std::to_string(sequence));
            return false;
        }
    }

    // 重试耗尽
    if (sendError == TransportError::Busy)
    {
        WriteLog("SendEnd: transport busy, retry exhausted for sequence " + std::to_string(sequence));
        ReportError("传输层持续繁忙，END帧发送失败");
    }

    return false;
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

            // 【流控机制】为重传添加Busy重试机制
            const int MAX_RETRY_COUNT = 10;
            const int RETRY_DELAY_MS = 50;

            int retryCount = 0;
            TransportError error = TransportError::Success;
            bool success = false;

            while (retryCount < MAX_RETRY_COUNT)
            {
                size_t written = 0;
                WriteLog("RetransmitPacketInternal: writing to transport (retry " + std::to_string(retryCount) + ")...");
                error = m_transport->Write(frameData.data(), frameData.size(), &written);
                WriteLog("RetransmitPacketInternal: transport write completed, written=" + std::to_string(written) +
                         ", error=" + std::to_string(static_cast<int>(error)));

                if (error == TransportError::Success && written == frameData.size())
                {
                    WriteLog("RetransmitPacketInternal: retransmission succeeded");
                    success = true;
                    break;
                }
                else if (error == TransportError::Busy)
                {
                    WriteLog("RetransmitPacketInternal: transport busy, retry " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRY_COUNT));
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    retryCount++;
                }
                else
                {
                    WriteLog("RetransmitPacketInternal: ERROR - transport write failed with error: " + std::to_string(static_cast<int>(error)));
                    break;
                }
            }

            if (!success)
            {
                if (error == TransportError::Busy && retryCount >= MAX_RETRY_COUNT)
                {
                    WriteLog("RetransmitPacketInternal: transport busy, retry exhausted");
                    ReportError("传输层持续繁忙，重传失败");
                }
                else
                {
                    ReportError("重传数据包失败，传输错误: " + std::to_string(static_cast<int>(error)));
                }
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
}

// 更新接收窗口
void ReliableChannel::UpdateReceiveWindow(uint16_t sequence)
{
    // 这里可以实现更复杂的接收窗口管理逻辑
}

// 分配序列号
uint16_t ReliableChannel::AllocateSequence()
{
    WriteLog("AllocateSequence called");

    std::lock_guard<std::mutex> lock(m_windowMutex);

    WriteLog("AllocateSequence: sendWindow.size()=" + std::to_string(m_sendWindow.size()) +
             ", windowSize=" + std::to_string(m_config.windowSize));

    // 确保窗口已初始化
    if (m_sendWindow.empty() || m_config.windowSize == 0)
    {
        WriteLog("AllocateSequence: ERROR - window not initialized or size is 0");
        ReportError("发送窗口未初始化或大小为0");
        return 0;
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
        WriteLog("ReportError: error count incremented to " + std::to_string(m_stats.errors));
    }

    if (m_errorCallback)
    {
        WriteLog("ReportError: calling error callback");
        m_errorCallback(error);
    }
    else
    {
        WriteLog("ReportError: no error callback set");
    }
}

// 【新增】报告警告信息（不计入错误统计）
void ReliableChannel::ReportWarning(const std::string &warning)
{
    WriteLog("ReportWarning called: " + warning);

    if (m_errorCallback)
    {
        WriteLog("ReportWarning: calling error callback with warning");
        m_errorCallback("[警告] " + warning);
    }
    else
    {
        WriteLog("ReportWarning: no error callback set");
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
    // 检查发送端或接收端是否活跃
    if (!m_sendFileActive && !m_recvFileActive) return false;

    auto now = std::chrono::steady_clock::now();

    // 【新增】短超时检查 - 优先级更高
    if (m_shortTimeoutActive) {
        auto shortElapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            now - m_shortTimeoutStart).count();

        if (shortElapsed > m_shortTimeoutDuration) {
            WriteLog("CheckTransferTimeout: 短超时触发，强制结束不完整传输");
            WriteLog("CheckTransferTimeout: 短超时时间 " + std::to_string(m_shortTimeoutDuration) +
                    " 秒，实际等待 " + std::to_string(shortElapsed) + " 秒");
            WriteLog("CheckTransferTimeout: 接收文件大小 " + std::to_string(m_recvFileSize) +
                    " 字节，当前进度 " + std::to_string(m_recvFileProgress) + " 字节");

            m_recvFileActive = false;
            m_shortTimeoutActive = false;
            ReportError("不完整传输短超时：等待补传超时，传输完成 " +
                       std::to_string((m_recvFileProgress * 100.0 / m_recvFileSize)) + "%");
            return true;
        }
    }

    // 原有的传输超时逻辑 - 检查发送端或接收端
    auto sendDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_sendStartTime);
    auto recvDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_recvStartTime);

    // 计算合理的传输超时时间
    // 【修改】调整超时参数：最小5分钟，最大10分钟（原为30分钟）
    int64_t maxExpectedSeconds;
    int64_t currentFileSize = m_sendFileActive ? m_sendFileSize : m_recvFileSize;
    if (currentFileSize > 0)
    {
        // 按1KB/s的最低速度计算，但至少5分钟，最多10分钟
        int64_t minSeconds = 300LL;    // 5分钟（原为60秒）
        int64_t maxSeconds = 600LL;    // 10分钟（原为30分钟）
        int64_t calculatedSeconds = currentFileSize / 1024;
        maxExpectedSeconds = (calculatedSeconds > minSeconds) ?
                            ((calculatedSeconds < maxSeconds) ? calculatedSeconds : maxSeconds) :
                            minSeconds;
    }
    else
    {
        // 无文件大小信息时，默认5分钟超时（提高至5分钟）
        maxExpectedSeconds = 300;
    }

    // 检查发送端超时
    if (m_sendFileActive && sendDuration.count() > maxExpectedSeconds)
    {
        WriteLog("CheckTransferTimeout: 发送超时，强制结束");
        WriteLog("CheckTransferTimeout: 预期最大时间 " + std::to_string(maxExpectedSeconds) +
                " 秒，实际用时 " + std::to_string(sendDuration.count()) + " 秒");
        WriteLog("CheckTransferTimeout: 文件大小 " + std::to_string(m_sendFileSize) +
                " 字节，当前进度 " + std::to_string(m_sendFileProgress) + " 字节");

        m_sendFileActive = false;
        m_shortTimeoutActive = false; // 清除短超时状态
        ReportError("文件发送超时：传输时间 " + std::to_string(sendDuration.count()) +
                   " 秒超过预期 " + std::to_string(maxExpectedSeconds) + " 秒");
        return true;
    }

    // 检查接收端超时
    if (m_recvFileActive && recvDuration.count() > maxExpectedSeconds)
    {
        WriteLog("CheckTransferTimeout: 接收超时，强制结束");
        WriteLog("CheckTransferTimeout: 预期最大时间 " + std::to_string(maxExpectedSeconds) +
                " 秒，实际用时 " + std::to_string(recvDuration.count()) + " 秒");
        WriteLog("CheckTransferTimeout: 文件大小 " + std::to_string(m_recvFileSize) +
                " 字节，当前进度 " + std::to_string(m_recvFileProgress) + " 字节");

        m_recvFileActive = false;
        m_shortTimeoutActive = false; // 清除短超时状态
        ReportError("文件接收超时：传输时间 " + std::to_string(recvDuration.count()) +
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
            }
        }
    }

    return handshakeSuccess;
}