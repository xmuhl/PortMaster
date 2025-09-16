#pragma execution_character_set("utf-8")
#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <atomic>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString

// 前置声明
class CPortMasterDlg;
class ITransport;
class ReliableChannel;

// 使用PortMasterDlg.h中定义的类型 - 前置声明避免循环依赖
enum class TransmissionState; // 前置声明，定义在PortMasterDlg.h中
struct TransmissionContext;   // 前置声明，定义在PortMasterDlg.h中

/**
 * @brief 数据传输管理器
 * SOLID-S: 单一职责 - 专门负责数据传输的逻辑管理
 * SOLID-O: 开放封闭原则 - 易于扩展新的传输模式和协议
 */
class DataTransmissionManager
{
public:
    /**
     * @brief 构造函数
     * @param dialog 主对话框指针，用于UI更新和回调
     */
    explicit DataTransmissionManager(CPortMasterDlg* dialog);

    /**
     * @brief 析构函数
     */
    virtual ~DataTransmissionManager();

    /**
     * @brief 执行发送操作
     * SOLID-S: 单一职责 - 专门处理发送逻辑
     * @return 发送成功返回true，失败返回false
     */
    bool ExecuteSend();

    /**
     * @brief 开始数据传输
     * @param data 要传输的数据
     * @param isFileTransmission 是否为文件传输
     * @return 启动成功返回true，失败返回false
     */
    bool StartDataTransmission(const std::vector<uint8_t>& data, bool isFileTransmission = false);

    /**
     * @brief 停止数据传输
     * @param completed 是否为正常完成
     */
    void StopDataTransmission(bool completed);

    /**
     * @brief 更新传输进度
     */
    void UpdateTransmissionProgress();

    /**
     * @brief 设置传输状态
     * @param newState 新的传输状态
     */
    void SetTransmissionState(TransmissionState newState);

    /**
     * @brief 获取当前传输状态
     * @return 当前传输状态
     */
    TransmissionState GetTransmissionState() const;

    /**
     * @brief 处理分块传输定时器
     */
    void OnChunkTransmissionTimer();

    /**
     * @brief 暂停传输
     * @return 暂停成功返回true，失败返回false
     */
    bool PauseTransmission();

    /**
     * @brief 恢复传输
     * @return 恢复成功返回true，失败返回false
     */
    bool ResumeTransmission();

    /**
     * @brief 保存传输上下文（断点续传）
     * @param filePath 文件路径
     * @param totalBytes 总字节数
     * @param transmittedBytes 已传输字节数
     */
    void SaveTransmissionContext(const CString& filePath, size_t totalBytes, size_t transmittedBytes);

    /**
     * @brief 清除传输上下文
     */
    void ClearTransmissionContext();

    /**
     * @brief 获取传输上下文
     * @return 传输上下文引用
     */
    const TransmissionContext& GetTransmissionContext() const;

    /**
     * @brief 设置传输对象
     * @param transport 传输对象
     * @param reliableChannel 可靠通道对象
     */
    void SetTransportObjects(std::shared_ptr<ITransport> transport, std::shared_ptr<ReliableChannel> reliableChannel);

    /**
     * @brief 根据传输模式执行传输
     * @param dataToSend 要发送的数据
     * @param isFileTransmission 是否为文件传输
     * @return 传输启动成功返回true，失败返回false
     */
    bool ExecuteTransmissionByMode(const std::vector<uint8_t>& dataToSend, bool isFileTransmission);

    /**
     * @brief 执行可靠传输
     * @param dataToSend 要发送的数据
     * @param isFileTransmission 是否为文件传输
     * @return 传输启动成功返回true，失败返回false
     */
    bool ExecuteReliableTransmission(const std::vector<uint8_t>& dataToSend, bool isFileTransmission);

private:
    /**
     * @brief 检查断点续传条件
     * @return 可以续传返回true，否则返回false
     */
    bool CheckResumeCondition();

    /**
     * @brief 处理用户续传选择
     * @return 用户选择结果
     */
    int ShowResumeDialog();

    /**
     * @brief 获取输入数据
     * @return 输入数据向量
     */
    std::vector<uint8_t> GetInputData();

    /**
     * @brief 处理文件传输数据
     * @param dataToSend 输出的数据向量
     * @param isFileTransmission 输出是否为文件传输
     * @return 获取成功返回true，失败返回false
     */
    bool GetTransmissionData(std::vector<uint8_t>& dataToSend, bool& isFileTransmission);

private:
    CPortMasterDlg* m_dialog;                           ///< 主对话框指针
    std::shared_ptr<ITransport> m_transport;           ///< 当前传输对象
    std::shared_ptr<ReliableChannel> m_reliableChannel; ///< 可靠通道对象

    // 传输状态管理
    std::atomic<TransmissionState> m_transmissionState; ///< 当前传输状态
    std::unique_ptr<TransmissionContext> m_transmissionContext; ///< 传输上下文（断点续传）

    // 分块传输管理
    std::vector<uint8_t> m_chunkTransmissionData;      ///< 分块传输数据
    size_t m_chunkTransmissionIndex;                   ///< 当前分块索引
    size_t m_chunkSize;                                ///< 分块大小
    UINT_PTR m_transmissionTimer;                      ///< 传输定时器ID

    // 传输统计
    DWORD m_transmissionStartTime;                     ///< 传输开始时间
    size_t m_totalBytesTransmitted;                    ///< 总传输字节数
    DWORD m_lastSpeedUpdateTime;                       ///< 上次速度更新时间
};