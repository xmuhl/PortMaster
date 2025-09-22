#pragma execution_character_set("utf-8")

#include "pch.h"
#include "LoopbackTransport.h"
#include "../Common/CommonTypes.h"
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>

// 构造函数
LoopbackTransport::LoopbackTransport()
    : m_state(TransportState::Closed)
    , m_stopLoopback(false)
    , m_loopbackTestRunning(false)
    , m_sequenceCounter(0)
    , m_randomGenerator(m_randomDevice())
    , m_percentDistribution(0, 99)
{
    m_stats = LoopbackStats();
    m_lastStatsUpdate = std::chrono::steady_clock::now();
    m_connectionStartTime = std::chrono::steady_clock::now();
    
    LogOperation("构造函数", "回路传输对象已创建");
}

// 析构函数
LoopbackTransport::~LoopbackTransport()
{
    Close();
    LogOperation("析构函数", "回路传输对象已销毁");
}

// 打开传输通道
TransportError LoopbackTransport::Open(const TransportConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != TransportState::Closed)
    {
        LogOperation("打开连接", "连接已存在，拒绝重复打开");
        return TransportError::AlreadyOpen;
    }
    
    // 验证配置类型
    const LoopbackConfig* loopbackConfig = dynamic_cast<const LoopbackConfig*>(&config);
    if (loopbackConfig)
    {
        m_config = *loopbackConfig;
    }
    else
    {
        // 使用默认配置
        m_config = LoopbackConfig();
        m_config.portName = config.portName.empty() ? "LOOPBACK" : config.portName;
        m_config.readTimeout = config.readTimeout;
        m_config.writeTimeout = config.writeTimeout;
        m_config.bufferSize = config.bufferSize;
        m_config.asyncMode = config.asyncMode;
    }
    
    // 验证配置参数
    if (m_config.errorRate > 100)
    {
        m_config.errorRate = 100;
    }
    if (m_config.packetLossRate > 100)
    {
        m_config.packetLossRate = 100;
    }
    if (m_config.delayMs > 10000)  // 最大10秒延迟
    {
        m_config.delayMs = 10000;
    }
    
    // 重置统计信息
    ResetStats();
    
    // 更新状态
    m_state = TransportState::Opening;
    NotifyStateChanged(m_state);
    
    // 启动回路工作线程
    m_stopLoopback = false;
    try
    {
        m_loopbackThread = std::thread(&LoopbackTransport::LoopbackWorkerThread, this);
        
        // 模拟连接建立时间
        SimulateDelay(m_config.delayMs);
        
        m_state = TransportState::Open;
        m_connectionStartTime = std::chrono::steady_clock::now();
        
        NotifyStateChanged(m_state);
        LogOperation("打开连接", "回路传输连接已建立，延迟:" + std::to_string(m_config.delayMs) + "ms");
        
        return TransportError::Success;
    }
    catch (const std::exception& e)
    {
        m_state = TransportState::Error;
        NotifyStateChanged(m_state);
        LogOperation("打开连接", "线程创建失败: " + std::string(e.what()));
        return TransportError::OpenFailed;
    }
}

// 关闭传输通道
TransportError LoopbackTransport::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == TransportState::Closed)
    {
        return TransportError::Success;
    }
    
    m_state = TransportState::Closing;
    NotifyStateChanged(m_state);
    
    // 停止回路测试
    m_loopbackTestRunning = false;
    
    // 停止工作线程
    m_stopLoopback = true;
    m_receiveCondition.notify_all();
    
    if (m_loopbackThread.joinable())
    {
        m_loopbackThread.join();
    }
    
    // 清空队列
    {
        std::lock_guard<std::mutex> sendLock(m_sendQueueMutex);
        std::queue<LoopbackPacket> empty;
        m_sendQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> receiveLock(m_receiveQueueMutex);
        std::queue<LoopbackPacket> empty;
        m_receiveQueue.swap(empty);
    }
    
    m_state = TransportState::Closed;
    NotifyStateChanged(m_state);
    
    LogOperation("关闭连接", "回路传输连接已关闭");
    return TransportError::Success;
}

// 同步写入数据
TransportError LoopbackTransport::Write(const void* data, size_t size, size_t* written)
{
    if (!data || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    if (m_state != TransportState::Open)
    {
        return TransportError::NotOpen;
    }
    
    // 创建数据包
    std::vector<uint8_t> dataVec(static_cast<const uint8_t*>(data), 
                                 static_cast<const uint8_t*>(data) + size);
    
    uint32_t sequenceId = m_sequenceCounter.fetch_add(1);
    LoopbackPacket packet(dataVec, sequenceId);
    
    // 模拟错误和丢包
    packet.shouldError = ShouldSimulateError();
    packet.shouldLoss = ShouldSimulatePacketLoss();
    
    {
        std::lock_guard<std::mutex> lock(m_sendQueueMutex);
        
        // 检查队列大小
        if (m_sendQueue.size() >= m_config.maxQueueSize)
        {
            LogOperation("写入数据", "发送队列已满，丢弃数据包 #" + std::to_string(sequenceId));
            return TransportError::Busy;
        }
        
        m_sendQueue.push(packet);
    }
    
    // 更新统计
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.bytesSent += size;
        m_stats.packetsTotal++;
        
        if (packet.shouldError)
        {
            m_stats.simulatedErrors++;
            m_stats.packetsError++;
        }
        
        if (packet.shouldLoss)
        {
            m_stats.simulatedLosses++;
            m_stats.packetsError++;
        }
    }
    
    if (written)
    {
        *written = size;
    }
    
    LogOperation("写入数据", "数据包 #" + std::to_string(sequenceId) + 
                 " 大小:" + std::to_string(size) + "字节" +
                 (packet.shouldError ? " [模拟错误]" : "") +
                 (packet.shouldLoss ? " [模拟丢包]" : ""));
    
    return TransportError::Success;
}

// 同步读取数据
TransportError LoopbackTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
    if (!buffer || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    if (m_state != TransportState::Open)
    {
        return TransportError::NotOpen;
    }
    
    std::unique_lock<std::mutex> lock(m_receiveQueueMutex);
    
    // 等待数据可用
    auto timeoutMs = (timeout == INFINITE) ? std::chrono::milliseconds(0x7FFFFFFF) : 
                     std::chrono::milliseconds(timeout);
    
    bool dataAvailable = m_receiveCondition.wait_for(lock, timeoutMs, 
        [this] { return !m_receiveQueue.empty() || m_state != TransportState::Open; });
    
    if (m_state != TransportState::Open)
    {
        return TransportError::ConnectionClosed;
    }
    
    if (!dataAvailable || m_receiveQueue.empty())
    {
        if (read) *read = 0;
        return TransportError::Timeout;
    }
    
    // 读取数据
    LoopbackPacket packet = m_receiveQueue.front();
    m_receiveQueue.pop();
    lock.unlock();
    
    // 计算延迟统计
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - packet.sendTime).count();
    
    // 复制数据到用户缓冲区
    size_t copySize = (size < packet.data.size()) ? size : packet.data.size();
    std::memcpy(buffer, packet.data.data(), copySize);
    
    // 更新统计
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.bytesReceived += copySize;
        
        // 更新平均延迟
        if (m_stats.packetsReceived == 0)
        {
            m_stats.averageLatencyMs = static_cast<double>(latency);
        }
        else
        {
            m_stats.averageLatencyMs = (m_stats.averageLatencyMs * m_stats.packetsReceived + latency) / 
                                      (m_stats.packetsReceived + 1);
        }
    }
    
    if (read)
    {
        *read = copySize;
    }
    
    LogOperation("读取数据", "数据包 #" + std::to_string(packet.sequenceId) + 
                 " 大小:" + std::to_string(copySize) + "字节" +
                 " 延迟:" + std::to_string(latency) + "ms");
    
    return TransportError::Success;
}

// 异步写入数据
TransportError LoopbackTransport::WriteAsync(const void* data, size_t size)
{
    // 异步写入与同步写入使用相同的逻辑，因为回路传输本身就是异步的
    return Write(data, size, nullptr);
}

// 启动异步读取
TransportError LoopbackTransport::StartAsyncRead()
{
    if (m_state != TransportState::Open)
    {
        return TransportError::NotOpen;
    }
    
    // 回路传输默认就是异步的，这里只记录操作
    LogOperation("启动异步读取", "异步读取模式已启用");
    return TransportError::Success;
}

// 停止异步读取
TransportError LoopbackTransport::StopAsyncRead()
{
    LogOperation("停止异步读取", "异步读取模式已停用");
    return TransportError::Success;
}

// 获取传输状态
TransportState LoopbackTransport::GetState() const
{
    return m_state.load();
}

// 检查是否已打开
bool LoopbackTransport::IsOpen() const
{
    return m_state.load() == TransportState::Open;
}

// 获取统计信息
TransportStats LoopbackTransport::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // 更新吞吐率
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_connectionStartTime).count();
    
    if (elapsed > 0)
    {
        const_cast<LoopbackStats&>(m_stats).throughputBps = 
            static_cast<double>(m_stats.bytesSent + m_stats.bytesReceived) / elapsed;
    }
    
    return m_stats;
}

// 获取回路统计信息
LoopbackStats LoopbackTransport::GetLoopbackStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // 更新队列深度
    {
        std::lock_guard<std::mutex> sendLock(m_sendQueueMutex);
        std::lock_guard<std::mutex> receiveLock(m_receiveQueueMutex);
        const_cast<LoopbackStats&>(m_stats).queueDepth = 
            static_cast<uint32_t>(m_sendQueue.size() + m_receiveQueue.size());
    }
    
    return m_stats;
}

// 重置统计信息
void LoopbackTransport::ResetStats()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = LoopbackStats();
    m_connectionStartTime = std::chrono::steady_clock::now();
    LogOperation("重置统计", "所有统计信息已清零");
}

// 获取端口名称
std::string LoopbackTransport::GetPortName() const
{
    return m_config.portName;
}

// 设置数据接收回调
void LoopbackTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataReceivedCallback = callback;
}

// 设置状态变化回调
void LoopbackTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateChangedCallback = callback;
}

// 设置错误发生回调
void LoopbackTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback)
{
    m_errorOccurredCallback = callback;
}

// 清空缓冲区
TransportError LoopbackTransport::FlushBuffers()
{
    {
        std::lock_guard<std::mutex> sendLock(m_sendQueueMutex);
        std::queue<LoopbackPacket> empty;
        m_sendQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> receiveLock(m_receiveQueueMutex);
        std::queue<LoopbackPacket> empty;
        m_receiveQueue.swap(empty);
    }
    
    LogOperation("清空缓冲区", "发送和接收队列已清空");
    return TransportError::Success;
}

// 获取可用字节数
size_t LoopbackTransport::GetAvailableBytes() const
{
    std::lock_guard<std::mutex> lock(m_receiveQueueMutex);
    
    size_t totalBytes = 0;
    std::queue<LoopbackPacket> tempQueue = m_receiveQueue;  // 复制队列
    
    while (!tempQueue.empty())
    {
        totalBytes += tempQueue.front().data.size();
        tempQueue.pop();
    }
    
    return totalBytes;
}

// 设置回路配置
void LoopbackTransport::SetLoopbackConfig(const LoopbackConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    LogOperation("设置配置", "回路传输配置已更新");
}

// 获取回路配置
LoopbackConfig LoopbackTransport::GetLoopbackConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

// 开始回路测试
void LoopbackTransport::StartLoopbackTest()
{
    m_loopbackTestRunning = true;
    LogOperation("开始回路测试", "自动回路测试已启动");
}

// 停止回路测试
void LoopbackTransport::StopLoopbackTest()
{
    m_loopbackTestRunning = false;
    LogOperation("停止回路测试", "自动回路测试已停止");
}

// 检查回路测试是否运行
bool LoopbackTransport::IsLoopbackTestRunning() const
{
    return m_loopbackTestRunning.load();
}

// 触发手动回路
void LoopbackTransport::TriggerManualRound()
{
    if (m_state == TransportState::Open)
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.loopbackRounds++;
        LogOperation("手动回路", "回路轮次 #" + std::to_string(m_stats.loopbackRounds));
    }
}

// 注入错误
void LoopbackTransport::InjectError()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.simulatedErrors++;
    m_stats.packetsError++;
    LogOperation("注入错误", "手动注入错误 #" + std::to_string(m_stats.simulatedErrors));
}

// 注入丢包
void LoopbackTransport::InjectPacketLoss()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.simulatedLosses++;
    m_stats.packetsError++;
    LogOperation("注入丢包", "手动注入丢包 #" + std::to_string(m_stats.simulatedLosses));
}

// 设置错误率
void LoopbackTransport::SetErrorRate(uint32_t rate)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.errorRate = (rate < 100u) ? rate : 100u;
    LogOperation("设置错误率", std::to_string(m_config.errorRate) + "%");
}

// 设置丢包率
void LoopbackTransport::SetPacketLossRate(uint32_t rate)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.packetLossRate = (rate < 100u) ? rate : 100u;
    LogOperation("设置丢包率", std::to_string(m_config.packetLossRate) + "%");
}

// 回路工作线程
void LoopbackTransport::LoopbackWorkerThread()
{
    LogOperation("工作线程", "回路工作线程已启动");
    
    while (!m_stopLoopback)
    {
        try
        {
            ProcessSendQueue();
            ProcessReceiveQueue();
            UpdateStatistics();
            
            // 自动回路测试
            if (m_loopbackTestRunning && m_sendQueue.empty() && m_receiveQueue.empty())
            {
                TriggerManualRound();
                
                // 生成测试数据
                std::string testData = "回路测试数据 #" + std::to_string(m_stats.loopbackRounds) + 
                                      " 时间戳:" + std::to_string(
                                          std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch()).count());
                
                WriteAsync(testData.c_str(), testData.length());
            }
            
            // 短暂休眠，避免过度占用CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        catch (const std::exception& e)
        {
            NotifyError(TransportError::ConnectionClosed, "工作线程异常: " + std::string(e.what()));
            break;
        }
    }
    
    LogOperation("工作线程", "回路工作线程已停止");
}

// 处理发送队列
void LoopbackTransport::ProcessSendQueue()
{
    std::unique_lock<std::mutex> sendLock(m_sendQueueMutex);
    
    if (m_sendQueue.empty())
    {
        return;
    }
    
    LoopbackPacket packet = m_sendQueue.front();
    m_sendQueue.pop();
    sendLock.unlock();
    
    // 模拟传输延迟
    uint32_t delay = CalculateDelay();
    SimulateDelay(delay);
    
    // 检查是否模拟丢包
    if (packet.shouldLoss)
    {
        LogOperation("处理数据包", "数据包 #" + std::to_string(packet.sequenceId) + " 被丢弃（模拟丢包）");
        return;
    }
    
    // 检查是否模拟错误
    if (packet.shouldError)
    {
        // 损坏数据（翻转第一个字节的某些位）
        if (!packet.data.empty())
        {
            packet.data[0] ^= 0x55;  // 翻转部分位
        }
        LogOperation("处理数据包", "数据包 #" + std::to_string(packet.sequenceId) + " 已损坏（模拟错误）");
    }
    
    // 将数据包移到接收队列
    {
        std::lock_guard<std::mutex> receiveLock(m_receiveQueueMutex);
        m_receiveQueue.push(packet);
        m_receiveCondition.notify_one();
    }
    
    // 触发异步回调
    if (m_dataReceivedCallback)
    {
        NotifyDataReceived(packet.data);
    }
}

// 处理接收队列
void LoopbackTransport::ProcessReceiveQueue()
{
    // 这里可以添加接收队列的额外处理逻辑
    // 目前主要功能在ProcessSendQueue中处理
}

// 检查是否应该模拟错误
bool LoopbackTransport::ShouldSimulateError() const
{
    if (m_config.errorRate == 0) return false;
    return m_percentDistribution(m_randomGenerator) < m_config.errorRate;
}

// 检查是否应该模拟丢包
bool LoopbackTransport::ShouldSimulatePacketLoss() const
{
    if (m_config.packetLossRate == 0) return false;
    return m_percentDistribution(m_randomGenerator) < m_config.packetLossRate;
}

// 计算延迟
uint32_t LoopbackTransport::CalculateDelay() const
{
    uint32_t baseDelay = m_config.delayMs;
    
    if (m_config.enableJitter && m_config.jitterMaxMs > 0)
    {
        std::uniform_int_distribution<uint32_t> jitterDist(0, m_config.jitterMaxMs);
        uint32_t jitter = jitterDist(m_randomGenerator);
        return baseDelay + jitter;
    }
    
    return baseDelay;
}

// 更新统计信息
void LoopbackTransport::UpdateStatistics()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_lastStatsUpdate).count();
    
    if (elapsed >= 1)  // 每秒更新一次
    {
        // 更新队列深度
        {
            std::lock_guard<std::mutex> sendLock(m_sendQueueMutex);
            std::lock_guard<std::mutex> receiveLock(m_receiveQueueMutex);
            m_stats.queueDepth = static_cast<uint32_t>(m_sendQueue.size() + m_receiveQueue.size());
        }
        
        m_lastStatsUpdate = now;
    }
}

// 通知数据接收
void LoopbackTransport::NotifyDataReceived(const std::vector<uint8_t>& data)
{
    if (m_dataReceivedCallback)
    {
        try
        {
            m_dataReceivedCallback(data);
        }
        catch (const std::exception& e)
        {
            LogOperation("回调异常", "数据接收回调发生异常: " + std::string(e.what()));
        }
    }
}

// 通知状态变化
void LoopbackTransport::NotifyStateChanged(TransportState newState)
{
    if (m_stateChangedCallback)
    {
        try
        {
            m_stateChangedCallback(newState);
        }
        catch (const std::exception& e)
        {
            LogOperation("回调异常", "状态变化回调发生异常: " + std::string(e.what()));
        }
    }
}

// 通知错误
void LoopbackTransport::NotifyError(TransportError error, const std::string& message)
{
    if (m_errorOccurredCallback)
    {
        try
        {
            m_errorOccurredCallback(error, message);
        }
        catch (const std::exception& e)
        {
            LogOperation("回调异常", "错误回调发生异常: " + std::string(e.what()));
        }
    }
}

// 记录操作日志
void LoopbackTransport::LogOperation(const std::string& operation, const std::string& details)
{
    if (m_config.enableLogging)
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        struct tm timeStruct;
        localtime_s(&timeStruct, &time_t);
        ss << "[" << std::put_time(&timeStruct, "%H:%M:%S") 
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "LoopbackTransport::" << operation;
        
        if (!details.empty())
        {
            ss << " - " << details;
        }
        
        // 这里可以添加实际的日志输出
        // 例如：OutputDebugStringA(ss.str().c_str());
    }
}

// 模拟延迟
void LoopbackTransport::SimulateDelay(uint32_t delayMs) const
{
    if (delayMs > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}