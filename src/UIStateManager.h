#pragma once
#pragma execution_character_set("utf-8")

// UIStateManager.h - UI状态管理器
// 用于解决UI控件响应问题，特别是状态栏重复显示问题

#include <string>
#include <mutex>
#include <chrono>

class CStatic; // 前向声明

class UIStateManager
{
public:
    // 状态类型枚举
    enum class StatusType
    {
        Connection,      // 连接状态
        Transmission,   // 传输状态
        Progress,        // 进度状态
        Error           // 错误状态
    };

    // 优先级枚举（数值越高优先级越高）
    enum class Priority
    {
        Low = 1,
        Normal = 2,
        High = 3,
        Critical = 4
    };

private:
    // 状态信息结构
    struct StatusInfo
    {
        std::string text;
        StatusType type;
        Priority priority;
        std::chrono::steady_clock::time_point timestamp;

        StatusInfo() : type(StatusType::Connection), priority(Priority::Normal) {}
        StatusInfo(const std::string& txt, StatusType t, Priority p)
            : text(txt), type(t), priority(p), timestamp(std::chrono::steady_clock::now()) {}
    };

    // 不同类型的状态信息
    StatusInfo m_connectionStatus;
    StatusInfo m_transmissionStatus;
    StatusInfo m_progressStatus;
    StatusInfo m_errorStatus;

    // 上次显示的状态
    std::string m_lastDisplayedText;
    StatusType m_lastDisplayedType;
    Priority m_lastDisplayedPriority;

    // 线程安全
    mutable std::mutex m_mutex;

    // 是否强制更新
    bool m_forceUpdate;

    // 获取当前应该显示的状态
    StatusInfo GetCurrentStatus() const;

    // 检查状态是否应该更新
    bool ShouldUpdate(const StatusInfo& newStatus) const;

public:
    UIStateManager();
    virtual ~UIStateManager();

    // 更新连接状态
    void UpdateConnectionStatus(const std::string& status, Priority priority = Priority::Normal);

    // 更新传输状态
    void UpdateTransmissionStatus(const std::string& status, Priority priority = Priority::Normal);

    // 更新进度状态
    void UpdateProgressStatus(const std::string& status, Priority priority = Priority::Normal);

    // 更新错误状态
    void UpdateErrorStatus(const std::string& status, Priority priority = Priority::High);

    // 应用状态到UI控件
    bool ApplyStatusToControl(CStatic* pStaticControl);

    // 强制下次更新
    void ForceNextUpdate();

    // 清除特定类型的状态
    void ClearStatus(StatusType type);

    // 清除所有状态
    void ClearAllStatus();

    // 获取当前状态文本
    std::string GetCurrentStatusText() const;

    // 检查是否有待更新的状态
    bool HasPendingUpdate() const;
};

// 全局状态管理器实例
extern UIStateManager* g_uiStateManager;

// 便捷函数
inline void UpdateConnectionStatus(const std::string& status, UIStateManager::Priority priority = UIStateManager::Priority::Normal)
{
    if (g_uiStateManager) {
        g_uiStateManager->UpdateConnectionStatus(status, priority);
    }
}

inline void UpdateTransmissionStatus(const std::string& status, UIStateManager::Priority priority = UIStateManager::Priority::Normal)
{
    if (g_uiStateManager) {
        g_uiStateManager->UpdateTransmissionStatus(status, priority);
    }
}

inline void UpdateErrorStatus(const std::string& status, UIStateManager::Priority priority = UIStateManager::Priority::High)
{
    if (g_uiStateManager) {
        g_uiStateManager->UpdateErrorStatus(status, priority);
    }
}