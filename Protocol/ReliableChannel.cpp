#include "pch.h"
#include "ReliableChannel.h"
#include "../Common/CommonTypes.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// 构造函数
ReliableChannel::ReliableChannel()
    : m_initialized(false)
    , m_connected(false)
    , m_shutdown(false)
    , m_sendBase(0)
    , m_sendNext(0)
    , m_receiveBase(0)
    , m_receiveNext(0)
    , m_currentFileSize(0)
    , m_currentFileProgress(0)
    , m_fileTransferActive(false)
    , m_rttMs(100)
    , m_timeoutMs(500)
    , m_frameCodec(std::make_unique<FrameCodec>())
{
    m_lastActivity = std::chrono::steady_clock::now();
}

// 析构函数
ReliableChannel::~ReliableChannel()
{
    Shutdown();
}

// 初始化
bool ReliableChannel::Initialize(std::shared_ptr<ITransport> transport, const ReliableConfig& config)
{
    if (!transport) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_windowMutex);
    
    m_transport = transport;
    m_config = config;
    m_frameCodec->SetMaxPayloadSize(config.maxPayloadSize);
    
    // 初始化窗口
    m_sendWindow.resize(config.windowSize);
    m_receiveWindow.resize(config.windowSize);
    
    // 重置序列号
    m_sendBase = 0;
    m_sendNext = 0;
    m_receiveBase = 0;
    m_receiveNext = 0;
    
    // 重置统计
    ResetStats();
    
    m_initialized = true;
    return true;
}

// 关闭
void ReliableChannel::Shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    m_shutdown = true;
    
    // 停止所有线程
    if (m_processThread.joinable()) {
        m_processThread.join();
    }
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    
    // 清理队列
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        while (!m_sendQueue.empty()) {
            m_sendQueue.pop();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        while (!m_receiveQueue.empty()) {
            m_receiveQueue.pop();
        }
    }
    
    // 清理窗口
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        for (auto& slot : m_sendWindow) {
            slot.inUse = false;
            slot.packet.reset();
        }
        for (auto& slot : m_receiveWindow) {
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
    if (!m_initialized || !m_transport) {
        return false;
    }
    
    if (m_connected) {
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
    if (!m_connected) {
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
bool ReliableChannel::Send(const std::vector<uint8_t>& data)
{
    if (!IsConnected()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_sendMutex);
    m_sendQueue.push(data);
    m_sendCondition.notify_one();
    
    return true;
}

bool ReliableChannel::Send(const void* data, size_t size)
{
    if (!data || size == 0) {
        return false;
    }
    
    std::vector<uint8_t> buffer(size);
    std::memcpy(buffer.data(), data, size);
    return Send(buffer);
}

// 接收数据
bool ReliableChannel::Receive(std::vector<uint8_t>& data, uint32_t timeout)
{
    if (!IsConnected()) {
        return false;
    }
    
    std::unique_lock<std::mutex> lock(m_receiveMutex);
    
    // 等待数据可用
    if (timeout > 0) {
        auto result = m_receiveCondition.wait_for(lock, std::chrono::milliseconds(timeout),
            [this] { return !m_receiveQueue.empty() || !m_connected; });
        
        if (!result) {
            return false; // 超时
        }
    } else {
        m_receiveCondition.wait(lock, [this] { return !m_receiveQueue.empty() || !m_connected; });
    }
    
    if (!m_connected && m_receiveQueue.empty()) {
        return false;
    }
    
    if (!m_receiveQueue.empty()) {
        data = m_receiveQueue.front();
        m_receiveQueue.pop();
        return true;
    }
    
    return false;
}

size_t ReliableChannel::Receive(void* buffer, size_t size, uint32_t timeout)
{
    if (!buffer || size == 0) {
        return 0;
    }
    
    std::vector<uint8_t> data;
    if (!Receive(data, timeout)) {
        return 0;
    }
    
    size_t copySize = (std::min)(size, data.size());
    std::memcpy(buffer, data.data(), copySize);
    return copySize;
}

// 发送文件
bool ReliableChannel::SendFile(const std::string& filePath, std::function<void(int64_t, int64_t)> progressCallback)
{
    if (!IsConnected()) {
        return false;
    }
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
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
    }
    
    // 发送开始帧
    auto modifyTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (!SendStart(filePath, fileSize, modifyTime)) {
        file.close();
        m_fileTransferActive = false;
        return false;
    }
    
    // 发送文件数据
    std::vector<uint8_t> buffer(m_config.maxPayloadSize);
    int64_t bytesSent = 0;
    
    while (!file.eof() && m_connected) {
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        size_t bytesRead = static_cast<size_t>(file.gcount());
        
        if (bytesRead > 0) {
            buffer.resize(bytesRead);
            if (!SendPacket(AllocateSequence(), buffer)) {
                ReportError("发送文件数据失败");
                file.close();
                m_fileTransferActive = false;
                return false;
            }
            
            bytesSent += bytesRead;
            UpdateProgress(bytesSent, fileSize);
            
            if (progressCallback) {
                progressCallback(bytesSent, fileSize);
            }
        }
    }
    
    file.close();
    
    // 发送结束帧
    if (m_connected && !SendEnd()) {
        ReportError("发送文件结束帧失败");
        m_fileTransferActive = false;
        return false;
    }
    
    m_fileTransferActive = false;
    return m_connected;
}

// 接收文件
bool ReliableChannel::ReceiveFile(const std::string& filePath, std::function<void(int64_t, int64_t)> progressCallback)
{
    if (!IsConnected()) {
        return false;
    }
    
    // 设置文件传输状态
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_currentFileName = filePath;
        m_currentFileSize = 0;
        m_currentFileProgress = 0;
        m_fileTransferActive = true;
    }
    
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        ReportError("无法创建文件: " + filePath);
        m_fileTransferActive = false;
        return false;
    }
    
    // 等待文件传输完成
    while (m_fileTransferActive && m_connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (progressCallback) {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            progressCallback(m_currentFileProgress, m_currentFileSize);
        }
    }
    
    file.close();
    return !m_fileTransferActive && m_connected;
}

// 设置配置
void ReliableChannel::SetConfig(const ReliableConfig& config)
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    m_config = config;
    m_frameCodec->SetMaxPayloadSize(config.maxPayloadSize);
    
    // 调整窗口大小
    if (m_sendWindow.size() != config.windowSize) {
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
void ReliableChannel::SetDataReceivedCallback(std::function<void(const std::vector<uint8_t>&)> callback)
{
    m_dataReceivedCallback = callback;
}

void ReliableChannel::SetStateChangedCallback(std::function<void(bool)> callback)
{
    m_stateChangedCallback = callback;
}

void ReliableChannel::SetErrorCallback(std::function<void(const std::string&)> callback)
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

// 处理线程
void ReliableChannel::ProcessThread()
{
    std::vector<uint8_t> buffer(4096);
    
    while (!m_shutdown) {
        if (!m_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 接收数据
        size_t bytesReceived = 0;
        TransportError error = m_transport->Read(buffer.data(), buffer.size(), &bytesReceived, 100);
        if (error == TransportError::Success && bytesReceived > 0) {
            // 添加到帧编解码器
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesReceived);
            m_frameCodec->AppendData(data);
            
            // 处理可用的帧
            Frame frame;
            while (m_frameCodec->TryGetFrame(frame)) {
                ProcessIncomingFrame(frame);
            }
            
            // 更新统计
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.bytesReceived += bytesReceived;
            }
        }
        
        // 处理重传
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            auto now = std::chrono::steady_clock::now();
            
            for (auto& slot : m_sendWindow) {
                if (slot.inUse && slot.packet && !slot.packet->acknowledged) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - slot.packet->timestamp).count();
                    
                    if (elapsed > CalculateTimeout() && slot.packet->retryCount < m_config.maxRetries) {
                        RetransmitPacket(slot.packet->sequence);
                    }
                }
            }
        }
    }
}

// 发送线程
void ReliableChannel::SendThread()
{
    while (!m_shutdown) {
        std::unique_lock<std::mutex> lock(m_sendMutex);
        
        // 等待发送数据
        m_sendCondition.wait(lock, [this] {
            return !m_sendQueue.empty() || !m_connected || m_shutdown;
        });
        
        if (m_shutdown) {
            break;
        }
        
        if (!m_connected) {
            continue;
        }
        
        // 处理发送队列
        while (!m_sendQueue.empty()) {
            std::vector<uint8_t> data = m_sendQueue.front();
            m_sendQueue.pop();
            lock.unlock();
            
            // 分配序列号并发送
            uint16_t sequence = AllocateSequence();
            if (!SendPacket(sequence, data)) {
                ReportError("发送数据包失败");
            }
            
            lock.lock();
        }
    }
}

// 接收线程
void ReliableChannel::ReceiveThread()
{
    while (!m_shutdown) {
        if (!m_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 检查接收窗口
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            
            while (true) {
                uint16_t expected = m_receiveBase;
                bool found = false;
                
                for (auto& slot : m_receiveWindow) {
                    if (slot.inUse && slot.packet && slot.packet->sequence == expected) {
                        // 将数据放入接收队列
                        {
                            std::lock_guard<std::mutex> receiveLock(m_receiveMutex);
                            m_receiveQueue.push(slot.packet->data);
                            m_receiveCondition.notify_one();
                        }
                        
                        // 更新接收窗口
                        slot.inUse = false;
                        slot.packet.reset();
                        m_receiveBase = (m_receiveBase + 1) % 65536;
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    break;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 心跳线程
void ReliableChannel::HeartbeatThread()
{
    while (!m_shutdown) {
        if (m_connected) {
            SendHeartbeat();
            
            // 检查连接超时
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_lastActivity).count();
            
            if (elapsed > m_config.timeoutMax * 3) {
                ReportError("连接超时");
                Disconnect();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.heartbeatInterval));
    }
}

// 处理入站帧
void ReliableChannel::ProcessIncomingFrame(const Frame& frame)
{
    if (!frame.valid) {
        return;
    }
    
    // 更新活动时间
    m_lastActivity = std::chrono::steady_clock::now();
    
    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsReceived++;
    }
    
    // 根据帧类型处理
    switch (frame.type) {
        case FrameType::FRAME_DATA:
            ProcessDataFrame(frame);
            break;
            
        case FrameType::FRAME_ACK:
            ProcessAckFrame(frame.sequence);
            break;
            
        case FrameType::FRAME_NAK:
            ProcessNakFrame(frame.sequence);
            break;
            
        case FrameType::FRAME_START:
            ProcessStartFrame(frame);
            break;
            
        case FrameType::FRAME_END:
            ProcessEndFrame(frame);
            break;
            
        case FrameType::FRAME_HEARTBEAT:
            ProcessHeartbeatFrame(frame);
            break;
            
        default:
            break;
    }
}

// 处理数据帧
void ReliableChannel::ProcessDataFrame(const Frame& frame)
{
    // 检查序列号是否在接收窗口内
    if (!IsSequenceInWindow(frame.sequence, m_receiveBase, m_config.windowSize)) {
        // 发送NAK
        SendNak(frame.sequence);
        return;
    }
    
    // 将数据放入接收窗口
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        
        uint16_t index = frame.sequence % m_config.windowSize;
        m_receiveWindow[index].inUse = true;
        
        if (!m_receiveWindow[index].packet) {
            m_receiveWindow[index].packet = std::make_shared<Packet>();
        }
        
        m_receiveWindow[index].packet->sequence = frame.sequence;
        m_receiveWindow[index].packet->data = frame.payload;
    }
    
    // 发送ACK
    SendAck(frame.sequence);
    
    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.bytesReceived += frame.payload.size();
    }
}

// 处理ACK帧
void ReliableChannel::ProcessAckFrame(uint16_t sequence)
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    
    // 在发送窗口中查找对应的包
    uint16_t index = sequence % m_config.windowSize;
    if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
        m_sendWindow[index].packet->sequence == sequence && !m_sendWindow[index].packet->acknowledged) {
        
        // 标记为已确认
        m_sendWindow[index].packet->acknowledged = true;
        m_sendWindow[index].inUse = false;
        
        // 更新RTT
        auto now = std::chrono::steady_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_sendWindow[index].packet->timestamp).count();
        UpdateRTT(static_cast<uint32_t>(rtt));
        
        // 推进发送窗口
        AdvanceSendWindow();
    }
}

// 处理NAK帧
void ReliableChannel::ProcessNakFrame(uint16_t sequence)
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    
    // 重传对应的包
    uint16_t index = sequence % m_config.windowSize;
    if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
        m_sendWindow[index].packet->sequence == sequence) {
        
        RetransmitPacket(sequence);
    }
}

// 处理开始帧
void ReliableChannel::ProcessStartFrame(const Frame& frame)
{
    StartMetadata metadata;
    if (m_frameCodec->DecodeStartMetadata(frame.payload, metadata)) {
        // 设置文件传输信息
        {
            std::lock_guard<std::mutex> lock(m_receiveMutex);
            m_currentFileName = metadata.fileName;
            m_currentFileSize = metadata.fileSize;
            m_currentFileProgress = 0;
        }
        
        UpdateProgress(0, metadata.fileSize);
    }
}

// 处理结束帧
void ReliableChannel::ProcessEndFrame(const Frame& frame)
{
    // 文件传输结束
    m_fileTransferActive = false;
    UpdateProgress(m_currentFileSize, m_currentFileSize);
}

// 处理心跳帧
void ReliableChannel::ProcessHeartbeatFrame(const Frame& frame)
{
    // 心跳帧不需要特殊处理，只需要更新活动时间
}

// 发送数据包
bool ReliableChannel::SendPacket(uint16_t sequence, const std::vector<uint8_t>& data, FrameType type)
{
    // 压缩数据（如果启用）
    std::vector<uint8_t> payload = data;
    if (m_config.enableCompression) {
        payload = CompressData(data);
    }
    
    // 加密数据（如果启用）
    if (m_config.enableEncryption) {
        payload = EncryptData(payload);
    }
    
    // 编码帧
    std::vector<uint8_t> frameData;
    switch (type) {
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
            return false;
    }
    
    // 发送数据
    size_t written = 0;
    if (m_transport->Write(frameData.data(), frameData.size(), &written) != TransportError::Success || written != frameData.size()) {
        return false;
    }
    
    // 更新统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.packetsSent++;
        m_stats.bytesSent += frameData.size();
    }
    
    // 对于数据帧，需要保存到发送窗口
    if (type == FrameType::FRAME_DATA) {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        
        uint16_t index = sequence % m_config.windowSize;
        m_sendWindow[index].inUse = true;
        
        if (!m_sendWindow[index].packet) {
            m_sendWindow[index].packet = std::make_shared<Packet>();
        }
        
        m_sendWindow[index].packet->sequence = sequence;
        m_sendWindow[index].packet->data = data; // 保存原始数据
        m_sendWindow[index].packet->timestamp = std::chrono::steady_clock::now();
        m_sendWindow[index].packet->retryCount = 0;
        m_sendWindow[index].packet->acknowledged = false;
    }
    
    return true;
}

// 发送ACK
bool ReliableChannel::SendAck(uint16_t sequence)
{
    std::vector<uint8_t> frameData = m_frameCodec->EncodeAckFrame(sequence);
    size_t written = 0;
    return m_transport->Write(frameData.data(), frameData.size(), &written) == TransportError::Success && written == frameData.size();
}

// 发送NAK
bool ReliableChannel::SendNak(uint16_t sequence)
{
    std::vector<uint8_t> frameData = m_frameCodec->EncodeNakFrame(sequence);
    size_t written = 0;
    return m_transport->Write(frameData.data(), frameData.size(), &written) == TransportError::Success && written == frameData.size();
}

// 发送心跳
bool ReliableChannel::SendHeartbeat()
{
    uint16_t sequence = AllocateSequence();
    std::vector<uint8_t> frameData = m_frameCodec->EncodeHeartbeatFrame(sequence);
    size_t written = 0;
    return m_transport->Write(frameData.data(), frameData.size(), &written) == TransportError::Success && written == frameData.size();
}

// 发送开始帧
bool ReliableChannel::SendStart(const std::string& fileName, uint64_t fileSize, uint64_t modifyTime)
{
    StartMetadata metadata;
    metadata.version = m_config.version;
    metadata.flags = 0;
    metadata.fileName = fileName;
    metadata.fileSize = fileSize;
    metadata.modifyTime = modifyTime;
    metadata.sessionId = 0; // TODO: 生成会话ID
    
    uint16_t sequence = AllocateSequence();
    
    // 编码开始帧
    std::vector<uint8_t> frameData = m_frameCodec->EncodeStartFrame(sequence, metadata);
    
    // 发送帧数据
    size_t written = 0;
    return m_transport->Write(frameData.data(), frameData.size(), &written) == TransportError::Success && written == frameData.size();
}

// 发送结束帧
bool ReliableChannel::SendEnd()
{
    uint16_t sequence = AllocateSequence();
    return SendPacket(sequence, {}, FrameType::FRAME_END);
}

// 重传数据包
void ReliableChannel::RetransmitPacket(uint16_t sequence)
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    
    uint16_t index = sequence % m_config.windowSize;
    if (m_sendWindow[index].inUse && m_sendWindow[index].packet) {
        m_sendWindow[index].packet->retryCount++;
        m_sendWindow[index].packet->timestamp = std::chrono::steady_clock::now();
        
        // 重新发送数据包
        std::vector<uint8_t> frameData = m_frameCodec->EncodeDataFrame(
            sequence, m_sendWindow[index].packet->data);
        
        size_t written = 0;
        m_transport->Write(frameData.data(), frameData.size(), &written);
        
        // 更新统计
        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.packetsRetransmitted++;
        }
    }
}

// 推进发送窗口
void ReliableChannel::AdvanceSendWindow()
{
    while (true) {
        uint16_t index = m_sendBase % m_config.windowSize;
        if (m_sendWindow[index].inUse && m_sendWindow[index].packet &&
            m_sendWindow[index].packet->acknowledged) {
            
            m_sendWindow[index].inUse = false;
            m_sendWindow[index].packet.reset();
            m_sendBase = (m_sendBase + 1) % 65536;
        } else {
            break;
        }
    }
}

// 更新接收窗口
void ReliableChannel::UpdateReceiveWindow(uint16_t sequence)
{
    // 这里可以实现更复杂的接收窗口管理逻辑
}

// 分配序列号
uint16_t ReliableChannel::AllocateSequence()
{
    std::lock_guard<std::mutex> lock(m_windowMutex);
    uint16_t sequence = m_sendNext;
    m_sendNext = (m_sendNext + 1) % 65536;
    return sequence;
}

// 检查序列号是否在窗口内
bool ReliableChannel::IsSequenceInWindow(uint16_t sequence, uint16_t base, uint16_t windowSize) const
{
    if (sequence >= base) {
        return (sequence - base) < windowSize;
    } else {
        return (65536 - base + sequence) < windowSize;
    }
}

// 获取窗口距离
uint16_t ReliableChannel::GetWindowDistance(uint16_t from, uint16_t to) const
{
    if (to >= from) {
        return to - from;
    } else {
        return 65536 - from + to;
    }
}

// 计算超时时间
uint32_t ReliableChannel::CalculateTimeout() const
{
    return (std::min)(m_timeoutMs, m_config.timeoutMax);
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
std::vector<uint8_t> ReliableChannel::CompressData(const std::vector<uint8_t>& data) const
{
    // TODO: 实现数据压缩
    return data;
}

// 解压缩数据
std::vector<uint8_t> ReliableChannel::DecompressData(const std::vector<uint8_t>& data) const
{
    // TODO: 实现数据解压缩
    return data;
}

// 加密数据
std::vector<uint8_t> ReliableChannel::EncryptData(const std::vector<uint8_t>& data) const
{
    // TODO: 实现数据加密
    return data;
}

// 解密数据
std::vector<uint8_t> ReliableChannel::DecryptData(const std::vector<uint8_t>& data) const
{
    // TODO: 实现数据解密
    return data;
}

// 报告错误
void ReliableChannel::ReportError(const std::string& error)
{
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.errors++;
    }
    
    if (m_errorCallback) {
        m_errorCallback(error);
    }
}

// 更新状态
void ReliableChannel::UpdateState(bool connected)
{
    if (m_stateChangedCallback) {
        m_stateChangedCallback(connected);
    }
}

// 更新进度
void ReliableChannel::UpdateProgress(int64_t current, int64_t total)
{
    if (m_progressCallback) {
        m_progressCallback(current, total);
    }
}