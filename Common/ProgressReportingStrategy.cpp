#include "ProgressReportingStrategy.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// 静态成员初始化
std::unique_ptr<SmartProgressManager> SmartProgressManagerSingleton::s_instance = nullptr;
std::mutex SmartProgressManagerSingleton::s_mutex;

SmartProgressManager::SmartProgressManager()
    : m_currentStrategy(ProgressReportingStrategy::SenderDriven)
    , m_sendComplete(false)
    , m_receiveComplete(false)
    , m_expectedReceiveBytes(0)
    , m_actualReceivedBytes(0)
{
}

WorkModeDetection SmartProgressManager::DetectWorkMode(PortType portType,
                                                     const std::string& portName,
                                                     bool useReliableMode)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    WorkModeDetection detection;

    // 检测是否为回路测试模式
    if (portType == PortType::PORT_TYPE_LOOPBACK)
    {
        detection.isLoopbackTest = true;
        detection.isLocalConnection = true;
        detection.strategy = ProgressReportingStrategy::ReceiverDriven;
        detection.detectionReason = "检测到LOOPBACK端口类型，使用接收方驱动进度策略";
    }
    // 检测本地连接（如127.0.0.1）
    else if (portName == "127.0.0.1" || portName == "localhost")
    {
        detection.isLoopbackTest = false;
        detection.isLocalConnection = true;
        detection.strategy = ProgressReportingStrategy::ReceiverDriven;
        detection.detectionReason = "检测到本地网络连接，使用接收方驱动进度策略";
    }
    // 检测其他本地回路场景
    else if (useReliableMode && ShouldUseReceiverDrivenStrategy(portType, portName, useReliableMode))
    {
        detection.isLoopbackTest = false;
        detection.isLocalConnection = true;
        detection.strategy = ProgressReportingStrategy::ReceiverDriven;
        detection.detectionReason = "检测到可靠传输本地场景，使用接收方驱动进度策略";
    }
    // 默认使用发送方驱动策略
    else
    {
        detection.isLoopbackTest = false;
        detection.isLocalConnection = false;
        detection.strategy = ProgressReportingStrategy::SenderDriven;

        std::ostringstream reason;
        reason << "检测到硬件端口模式（" << portName << "），使用发送方驱动进度策略";
        detection.detectionReason = reason.str();
    }

    m_lastDetection = detection;
    m_currentStrategy = detection.strategy;

    return detection;
}

void SmartProgressManager::SetProgressReportingStrategy(ProgressReportingStrategy strategy)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentStrategy = strategy;

    std::string strategyName = (strategy == ProgressReportingStrategy::SenderDriven) ? "发送方驱动" : "接收方驱动";

    if (m_statusTextCallback)
    {
        std::string message = "进度报告策略已切换为：" + strategyName;
        m_statusTextCallback(message);
    }
}

ProgressReportingStrategy SmartProgressManager::GetCurrentStrategy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentStrategy;
}

void SmartProgressManager::HandleSenderProgress(const TransmissionProgress& progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 总是更新状态文本（"X/Y 字节"信息在所有模式下都有价值）
    UpdateStatusText(progress.statusText);

    // 根据当前策略决定是否更新进度条
    if (m_currentStrategy == ProgressReportingStrategy::SenderDriven)
    {
        UpdateProgressBar(progress.progressPercent);
    }
    // 在接收方驱动策略下，发送方进度不驱动进度条更新
    // 进度条将等待接收方进度更新
}

void SmartProgressManager::HandleReceiverProgress(size_t receivedBytes, size_t totalBytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_actualReceivedBytes = receivedBytes;
    if (totalBytes > 0)
    {
        m_expectedReceiveBytes = totalBytes;
    }

    // 只有在接收方驱动策略下才更新进度条
    if (m_currentStrategy == ProgressReportingStrategy::ReceiverDriven)
    {
        int progressPercent = CalculateReceiverDrivenProgress(receivedBytes, totalBytes);
        UpdateProgressBar(progressPercent);

        // 同时更新状态文本显示接收进度
        std::ostringstream statusText;
        statusText << "已接收 " << receivedBytes << "/" << totalBytes << " 字节";
        UpdateStatusText(statusText.str());
    }
    // 在发送方驱动策略下，接收方进度被忽略
}

void SmartProgressManager::MarkSendComplete()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sendComplete = true;

    if (m_currentStrategy == ProgressReportingStrategy::SenderDriven)
    {
        // 发送方驱动策略：发送完成即标记100%
        UpdateProgressBar(100);
        UpdateStatusText("发送完成");
    }
    else
    {
        // 接收方驱动策略：发送完成但等待接收完成
        UpdateStatusText("发送完成，等待接收确认...");
    }
}

void SmartProgressManager::MarkReceiveComplete()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_receiveComplete = true;

    if (m_currentStrategy == ProgressReportingStrategy::ReceiverDriven)
    {
        // 接收方驱动策略：接收完成才标记100%
        UpdateProgressBar(100);
        UpdateStatusText("接收完成");
    }
    else
    {
        // 发送方驱动策略：接收完成是额外信息
        UpdateStatusText("数据接收完成");
    }
}

void SmartProgressManager::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sendComplete = false;
    m_receiveComplete = false;
    m_expectedReceiveBytes = 0;
    m_actualReceivedBytes = 0;

    // 保持当前策略不变，但重置显示
    UpdateProgressBar(0);
    UpdateStatusText("就绪");
}

void SmartProgressManager::SetProgressDataCallback(ProgressDataCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressDataCallback = callback;
}

void SmartProgressManager::SetStatusTextCallback(StatusTextCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusTextCallback = callback;
}

bool SmartProgressManager::IsLoopbackTest() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastDetection.isLoopbackTest;
}

bool SmartProgressManager::IsSendComplete() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sendComplete;
}

bool SmartProgressManager::IsReceiveComplete() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_receiveComplete;
}

bool SmartProgressManager::IsTransmissionComplete() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_currentStrategy == ProgressReportingStrategy::SenderDriven)
    {
        return m_sendComplete;
    }
    else
    {
        return m_sendComplete && m_receiveComplete;
    }
}

void SmartProgressManager::UpdateProgressBar(int progressPercent)
{
    // 确保进度在有效范围内
    progressPercent = std::max(0, std::min(100, progressPercent));

    if (m_progressDataCallback)
    {
        m_progressDataCallback(progressPercent);
    }
}

void SmartProgressManager::UpdateStatusText(const std::string& statusText)
{
    if (m_statusTextCallback)
    {
        m_statusTextCallback(statusText);
    }
}

int SmartProgressManager::CalculateReceiverDrivenProgress(size_t receivedBytes, size_t totalBytes) const
{
    if (totalBytes == 0)
    {
        return 0;
    }

    // 计算接收进度百分比
    double progress = (static_cast<double>(receivedBytes) / static_cast<double>(totalBytes)) * 100.0;
    return static_cast<int>(std::round(progress));
}

bool SmartProgressManager::ShouldUseReceiverDrivenStrategy(PortType portType,
                                                          const std::string& portName,
                                                          bool useReliableMode) const
{
    // 如果已经是回路测试，直接使用接收方驱动
    if (portType == PortType::PORT_TYPE_LOOPBACK)
    {
        return true;
    }

    // 如果是本地网络连接，使用接收方驱动
    if (portName == "127.0.0.1" || portName == "localhost")
    {
        return true;
    }

    // 如果启用了可靠传输模式且端口名包含本地特征，使用接收方驱动
    if (useReliableMode)
    {
        // 检查端口名是否包含本地回路的特征
        if (portName.find("loopback") != std::string::npos ||
            portName.find("local") != std::string::npos ||
            portName.find("test") != std::string::npos)
        {
            return true;
        }
    }

    // 其他情况使用发送方驱动
    return false;
}

// ========== SmartProgressManagerSingleton 实现 ==========

SmartProgressManager& SmartProgressManagerSingleton::GetInstance()
{
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_instance)
    {
        s_instance = std::make_unique<SmartProgressManager>();
    }

    return *s_instance;
}