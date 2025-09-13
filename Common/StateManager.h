#pragma execution_character_set("utf-8")
#pragma once

#include <memory>
#include <functional>
#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString

/**
 * @brief 应用程序状态枚举
 * SOLID-O: 开放封闭原则 - 易于扩展新的状态类型
 */
enum class ApplicationState
{
    INITIALIZING = 0,    // 初始化中
    READY,               // 就绪状态
    CONNECTING,          // 连接中
    CONNECTED,           // 已连接
    TRANSMITTING,        // 传输中
    PAUSED,              // 暂停状态
    DISCONNECTING,       // 断开连接中
    ERROR,               // 错误状态
    SHUTDOWN             // 关闭中
};

/**
 * @brief 状态优先级枚举
 */
enum class StatePriority
{
    LOW = 0,        // 低优先级
    NORMAL,         // 正常优先级
    HIGH,           // 高优先级
    CRITICAL        // 关键优先级
};

/**
 * @brief 状态信息结构
 * SOLID-S: 单一职责原则 - 专门管理状态信息
 */
struct StateInfo
{
    ApplicationState state;                  // 状态值
    std::string message;                     // 状态消息
    StatePriority priority;                  // 状态优先级
    std::chrono::steady_clock::time_point timestamp; // 状态更新时间
    std::string source;                      // 状态来源
    
    StateInfo()
        : state(ApplicationState::INITIALIZING)
        , priority(StatePriority::NORMAL)
        , timestamp(std::chrono::steady_clock::now())
    {
    }
    
    StateInfo(ApplicationState st, const std::string& msg = "", 
             StatePriority prio = StatePriority::NORMAL, const std::string& src = "")
        : state(st)
        , message(msg)
        , priority(prio)
        , timestamp(std::chrono::steady_clock::now())
        , source(src)
    {
    }
};

/**
 * @brief 状态变化回调接口
 * SOLID-I: 接口隔离原则 - 分离状态变化事件
 */
class IStateChangeCallback
{
public:
    virtual ~IStateChangeCallback() = default;
    
    /**
     * @brief 状态变化通知
     * @param oldState 旧状态信息
     * @param newState 新状态信息
     */
    virtual void OnStateChanged(const StateInfo& oldState, const StateInfo& newState) = 0;
    
    /**
     * @brief 状态更新通知
     * @param stateInfo 当前状态信息
     */
    virtual void OnStateUpdate(const StateInfo& stateInfo) = 0;
    
    /**
     * @brief 错误状态通知
     * @param errorState 错误状态信息
     */
    virtual void OnErrorState(const StateInfo& errorState) = 0;
};

/**
 * @brief UI状态更新接口
 * SOLID-I: 接口隔离原则 - 分离UI更新职责
 */
class IUIStateUpdater
{
public:
    virtual ~IUIStateUpdater() = default;
    
    /**
     * @brief 更新连接状态显示
     * @param connected 是否已连接
     * @param info 连接信息
     */
    virtual void UpdateConnectionStatus(bool connected, const std::string& info = "") = 0;
    
    /**
     * @brief 更新传输状态显示
     * @param state 传输状态
     * @param progress 传输进度（0-100）
     */
    virtual void UpdateTransmissionStatus(ApplicationState state, double progress = 0.0) = 0;
    
    /**
     * @brief 更新按钮状态
     * @param state 当前应用状态
     */
    virtual void UpdateButtonStates(ApplicationState state) = 0;
    
    /**
     * @brief 更新状态栏信息
     * @param message 状态消息
     * @param priority 消息优先级
     */
    virtual void UpdateStatusBar(const std::string& message, StatePriority priority = StatePriority::NORMAL) = 0;
    
    /**
     * @brief 显示错误消息
     * @param title 错误标题
     * @param message 错误消息
     */
    virtual void ShowErrorMessage(const std::string& title, const std::string& message) = 0;
};

/**
 * @brief 统一状态管理器
 * SOLID-S: 单一职责原则 - 专门负责应用状态管理
 * SOLID-D: 依赖倒置原则 - 依赖于状态回调接口抽象
 */
class StateManager
{
public:
    /**
     * @brief 构造函数
     */
    StateManager();
    
    /**
     * @brief 析构函数
     */
    ~StateManager() = default;

    // 禁用拷贝构造和拷贝赋值 - RAII原则
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
    
    /**
     * @brief 设置状态变化回调
     * @param callback 状态变化回调接口
     */
    void SetStateChangeCallback(std::shared_ptr<IStateChangeCallback> callback);
    
    /**
     * @brief 设置UI状态更新器
     * @param uiUpdater UI状态更新接口
     */
    void SetUIStateUpdater(std::shared_ptr<IUIStateUpdater> uiUpdater);
    
    /**
     * @brief 设置应用状态
     * @param state 新状态
     * @param message 状态消息
     * @param priority 状态优先级
     * @param source 状态来源
     */
    void SetApplicationState(ApplicationState state, const std::string& message = "",
                           StatePriority priority = StatePriority::NORMAL, 
                           const std::string& source = "");
    
    /**
     * @brief 获取当前应用状态
     * @return 当前状态信息
     */
    StateInfo GetCurrentState() const;
    
    /**
     * @brief 获取当前状态值
     * @return 当前状态枚举值
     */
    ApplicationState GetCurrentStateValue() const;
    
    /**
     * @brief 检查是否处于指定状态
     * @param state 要检查的状态
     * @return 是否处于该状态
     */
    bool IsInState(ApplicationState state) const;
    
    /**
     * @brief 检查是否允许状态转换
     * @param fromState 源状态
     * @param toState 目标状态
     * @return 是否允许转换
     */
    bool IsStateTransitionAllowed(ApplicationState fromState, ApplicationState toState) const;
    
    /**
     * @brief 更新状态消息（不改变状态值）
     * @param message 新的状态消息
     * @param priority 消息优先级
     * @param source 消息来源
     */
    void UpdateStateMessage(const std::string& message, 
                           StatePriority priority = StatePriority::NORMAL,
                           const std::string& source = "");
    
    /**
     * @brief 设置错误状态
     * @param errorMessage 错误消息
     * @param source 错误来源
     */
    void SetErrorState(const std::string& errorMessage, const std::string& source = "");
    
    /**
     * @brief 清除错误状态
     * @param newState 要切换到的新状态
     * @param message 状态消息
     */
    void ClearErrorState(ApplicationState newState = ApplicationState::READY, 
                        const std::string& message = "");
    
    /**
     * @brief 获取状态历史
     * @param maxCount 最大返回数量（0表示全部）
     * @return 状态历史列表
     */
    std::vector<StateInfo> GetStateHistory(size_t maxCount = 10) const;
    
    /**
     * @brief 获取状态持续时间
     * @return 当前状态持续时间（毫秒）
     */
    std::chrono::milliseconds GetStateDuration() const;
    
    /**
     * @brief 获取状态字符串描述
     * @param state 状态值
     * @return 状态描述字符串
     */
    static std::string GetStateString(ApplicationState state);
    
    /**
     * @brief 获取优先级字符串描述
     * @param priority 优先级值
     * @return 优先级描述字符串
     */
    static std::string GetPriorityString(StatePriority priority);
    
    /**
     * @brief 重置状态管理器
     */
    void Reset();
    
    /**
     * @brief 启用/禁用自动UI更新
     * @param enable 是否启用
     */
    void SetAutoUIUpdate(bool enable);

private:
    // 核心状态数据
    StateInfo m_currentState;                            // 当前状态
    std::vector<StateInfo> m_stateHistory;              // 状态历史
    const size_t MAX_HISTORY_SIZE = 100;                // 最大历史记录数
    
    // 回调接口
    std::shared_ptr<IStateChangeCallback> m_stateCallback;   // 状态变化回调
    std::shared_ptr<IUIStateUpdater> m_uiUpdater;            // UI更新器
    
    // 配置选项
    std::atomic<bool> m_autoUIUpdate;                    // 自动UI更新标志
    
    // 线程安全保护
    mutable std::mutex m_stateMutex;                     // 状态访问互斥锁
    mutable std::mutex m_historyMutex;                   // 历史访问互斥锁
    
    // 状态转换规则表
    std::map<ApplicationState, std::vector<ApplicationState>> m_transitionRules;
    
    // 内部方法
    void InitializeTransitionRules();                   // 初始化状态转换规则
    void AddToHistory(const StateInfo& stateInfo);      // 添加到历史记录
    void NotifyStateChange(const StateInfo& oldState, const StateInfo& newState); // 通知状态变化
    void UpdateUI(const StateInfo& stateInfo);          // 更新UI显示
    void TrimHistory();                                  // 修剪历史记录
};

/**
 * @brief 状态管理器工厂类
 * SOLID-O: 开放封闭原则 - 支持扩展不同的管理器类型
 */
class StateManagerFactory
{
public:
    /**
     * @brief 创建默认的状态管理器
     * @return 状态管理器实例
     */
    static std::unique_ptr<StateManager> CreateDefault();
    
    /**
     * @brief 创建带回调的状态管理器
     * @param stateCallback 状态变化回调
     * @param uiUpdater UI更新器
     * @return 状态管理器实例
     */
    static std::unique_ptr<StateManager> CreateWithCallbacks(
        std::shared_ptr<IStateChangeCallback> stateCallback,
        std::shared_ptr<IUIStateUpdater> uiUpdater);
};