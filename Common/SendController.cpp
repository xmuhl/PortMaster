#include "pch.h"
#include "SendController.h"
#include "../Protocol/ReliableChannel.h"
#include <sstream>
#include <iomanip>

// 🔑 架构重构：SendController专职管理器实现
// SOLID-S: 单一职责 - 专注发送操作的业务逻辑处理
// SOLID-L: 里氏替换 - 所有发送策略都能安全替换
// KISS原则: 保持发送逻辑简单直观

SendResult SendController::ExecuteSend(
    const std::vector<uint8_t>& inputData,
    const std::vector<uint8_t>& transmissionData,
    const std::wstring& currentFileName,
    bool isConnected,
    bool isReliableMode,
    std::shared_ptr<ReliableChannel> reliableChannel)
{
    // 1. 准备发送数据 (SOLID-S: 数据准备单一职责)
    std::vector<uint8_t> dataToSend;
    bool isFileTransmission = false;
    
    if (!PrepareSendData(inputData, transmissionData, dataToSend, isFileTransmission))
    {
        return SendResult::NO_DATA;
    }

    // 2. 验证发送条件 (DRY原则: 统一的条件验证逻辑)
    SendResult validationResult = ValidateSendConditions(
        dataToSend, 
        isConnected, 
        m_hasActiveTransmission
    );
    
    if (validationResult != SendResult::SUCCESS)
    {
        return validationResult;
    }

    // 3. 根据模式执行发送 (SOLID-O: 开闭原则 - 可扩展发送策略)
    bool transmissionStarted = false;
    
    if (isReliableMode && reliableChannel)
    {
        transmissionStarted = StartReliableTransmission(
            dataToSend, 
            isFileTransmission ? currentFileName : L"", 
            reliableChannel
        );
    }
    else
    {
        transmissionStarted = StartNormalTransmission(dataToSend);
    }

    // 4. 更新内部状态并返回结果
    if (transmissionStarted)
    {
        m_hasActiveTransmission = true;
        return SendResult::SUCCESS;
    }
    else
    {
        return SendResult::FAILED;
    }
}

bool SendController::HasResumableTransmission() const
{
    // YAGNI原则：目前简化实现，后续可扩展为完整的断点续传检查
    return false;
}

bool SendController::HandleResumeTransmission()
{
    // YAGNI原则：目前简化实现，后续可扩展为完整的断点续传处理
    return false;
}

void SendController::ClearTransmissionContext()
{
    // KISS原则：简单的状态清理
    m_hasActiveTransmission = false;
}

SendResult SendController::ValidateSendConditions(
    const std::vector<uint8_t>& data,
    bool isConnected,
    bool isTransmissionActive)
{
    // SOLID-S: 单一职责 - 专注条件验证逻辑
    if (data.empty())
    {
        return SendResult::NO_DATA;
    }

    if (!isConnected)
    {
        return SendResult::NOT_CONNECTED;
    }

    if (isTransmissionActive)
    {
        return SendResult::ALREADY_ACTIVE;
    }

    return SendResult::SUCCESS;
}

bool SendController::StartReliableTransmission(
    const std::vector<uint8_t>& data,
    const std::wstring& fileName,
    std::shared_ptr<ReliableChannel> reliableChannel)
{
    // SOLID-S: 单一职责 - 专注可靠传输启动逻辑
    if (!reliableChannel)
    {
        return false;
    }

    // 验证可靠传输通道状态
    if (!reliableChannel->IsActive())
    {
        if (!reliableChannel->Start())
        {
            return false;
        }
    }

    // 检查通道状态
    ReliableState currentState = reliableChannel->GetState();
    if (currentState != RELIABLE_IDLE)
    {
        return false;
    }

    // 执行发送操作
    bool result = false;
    if (!fileName.empty())
    {
        // 发送文件（带文件名）
        std::string fileNameStr(fileName.begin(), fileName.end());
        result = reliableChannel->SendFile(fileNameStr, data);
    }
    else
    {
        // 发送数据
        result = reliableChannel->SendData(data);
    }

    return result;
}

bool SendController::StartNormalTransmission(const std::vector<uint8_t>& data)
{
    // SOLID-S: 单一职责 - 专注普通传输启动逻辑
    // YAGNI原则：目前简化实现，后续可扩展为完整的普通传输处理
    
    if (data.empty())
    {
        return false;
    }
    
    // 模拟发送成功（实际应连接到ITransport层）
    return true;
}

std::wstring SendController::GetResultDescription(SendResult result)
{
    // SOLID-S: 单一职责 - 专注结果描述格式化
    switch (result)
    {
        case SendResult::SUCCESS:
            return L"Send operation started successfully";
        case SendResult::NO_DATA:
            return L"No data to send";
        case SendResult::NOT_CONNECTED:
            return L"Not connected to port";
        case SendResult::ALREADY_ACTIVE:
            return L"Transmission already active";
        case SendResult::FAILED:
            return L"Send operation failed";
        default:
            return L"Unknown status";
    }
}

std::wstring SendController::FormatSendLogMessage(
    const std::wstring& operation,
    size_t dataSize,
    const std::wstring& fileName)
{
    // SOLID-S: 单一职责 - 专注日志消息格式化
    // KISS原则：简单直观的日志格式
    std::wostringstream oss;
    oss << operation;
    
    if (!fileName.empty())
    {
        oss << L" [File: " << fileName << L"]";
    }
    
    oss << L" (Size: " << dataSize << L" bytes)";
    
    return oss.str();
}

// 私有辅助方法实现

bool SendController::PrepareSendData(
    const std::vector<uint8_t>& inputData,
    const std::vector<uint8_t>& transmissionData,
    std::vector<uint8_t>& outData,
    bool& outIsFileTransmission)
{
    // SOLID-S: 单一职责 - 专注数据准备逻辑
    // DRY原则：统一的数据优先级选择逻辑
    
    if (!transmissionData.empty())
    {
        // 优先使用文件数据
        outData = transmissionData;
        outIsFileTransmission = true;
        return true;
    }
    else if (!inputData.empty())
    {
        // 使用输入框数据
        outData = inputData;
        outIsFileTransmission = false;
        return true;
    }
    
    // 无可用数据
    outData.clear();
    outIsFileTransmission = false;
    return false;
}

bool SendController::ValidateReliableChannel(std::shared_ptr<ReliableChannel> reliableChannel)
{
    // SOLID-S: 单一职责 - 专注可靠传输通道验证
    if (!reliableChannel)
    {
        return false;
    }

    if (!reliableChannel->IsActive())
    {
        // 尝试启动通道
        return reliableChannel->Start();
    }

    // 检查通道状态
    ReliableState currentState = reliableChannel->GetState();
    return (currentState == RELIABLE_IDLE);
}