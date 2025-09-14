#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

// 🔑 架构重构：从PortMasterDlg提取的传输控制专职管理器
// SOLID-S: 单一职责 - 专注传输进度控制和状态管理
// SOLID-O: 开闭原则 - 可扩展不同的传输策略和进度算法
// SOLID-D: 依赖倒置 - 依赖抽象接口而非具体UI实现

// 传输状态枚举 (简化版本避免编译依赖问题)
enum class TransmissionControllerState : int
{
    IDLE = 0,           // 空闲
    TRANSMITTING = 1,   // 传输中
    PAUSED = 2,         // 暂停
    COMPLETED = 3,      // 完成
    FAILED = 4          // 失败
};

// 传输控制器 - 专职管理传输进度和状态控制的业务逻辑 (简化版本)
class TransmissionController
{
public:
    TransmissionController();
    ~TransmissionController();

    // 禁用拷贝构造和赋值 (RAII + 单例设计)
    TransmissionController(const TransmissionController&) = delete;
    TransmissionController& operator=(const TransmissionController&) = delete;

    /**
     * @brief 启动分块传输
     * @param data 要传输的完整数据
     * @param chunkSize 每块大小（字节）
     * @return 是否成功启动
     */
    bool StartChunkedTransmission(
        const std::vector<uint8_t>& data, 
        size_t chunkSize = 256
    );

    /**
     * @brief 停止传输
     * @param completed 是否为正常完成停止
     */
    void StopTransmission(bool completed = false);

    /**
     * @brief 暂停传输
     * @return 是否成功暂停
     */
    bool PauseTransmission();

    /**
     * @brief 恢复传输
     * @return 是否成功恢复
     */
    bool ResumeTransmission();

    /**
     * @brief 获取当前传输状态
     */
    TransmissionControllerState GetCurrentState() const { return m_currentState; }

    /**
     * @brief 检查是否有活跃的传输
     */
    bool IsTransmissionActive() const;

    /**
     * @brief 重置所有状态和数据
     */
    void Reset();

    // 静态工具函数 (SOLID-S: 单一职责的工具方法)

    /**
     * @brief 计算传输速度
     * @param bytes 传输字节数
     * @param elapsedMs 耗时毫秒数
     * @return 速度 (B/s)
     */
    static double CalculateSpeed(size_t bytes, uint32_t elapsedMs);

    /**
     * @brief 格式化速度显示
     * @param speedBps 速度 (B/s)
     * @return 格式化后的速度字符串
     */
    static std::wstring FormatSpeed(double speedBps);

    /**
     * @brief 获取传输状态的用户友好描述
     */
    static std::wstring GetStateDescription(TransmissionControllerState state);

private:
    // 核心状态变量 (SOLID-S: 最小化状态复杂度)
    TransmissionControllerState m_currentState = TransmissionControllerState::IDLE;
    
    // 传输数据管理
    std::vector<uint8_t> m_transmissionData;
    size_t m_currentChunkIndex = 0;
    size_t m_chunkSize = 256;
    
    /**
     * @brief 获取当前时间戳（毫秒）
     */
    uint32_t GetCurrentTimeMs() const;
};