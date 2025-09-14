#pragma execution_character_set("utf-8")
#include "pch.h"
#include "StateManager.h"
#include <algorithm>

extern void WriteDebugLog(const char* message);

// StateManager 实现

StateManager::StateManager()
    : m_currentState(ApplicationState::INITIALIZING, "系统初始化中...", StatePriority::NORMAL, "StateManager")
    , m_autoUIUpdate(true)
{
    // 初始化状态转换规则
    InitializeTransitionRules();
    
    // 🔑 死锁修复：延迟历史记录添加，避免构造函数中加锁冲突
    // AddToHistory将在首次SetApplicationState调用时自动添加
    
    WriteDebugLog("[DEBUG] StateManager构造完成");
}

void StateManager::SetStateChangeCallback(std::shared_ptr<IStateChangeCallback> callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    m_stateCallback = callback;
    WriteDebugLog("[DEBUG] StateManager::SetStateChangeCallback: 状态变化回调已设置");
}

void StateManager::SetUIStateUpdater(std::shared_ptr<IUIStateUpdater> uiUpdater)
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    m_uiUpdater = uiUpdater;
    WriteDebugLog("[DEBUG] StateManager::SetUIStateUpdater: UI状态更新器已设置");
}

void StateManager::SetApplicationState(ApplicationState state, const std::string& message,
                                     StatePriority priority, const std::string& source)
{
    StateInfo oldState;
    StateInfo newState(state, message, priority, source);
    
    {
        std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
        
        // 🔑 死锁修复：首次调用时初始化历史记录
        static bool s_historyInitialized = false;
        if (!s_historyInitialized) {
            AddToHistory(m_currentState);  // 添加构造函数中的初始状态
            s_historyInitialized = true;
        }
        
        // 检查状态转换是否允许
        if (!IsStateTransitionAllowed(m_currentState.state, state)) {
            CString errorMsg;
            errorMsg.Format(L"[WARNING] StateManager: 不允许的状态转换 %s -> %s", 
                           CA2W(GetStateString(m_currentState.state).c_str()),
                           CA2W(GetStateString(state).c_str()));
            WriteDebugLog(CW2A(errorMsg));
            return;
        }
        
        oldState = m_currentState;
        m_currentState = newState;
        
        // 添加到历史记录
        AddToHistory(m_currentState);
    }
    
    // 通知状态变化
    NotifyStateChange(oldState, newState);
    
    // 更新UI
    if (m_autoUIUpdate.load()) {
        UpdateUI(newState);
    }
    
    CString logMsg;
    logMsg.Format(L"[DEBUG] StateManager状态变更: %s -> %s (%s)", 
                 CA2W(GetStateString(oldState.state).c_str()),
                 CA2W(GetStateString(newState.state).c_str()),
                 CA2W(message.c_str()));
    WriteDebugLog(CW2A(logMsg));
}

StateInfo StateManager::GetCurrentState() const
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    return m_currentState;
}

ApplicationState StateManager::GetCurrentStateValue() const
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    return m_currentState.state;
}

bool StateManager::IsInState(ApplicationState state) const
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    return m_currentState.state == state;
}

bool StateManager::IsStateTransitionAllowed(ApplicationState fromState, ApplicationState toState) const
{
    // 允许相同状态的"转换"（实际上是状态更新）
    if (fromState == toState) {
        return true;
    }
    
    // 错误状态可以转换到任何状态
    if (fromState == ApplicationState::APP_ERROR) {
        return true;
    }
    
    // 查找转换规则
    auto it = m_transitionRules.find(fromState);
    if (it != m_transitionRules.end()) {
        const auto& allowedStates = it->second;
        return std::find(allowedStates.begin(), allowedStates.end(), toState) != allowedStates.end();
    }
    
    // 如果没有定义规则，默认不允许
    return false;
}

void StateManager::UpdateStateMessage(const std::string& message, StatePriority priority, const std::string& source)
{
    StateInfo oldState;
    StateInfo newState;
    
    {
        std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
        oldState = m_currentState;
        
        // 只更新消息和优先级，保持状态不变
        m_currentState.message = message;
        m_currentState.priority = priority;
        m_currentState.source = source;
        m_currentState.timestamp = std::chrono::steady_clock::now();
        
        newState = m_currentState;
        
        // 添加到历史记录
        AddToHistory(m_currentState);
    }
    
    // 通知状态更新
    if (m_stateCallback) {
        m_stateCallback->OnStateUpdate(newState);
    }
    
    // 更新UI
    if (m_autoUIUpdate.load()) {
        UpdateUI(newState);
    }
}

void StateManager::SetErrorState(const std::string& errorMessage, const std::string& source)
{
    SetApplicationState(ApplicationState::APP_ERROR, errorMessage, StatePriority::CRITICAL, source);
    
    // 额外的错误通知
    if (m_stateCallback) {
        m_stateCallback->OnErrorState(m_currentState);
    }
}

void StateManager::ClearErrorState(ApplicationState newState, const std::string& message)
{
    if (IsInState(ApplicationState::APP_ERROR)) {
        SetApplicationState(newState, message, StatePriority::NORMAL, "ErrorRecovery");
    }
}

std::vector<StateInfo> StateManager::GetStateHistory(size_t maxCount) const
{
    std::lock_guard<std::recursive_mutex> lock(m_historyMutex);
    
    if (maxCount == 0 || maxCount >= m_stateHistory.size()) {
        return m_stateHistory;
    }
    
    // 返回最新的maxCount条记录
    std::vector<StateInfo> result;
    size_t startIndex = m_stateHistory.size() - maxCount;
    result.assign(m_stateHistory.begin() + startIndex, m_stateHistory.end());
    
    return result;
}

std::chrono::milliseconds StateManager::GetStateDuration() const
{
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_currentState.timestamp);
}

std::string StateManager::GetStateString(ApplicationState state)
{
    switch (state) {
    case ApplicationState::INITIALIZING: return "初始化中";
    case ApplicationState::READY: return "就绪";
    case ApplicationState::CONNECTING: return "连接中";
    case ApplicationState::CONNECTED: return "已连接";
    case ApplicationState::TRANSMITTING: return "传输中";
    case ApplicationState::PAUSED: return "已暂停";
    case ApplicationState::DISCONNECTING: return "断开连接中";
    case ApplicationState::APP_ERROR: return "错误";
    case ApplicationState::SHUTDOWN: return "关闭中";
    default: return "未知状态";
    }
}

std::string StateManager::GetPriorityString(StatePriority priority)
{
    switch (priority) {
    case StatePriority::LOW: return "低";
    case StatePriority::NORMAL: return "正常";
    case StatePriority::HIGH: return "高";
    case StatePriority::CRITICAL: return "关键";
    default: return "未知";
    }
}

void StateManager::Reset()
{
    {
        std::lock_guard<std::recursive_mutex> stateLock(m_stateMutex);
        std::lock_guard<std::recursive_mutex> historyLock(m_historyMutex);
        
        // 重置当前状态
        m_currentState = StateInfo(ApplicationState::INITIALIZING, "系统重置", StatePriority::NORMAL, "Reset");
        
        // 清空历史记录
        m_stateHistory.clear();
        AddToHistory(m_currentState);
    }
    
    WriteDebugLog("[DEBUG] StateManager::Reset: 状态管理器已重置");
}

void StateManager::SetAutoUIUpdate(bool enable)
{
    m_autoUIUpdate = enable;
    
    if (enable) {
        WriteDebugLog("[DEBUG] StateManager::SetAutoUIUpdate: 自动UI更新已启用");
    } else {
        WriteDebugLog("[DEBUG] StateManager::SetAutoUIUpdate: 自动UI更新已禁用");
    }
}

// 🔑 架构重构：统一状态显示更新 (从PortMasterDlg转移)
void StateManager::UpdateStatusDisplay(const std::string& connectionStatus,
                                     const std::string& protocolStatus,
                                     const std::string& transferStatus,
                                     const std::string& speedInfo,
                                     StatePriority priority)
{
    // 静态变量记录当前状态优先级，防止低优先级覆盖高优先级状态
    static StatePriority s_currentPriority = StatePriority::NORMAL;
    static std::chrono::steady_clock::time_point s_lastHighPriorityTime = std::chrono::steady_clock::now();
    
    try {
        // 🔑 死锁修复：使用递归互斥锁，支持同一线程重复加锁
        std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
        
        // 🔑 修复状态栏信息矛盾问题：完成状态始终优先显示
        bool isCompletionStatus = (!transferStatus.empty() && 
            (transferStatus.find("完成") != std::string::npos || 
             transferStatus.find("失败") != std::string::npos || 
             transferStatus.find("已连接") != std::string::npos));
             
        // 高优先级状态保持至少2秒钟，但完成状态可立即覆盖
        auto currentTime = std::chrono::steady_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - s_lastHighPriorityTime);
        
        if (s_currentPriority > StatePriority::NORMAL && 
            timeDiff.count() < 2000 && 
            priority < s_currentPriority && 
            !isCompletionStatus) { // 完成状态不受优先级阻塞限制
            WriteDebugLog("[DEBUG] StateManager::UpdateStatusDisplay: 跳过低优先级更新");
            return; // 跳过低优先级更新
        }
        
        // 更新优先级和时间戳
        if (priority > StatePriority::NORMAL) {
            s_lastHighPriorityTime = currentTime;
        }
        
        // 🔑 完成状态重置优先级阻塞，确保后续状态正常更新
        if (isCompletionStatus) {
            s_currentPriority = StatePriority::NORMAL;
        } else {
            s_currentPriority = priority;
        }
        
        // 通过UI更新器进行线程安全的UI更新
        if (m_uiUpdater && m_autoUIUpdate) {
            // 更新连接状态
            if (!connectionStatus.empty()) {
                bool connected = (connectionStatus.find("已连接") != std::string::npos);
                m_uiUpdater->UpdateConnectionStatus(connected, connectionStatus);
            }
            
            // 更新传输状态
            if (!transferStatus.empty()) {
                ApplicationState state = m_currentState.state;
                if (transferStatus.find("传输中") != std::string::npos) {
                    state = ApplicationState::TRANSMITTING;
                } else if (transferStatus.find("已连接") != std::string::npos) {
                    state = ApplicationState::CONNECTED;
                } else if (transferStatus.find("连接中") != std::string::npos) {
                    state = ApplicationState::CONNECTING;
                }
                m_uiUpdater->UpdateTransmissionStatus(state);
            }
            
            // 更新状态栏
            std::string statusMessage;
            if (!transferStatus.empty()) {
                statusMessage = transferStatus;
                if (!speedInfo.empty()) {
                    statusMessage += " - " + speedInfo;
                }
            } else if (!connectionStatus.empty()) {
                statusMessage = connectionStatus;
            } else if (!protocolStatus.empty()) {
                statusMessage = protocolStatus;
            }
            
            if (!statusMessage.empty()) {
                m_uiUpdater->UpdateStatusBar(statusMessage, priority);
            }
        }
        
        // 更新内部状态消息
        std::string combinedMessage;
        if (!transferStatus.empty()) {
            combinedMessage = transferStatus;
        } else if (!connectionStatus.empty()) {
            combinedMessage = connectionStatus;
        } else if (!protocolStatus.empty()) {
            combinedMessage = protocolStatus;
        }
        
        if (!combinedMessage.empty()) {
            UpdateStateMessage(combinedMessage, priority, "StatusDisplay");
        }
        
        WriteDebugLog("[DEBUG] StateManager::UpdateStatusDisplay: 状态显示更新完成");
    }
    catch (const std::exception& e) {
        WriteDebugLog(("[ERROR] StateManager::UpdateStatusDisplay: 更新异常 - " + std::string(e.what())).c_str());
    }
}

// 私有方法实现

void StateManager::InitializeTransitionRules()
{
    // 定义状态转换规则
    m_transitionRules[ApplicationState::INITIALIZING] = {
        ApplicationState::READY, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::READY] = {
        ApplicationState::CONNECTING, ApplicationState::APP_ERROR, ApplicationState::SHUTDOWN
    };
    
    m_transitionRules[ApplicationState::CONNECTING] = {
        ApplicationState::CONNECTED, ApplicationState::READY, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::CONNECTED] = {
        ApplicationState::TRANSMITTING, ApplicationState::DISCONNECTING, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::TRANSMITTING] = {
        ApplicationState::PAUSED, ApplicationState::CONNECTED, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::PAUSED] = {
        ApplicationState::TRANSMITTING, ApplicationState::CONNECTED, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::DISCONNECTING] = {
        ApplicationState::READY, ApplicationState::APP_ERROR
    };
    
    m_transitionRules[ApplicationState::APP_ERROR] = {
        ApplicationState::READY, ApplicationState::CONNECTING, ApplicationState::CONNECTED,
        ApplicationState::TRANSMITTING, ApplicationState::PAUSED, ApplicationState::DISCONNECTING,
        ApplicationState::SHUTDOWN  // 错误状态可以转换到任何状态
    };
    
    m_transitionRules[ApplicationState::SHUTDOWN] = {
        // 关闭状态不允许转换到其他状态
    };
}

void StateManager::AddToHistory(const StateInfo& stateInfo)
{
    // 注意：此方法假设调用者已持有适当的锁
    
    m_stateHistory.push_back(stateInfo);
    
    // 限制历史记录大小
    if (m_stateHistory.size() > MAX_HISTORY_SIZE) {
        TrimHistory();
    }
}

void StateManager::NotifyStateChange(const StateInfo& oldState, const StateInfo& newState)
{
    if (m_stateCallback) {
        try {
            m_stateCallback->OnStateChanged(oldState, newState);
        }
        catch (const std::exception& e) {
            CString errorMsg;
            errorMsg.Format(L"[ERROR] StateManager状态变化回调异常: %s", CA2W(e.what()));
            WriteDebugLog(CW2A(errorMsg));
        }
    }
}

void StateManager::UpdateUI(const StateInfo& stateInfo)
{
    if (!m_uiUpdater) {
        return;
    }
    
    try {
        // 更新连接状态
        bool connected = (stateInfo.state == ApplicationState::CONNECTED ||
                         stateInfo.state == ApplicationState::TRANSMITTING ||
                         stateInfo.state == ApplicationState::PAUSED);
        m_uiUpdater->UpdateConnectionStatus(connected, stateInfo.message);
        
        // 更新传输状态
        m_uiUpdater->UpdateTransmissionStatus(stateInfo.state);
        
        // 更新按钮状态
        m_uiUpdater->UpdateButtonStates(stateInfo.state);
        
        // 更新状态栏
        m_uiUpdater->UpdateStatusBar(stateInfo.message, stateInfo.priority);
        
        // 如果是错误状态，显示错误消息
        if (stateInfo.state == ApplicationState::APP_ERROR) {
            m_uiUpdater->ShowErrorMessage("状态错误", stateInfo.message);
        }
    }
    catch (const std::exception& e) {
        CString errorMsg;
        errorMsg.Format(L"[ERROR] StateManager UI更新异常: %s", CA2W(e.what()));
        WriteDebugLog(CW2A(errorMsg));
    }
}

void StateManager::TrimHistory()
{
    // 注意：此方法假设调用者已持有适当的锁
    
    if (m_stateHistory.size() <= MAX_HISTORY_SIZE) {
        return;
    }
    
    // 保留最新的MAX_HISTORY_SIZE/2条记录
    size_t keepCount = MAX_HISTORY_SIZE / 2;
    std::vector<StateInfo> trimmedHistory;
    trimmedHistory.assign(m_stateHistory.end() - keepCount, m_stateHistory.end());
    
    m_stateHistory = std::move(trimmedHistory);
    
    WriteDebugLog("[DEBUG] StateManager::TrimHistory: 历史记录已修剪");
}

// StateManagerFactory 实现

std::unique_ptr<StateManager> StateManagerFactory::CreateDefault()
{
    return std::make_unique<StateManager>();
}

std::unique_ptr<StateManager> StateManagerFactory::CreateWithCallbacks(
    std::shared_ptr<IStateChangeCallback> stateCallback,
    std::shared_ptr<IUIStateUpdater> uiUpdater)
{
    auto stateManager = std::make_unique<StateManager>();
    stateManager->SetStateChangeCallback(stateCallback);
    stateManager->SetUIStateUpdater(uiUpdater);
    return stateManager;
}