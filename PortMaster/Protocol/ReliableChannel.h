#pragma once

#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "../Common/CommonTypes.h"
#include "../Transport/ITransport.h"

namespace PortMaster {

    /// <summary>
    /// 可靠传输信道
    /// 在ITransport基础上提供可靠的数据传输机制
    /// 实现状态机管理、重传机制、校验和验证
    /// </summary>
    class ReliableChannel {
    public:
        ReliableChannel();
        virtual ~ReliableChannel();

        /// <summary>
        /// 绑定底层传输层
        /// </summary>
        /// <param name="transport">传输层实例</param>
        void AttachTransport(std::shared_ptr<ITransport> transport);

        /// <summary>
        /// 配置可靠传输参数
        /// </summary>
        /// <param name="config">配置参数</param>
        void Configure(const ReliableConfig& config);

        /// <summary>
        /// 启动传输会话
        /// </summary>
        /// <param name="meta">会话元数据</param>
        /// <returns>成功返回true，失败返回false</returns>
        bool StartSession(const SessionMeta& meta);

        /// <summary>
        /// 执行协议状态机步骤
        /// </summary>
        /// <param name="payload">输入数据</param>
        /// <returns>下一步操作指示</returns>
        ReliableEventStep Pump(const BufferView& payload);

        /// <summary>
        /// 获取传输统计信息
        /// </summary>
        /// <returns>统计信息结构</returns>
        ReliableStats GetStats() const;

        /// <summary>
        /// 获取当前状态
        /// </summary>
        /// <returns>当前可靠传输状态</returns>
        ReliableState GetState() const;

        /// <summary>
        /// 重置信道状态
        /// </summary>
        void Reset();

    private:
        // 状态机实现
        ReliableEventStep HandleIdleState(const BufferView& payload);
        ReliableEventStep HandleStartingState(const BufferView& payload);
        ReliableEventStep HandleSendingState(const BufferView& payload);
        ReliableEventStep HandleEndingState(const BufferView& payload);
        ReliableEventStep HandleReadyState(const BufferView& payload);
        ReliableEventStep HandleReceivingState(const BufferView& payload);

        // 帧处理
        bool SendFrame(FrameType type, const BufferView& data);
        bool ReceiveFrame(Buffer& frameData, FrameType& type);
        uint32_t CalculateChecksum(const BufferView& data);
        bool ValidateChecksum(const BufferView& data, uint32_t expected);

        // 成员变量
        std::shared_ptr<ITransport> transport_;     // 底层传输层
        ReliableConfig config_;                     // 配置参数
        ReliableState state_;                       // 当前状态
        SessionMeta sessionMeta_;                   // 会话元数据
        ReliableStats stats_;                       // 统计信息

        uint32_t currentSequence_;                  // 当前序列号
        uint32_t expectedSequence_;                 // 期望序列号
        std::map<uint32_t, Buffer> sendWindow_;     // 发送窗口
        std::map<uint32_t, Buffer> receiveWindow_;  // 接收窗口

        DWORD lastSendTime_;                        // 最后发送时间
        uint32_t retryCount_;                       // 重试计数

        mutable std::mutex stateMutex_;             // 状态保护锁
    };

} // namespace PortMaster