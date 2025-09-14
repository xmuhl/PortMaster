#pragma once

#include <memory>
#include <vector>
#include <string>

// 🔑 架构重构：从PortMasterDlg提取的发送控制专职管理器
// SOLID-S: 单一职责 - 专注发送操作的业务逻辑和流程控制
// SOLID-O: 开闭原则 - 可扩展不同的发送策略和模式
// SOLID-D: 依赖倒置 - 依赖抽象接口而非具体UI实现

// 前置声明
class ReliableChannel;
class ITransport;

// 发送结果状态枚举 (KISS原则：简单明确的状态定义)
enum class SendResult : int
{
    SUCCESS = 0,        // 发送成功启动
    NO_DATA,            // 无数据可发送
    NOT_CONNECTED,      // 未连接
    ALREADY_ACTIVE,     // 传输已激活
    FAILED              // 发送失败
};

// 发送控制器 - 专职管理发送相关的所有业务逻辑
class SendController
{
public:
    SendController() = default;
    ~SendController() = default;

    // 禁用拷贝构造和赋值 (RAII + 单例设计)
    SendController(const SendController&) = delete;
    SendController& operator=(const SendController&) = delete;

    /**
     * @brief 执行发送操作的主要入口点
     * @param inputData 输入数据（从UI输入框获取）
     * @param transmissionData 文件数据（拖放或加载的文件）
     * @param currentFileName 当前文件名
     * @param isConnected 连接状态
     * @param isReliableMode 是否使用可靠传输模式
     * @param reliableChannel 可靠传输通道实例
     * @return SendResult 发送结果状态
     */
    SendResult ExecuteSend(
        const std::vector<uint8_t>& inputData,
        const std::vector<uint8_t>& transmissionData, 
        const std::wstring& currentFileName,
        bool isConnected,
        bool isReliableMode,
        std::shared_ptr<ReliableChannel> reliableChannel
    );

    /**
     * @brief 检查是否有断点续传的数据
     * @return 如果有可恢复的传输上下文返回true
     */
    bool HasResumableTransmission() const;

    /**
     * @brief 处理断点续传逻辑
     * @return 如果用户选择续传并成功启动返回true
     */
    bool HandleResumeTransmission();

    /**
     * @brief 清除传输上下文
     */
    void ClearTransmissionContext();

    /**
     * @brief 验证发送前的条件检查
     * @param data 要发送的数据
     * @param isConnected 连接状态
     * @param isTransmissionActive 传输是否激活
     * @return SendResult 验证结果
     */
    static SendResult ValidateSendConditions(
        const std::vector<uint8_t>& data,
        bool isConnected,
        bool isTransmissionActive
    );

    /**
     * @brief 启动可靠传输模式
     * @param data 要发送的数据
     * @param fileName 文件名（可选）
     * @param reliableChannel 可靠传输通道
     * @return 是否成功启动
     */
    static bool StartReliableTransmission(
        const std::vector<uint8_t>& data,
        const std::wstring& fileName,
        std::shared_ptr<ReliableChannel> reliableChannel
    );

    /**
     * @brief 启动普通传输模式
     * @param data 要发送的数据
     * @return 是否成功启动
     */
    static bool StartNormalTransmission(
        const std::vector<uint8_t>& data
    );

    /**
     * @brief 获取发送结果的用户友好描述
     * @param result 发送结果
     * @return 描述字符串
     */
    static std::wstring GetResultDescription(SendResult result);

    /**
     * @brief 格式化发送操作的日志消息
     * @param operation 操作描述
     * @param dataSize 数据大小
     * @param fileName 文件名（可选）
     * @return 格式化后的日志消息
     */
    static std::wstring FormatSendLogMessage(
        const std::wstring& operation,
        size_t dataSize,
        const std::wstring& fileName = L""
    );

private:
    // 🔑 YAGNI原则：仅保留必要的内部状态
    bool m_hasActiveTransmission = false;
    
    // 内部辅助方法 (SOLID-S: 单一职责的细分功能)
    
    /**
     * @brief 准备发送数据（选择输入数据或文件数据）
     * @param inputData 输入框数据
     * @param transmissionData 文件数据
     * @param outData 输出数据引用
     * @param outIsFileTransmission 输出是否为文件传输标志
     * @return 是否成功准备数据
     */
    bool PrepareSendData(
        const std::vector<uint8_t>& inputData,
        const std::vector<uint8_t>& transmissionData,
        std::vector<uint8_t>& outData,
        bool& outIsFileTransmission
    );

    /**
     * @brief 验证可靠传输通道状态
     * @param reliableChannel 可靠传输通道
     * @return 通道是否可用
     */
    bool ValidateReliableChannel(std::shared_ptr<ReliableChannel> reliableChannel);
};