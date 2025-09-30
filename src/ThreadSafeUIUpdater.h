// ThreadSafeUIUpdater.h - 线程安全UI更新器
#pragma once
#pragma execution_character_set("utf-8")

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

// UI更新操作类型
enum class UIUpdateType
{
    UpdateStatusText,        // 更新状态文本
    UpdateProgressBar,       // 更新进度条
    UpdateButtonState,       // 更新按钮状态
    UpdateEditText,          // 更新编辑框文本
    UpdateListView,          // 更新列表视图
    CustomUpdate             // 自定义更新
};

// UI更新操作结构
struct UIUpdateOperation
{
    UIUpdateType type;
    int controlId;                        // 控件ID（如适用）
    std::string text;                     // 文本内容（如适用）
    int numericValue;                     // 数值（如适用）
    std::function<void()> customFunction; // 自定义更新函数
    std::string reason;                   // 更新原因

    UIUpdateOperation()
        : type(UIUpdateType::CustomUpdate)
        , controlId(0)
        , numericValue(0)
    {}

    UIUpdateOperation(UIUpdateType t, int id, const std::string& txt, int val = 0, const std::string& r = "")
        : type(t), controlId(id), text(txt), numericValue(val), reason(r)
    {}

    UIUpdateOperation(std::function<void()> func, const std::string& r = "")
        : type(UIUpdateType::CustomUpdate), controlId(0), numericValue(0), customFunction(func), reason(r)
    {}
};

class ThreadSafeUIUpdater
{
private:
    // UI更新队列
    std::queue<UIUpdateOperation> m_updateQueue;

    // 线程同步
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::thread m_workerThread;
    std::atomic<bool> m_running;

    // UI控件映射（用于控件ID到实际控件的映射）
    std::unordered_map<int, void*> m_controlMap; // void* 指向实际的控件对象

    // 统计信息
    std::atomic<uint64_t> m_processedCount;
    std::atomic<uint64_t> m_queuedCount;
    std::atomic<uint64_t> m_droppedCount;

    // 内部方法
    void WorkerThread();
    void ProcessUpdateOperation(const UIUpdateOperation& operation);
    bool EnsureUIThread();

public:
    ThreadSafeUIUpdater();
    virtual ~ThreadSafeUIUpdater();

    // 启动和停止
    bool Start();
    void Stop();

    // 注册控件
    void RegisterControl(int controlId, void* control);
    void UnregisterControl(int controlId);

    // 添加UI更新操作
    bool QueueUpdate(UIUpdateType type, int controlId, const std::string& text, const std::string& reason = "");
    bool QueueUpdate(UIUpdateType type, int controlId, int numericValue, const std::string& reason = "");
    bool QueueUpdate(std::function<void()> customFunction, const std::string& reason = "");

    // 便捷方法
    bool QueueStatusUpdate(int controlId, const std::string& status, const std::string& reason = "");
    bool QueueProgressUpdate(int controlId, int progress, const std::string& reason = "");
    bool QueueButtonTextUpdate(int controlId, const std::string& text, const std::string& reason = "");
    bool QueueEditTextUpdate(int controlId, const std::string& text, const std::string& reason = "");

    // 批量更新
    bool QueueBatchUpdates(const std::vector<UIUpdateOperation>& operations);

    // 优先级更新（插队）
    bool QueuePriorityUpdate(UIUpdateType type, int controlId, const std::string& text, const std::string& reason = "");

    // 清空队列
    void ClearQueue();

    // 状态查询
    size_t GetQueueSize() const;
    bool IsRunning() const;

    // 统计信息
    uint64_t GetProcessedCount() const;
    uint64_t GetQueuedCount() const;
    uint64_t GetDroppedCount() const;

    // 等待队列处理完成
    bool WaitForCompletion(int timeoutMs = 5000);

    // 调试方法
    void DumpStatistics() const;

    // 设置最大队列大小（防止内存溢出）
    void SetMaxQueueSize(size_t maxSize);
    size_t GetMaxQueueSize() const;
};

// 全局UI更新器实例
extern ThreadSafeUIUpdater* g_threadSafeUIUpdater;

// 便捷函数
inline bool QueueUIUpdate(UIUpdateType type, int controlId, const std::string& text, const std::string& reason = "")
{
    return g_threadSafeUIUpdater ? g_threadSafeUIUpdater->QueueUpdate(type, controlId, text, reason) : false;
}

inline bool QueueStatusUpdate(int controlId, const std::string& status, const std::string& reason = "")
{
    return g_threadSafeUIUpdater ? g_threadSafeUIUpdater->QueueStatusUpdate(controlId, status, reason) : false;
}

inline bool QueueProgressUpdate(int controlId, int progress, const std::string& reason = "")
{
    return g_threadSafeUIUpdater ? g_threadSafeUIUpdater->QueueProgressUpdate(controlId, progress, reason) : false;
}