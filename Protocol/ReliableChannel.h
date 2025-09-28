#pragma once
#pragma execution_character_set("utf-8")

#include "FrameCodec.h"
#include "../Transport/ITransport.h"
#include "../Common/RingBuffer.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <functional>

// 可靠传输配置
struct ReliableConfig
{
    uint8_t version = 1;               // 协议版本
    uint16_t windowSize = 4;           // 滑动窗口大小
    uint16_t maxRetries = 3;           // 最大重试次数
    uint32_t timeoutBase = 500;        // 基础超时时间(ms)
    uint32_t timeoutMax = 2000;        // 最大超时时间(ms)
    uint32_t heartbeatInterval = 1000; // 心跳间隔(ms)
    uint32_t maxPayloadSize = 1024;    // 最大负载大小
    bool enableCompression = false;    // 启用压缩
    bool enableEncryption = false;     // 启用加密
    std::string encryptionKey;         // 加密密钥
};

// 可靠传输统计信息
struct ReliableStats
{
    uint64_t packetsSent = 0;          // 发送包数
    uint64_t packetsReceived = 0;      // 接收包数
    uint64_t packetsRetransmitted = 0; // 重传包数
    uint64_t packetsInvalid = 0;       // 无效包数
    uint64_t bytesSent = 0;            // 发送字节数
    uint64_t bytesReceived = 0;        // 接收字节数
    uint64_t timeouts = 0;             // 超时次数
    uint64_t errors = 0;               // 错误次数
    double throughputBps = 0.0;        // 吞吐率
    uint32_t rttMs = 0;                // 往返时延
    uint8_t packetLossRate = 0;        // 丢包率(百分比)
};

// 可靠传输通道
class ReliableChannel
{
public:
    // 构造函数和析构函数
    ReliableChannel();
    ~ReliableChannel();

    // 初始化
    bool Initialize(std::shared_ptr<ITransport> transport, const ReliableConfig &config);
    void Shutdown();

    // 连接管理
    bool Connect();
    bool Disconnect();
    bool IsConnected() const;

    // 数据传输
    bool Send(const std::vector<uint8_t> &data);
    bool Send(const void *data, size_t size);
    bool Receive(std::vector<uint8_t> &data, uint32_t timeout = 0);
    size_t Receive(void *buffer, size_t size, uint32_t timeout = 0);

    // 文件传输
    bool SendFile(const std::string &filePath, std::function<void(int64_t, int64_t)> progressCallback = nullptr);
    bool ReceiveFile(const std::string &filePath, std::function<void(int64_t, int64_t)> progressCallback = nullptr);

    // 配置和统计
    void SetConfig(const ReliableConfig &config);
    ReliableConfig GetConfig() const;
    ReliableStats GetStats() const;
    void ResetStats();

    // 回调函数
    void SetDataReceivedCallback(std::function<void(const std::vector<uint8_t> &)> callback);
    void SetStateChangedCallback(std::function<void(bool)> callback);
    void SetErrorCallback(std::function<void(const std::string &)> callback);
    void SetProgressCallback(std::function<void(int64_t, int64_t)> callback);

    // 状态查询
    uint16_t GetLocalSequence() const;
    uint16_t GetRemoteSequence() const;
    size_t GetSendQueueSize() const;
    size_t GetReceiveQueueSize() const;

    // 【可靠模式按钮管控】文件传输活跃状态检查接口
    bool IsFileTransferActive() const;

private:
    // 内部类型定义
    struct Packet
    {
        uint16_t sequence;
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
        uint32_t retryCount;
        bool acknowledged;

        Packet() : sequence(0), retryCount(0), acknowledged(false) {}
        Packet(uint16_t seq, const std::vector<uint8_t> &d)
            : sequence(seq), data(d), retryCount(0), acknowledged(false)
        {
            timestamp = std::chrono::steady_clock::now();
        }
    };

    struct WindowSlot
    {
        std::shared_ptr<Packet> packet;
        bool inUse;

        WindowSlot() : inUse(false) {}
    };

    // 内部方法
    void ProcessThread();
    void SendThread();
    void ReceiveThread();
    void HeartbeatThread();

    // 日志函数
    void WriteLog(const std::string &message);

    void ProcessIncomingFrame(const Frame &frame);
    void ProcessDataFrame(const Frame &frame);
    void ProcessAckFrame(uint16_t sequence);
    void ProcessNakFrame(uint16_t sequence);
    void ProcessStartFrame(const Frame &frame);
    void ProcessEndFrame(const Frame &frame);
    void ProcessHeartbeatFrame(const Frame &frame);

    bool SendPacket(uint16_t sequence, const std::vector<uint8_t> &data, FrameType type = FrameType::FRAME_DATA);
    bool SendAck(uint16_t sequence);
    bool SendNak(uint16_t sequence);
    bool SendHeartbeat();
    bool SendStart(const std::string &fileName, uint64_t fileSize, uint64_t modifyTime);
    bool SendEnd();

    void RetransmitPacket(uint16_t sequence);
    void RetransmitPacketInternal(uint16_t sequence); // 内部版本，假设已持有窗口锁
    void AdvanceSendWindow();
    void UpdateReceiveWindow(uint16_t sequence);

    uint16_t AllocateSequence();
    bool IsSequenceInWindow(uint16_t sequence, uint16_t base, uint16_t windowSize) const;
    uint16_t GetWindowDistance(uint16_t from, uint16_t to) const;

    // 握手管理
    uint16_t GenerateSessionId();
    bool WaitForHandshakeCompletion(uint32_t timeoutMs);

    uint32_t CalculateTimeout() const;
    void UpdateRTT(uint32_t rttMs);

    std::vector<uint8_t> CompressData(const std::vector<uint8_t> &data) const;
    std::vector<uint8_t> DecompressData(const std::vector<uint8_t> &data) const;

    std::vector<uint8_t> EncryptData(const std::vector<uint8_t> &data) const;
    std::vector<uint8_t> DecryptData(const std::vector<uint8_t> &data) const;

    void ReportError(const std::string &error);
    void UpdateState(bool connected);
    void UpdateProgress(int64_t current, int64_t total);

    // 传输超时保护
    bool CheckTransferTimeout();

private:
    // 成员变量
    std::shared_ptr<ITransport> m_transport; // 底层传输
    ReliableConfig m_config;                 // 配置
    ReliableStats m_stats;                   // 统计信息

    std::atomic<bool> m_initialized; // 初始化标志
    std::atomic<bool> m_connected;   // 连接状态
    std::atomic<bool> m_shutdown;    // 关闭标志
    std::atomic<bool> m_retransmitting; // 重传进行标志

    // 线程
    std::thread m_processThread;   // 处理线程
    std::thread m_sendThread;      // 发送线程
    std::thread m_receiveThread;   // 接收线程
    std::thread m_heartbeatThread; // 心跳线程

    // 滑动窗口
    std::vector<WindowSlot> m_sendWindow;    // 发送窗口
    std::vector<WindowSlot> m_receiveWindow; // 接收窗口
    uint16_t m_sendBase;                     // 发送窗口基
    uint16_t m_sendNext;                     // 下一个发送序列
    uint16_t m_receiveBase;                  // 接收窗口基
    uint16_t m_receiveNext;                  // 下一个接收序列
    uint16_t m_heartbeatSequence;            // 心跳包独立序列号

    // 队列
    std::queue<std::vector<uint8_t>> m_sendQueue;    // 发送队列
    std::queue<std::vector<uint8_t>> m_receiveQueue; // 接收队列

    // 同步对象
    mutable std::mutex m_sendMutex;             // 发送锁
    mutable std::mutex m_receiveMutex;          // 接收锁
    mutable std::mutex m_windowMutex;           // 窗口锁
    mutable std::mutex m_statsMutex;            // 统计锁
    std::condition_variable m_sendCondition;    // 发送条件变量
    std::condition_variable m_receiveCondition; // 接收条件变量

    // 文件传输相关
    std::string m_currentFileName; // 当前文件名
    int64_t m_currentFileSize;     // 当前文件大小
    int64_t m_currentFileProgress; // 当前文件进度
    bool m_fileTransferActive;     // 文件传输活动状态
    std::chrono::steady_clock::time_point m_transferStartTime; // 传输开始时间

    // 握手状态管理
    std::atomic<bool> m_handshakeCompleted;    // 握手完成标志
    std::atomic<uint16_t> m_handshakeSequence; // 当前握手帧的序列号
    std::atomic<uint16_t> m_sessionId;         // 当前会话ID
    mutable std::mutex m_handshakeMutex;       // 握手状态锁
    std::condition_variable m_handshakeCondition; // 握手完成条件变量

    // 性能相关
    uint32_t m_rttMs;                                     // 往返时延
    uint32_t m_timeoutMs;                                 // 当前超时时间
    std::chrono::steady_clock::time_point m_lastActivity; // 最后活动时间

    // 帧编解码器
    std::unique_ptr<FrameCodec> m_frameCodec; // 帧编解码器

    // 回调函数
    std::function<void(const std::vector<uint8_t> &)> m_dataReceivedCallback;
    std::function<void(bool)> m_stateChangedCallback;
    std::function<void(const std::string &)> m_errorCallback;
    std::function<void(int64_t, int64_t)> m_progressCallback;
};