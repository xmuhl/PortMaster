#pragma once
#pragma execution_character_set("utf-8")

#include "ITransport.h"
#include <queue>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <random>
#include <mutex>

// 回路传输配置
struct LoopbackConfig : public TransportConfig
{
    uint32_t delayMs = 10;              // 模拟传输延迟(ms)
    uint32_t errorRate = 0;             // 错误率(0-100%)
    uint32_t packetLossRate = 0;        // 丢包率(0-100%)
    bool enableJitter = false;          // 启用抖动模拟
    uint32_t jitterMaxMs = 5;           // 最大抖动时间(ms)
    uint32_t maxQueueSize = 10000;      // 最大队列大小（扩大以支持大文件传输）
    bool enableLogging = true;          // 启用详细日志
    
    LoopbackConfig()
    {
        portName = "LOOPBACK";
        readTimeout = 1000;
        writeTimeout = 1000;
        bufferSize = 4096;
        asyncMode = true;
    }
};

// 回路传输统计信息
struct LoopbackStats : public TransportStats
{
    uint64_t loopbackRounds = 0;        // 回路轮次
    uint64_t successfulRounds = 0;      // 成功轮次
    uint64_t failedRounds = 0;          // 失败轮次
    uint64_t simulatedErrors = 0;       // 模拟错误次数
    uint64_t simulatedLosses = 0;       // 模拟丢包次数
    double averageLatencyMs = 0.0;      // 平均延迟(ms)
    uint32_t queueDepth = 0;            // 当前队列深度
    uint64_t packetsReceived = 0;       // 接收包数（用于延迟计算）
    
    LoopbackStats() : TransportStats()
    {
        // 继承基类统计信息
    }
};

// 回路数据包
struct LoopbackPacket
{
    std::vector<uint8_t> data;          // 数据内容
    std::chrono::steady_clock::time_point sendTime;  // 发送时间
    uint32_t sequenceId;                // 序列号
    bool shouldError;                   // 是否模拟错误
    bool shouldLoss;                    // 是否模拟丢包
    
    LoopbackPacket() : sequenceId(0), shouldError(false), shouldLoss(false)
    {
        sendTime = std::chrono::steady_clock::now();
    }
    
    LoopbackPacket(const std::vector<uint8_t>& d, uint32_t seq) 
        : data(d), sequenceId(seq), shouldError(false), shouldLoss(false)
    {
        sendTime = std::chrono::steady_clock::now();
    }
};

// 回路传输实现类
class LoopbackTransport : public ITransport
{
public:
    LoopbackTransport();
    virtual ~LoopbackTransport();
    
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
    
    // 回路测试特有功能
    LoopbackStats GetLoopbackStats() const;
    void SetLoopbackConfig(const LoopbackConfig& config);
    LoopbackConfig GetLoopbackConfig() const;
    
    // 测试控制功能
    void StartLoopbackTest();
    void StopLoopbackTest();
    bool IsLoopbackTestRunning() const;
    void TriggerManualRound();
    
    // 错误注入功能
    void InjectError();
    void InjectPacketLoss();
    void SetErrorRate(uint32_t rate);        // 设置错误率(0-100%)
    void SetPacketLossRate(uint32_t rate);   // 设置丢包率(0-100%)

private:
    // 内部状态
    mutable std::mutex m_mutex;
    std::atomic<TransportState> m_state;
    LoopbackConfig m_config;
    LoopbackStats m_stats;
    
    // 数据队列
    std::queue<LoopbackPacket> m_sendQueue;      // 发送队列
    std::queue<LoopbackPacket> m_receiveQueue;   // 接收队列
    mutable std::mutex m_sendQueueMutex;
    mutable std::mutex m_receiveQueueMutex;
    std::condition_variable m_receiveCondition;
    
    // 工作线程
    std::thread m_loopbackThread;
    std::atomic<bool> m_stopLoopback;
    std::atomic<bool> m_loopbackTestRunning;
    
    // 序列号管理
    std::atomic<uint32_t> m_sequenceCounter;
    
    // 回调函数
    DataReceivedCallback m_dataReceivedCallback;
    StateChangedCallback m_stateChangedCallback;
    ErrorOccurredCallback m_errorOccurredCallback;
    
    // 统计数据
    mutable std::mutex m_statsMutex;
    std::chrono::steady_clock::time_point m_lastStatsUpdate;
    
    // 内部方法
    void LoopbackWorkerThread();
    void ProcessSendQueue();
    void ProcessReceiveQueue();
    bool ShouldSimulateError() const;
    bool ShouldSimulatePacketLoss() const;
    uint32_t CalculateDelay() const;
    void UpdateStatistics();
    void NotifyDataReceived(const std::vector<uint8_t>& data);
    void NotifyStateChanged(TransportState newState);
    void NotifyError(TransportError error, const std::string& message);
    void LogOperation(const std::string& operation, const std::string& details = "");
    
    // 随机数生成
    mutable std::random_device m_randomDevice;
    mutable std::mt19937 m_randomGenerator;
    mutable std::uniform_int_distribution<uint32_t> m_percentDistribution;
    
    // 延迟模拟
    void SimulateDelay(uint32_t baseDelayMs) const;
    std::chrono::steady_clock::time_point m_connectionStartTime;
};