// ThreadSafeProgressManager.h - 线程安全进度管理器
#pragma once
#pragma execution_character_set("utf-8")

#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <string>

// 进度信息结构
struct ProgressInfo
{
    uint64_t current;          // 当前进度值
    uint64_t total;            // 总进度值
    int percentage;            // 百分比进度 (0-100)
    std::string statusText;    // 状态文本
    std::chrono::steady_clock::time_point timestamp; // 时间戳

    ProgressInfo()
        : current(0), total(0), percentage(0)
        , timestamp(std::chrono::steady_clock::now())
    {}

    ProgressInfo(uint64_t curr, uint64_t tot, const std::string& text = "")
        : current(curr), total(tot)
        , percentage(total > 0 ? static_cast<int>((curr * 100) / total) : 0)
        , statusText(text)
        , timestamp(std::chrono::steady_clock::now())
    {}
};

// 进度变化回调类型
using ProgressChangeCallback = std::function<void(const ProgressInfo& progress)>;

class ThreadSafeProgressManager
{
private:
    // 进度信息
    std::atomic<uint64_t> m_currentProgress;
    std::atomic<uint64_t> m_totalProgress;
    std::atomic<int> m_percentageProgress;

    // 状态文本
    std::string m_statusText;
    mutable std::mutex m_statusTextMutex;

    // 回调函数
    ProgressChangeCallback m_progressCallback;
    mutable std::mutex m_callbackMutex;

    // 时间控制
    std::chrono::steady_clock::time_point m_lastUpdate;
    std::chrono::milliseconds m_minUpdateInterval;
    mutable std::mutex m_timingMutex;

    // 统计信息
    std::atomic<uint64_t> m_updateCount;
    std::atomic<uint64_t> m_callbackCount;
    std::chrono::steady_clock::time_point m_startTime;

    // 进度历史记录（用于调试）
    std::vector<ProgressInfo> m_progressHistory;
    mutable std::mutex m_historyMutex;
    static const size_t MAX_HISTORY_SIZE = 100;

    // 内部方法
    void TriggerCallback(const ProgressInfo& progress);
    bool ShouldUpdate() const;
    void AddToHistory(const ProgressInfo& progress);

public:
    ThreadSafeProgressManager();
    virtual ~ThreadSafeProgressManager();

    // 进度设置
    void SetProgress(uint64_t current, uint64_t total, const std::string& statusText = "");
    void SetCurrentProgress(uint64_t current);
    void SetTotalProgress(uint64_t total);
    void SetPercentageProgress(int percentage);
    void SetStatusText(const std::string& statusText);

    // 进度获取
    uint64_t GetCurrentProgress() const;
    uint64_t GetTotalProgress() const;
    int GetPercentageProgress() const;
    std::string GetStatusText() const;
    ProgressInfo GetProgressInfo() const;

    // 进度操作
    void IncrementProgress(uint64_t increment = 1, const std::string& statusText = "");
    void ResetProgress(const std::string& statusText = "");
    void SetComplete(const std::string& completionText = "完成");

    // 回调管理
    void SetProgressCallback(ProgressChangeCallback callback);
    void ClearProgressCallback();

    // 时间控制
    void SetMinUpdateInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds GetMinUpdateInterval() const;

    // 状态检查
    bool IsComplete() const;
    bool IsInProgress() const;
    bool IsValidProgress() const;

    // 统计信息
    uint64_t GetUpdateCount() const;
    uint64_t GetCallbackCount() const;
    std::chrono::milliseconds GetElapsedTime() const;
    double GetProgressRate() const; // 每秒进度

    // 历史记录
    std::vector<ProgressInfo> GetProgressHistory() const;
    void ClearProgressHistory();

    // 调试方法
    void DumpProgressInfo() const;
    void DumpStatistics() const;

    // 便捷方法
    void UpdateProgressWithPercentage(int percentage, const std::string& statusText = "");
    void UpdateProgressWithRatio(double ratio, const std::string& statusText = ""); // ratio: 0.0 - 1.0
};

// 全局进度管理器实例
extern ThreadSafeProgressManager* g_threadSafeProgressManager;

// 便捷函数
inline void SetProgress(uint64_t current, uint64_t total, const std::string& statusText = "")
{
    if (g_threadSafeProgressManager) {
        g_threadSafeProgressManager->SetProgress(current, total, statusText);
    }
}

inline void UpdateProgress(int percentage, const std::string& statusText = "")
{
    if (g_threadSafeProgressManager) {
        g_threadSafeProgressManager->SetPercentageProgress(percentage);
        if (!statusText.empty()) {
            g_threadSafeProgressManager->SetStatusText(statusText);
        }
    }
}

inline void IncrementProgress(const std::string& statusText = "")
{
    if (g_threadSafeProgressManager) {
        g_threadSafeProgressManager->IncrementProgress(1, statusText);
    }
}