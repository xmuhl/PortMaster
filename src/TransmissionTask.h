#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include "../Protocol/ReliableChannel.h"
#include "../Transport/ITransport.h"

// 传输任务状态枚举
enum class TransmissionTaskState
{
    Ready,      // 准备就绪
    Running,    // 正在运行
    Paused,     // 已暂停
    Cancelled,  // 已取消
    Completed,  // 已完成
    Failed      // 失败
};

// 传输进度信息
struct TransmissionProgress
{
    size_t bytesTransmitted; // 已传输字节数
    size_t totalBytes;       // 总字节数
    int progressPercent;     // 进度百分比
    std::string statusText;  // 状态文本
    std::chrono::steady_clock::time_point timestamp; // 时间戳

    TransmissionProgress()
        : bytesTransmitted(0), totalBytes(0), progressPercent(0)
        , timestamp(std::chrono::steady_clock::now()) {}

    TransmissionProgress(size_t transmitted, size_t total, const std::string& status)
        : bytesTransmitted(transmitted), totalBytes(total), statusText(status)
        , timestamp(std::chrono::steady_clock::now())
    {
        progressPercent = total > 0 ? static_cast<int>((transmitted * 100) / total) : 0;
    }
};

// 传输任务结果
struct TransmissionResult
{
    TransmissionTaskState finalState;
    TransportError errorCode;
    size_t bytesTransmitted;
    std::string errorMessage;
    std::chrono::milliseconds duration;

    TransmissionResult()
        : finalState(TransmissionTaskState::Ready)
        , errorCode(TransportError::Success)
        , bytesTransmitted(0)
        , duration(0) {}
};

// 【P1修复】传输任务抽象类 - 实现UI与传输任务解耦
class TransmissionTask
{
public:
    // 进度回调函数类型
    using ProgressCallback = std::function<void(const TransmissionProgress&)>;

    // 完成回调函数类型
    using CompletionCallback = std::function<void(const TransmissionResult&)>;

    // 日志回调函数类型
    using LogCallback = std::function<void(const std::string&)>;

public:
    TransmissionTask();
    virtual ~TransmissionTask();

    // 核心控制接口
    bool Start(const std::vector<uint8_t>& data);
    void Pause();
    void Resume();
    void Cancel();
    void Stop();

    // 状态查询接口
    TransmissionTaskState GetState() const;
    TransmissionProgress GetProgress() const;
    bool IsRunning() const;
    bool IsPaused() const;
    bool IsCompleted() const;
    bool IsCancelled() const;

    // 回调设置接口
    void SetProgressCallback(ProgressCallback callback);
    void SetCompletionCallback(CompletionCallback callback);
    void SetLogCallback(LogCallback callback);

    // 配置接口
    void SetChunkSize(size_t chunkSize);
    void SetRetrySettings(int maxRetries, int retryDelayMs);
    void SetProgressUpdateInterval(int intervalMs);

protected:
    // 抽象方法 - 由具体实现类重写
    virtual TransportError DoSendChunk(const uint8_t* data, size_t size) = 0;
    virtual bool IsTransportReady() const = 0;
    virtual std::string GetTransportDescription() const = 0;

private:
    // 后台线程主函数
    void ExecuteTransmission();

    // 内部辅助方法
    void UpdateProgress(size_t transmitted, size_t total, const std::string& status);
    void ReportCompletion(TransmissionTaskState finalState, TransportError errorCode, const std::string& errorMsg = "");
    void WriteLog(const std::string& message);
    bool CheckPauseAndCancel();

private:
    // 状态管理
    std::atomic<TransmissionTaskState> m_state;
    mutable std::mutex m_stateMutex;

    // 数据管理
    std::vector<uint8_t> m_data;
    size_t m_totalBytes;
    std::atomic<size_t> m_bytesTransmitted;

    // 线程管理
    std::unique_ptr<std::thread> m_workerThread;

    // 配置参数
    size_t m_chunkSize;
    int m_maxRetries;
    int m_retryDelayMs;
    int m_progressUpdateIntervalMs;

    // 回调函数
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    LogCallback m_logCallback;

    // 时间跟踪
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastProgressUpdate;
};

// 可靠传输任务实现类
class ReliableTransmissionTask : public TransmissionTask
{
public:
    explicit ReliableTransmissionTask(std::shared_ptr<ReliableChannel> reliableChannel);
    virtual ~ReliableTransmissionTask() = default;

protected:
    TransportError DoSendChunk(const uint8_t* data, size_t size) override;
    bool IsTransportReady() const override;
    std::string GetTransportDescription() const override;

private:
    std::shared_ptr<ReliableChannel> m_reliableChannel;
};

// 原始传输任务实现类
class RawTransmissionTask : public TransmissionTask
{
public:
    explicit RawTransmissionTask(std::shared_ptr<ITransport> transport);
    virtual ~RawTransmissionTask() = default;

protected:
    TransportError DoSendChunk(const uint8_t* data, size_t size) override;
    bool IsTransportReady() const override;
    std::string GetTransportDescription() const override;

private:
    std::shared_ptr<ITransport> m_transport;
};