// UIStateManager.cpp - UI状态管理器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "UIStateManager.h"
#include <algorithm>

// 全局状态管理器实例
UIStateManager* g_uiStateManager = nullptr;

UIStateManager::UIStateManager()
    : m_lastDisplayedType(StatusType::Connection)
    , m_lastDisplayedPriority(Priority::Normal)
    , m_forceUpdate(false)
{
    // 初始化所有状态
    m_connectionStatus = StatusInfo("未连接", StatusType::Connection, Priority::Normal);
    m_transmissionStatus = StatusInfo("", StatusType::Transmission, Priority::Normal);
    m_progressStatus = StatusInfo("", StatusType::Progress, Priority::Normal);
    m_errorStatus = StatusInfo("", StatusType::Error, Priority::Normal);
}

UIStateManager::~UIStateManager()
{
    // 清理全局指针
    if (g_uiStateManager == this) {
        g_uiStateManager = nullptr;
    }
}

UIStateManager::StatusInfo UIStateManager::GetCurrentStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 优先级：错误状态 > 传输状态 > 连接状态 > 进度状态
    // 如果有错误状态，优先显示错误
    if (!m_errorStatus.text.empty()) {
        return m_errorStatus;
    }

    // 如果有传输状态且优先级高于连接状态，显示传输状态
    if (!m_transmissionStatus.text.empty() &&
        m_transmissionStatus.priority >= m_connectionStatus.priority) {
        return m_transmissionStatus;
    }

    // 显示连接状态
    if (!m_connectionStatus.text.empty()) {
        return m_connectionStatus;
    }

    // 显示进度状态
    if (!m_progressStatus.text.empty()) {
        return m_progressStatus;
    }

    // 默认返回连接状态
    return m_connectionStatus;
}

bool UIStateManager::ShouldUpdate(const StatusInfo& newStatus) const
{
    // 强制更新时总是允许
    if (m_forceUpdate) {
        return true;
    }

    // 文本不同时需要更新
    if (m_lastDisplayedText != newStatus.text) {
        return true;
    }

    // 优先级不同时可能需要更新（更高优先级应该覆盖低优先级）
    if (newStatus.priority > m_lastDisplayedPriority) {
        return true;
    }

    // 类型不同时可能需要更新
    if (newStatus.type != m_lastDisplayedType) {
        return true;
    }

    return false;
}

void UIStateManager::UpdateConnectionStatus(const std::string& status, Priority priority)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionStatus = StatusInfo(status, StatusType::Connection, priority);
}

void UIStateManager::UpdateTransmissionStatus(const std::string& status, Priority priority)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_transmissionStatus = StatusInfo(status, StatusType::Transmission, priority);
}

void UIStateManager::UpdateProgressStatus(const std::string& status, Priority priority)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressStatus = StatusInfo(status, StatusType::Progress, priority);
}

void UIStateManager::UpdateErrorStatus(const std::string& status, Priority priority)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorStatus = StatusInfo(status, StatusType::Error, priority);
}

bool UIStateManager::ApplyStatusToControl(CStatic* pStaticControl)
{
    if (!pStaticControl) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    StatusInfo currentStatus = GetCurrentStatus();

    // 检查是否需要更新
    if (!ShouldUpdate(currentStatus)) {
        return false;
    }

    // 转换为CString
    CString statusText;
    if (!currentStatus.text.empty()) {
        statusText = CString(currentStatus.text.c_str());
    } else {
        statusText = _T("就绪");
    }

    // 更新UI控件
    try {
        pStaticControl->SetWindowText(statusText);

        // 记录更新状态
        m_lastDisplayedText = currentStatus.text;
        m_lastDisplayedType = currentStatus.type;
        m_lastDisplayedPriority = currentStatus.priority;
        m_forceUpdate = false;

        return true;
    }
    catch (...) {
        return false;
    }
}

void UIStateManager::ForceNextUpdate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_forceUpdate = true;
}

void UIStateManager::ClearStatus(StatusType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (type) {
    case StatusType::Connection:
        m_connectionStatus.text.clear();
        break;
    case StatusType::Transmission:
        m_transmissionStatus.text.clear();
        break;
    case StatusType::Progress:
        m_progressStatus.text.clear();
        break;
    case StatusType::Error:
        m_errorStatus.text.clear();
        break;
    }

    // 强制下次更新
    m_forceUpdate = true;
}

void UIStateManager::ClearAllStatus()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_connectionStatus.text.clear();
    m_transmissionStatus.text.clear();
    m_progressStatus.text.clear();
    m_errorStatus.text.clear();

    // 重置为默认连接状态
    m_connectionStatus = StatusInfo("未连接", StatusType::Connection, Priority::Normal);

    // 强制下次更新
    m_forceUpdate = true;
}

std::string UIStateManager::GetCurrentStatusText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StatusInfo currentStatus = GetCurrentStatus();
    return currentStatus.text.empty() ? "就绪" : currentStatus.text;
}

bool UIStateManager::HasPendingUpdate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StatusInfo currentStatus = GetCurrentStatus();
    return ShouldUpdate(currentStatus);
}