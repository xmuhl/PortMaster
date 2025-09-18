#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "ReliableChannel.h"

namespace PortMaster {

    ReliableChannel::ReliableChannel()
        : state_(ReliableState::RELIABLE_IDLE)
        , currentSequence_(0)
        , expectedSequence_(0)
        , lastSendTime_(0)
        , retryCount_(0) {

        // 设置默认配置
        config_.windowSize = 4;
        config_.timeoutMs = 500;
        config_.maxRetries = 3;
        config_.enableChecksum = true;

        // 初始化统计信息
        memset(&stats_, 0, sizeof(stats_));
        memset(&sessionMeta_, 0, sizeof(sessionMeta_));
    }

    ReliableChannel::~ReliableChannel() {
        Reset();
    }

    void ReliableChannel::AttachTransport(std::shared_ptr<ITransport> transport) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        transport_ = transport;
    }

    void ReliableChannel::Configure(const ReliableConfig& config) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        config_ = config;
    }

    bool ReliableChannel::StartSession(const SessionMeta& meta) {
        std::lock_guard<std::mutex> lock(stateMutex_);

        if (!transport_ || state_ != ReliableState::RELIABLE_IDLE) {
            return false;
        }

        sessionMeta_ = meta;
        state_ = ReliableState::RELIABLE_STARTING;

        // 重置统计信息
        memset(&stats_, 0, sizeof(stats_));
        stats_.startTime = GetTickCount();

        // 清空窗口
        sendWindow_.clear();
        receiveWindow_.clear();

        currentSequence_ = 0;
        expectedSequence_ = 0;
        retryCount_ = 0;

        return true;
    }

    ReliableEventStep ReliableChannel::Pump(const BufferView& payload) {
        std::lock_guard<std::mutex> lock(stateMutex_);

        switch (state_) {
        case ReliableState::RELIABLE_IDLE:
            return HandleIdleState(payload);
        case ReliableState::RELIABLE_STARTING:
            return HandleStartingState(payload);
        case ReliableState::RELIABLE_SENDING:
            return HandleSendingState(payload);
        case ReliableState::RELIABLE_ENDING:
            return HandleEndingState(payload);
        case ReliableState::RELIABLE_READY:
            return HandleReadyState(payload);
        case ReliableState::RELIABLE_RECEIVING:
            return HandleReceivingState(payload);
        case ReliableState::RELIABLE_DONE:
        case ReliableState::RELIABLE_FAILED:
            return ReliableEventStep::STEP_COMPLETED;
        default:
            return ReliableEventStep::STEP_FAILED;
        }
    }

    ReliableStats ReliableChannel::GetStats() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return stats_;
    }

    ReliableState ReliableChannel::GetState() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return state_;
    }

    void ReliableChannel::Reset() {
        std::lock_guard<std::mutex> lock(stateMutex_);

        state_ = ReliableState::RELIABLE_IDLE;
        sendWindow_.clear();
        receiveWindow_.clear();
        currentSequence_ = 0;
        expectedSequence_ = 0;
        retryCount_ = 0;

        memset(&stats_, 0, sizeof(stats_));
        memset(&sessionMeta_, 0, sizeof(sessionMeta_));
    }

    // 状态机处理方法（骨架实现）
    ReliableEventStep ReliableChannel::HandleIdleState(const BufferView& payload) {
        UNREFERENCED_PARAMETER(payload);
        // TODO: 实现空闲状态处理
        return ReliableEventStep::STEP_WAIT_DATA;
    }

    ReliableEventStep ReliableChannel::HandleStartingState(const BufferView& payload) {
        UNREFERENCED_PARAMETER(payload);
        // TODO: 实现启动状态处理
        // 发送START帧包含会话元数据
        state_ = ReliableState::RELIABLE_SENDING;
        return ReliableEventStep::STEP_CONTINUE;
    }

    ReliableEventStep ReliableChannel::HandleSendingState(const BufferView& payload) {
        // TODO: 实现发送状态处理
        // 处理数据分片和窗口管理
        if (payload.empty()) {
            state_ = ReliableState::RELIABLE_ENDING;
            return ReliableEventStep::STEP_CONTINUE;
        }
        return ReliableEventStep::STEP_WAIT_DATA;
    }

    ReliableEventStep ReliableChannel::HandleEndingState(const BufferView& payload) {
        UNREFERENCED_PARAMETER(payload);
        // TODO: 实现结束状态处理
        // 发送END帧并等待确认
        state_ = ReliableState::RELIABLE_DONE;
        stats_.endTime = GetTickCount();
        return ReliableEventStep::STEP_COMPLETED;
    }

    ReliableEventStep ReliableChannel::HandleReadyState(const BufferView& payload) {
        UNREFERENCED_PARAMETER(payload);
        // TODO: 实现准备接收状态处理
        state_ = ReliableState::RELIABLE_RECEIVING;
        return ReliableEventStep::STEP_CONTINUE;
    }

    ReliableEventStep ReliableChannel::HandleReceivingState(const BufferView& payload) {
        UNREFERENCED_PARAMETER(payload);
        // TODO: 实现接收状态处理
        // 处理接收到的帧并发送确认
        return ReliableEventStep::STEP_SEND_ACK;
    }

    // 帧处理方法（骨架实现）
    bool ReliableChannel::SendFrame(FrameType type, const BufferView& data) {
        UNREFERENCED_PARAMETER(type);
        if (!transport_) {
            return false;
        }

        // TODO: 实现帧格式化和发送
        // 格式：0xAA55 + Type + Sequence + Length + CRC32 + Data + 0x55AA

        stats_.framesSent++;
        lastSendTime_ = GetTickCount();

        return transport_->Send(data);
    }

    bool ReliableChannel::ReceiveFrame(Buffer& frameData, FrameType& type) {
        if (!transport_) {
            return false;
        }

        // TODO: 实现帧接收和解析
        Buffer buffer;
        if (transport_->Receive(buffer, config_.timeoutMs)) {
            frameData = std::move(buffer);
            type = FrameType::FRAME_DATA; // 临时设置
            stats_.framesReceived++;
            return true;
        }

        return false;
    }

    uint32_t ReliableChannel::CalculateChecksum(const BufferView& data) {
        // TODO: 实现CRC32校验算法（IEEE 802.3标准）
        // 多项式：0xEDB88320
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < data.size(); ++i) {
            crc ^= data.data()[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
        }

        return crc ^ 0xFFFFFFFF;
    }

    bool ReliableChannel::ValidateChecksum(const BufferView& data, uint32_t expected) {
        if (!config_.enableChecksum) {
            return true;
        }

        uint32_t calculated = CalculateChecksum(data);
        if (calculated != expected) {
            stats_.checksumErrors++;
            return false;
        }

        return true;
    }

} // namespace PortMaster


