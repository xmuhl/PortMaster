// ButtonStateManager.h - 按钮状态管理器
#pragma once
#pragma execution_character_set("utf-8")

#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>
#include <string>

// 按钮ID枚举
enum class ButtonID
{
    Connect,
    Disconnect,
    Send,
    Stop,
    File,
    ClearAll,
    ClearReceive,
    CopyAll,
    SaveAll,
    PauseResume,
    Unknown
};

// 按钮状态枚举
enum class ButtonState
{
    Enabled,
    Disabled,
    Visible,
    Hidden
};

// 按钮状态变化回调类型
using ButtonStateChangeCallback = std::function<void(ButtonID buttonId, ButtonState newState, const std::string& reason)>;

class ButtonStateManager
{
private:
    // 按钮状态映射表
    std::unordered_map<ButtonID, ButtonState> m_buttonStates;

    // 按钮ID到控件ID的映射
    std::unordered_map<ButtonID, int> m_buttonToControlMap;

    // 状态变化回调
    ButtonStateChangeCallback m_stateChangeCallback;

    // 线程安全
    mutable std::mutex m_mutex;

    // 内部方法
    bool IsValidButtonID(ButtonID buttonId) const;
    const char* GetButtonName(ButtonID buttonId) const;
    const char* GetStateName(ButtonState state) const;
    void NotifyStateChange(ButtonID buttonId, ButtonState newState, const std::string& reason);

public:
    ButtonStateManager();
    virtual ~ButtonStateManager();

    // 设置状态变化回调
    void SetStateChangeCallback(ButtonStateChangeCallback callback);

    // 设置按钮状态
    bool SetButtonState(ButtonID buttonId, ButtonState newState, const std::string& reason = "");

    // 获取按钮状态
    ButtonState GetButtonState(ButtonID buttonId) const;

    // 批量设置按钮状态
    void SetButtonStates(const std::unordered_map<ButtonID, ButtonState>& states, const std::string& reason = "");

    // 检查按钮是否可用
    bool IsButtonEnabled(ButtonID buttonId) const;

    // 检查按钮是否可见
    bool IsButtonVisible(ButtonID buttonId) const;

    // 传输场景下的按钮状态预设
    void ApplyIdleState();                    // 空闲状态
    void ApplyConnectingState();              // 连接中状态
    void ApplyConnectedState();               // 已连接状态
    void ApplyTransmittingState();            // 传输中状态
    void ApplyPausedState();                  // 暂停状态
    void ApplyCompletedState();               // 完成状态
    void ApplyErrorState();                   // 错误状态

    // 可靠传输模式下的按钮状态预设
    void ApplyReliableModeIdleState();        // 可靠模式空闲状态
    void ApplyReliableModeTransmittingState(); // 可靠模式传输中状态
    void ApplyReliableModePausedState();      // 可靠模式暂停状态
    void ApplyReliableModeCompletedState();   // 可靠模式完成状态

    // 调试方法
    void DumpButtonStates() const;

    // 重置所有按钮到默认状态
    void ResetToDefault();

    // 清除回调
    void ClearCallbacks();
};

// 全局按钮状态管理器实例
extern ButtonStateManager* g_buttonStateManager;

// 便捷函数
inline bool IsButtonEnabled(ButtonID buttonId)
{
    return g_buttonStateManager ? g_buttonStateManager->IsButtonEnabled(buttonId) : false;
}

inline void SetButtonState(ButtonID buttonId, ButtonState newState, const std::string& reason = "")
{
    if (g_buttonStateManager) {
        g_buttonStateManager->SetButtonState(buttonId, newState, reason);
    }
}