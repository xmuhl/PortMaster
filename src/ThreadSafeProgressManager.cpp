// ThreadSafeProgressManager.cpp - 线程安全进度管理器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ThreadSafeProgressManager.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// 全局进度管理器实例
ThreadSafeProgressManager* g_threadSafeProgressManager = nullptr;

ThreadSafeProgressManager::ThreadSafeProgressManager()
    : m_currentProgress(0)
    , m_totalProgress(0)
    , m_percentageProgress(0)
    , m_lastUpdate(std::chrono::steady_clock::now())
    , m_minUpdateInterval(std::chrono::milliseconds(50)) // 默认50ms最小更新间隔
    , m_updateCount(0)
    , m_callbackCount(0)
    , m_startTime(std::chrono::steady_clock::now())
{
}

ThreadSafeProgressManager::~ThreadSafeProgressManager()
{
    // 清理全局指针
    if (g_threadSafeProgressManager == this) {
        g_threadSafeProgressManager = nullptr;
    }
}

void ThreadSafeProgressManager::SetProgress(uint64_t current, uint64_t total, const std::string& statusText)
{
    if (!ShouldUpdate()) {
        return;
    }

    // 更新进度值
    m_currentProgress.store(current);
    m_totalProgress.store(total);

    // 计算百分比
    int percentage = (total > 0) ? static_cast<int>((current * 100) / total) : 0;
    m_percentageProgress.store(percentage);

    // 更新状态文本
    if (!statusText.empty()) {
        std::lock_guard<std::mutex> lock(m_statusTextMutex);
        m_statusText = statusText;
    }

    // 更新时间戳
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_lastUpdate = std::chrono::steady_clock::now();
    }

    // 增加更新计数
    m_updateCount.fetch_add(1);

    // 创建进度信息
    ProgressInfo progressInfo(current, total, GetStatusText());

    // 添加到历史记录
    AddToHistory(progressInfo);

    // 触发回调
    TriggerCallback(progressInfo);
}

void ThreadSafeProgressManager::SetCurrentProgress(uint64_t current)
{
    uint64_t total = m_totalProgress.load();
    SetProgress(current, total);
}

void ThreadSafeProgressManager::SetTotalProgress(uint64_t total)
{
    uint64_t current = m_currentProgress.load();
    SetProgress(current, total);
}

void ThreadSafeProgressManager::SetPercentageProgress(int percentage)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    if (!ShouldUpdate()) {
        return;
    }

    // 根据百分比计算当前进度
    uint64_t total = m_totalProgress.load();
    uint64_t current = (total > 0) ? (percentage * total) / 100 : 0;

    SetProgress(current, total);
}

void ThreadSafeProgressManager::SetStatusText(const std::string& statusText)
{
    std::lock_guard<std::mutex> lock(m_statusTextMutex);
    m_statusText = statusText;

    // 创建进度信息并触发回调
    ProgressInfo progressInfo(m_currentProgress.load(), m_totalProgress.load(), statusText);
    TriggerCallback(progressInfo);
}

uint64_t ThreadSafeProgressManager::GetCurrentProgress() const
{
    return m_currentProgress.load();
}

uint64_t ThreadSafeProgressManager::GetTotalProgress() const
{
    return m_totalProgress.load();
}

int ThreadSafeProgressManager::GetPercentageProgress() const
{
    return m_percentageProgress.load();
}

std::string ThreadSafeProgressManager::GetStatusText() const
{
    std::lock_guard<std::mutex> lock(m_statusTextMutex);
    return m_statusText;
}

ProgressInfo ThreadSafeProgressManager::GetProgressInfo() const
{
    return ProgressInfo(m_currentProgress.load(), m_totalProgress.load(), GetStatusText());
}

void ThreadSafeProgressManager::IncrementProgress(uint64_t increment, const std::string& statusText)
{
    uint64_t current = m_currentProgress.load() + increment;
    uint64_t total = m_totalProgress.load();
    SetProgress(current, total, statusText);
}

void ThreadSafeProgressManager::ResetProgress(const std::string& statusText)
{
    m_currentProgress.store(0);
    m_totalProgress.store(0);
    m_percentageProgress.store(0);
    m_updateCount.store(0);
    m_callbackCount.store(0);

    {
        std::lock_guard<std::mutex> lock(m_statusTextMutex);
        m_statusText = statusText;
    }

    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_lastUpdate = std::chrono::steady_clock::now();
        m_startTime = std::chrono::steady_clock::now();
    }

    // 清空历史记录
    ClearProgressHistory();

    // 触发回调
    ProgressInfo progressInfo(0, 0, statusText);
    TriggerCallback(progressInfo);
}

void ThreadSafeProgressManager::SetComplete(const std::string& completionText)
{
    uint64_t total = m_totalProgress.load();
    SetProgress(total, total, completionText);
}

void ThreadSafeProgressManager::SetProgressCallback(ProgressChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_progressCallback = callback;
}

void ThreadSafeProgressManager::ClearProgressCallback()
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_progressCallback = nullptr;
}

void ThreadSafeProgressManager::SetMinUpdateInterval(std::chrono::milliseconds interval)
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    m_minUpdateInterval = interval;
}

std::chrono::milliseconds ThreadSafeProgressManager::GetMinUpdateInterval() const
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    return m_minUpdateInterval;
}

bool ThreadSafeProgressManager::IsComplete() const
{
    uint64_t current = m_currentProgress.load();
    uint64_t total = m_totalProgress.load();
    return (total > 0) && (current >= total);
}

bool ThreadSafeProgressManager::IsInProgress() const
{
    uint64_t current = m_currentProgress.load();
    uint64_t total = m_totalProgress.load();
    return (total > 0) && (current > 0) && (current < total);
}

bool ThreadSafeProgressManager::IsValidProgress() const
{
    uint64_t current = m_currentProgress.load();
    uint64_t total = m_totalProgress.load();
    return current <= total;
}

uint64_t ThreadSafeProgressManager::GetUpdateCount() const
{
    return m_updateCount.load();
}

uint64_t ThreadSafeProgressManager::GetCallbackCount() const
{
    return m_callbackCount.load();
}

std::chrono::milliseconds ThreadSafeProgressManager::GetElapsedTime() const
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
}

double ThreadSafeProgressManager::GetProgressRate() const
{
    uint64_t current = m_currentProgress.load();
    auto elapsedMs = GetElapsedTime().count();

    if (elapsedMs <= 0) {
        return 0.0;
    }

    return (static_cast<double>(current) * 1000.0) / static_cast<double>(elapsedMs); // 每秒进度
}

std::vector<ProgressInfo> ThreadSafeProgressManager::GetProgressHistory() const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return m_progressHistory;
}

void ThreadSafeProgressManager::ClearProgressHistory()
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_progressHistory.clear();
}

void ThreadSafeProgressManager::DumpProgressInfo() const
{
    std::cout << "=== 进度信息 ===" << std::endl;
    std::cout << "当前进度: " << m_currentProgress.load() << " / " << m_totalProgress.load() << std::endl;
    std::cout << "百分比进度: " << m_percentageProgress.load() << "%" << std::endl;
    std::cout << "状态文本: " << GetStatusText() << std::endl;
    std::cout << "是否完成: " << (IsComplete() ? "是" : "否") << std::endl;
    std::cout << "是否进行中: " << (IsInProgress() ? "是" : "否") << std::endl;
    std::cout << "进度速率: " << std::fixed << std::setprecision(2) << GetProgressRate() << " /秒" << std::endl;
    std::cout << "===============" << std::endl;
}

void ThreadSafeProgressManager::DumpStatistics() const
{
    std::cout << "=== 进度管理器统计 ===" << std::endl;
    std::cout << "更新次数: " << m_updateCount.load() << std::endl;
    std::cout << "回调次数: " << m_callbackCount.load() << std::endl;
    std::cout << "运行时间: " << GetElapsedTime().count() << "ms" << std::endl;
    std::cout << "平均更新频率: " << std::fixed << std::setprecision(2);

    auto elapsedMs = GetElapsedTime().count();
    if (elapsedMs > 0) {
        std::cout << (static_cast<double>(m_updateCount.load()) * 1000.0 / elapsedMs) << " 次/秒";
    } else {
        std::cout << "N/A";
    }

    std::cout << std::endl;
    std::cout << "历史记录数量: " << m_progressHistory.size() << std::endl;
    std::cout << "====================" << std::endl;
}

void ThreadSafeProgressManager::UpdateProgressWithPercentage(int percentage, const std::string& statusText)
{
    SetPercentageProgress(percentage);
    if (!statusText.empty()) {
        SetStatusText(statusText);
    }
}

void ThreadSafeProgressManager::UpdateProgressWithRatio(double ratio, const std::string& statusText)
{
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    int percentage = static_cast<int>(ratio * 100.0);
    SetPercentageProgress(percentage);
    if (!statusText.empty()) {
        SetStatusText(statusText);
    }
}

// 私有方法实现

void ThreadSafeProgressManager::TriggerCallback(const ProgressInfo& progress)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_progressCallback) {
        try {
            m_progressCallback(progress);
            m_callbackCount.fetch_add(1);
        } catch (const std::exception& e) {
            std::cerr << "进度回调异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "进度回调未知异常" << std::endl;
        }
    }
}

bool ThreadSafeProgressManager::ShouldUpdate() const
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdate);
    return elapsed >= m_minUpdateInterval;
}

void ThreadSafeProgressManager::AddToHistory(const ProgressInfo& progress)
{
    std::lock_guard<std::mutex> lock(m_historyMutex);

    m_progressHistory.push_back(progress);

    // 限制历史记录大小
    if (m_progressHistory.size() > MAX_HISTORY_SIZE) {
        m_progressHistory.erase(m_progressHistory.begin(),
                              m_progressHistory.begin() + (m_progressHistory.size() - MAX_HISTORY_SIZE));
    }
}