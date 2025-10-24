// TransmissionStateManager.cpp - 传输状态管理器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "TransmissionStateManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>

// 全局传输状态管理器实例
TransmissionStateManager* g_transmissionStateManager = nullptr;

// 状态转换映射表（合法的状态转换）
static const bool s_validTransitions[11][11] = {
    // Idle, Connecting, Connected, Initializing, Handshaking, Transmitting, Paused, Completing, Completed, Failed, Error
    {true,  true,      true,     true,        false,         false,      false,      false,      false,   true,   true},   // Idle【P0修复】允许转到Connected
    {false, false,     false,    false,       false,         false,      false,      false,      false,   true,   true},   // Connecting
    {true,  false,     false,    true,        true,          false,      false,      false,      false,   true,   true},   // Connected【P0修复】允许转回Idle（断开连接）
    {false, false,     false,    false,       true,          true,       false,      false,      false,   true,   true},   // Initializing【修复】允许转到Transmitting
    {false, false,     false,    false,       true,          true,       false,      false,      false,   true,   true},   // Handshaking【修复】允许转到Transmitting
    {false, false,     false,    false,       false,         true,       true,       true,       true,    true,   true},   // Transmitting【P0修复】允许转到Completing和Completed
    {false, false,     false,    false,       false,         true,       false,      false,      false,   true,   true},   // Paused【修复】允许从暂停恢复到Transmitting
    {false, false,     false,    false,       false,         true,       false,      false,      true,    true,   true},   // Completing【P0修复】允许转到Completed
    {true,  false,     true,     true,        false,         false,      false,      false,      false,   true,   true},   // Completed【P0修复】允许转到Idle/Connected/Initializing（支持重传）
    {true,  false,     true,     true,        true,          true,       true,       true,       false,   true,   true},   // Failed【P0修复】增加转到Idle（重置）
    {true,  false,     true,     true,        true,          true,       true,       true,       true,    true,   true},    // Error【P0修复】增加转到Idle（重置）
};

// 状态描述映射表
static const char* s_stateDescriptions[] = {
    "空闲",
    "连接中",
    "已连接",
    "初始化中",
    "握手中",
    "传输中",
    "已暂停",
    "完成中",
    "已完成",
    "失败",
    "错误"
};

static_assert(sizeof(s_validTransitions) / sizeof(s_validTransitions[0]) == 11, "Valid transitions table size mismatch");
static_assert(sizeof(s_stateDescriptions) / sizeof(s_stateDescriptions[0]) == 11, "State descriptions table size mismatch");

TransmissionStateManager::TransmissionStateManager()
    : m_currentState(TransmissionUIState::Idle)
    , m_lastStateChange(std::chrono::steady_clock::now())
    , m_stateChangeCallback(nullptr)
    , m_stateChangeCount(0)
{
    LogStateChange(TransmissionUIState::Idle, TransmissionUIState::Idle, "初始化");
}

TransmissionStateManager::~TransmissionStateManager()
{
    // 清理全局指针
    if (g_transmissionStateManager == this) {
        g_transmissionStateManager = nullptr;
    }
}

void TransmissionStateManager::SetStateChangeCallback(StateChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateChangeCallback = callback;
}

TransmissionUIState TransmissionStateManager::GetCurrentState() const
{
    return m_currentState.load();
}

bool TransmissionStateManager::RequestStateTransition(TransmissionUIState newState, const std::string& reason)
{
    TransmissionUIState oldState = m_currentState.load();

    // 检查是否是有效的状态转换
    if (!IsValidStateTransition(oldState, newState)) {
        LogStateChange(oldState, newState, "无效状态转换: " + reason);
        return false;
    }

    // 如果状态没有变化，不需要处理
    if (oldState == newState) {
        return true;
    }

    // 更新状态
    m_currentState.store(newState);
    m_lastStateChange = std::chrono::steady_clock::now();
    m_stateChangeCount++;

    LogStateChange(oldState, newState, reason);

    // 调用状态变化回调
    if (m_stateChangeCallback) {
        m_stateChangeCallback(oldState, newState);
    }

    return true;
}

void TransmissionStateManager::ForceState(TransmissionUIState newState, const std::string& reason)
{
    TransmissionUIState oldState = m_currentState.load();

    m_currentState.store(newState);
    m_lastStateChange = std::chrono::steady_clock::now();
    m_stateChangeCount++;

    LogStateChange(oldState, newState, "强制状态: " + reason);

    // 调用状态变化回调
    if (m_stateChangeCallback) {
        m_stateChangeCallback(oldState, newState);
    }
}

bool TransmissionStateManager::IsTransmitting() const
{
    TransmissionUIState state = m_currentState.load();
    return (state == TransmissionUIState::Transmitting ||
            state == TransmissionUIState::Paused ||
            state == TransmissionUIState::Completing);
}

bool TransmissionStateManager::IsErrorState() const
{
    TransmissionUIState state = m_currentState.load();
    return (state == TransmissionUIState::Failed || state == TransmissionUIState::Error);
}

bool TransmissionStateManager::CanStartNewTransmission() const
{
    TransmissionUIState state = m_currentState.load();
    return (state == TransmissionUIState::Idle ||
            state == TransmissionUIState::Connected ||
            state == TransmissionUIState::Completed);
}

bool TransmissionStateManager::CanPauseTransmission() const
{
    TransmissionUIState state = m_currentState.load();
    return state == TransmissionUIState::Transmitting;
}

bool TransmissionStateManager::CanResumeTransmission() const
{
    TransmissionUIState state = m_currentState.load();
    return state == TransmissionUIState::Paused;
}

bool TransmissionStateManager::CanCancelTransmission() const
{
    TransmissionUIState state = m_currentState.load();
    return (state == TransmissionUIState::Transmitting ||
            state == TransmissionUIState::Paused ||
            state == TransmissionUIState::Initializing ||
            state == TransmissionUIState::Handshaking ||
            state == TransmissionUIState::Completing);
}

std::string TransmissionStateManager::GetStateDescription(TransmissionUIState state) const
{
    if (state >= TransmissionUIState::Idle && state <= TransmissionUIState::Error) {
        return s_stateDescriptions[static_cast<int>(state)];
    }
    return "未知状态";
}

std::chrono::milliseconds TransmissionStateManager::GetStateDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastStateChange);
}

uint64_t TransmissionStateManager::GetStateChangeCount() const
{
    return m_stateChangeCount;
}

void TransmissionStateManager::Reset()
{
    ForceState(TransmissionUIState::Idle, "重置状态管理器");
}

void TransmissionStateManager::ClearCallbacks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateChangeCallback = nullptr;
}

bool TransmissionStateManager::IsValidStateTransition(TransmissionUIState from, TransmissionUIState to) const
{
    return s_validTransitions[static_cast<int>(from)][static_cast<int>(to)];
}

void TransmissionStateManager::LogStateChange(TransmissionUIState oldState, TransmissionUIState newState, const std::string& reason)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time_t);

    char timeStr[32];
    sprintf_s(timeStr, sizeof(timeStr), "[%02d:%02d:%02d.%03d] ",
              tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

    std::ostringstream log;
    log << timeStr << "状态变化: "
        << GetStateDescription(oldState) << " -> "
        << GetStateDescription(newState)
        << " (" << reason << ") "
        << "[变化次数: " << m_stateChangeCount << "]";

    std::cout << log.str() << std::endl;

    // 同时写入日志文件
    try {
        std::ofstream logFile("TransmissionState.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << log.str() << std::endl;
            logFile.close();
        }
    } catch (...) {
        // 忽略日志写入错误
    }
}

void TransmissionStateManager::DumpStateHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    TransmissionUIState currentState = m_currentState.load();
    auto duration = GetStateDuration();

    std::cout << "=== 传输状态管理器调试信息 ===" << std::endl;
    std::cout << "当前状态: " << GetStateDescription(currentState) << std::endl;
    std::cout << "持续时间: " << duration.count() << "ms" << std::endl;
    std::cout << "状态变化次数: " << m_stateChangeCount << std::endl;
    std::cout << "最后变化时间: "
              << std::chrono::duration_cast<std::chrono::seconds>(
                     m_lastStateChange.time_since_epoch()).count() << "s ago" << std::endl;
    std::cout << "===============================" << std::endl;
}