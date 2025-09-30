#pragma once
#pragma execution_character_set("utf-8")

// TransmissionStateManager.h - 传输状态管理器
// 用于统一UI层、任务层、协议层状态同步

#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

// 传输状态枚举（扩展现有状态）
enum class TransmissionUIState
{
    Idle,          // 空闲
    Connecting,    // 连接中
    Connected,     // 已连接
    Initializing,  // 初始化中
    Handshaking,   // 握手中
    Transmitting,   // 传输中
    Paused,        // 已暂停
    Completing,    // 完成中
    Completed,     // 已完成
    Failed,        // 失败
    Error          // 错误
};

// 状态变化回调类型
using StateChangeCallback = std::function<void(TransmissionUIState oldState, TransmissionUIState newState)>;

class TransmissionStateManager
{
private:
    // 当前状态
    std::atomic<TransmissionUIState> m_currentState;

    // 上次状态变化的时间戳
    std::chrono::steady_clock::time_point m_lastStateChange;

    // 状态变化回调函数
    StateChangeCallback m_stateChangeCallback;

    // 线程安全
    mutable std::mutex m_mutex;

    // 状态变化计数器（用于调试）
    uint64_t m_stateChangeCount;

    // 内部状态转换验证
    bool IsValidStateTransition(TransmissionUIState from, TransmissionUIState to) const;

    // 记录状态变化
    void LogStateChange(TransmissionUIState oldState, TransmissionUIState newState, const std::string& reason);

public:
    TransmissionStateManager();
    virtual ~TransmissionStateManager();

    // 设置状态变化回调
    void SetStateChangeCallback(StateChangeCallback callback);

    // 获取当前状态
    TransmissionUIState GetCurrentState() const;

    // 请求状态转换
    bool RequestStateTransition(TransmissionUIState newState, const std::string& reason = "");

    // 强制设置状态（仅用于紧急情况）
    void ForceState(TransmissionUIState newState, const std::string& reason = "");

    // 检查是否处于活跃传输状态
    bool IsTransmitting() const;

    // 检查是否处于错误状态
    bool IsErrorState() const;

    // 检查是否可以开始新的传输
    bool CanStartNewTransmission() const;

    // 检查是否可以暂停传输
    bool CanPauseTransmission() const;

    // 检查是否可以恢复传输
    bool CanResumeTransmission() const;

    // 检查是否可以取消传输
    bool CanCancelTransmission() const;

    // 获取状态描述文本
    std::string GetStateDescription(TransmissionUIState state = TransmissionUIState::Idle) const;

    // 获取当前状态的持续时间
    std::chrono::milliseconds GetStateDuration() const;

    // 获取状态变化统计
    uint64_t GetStateChangeCount() const;

    // 重置状态到初始状态
    void Reset();

    // 清除所有回调
    void ClearCallbacks();

    // 调试方法
    void DumpStateHistory() const;
};

// 全局传输状态管理器实例
extern TransmissionStateManager* g_transmissionStateManager;

// 便捷函数
inline bool IsTransmitting()
{
    return g_transmissionStateManager ? g_transmissionStateManager->IsTransmitting() : false;
}

inline bool IsErrorState()
{
    return g_transmissionStateManager ? g_transmissionStateManager->IsErrorState() : false;
}

inline bool CanStartNewTransmission()
{
    return g_transmissionStateManager ? g_transmissionStateManager->CanStartNewTransmission() : false;
}