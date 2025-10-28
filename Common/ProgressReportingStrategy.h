#pragma once
#pragma execution_character_set("utf-8")

#include <functional>
#include <memory>
#include "../src/TransmissionTask.h"
#include "CommonTypes.h"

/**
 * @brief 进度报告策略枚举
 */
enum class ProgressReportingStrategy
{
    SenderDriven,   // 发送方驱动进度（默认硬件端口模式）
    ReceiverDriven  // 接收方驱动进度（本地回路测试模式）
};

/**
 * @brief 工作模式检测结果
 */
struct WorkModeDetection
{
    bool isLoopbackTest = false;           // 是否为回路测试
    bool isLocalConnection = false;        // 是否为本地连接
    ProgressReportingStrategy strategy = ProgressReportingStrategy::SenderDriven;  // 推荐策略
    std::string detectionReason;           // 检测原因说明

    WorkModeDetection() = default;

    WorkModeDetection(bool loopback, bool local, ProgressReportingStrategy strat, const std::string& reason)
        : isLoopbackTest(loopback), isLocalConnection(local), strategy(strat), detectionReason(reason) {}
};

/**
 * @brief 智能进度报告管理器
 *
 * 职责：
 * - 自动检测当前工作模式
 * - 为不同模式提供最适合的进度报告策略
 * - 协调发送方和接收方的进度报告
 *
 * 设计原则：
 * - 默认策略：硬件端口模式使用发送方驱动进度
 * - 特殊策略：本地回路测试模式使用接收方驱动进度
 * - 智能检测：根据传输配置自动选择策略
 */
class SmartProgressManager
{
public:
    /**
     * @brief 进度数据回调类型
     */
    using ProgressDataCallback = std::function<void(int progressPercent)>;

    /**
     * @brief 状态文本回调类型
     */
    using StatusTextCallback = std::function<void(const std::string& statusText)>;

public:
    SmartProgressManager();
    ~SmartProgressManager() = default;

    // 禁止拷贝和赋值
    SmartProgressManager(const SmartProgressManager&) = delete;
    SmartProgressManager& operator=(const SmartProgressManager&) = delete;

    // ========== 核心接口 ==========

    /**
     * @brief 检测工作模式并确定进度报告策略
     * @param portType 端口类型
     * @param portName 端口名称
     * @param useReliableMode 是否使用可靠传输模式
     * @return 工作模式检测结果
     */
    WorkModeDetection DetectWorkMode(PortType portType,
                                    const std::string& portName,
                                    bool useReliableMode);

    /**
     * @brief 设置进度报告策略
     * @param strategy 进度报告策略
     */
    void SetProgressReportingStrategy(ProgressReportingStrategy strategy);

    /**
     * @brief 获取当前进度报告策略
     * @return 当前进度报告策略
     */
    ProgressReportingStrategy GetCurrentStrategy() const;

    /**
     * @brief 处理发送方进度更新
     * @param progress 传输进度信息
     *
     * 说明：
     * - 在发送方驱动策略下，此方法直接更新进度条
     * - 在接收方驱动策略下，此方法仅更新状态文本
     */
    void HandleSenderProgress(const TransmissionProgress& progress);

    /**
     * @brief 处理接收方进度更新
     * @param receivedBytes 已接收字节数
     * @param totalBytes 总字节数
     *
     * 说明：
     * - 在接收方驱动策略下，此方法更新进度条
     * - 在发送方驱动策略下，此方法被忽略
     */
    void HandleReceiverProgress(size_t receivedBytes, size_t totalBytes);

    /**
     * @brief 标记发送完成
     *
     * 说明：
     * - 在接收方驱动策略下，发送完成后等待接收完成才标记100%
     * - 在发送方驱动策略下，发送完成即标记100%
     */
    void MarkSendComplete();

    /**
     * @brief 标记接收完成
     */
    void MarkReceiveComplete();

    /**
     * @brief 重置管理器状态
     */
    void Reset();

    // ========== 回调设置接口 ==========

    /**
     * @brief 设置进度数据回调
     * @param callback 进度回调函数
     */
    void SetProgressDataCallback(ProgressDataCallback callback);

    /**
     * @brief 设置状态文本回调
     * @param callback 状态文本回调函数
     */
    void SetStatusTextCallback(StatusTextCallback callback);

    // ========== 状态查询接口 ==========

    /**
     * @brief 检查是否为回路测试模式
     * @return 是否为回路测试
     */
    bool IsLoopbackTest() const;

    /**
     * @brief 检查发送是否完成
     * @return 发送是否完成
     */
    bool IsSendComplete() const;

    /**
     * @brief 检查接收是否完成
     * @return 接收是否完成
     */
    bool IsReceiveComplete() const;

    /**
     * @brief 检查整体传输是否完成
     * @return 传输是否完成
     */
    bool IsTransmissionComplete() const;

private:
    // ========== 内部方法 ==========

    /**
     * @brief 更新进度条显示
     * @param progressPercent 进度百分比
     */
    void UpdateProgressBar(int progressPercent);

    /**
     * @brief 更新状态文本显示
     * @param statusText 状态文本
     */
    void UpdateStatusText(const std::string& statusText);

    /**
     * @brief 计算接收方驱动下的进度百分比
     * @param receivedBytes 已接收字节数
     * @param totalBytes 总字节数
     * @return 进度百分比
     */
    int CalculateReceiverDrivenProgress(size_t receivedBytes, size_t totalBytes) const;

    /**
     * @brief 判断是否应该使用接收方驱动策略
     * @param portType 端口类型
     * @param portName 端口名称
     * @param useReliableMode 是否使用可靠模式
     * @return 是否应该使用接收方驱动策略
     */
    bool ShouldUseReceiverDrivenStrategy(PortType portType,
                                       const std::string& portName,
                                       bool useReliableMode) const;

private:
    // ========== 成员变量 ==========

    // 当前策略和状态
    ProgressReportingStrategy m_currentStrategy;
    WorkModeDetection m_lastDetection;

    // 传输状态标志
    bool m_sendComplete;
    bool m_receiveComplete;
    size_t m_expectedReceiveBytes;
    size_t m_actualReceivedBytes;

    // 回调函数
    ProgressDataCallback m_progressDataCallback;
    StatusTextCallback m_statusTextCallback;

    // 线程安全
    mutable std::recursive_mutex m_mutex;
};

/**
 * @brief 全局智能进度管理器实例访问器
 */
class SmartProgressManagerSingleton
{
public:
    static SmartProgressManager& GetInstance();

private:
    SmartProgressManagerSingleton() = default;
    static std::unique_ptr<SmartProgressManager> s_instance;
    static std::recursive_mutex s_mutex;
};