#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>

// 消息优先级定义
enum class MessagePriority
{
    LOW = 0,        // 低优先级：统计更新、日志记录
    NORMAL = 1,     // 普通优先级：数据传输、状态更新
    HIGH = 2,       // 高优先级：错误处理、连接状态变化
    CRITICAL = 3    // 关键优先级：系统错误、紧急状态
};

// 消息类型定义
enum class MessageType
{
    // 传输层消息
    TRANSPORT_DATA_RECEIVED,
    TRANSPORT_STATE_CHANGED,
    TRANSPORT_ERROR,
    
    // 协议层消息
    PROTOCOL_PROGRESS_UPDATE,
    PROTOCOL_COMPLETION,
    PROTOCOL_FILE_RECEIVED,
    PROTOCOL_CHUNK_RECEIVED,
    PROTOCOL_STATE_CHANGED,
    
    // UI更新消息
    UI_STATUS_UPDATE,
    UI_LOG_APPEND,
    UI_PROGRESS_UPDATE,
    
    // 系统消息
    SYSTEM_ERROR,
    SYSTEM_SHUTDOWN
};

// 异步消息基类
class AsyncMessage
{
public:
    AsyncMessage(MessageType type, MessagePriority priority = MessagePriority::NORMAL)
        : m_type(type)
        , m_priority(priority)
        , m_timestamp(std::chrono::steady_clock::now())
        , m_retryCount(0)
    {
    }
    
    virtual ~AsyncMessage() = default;
    
    MessageType GetType() const { return m_type; }
    MessagePriority GetPriority() const { return m_priority; }
    std::chrono::steady_clock::time_point GetTimestamp() const { return m_timestamp; }
    int GetRetryCount() const { return m_retryCount; }
    void IncrementRetryCount() { m_retryCount++; }
    
    // 纯虚函数，由具体消息类型实现
    virtual void Execute() = 0;
    virtual std::string GetDescription() const = 0;
    virtual bool CanRetry() const { return m_retryCount < 3; }

protected:
    MessageType m_type;
    MessagePriority m_priority;
    std::chrono::steady_clock::time_point m_timestamp;
    int m_retryCount;
};

// 消息优先级比较器
struct MessageComparator
{
    bool operator()(const std::shared_ptr<AsyncMessage>& a, const std::shared_ptr<AsyncMessage>& b) const
    {
        // 高优先级排在前面
        if (a->GetPriority() != b->GetPriority())
            return static_cast<int>(a->GetPriority()) < static_cast<int>(b->GetPriority());
        
        // 同优先级按时间戳排序
        return a->GetTimestamp() > b->GetTimestamp();
    }
};

// 异步消息管理器 - SOLID-S: 专门负责异步消息的调度和管理
class AsyncMessageManager
{
public:
    // 单例模式 - SOLID-S: 确保全局消息管理的一致性
    static AsyncMessageManager& GetInstance()
    {
        static AsyncMessageManager instance;
        return instance;
    }
    
    ~AsyncMessageManager();
    
    // 基本操作
    void Initialize(HWND mainWindowHandle = nullptr);
    void Shutdown();
    bool IsRunning() const;
    
    // 消息发送 - SOLID-O: 可扩展不同类型的消息
    void PostMessage(std::shared_ptr<AsyncMessage> message);
    template<typename MessageClass, typename... Args>
    void PostMessage(MessagePriority priority, Args&&... args);
    
    // 紧急消息处理（绕过队列直接执行）
    void PostUrgentMessage(std::shared_ptr<AsyncMessage> message);
    
    // 消息过滤和限流
    void SetMessageFilter(MessageType type, bool enabled);
    void SetRateLimit(MessageType type, int maxMessagesPerSecond);
    
    // 统计信息
    struct Statistics
    {
        size_t totalProcessed;
        size_t currentQueueSize;
        size_t failedMessages;
        size_t retriedMessages;
        std::unordered_map<MessageType, size_t> messageTypeCounts;
        
        Statistics() : totalProcessed(0), currentQueueSize(0), failedMessages(0), retriedMessages(0) {}
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();
    
    // 错误处理回调
    using ErrorHandler = std::function<void(const std::string& error, MessageType type)>;
    void SetErrorHandler(ErrorHandler handler);

private:
    AsyncMessageManager();
    AsyncMessageManager(const AsyncMessageManager&) = delete;
    AsyncMessageManager& operator=(const AsyncMessageManager&) = delete;
    
    // 工作线程主函数
    void WorkerThreadFunc();
    void ProcessMessage(std::shared_ptr<AsyncMessage> message);
    bool ShouldProcessMessage(const AsyncMessage& message);
    void HandleMessageError(std::shared_ptr<AsyncMessage> message, const std::string& error);
    
    // 限流控制
    bool CheckRateLimit(MessageType type);
    
    // 成员变量
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::priority_queue<std::shared_ptr<AsyncMessage>, 
                       std::vector<std::shared_ptr<AsyncMessage>>, 
                       MessageComparator> m_messageQueue;
    
    std::thread m_workerThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shutdown;
    
    // 消息过滤器
    std::unordered_map<MessageType, bool> m_messageFilters;
    
    // 限流控制
    struct RateLimiter
    {
        int maxPerSecond;
        std::chrono::steady_clock::time_point lastReset;
        int currentCount;
        
        RateLimiter() : maxPerSecond(0), lastReset(std::chrono::steady_clock::now()), currentCount(0) {}
    };
    std::unordered_map<MessageType, RateLimiter> m_rateLimiters;
    
    // 统计信息
    mutable std::mutex m_statsMutex;
    Statistics m_statistics;
    
    // 错误处理
    ErrorHandler m_errorHandler;
    
    // Windows消息集成
    HWND m_mainWindowHandle;
    static const UINT WM_ASYNC_MESSAGE = WM_USER + 100;
};

// 模板方法实现
template<typename MessageClass, typename... Args>
void AsyncMessageManager::PostMessage(MessagePriority priority, Args&&... args)
{
    static_assert(std::is_base_of<AsyncMessage, MessageClass>::value, 
                  "MessageClass must inherit from AsyncMessage");
    
    auto message = std::make_shared<MessageClass>(priority, std::forward<Args>(args)...);
    PostMessage(message);
}

// 具体消息类型示例 - 数据接收消息
class DataReceivedMessage : public AsyncMessage
{
public:
    DataReceivedMessage(MessagePriority priority, const std::vector<uint8_t>& data, 
                       std::function<void(const std::vector<uint8_t>&)> handler)
        : AsyncMessage(MessageType::TRANSPORT_DATA_RECEIVED, priority)
        , m_data(data)
        , m_handler(handler)
    {
    }
    
    void Execute() override
    {
        if (m_handler)
            m_handler(m_data);
    }
    
    std::string GetDescription() const override
    {
        return "Data received: " + std::to_string(m_data.size()) + " bytes";
    }

private:
    std::vector<uint8_t> m_data;
    std::function<void(const std::vector<uint8_t>&)> m_handler;
};

// 状态变化消息
class StateChangedMessage : public AsyncMessage
{
public:
    StateChangedMessage(MessagePriority priority, int state, const std::string& description,
                       std::function<void(int, const std::string&)> handler)
        : AsyncMessage(MessageType::TRANSPORT_STATE_CHANGED, priority)
        , m_state(state)
        , m_description(description)
        , m_handler(handler)
    {
    }
    
    void Execute() override
    {
        if (m_handler)
            m_handler(m_state, m_description);
    }
    
    std::string GetDescription() const override
    {
        return "State changed: " + m_description;
    }

private:
    int m_state;
    std::string m_description;
    std::function<void(int, const std::string&)> m_handler;
};

// UI更新消息
class UIUpdateMessage : public AsyncMessage
{
public:
    UIUpdateMessage(MessagePriority priority, std::function<void()> handler)
        : AsyncMessage(MessageType::UI_STATUS_UPDATE, priority)
        , m_handler(handler)
    {
    }
    
    void Execute() override
    {
        if (m_handler)
            m_handler();
    }
    
    std::string GetDescription() const override
    {
        return "UI Update";
    }
    
    bool CanRetry() const override
    {
        // UI更新消息不重试，避免界面闪烁
        return false;
    }

private:
    std::function<void()> m_handler;
};