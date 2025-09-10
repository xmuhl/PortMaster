#pragma once

#include "FrameCodec.h"
#include "../Transport/ITransport.h"
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>

// 可靠传输状态
enum ReliableState
{
    RELIABLE_IDLE,           // 空闲
    RELIABLE_STARTING,       // 发送端：发送START帧
    RELIABLE_SENDING,        // 发送端：发送数据帧
    RELIABLE_ENDING,         // 发送端：发送END帧
    RELIABLE_READY,          // 接收端：准备接收
    RELIABLE_RECEIVING,      // 接收端：接收数据
    RELIABLE_DONE,           // 完成
    RELIABLE_FAILED          // 失败
};

// 传输统计信息
struct TransferStats
{
    size_t totalBytes;      // 总字节数
    size_t transferredBytes; // 已传输字节数
    size_t totalFrames;     // 总帧数
    size_t sentFrames;      // 已发送帧数
    size_t retransmissions; // 重传次数
    size_t crcErrors;       // CRC错误次数
    size_t timeouts;        // 超时次数
    
    // ⭐ 新增：滑动窗口和协议参数统计
    uint8_t window_size;    // 滑动窗口大小
    uint32_t ack_timeout_ms; // ACK超时时间(毫秒)
    uint32_t throughput_bps; // 当前吞吐量(字节/秒)
    size_t frames_sent;     // 已发送帧数(与sentFrames相同，保持兼容性)
    size_t frames_received; // 已接收帧数
    
    TransferStats() 
        : totalBytes(0), transferredBytes(0), totalFrames(0)
        , sentFrames(0), retransmissions(0), crcErrors(0), timeouts(0)
        , window_size(8), ack_timeout_ms(3000), throughput_bps(0)
        , frames_sent(0), frames_received(0) {}
    
    double GetProgress() const
    {
        return totalBytes > 0 ? static_cast<double>(transferredBytes) / totalBytes : 0.0;
    }
};

// 回调函数类型
typedef std::function<void(const TransferStats&)> ProgressCallback;
typedef std::function<void(bool, const std::string&)> CompletionCallback;
typedef std::function<void(const std::string&, const std::vector<uint8_t>&)> FileReceivedCallback;
typedef std::function<void(const std::vector<uint8_t>&, size_t, size_t)> ChunkReceivedCallback;

// 可靠传输通道
class ReliableChannel
{
public:
    ReliableChannel(std::shared_ptr<ITransport> transport);
    ~ReliableChannel();

    // 基本操作
    bool Start();
    void Stop();
    bool IsActive() const;
    
    // 发送操作
    bool SendData(const std::vector<uint8_t>& data);
    bool SendFile(const std::string& filePath);
    bool SendFile(const std::string& filename, const std::vector<uint8_t>& fileData);
    
    // 接收操作
    void EnableReceiving(bool enable);
    bool IsReceivingEnabled() const;
    
    // 状态查询
    ReliableState GetState() const;
    TransferStats GetStats() const;
    std::string GetLastError() const;
    
    // 配置
    void SetAckTimeout(int timeoutMs) { m_ackTimeoutMs = timeoutMs; }
    void SetMaxRetries(int maxRetries) { m_maxRetries = maxRetries; }
    void SetMaxPayloadSize(size_t size) { m_frameCodec.SetMaxPayloadSize(size); }
    void SetReceiveDirectory(const std::string& directory) { m_receiveDirectory = directory; }
    void SetWindowSize(uint8_t size) { m_windowSize = (size > 0 && size <= 255) ? size : 8; }
    
    int GetAckTimeout() const { return m_ackTimeoutMs; }
    int GetMaxRetries() const { return m_maxRetries; }
    size_t GetMaxPayloadSize() const { return m_frameCodec.GetMaxPayloadSize(); }
    std::string GetReceiveDirectory() const { return m_receiveDirectory; }
    uint8_t GetWindowSize() const { return m_windowSize; }
    
    // 回调设置
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    void SetCompletionCallback(CompletionCallback callback) { m_completionCallback = callback; }
    void SetFileReceivedCallback(FileReceivedCallback callback) { m_fileReceivedCallback = callback; }
    void SetChunkReceivedCallback(ChunkReceivedCallback callback) { m_chunkReceivedCallback = callback; }
    
    // 重新配置传输层回调（用于模式切换时恢复正确的回调）
    void ReconfigureTransportCallback();

private:
    std::shared_ptr<ITransport> m_transport;
    FrameCodec m_frameCodec;
    
    // 状态变量
    std::atomic<ReliableState> m_state;
    std::atomic<bool> m_active;
    std::atomic<bool> m_receivingEnabled;
    std::string m_lastError;
    mutable std::mutex m_mutex;
    
    // 发送端状态
    std::vector<uint8_t> m_sendData;
    std::string m_sendFilename;
    size_t m_sendOffset;
    uint16_t m_sendSequence;
    int m_retryCount;
    std::chrono::steady_clock::time_point m_lastSendTime;
    std::atomic<uint16_t> m_lastAckedSequence;
    
    // 滑动窗口发送端
    struct SendWindowEntry {
        uint16_t sequence;
        Frame frame;
        std::chrono::steady_clock::time_point sendTime;
        int retryCount;
        
        SendWindowEntry(uint16_t seq, const Frame& f) 
            : sequence(seq), frame(f), sendTime(std::chrono::steady_clock::now()), retryCount(0) {}
    };
    
    uint8_t m_windowSize = 8;                           // 窗口大小（从START帧协商）
    uint16_t m_sendBase = 1;                            // 发送窗口基序号
    uint16_t m_nextSequence = 1;                        // 下一个发送序号
    std::map<uint16_t, SendWindowEntry> m_sendWindow;   // 发送窗口：seq -> entry
    std::mutex m_sendWindowMutex;                       // 发送窗口保护锁
    
    // 接收端状态
    std::vector<uint8_t> m_receiveBuffer;
    std::vector<uint8_t> m_receivedData;
    std::string m_receivedFilename;
    uint16_t m_expectedSequence;
    StartMetadata m_receiveMetadata;
    
    // 滑动窗口接收端
    struct ReceiveWindowEntry {
        uint16_t sequence;
        std::vector<uint8_t> data;
        bool isData;  // true=DATA帧, false=END帧
        
        ReceiveWindowEntry(uint16_t seq, const std::vector<uint8_t>& d, bool data) 
            : sequence(seq), data(d), isData(data) {}
    };
    
    std::map<uint16_t, ReceiveWindowEntry> m_receiveWindow;  // 乱序接收缓存：seq -> entry
    uint16_t m_receiveBase = 2;                               // 接收窗口基序号（START=1后从2开始）
    std::mutex m_receiveWindowMutex;                          // 接收窗口保护锁
    
    // 统计信息
    TransferStats m_stats;
    
    // 配置参数
    int m_ackTimeoutMs = 200;   // ACK超时时间（适合本地回环）
    int m_maxRetries = 3;       // 最大重试次数
    std::string m_receiveDirectory; // 接收目录
    
    // 工作线程
    std::thread m_protocolThread;
    std::atomic<bool> m_stopThread;
    std::condition_variable m_protocolCV;
    
    // 回调函数
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    FileReceivedCallback m_fileReceivedCallback;
    ChunkReceivedCallback m_chunkReceivedCallback;
    
    // 内部方法
    void ProtocolThreadFunc();
    void OnDataReceived(const std::vector<uint8_t>& data);
    void ProcessReceivedData();
    void ProcessReceivedFrame(const Frame& frame);
    
    // 发送端状态机
    void HandleSending();
    void SendStartFrame();
    void SendDataFrame();
    void SendEndFrame();
    bool WaitForAck(uint16_t sequence);
    
    // 滑动窗口发送端管理
    bool CanSendNewFrame() const;                     // 检查窗口是否允许发送新帧
    bool SendFrameInWindow(const Frame& frame);       // 在窗口中发送帧
    void HandleWindowAck(uint16_t sequence);          // 处理窗口ACK（累积确认）
    void HandleWindowNak(uint16_t sequence);          // 处理窗口NAK（快速重传）
    void CheckWindowTimeouts();                       // 检查窗口超时并重传
    void AdvanceWindow();                            // 滑动发送窗口
    
    // 接收端状态机
    void HandleReceiving();
    void SendAck(uint16_t sequence);
    void SendNak(uint16_t sequence);
    void CompleteReceive();
    void AbortReceive(const std::string& reason);
    
    // 滑动窗口接收端管理
    void HandleWindowDataFrame(const Frame& frame);   // 处理窗口内的DATA帧
    void HandleWindowEndFrame(const Frame& frame);    // 处理窗口内的END帧
    void DeliverOrderedFrames();                     // 按序提交已收到的帧
    bool IsFrameInWindow(uint16_t sequence) const;   // 检查帧是否在接收窗口内
    
    // 辅助方法
    void SetState(ReliableState state);
    void SetError(const std::string& error);
    void UpdateProgress();
    void NotifyCompletion(bool success, const std::string& message);
    std::string GenerateUniqueFilename(const std::string& originalName);
    bool SaveReceivedFile();
    
    // 发送原始帧
    bool SendFrame(const Frame& frame);
};