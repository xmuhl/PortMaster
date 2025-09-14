#include "pch.h"
#include "TransmissionController.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

// Windows-specific includes for time functions
#ifdef _WIN32
#include <windows.h>
#endif

// 🔑 架构重构：TransmissionController专职管理器实现 (简化版本)
// SOLID-S: 单一职责 - 专注传输控制和进度管理业务逻辑处理
// KISS原则: 保持传输控制逻辑简单直观

TransmissionController::TransmissionController()
{
    Reset();
}

TransmissionController::~TransmissionController()
{
    // 确保停止传输
    if (IsTransmissionActive())
    {
        StopTransmission(false);
    }
}

bool TransmissionController::StartChunkedTransmission(
    const std::vector<uint8_t>& data, 
    size_t chunkSize)
{
    // SOLID-S: 单一职责 - 专注启动传输的条件检查和初始化
    if (data.empty())
    {
        return false;
    }

    if (IsTransmissionActive())
    {
        return false;
    }

    // 初始化传输数据和参数
    m_transmissionData = data;
    m_currentChunkIndex = 0;
    m_chunkSize = std::max(size_t(1), chunkSize); // 确保块大小至少为1

    // 设置传输状态
    m_currentState = TransmissionControllerState::TRANSMITTING;

    return true;
}

void TransmissionController::StopTransmission(bool completed)
{
    // SOLID-S: 单一职责 - 专注传输停止的状态管理和清理
    
    // 设置最终状态
    m_currentState = completed ? 
        TransmissionControllerState::COMPLETED : TransmissionControllerState::IDLE;

    // 清理传输数据 (YAGNI: 及时释放不需要的资源)
    m_transmissionData.clear();
    m_currentChunkIndex = 0;
}

bool TransmissionController::PauseTransmission()
{
    // SOLID-S: 单一职责 - 专注暂停逻辑处理
    if (m_currentState != TransmissionControllerState::TRANSMITTING)
    {
        return false; // 只有传输中状态才能暂停
    }

    m_currentState = TransmissionControllerState::PAUSED;
    return true;
}

bool TransmissionController::ResumeTransmission()
{
    // SOLID-S: 单一职责 - 专注恢复逻辑处理
    if (m_currentState != TransmissionControllerState::PAUSED)
    {
        return false; // 只有暂停状态才能恢复
    }

    m_currentState = TransmissionControllerState::TRANSMITTING;
    return true;
}

bool TransmissionController::IsTransmissionActive() const
{
    // SOLID-S: 单一职责 - 专注活跃状态判断逻辑
    return m_currentState == TransmissionControllerState::TRANSMITTING || 
           m_currentState == TransmissionControllerState::PAUSED;
}

void TransmissionController::Reset()
{
    // KISS原则：简单的状态重置
    m_currentState = TransmissionControllerState::IDLE;
    m_transmissionData.clear();
    m_currentChunkIndex = 0;
    m_chunkSize = 256;
}

// 静态工具函数实现 (SOLID-S: 单一职责的工具方法)

double TransmissionController::CalculateSpeed(size_t bytes, uint32_t elapsedMs)
{
    if (elapsedMs == 0) return 0.0;
    return (double(bytes) * 1000.0) / elapsedMs; // B/s
}

std::wstring TransmissionController::FormatSpeed(double speedBps)
{
    // KISS原则：简单直观的速度格式化
    std::wostringstream oss;
    
    if (speedBps >= 1024 * 1024) // MB/s
    {
        oss << std::fixed << std::setprecision(1) 
            << (speedBps / (1024 * 1024)) << L" MB/s";
    }
    else if (speedBps >= 1024) // KB/s
    {
        oss << std::fixed << std::setprecision(1) 
            << (speedBps / 1024) << L" KB/s";
    }
    else // B/s
    {
        oss << std::fixed << std::setprecision(0) 
            << speedBps << L" B/s";
    }
    
    return oss.str();
}

std::wstring TransmissionController::GetStateDescription(TransmissionControllerState state)
{
    // SOLID-S: 单一职责 - 专注状态描述格式化
    switch (state)
    {
        case TransmissionControllerState::IDLE:
            return L"Idle";
        case TransmissionControllerState::TRANSMITTING:
            return L"Transmitting";
        case TransmissionControllerState::PAUSED:
            return L"Paused";
        case TransmissionControllerState::COMPLETED:
            return L"Completed";
        case TransmissionControllerState::FAILED:
            return L"Failed";
        default:
            return L"Unknown";
    }
}

uint32_t TransmissionController::GetCurrentTimeMs() const
{
    // 跨平台时间获取 (SOLID-S: 单一职责的时间抽象)
#ifdef _WIN32
    return GetTickCount();
#else
    return 0; // 简化实现
#endif
}