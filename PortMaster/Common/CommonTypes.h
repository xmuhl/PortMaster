#pragma once

#pragma execution_character_set("utf-8")

#include "../stdafx.h"

namespace PortMaster {

    // 传输状态枚举
    enum class TransportStatus {
        TRANSPORT_CLOSED = 0,           // 传输已关闭
        TRANSPORT_OPENING,              // 传输正在打开
        TRANSPORT_OPEN,                 // 传输已打开
        TRANSPORT_CLOSING,              // 传输正在关闭
        TRANSPORT_ERROR                 // 传输错误状态
    };

    // 可靠传输状态枚举
    enum class ReliableState {
        RELIABLE_IDLE = 0,              // 空闲状态
        RELIABLE_STARTING,              // 启动中
        RELIABLE_SENDING,               // 发送中
        RELIABLE_ENDING,                // 结束中
        RELIABLE_READY,                 // 准备接收
        RELIABLE_RECEIVING,             // 接收中
        RELIABLE_DONE,                  // 完成
        RELIABLE_FAILED                 // 失败
    };

    // 帧类型枚举
    enum class FrameType : uint8_t {
        FRAME_START = 0x01,             // 启动帧
        FRAME_DATA = 0x02,              // 数据帧
        FRAME_END = 0x03,               // 结束帧
        FRAME_ACK = 0x04,               // 确认帧
        FRAME_NAK = 0x05                // 否认帧
    };

    // 缓冲区视图（只读）
    class BufferView {
    public:
        BufferView() : data_(nullptr), size_(0) {}
        BufferView(const void* data, size_t size) : data_(static_cast<const uint8_t*>(data)), size_(size) {}

        const uint8_t* data() const { return data_; }
        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

    private:
        const uint8_t* data_;
        size_t size_;
    };

    // 可写缓冲区
    class Buffer {
    public:
        Buffer() = default;
        explicit Buffer(size_t size) : data_(size) {}

        uint8_t* data() { return data_.data(); }
        const uint8_t* data() const { return data_.data(); }
        size_t size() const { return data_.size(); }
        void resize(size_t size) { data_.resize(size); }
        void clear() { data_.clear(); }
        bool empty() const { return data_.empty(); }

        BufferView view() const { return BufferView(data(), size()); }

    private:
        std::vector<uint8_t> data_;
    };

    // 端口配置
    struct PortConfig {
        CString portName;               // 端口名称（如"COM1", "LPT1", "USB001"）
        DWORD baudRate = 9600;          // 波特率（串口用）
        DWORD timeout = 5000;           // 超时时间（毫秒）
        CString hostAddress;            // 主机地址（网络打印用）
        WORD port = 9100;               // 端口号（网络打印用）
    };

    // 传输上下文
    struct TransportContext {
        DWORD lastError = 0;            // 最后错误码
        CString errorMessage;           // 错误消息
        DWORD bytesTransferred = 0;     // 传输字节数
        DWORD totalBytes = 0;           // 总字节数
    };

    // 可靠传输配置
    struct ReliableConfig {
        uint32_t windowSize = 4;        // 滑动窗口大小
        uint32_t timeoutMs = 500;       // 超时时间（毫秒）
        uint32_t maxRetries = 3;        // 最大重试次数
        bool enableChecksum = true;     // 启用校验和
    };

    // 会话元数据
    struct SessionMeta {
        CString fileName;               // 文件名
        uint64_t fileSize = 0;          // 文件大小
        uint32_t sessionId = 0;         // 会话ID
        SYSTEMTIME createTime;          // 创建时间
    };

    // 可靠传输事件步骤
    enum class ReliableEventStep {
        STEP_CONTINUE = 0,              // 继续处理
        STEP_WAIT_DATA,                 // 等待数据
        STEP_SEND_ACK,                  // 发送确认
        STEP_SEND_NAK,                  // 发送否认
        STEP_COMPLETED,                 // 完成
        STEP_FAILED                     // 失败
    };

    // 可靠传输统计信息
    struct ReliableStats {
        uint64_t bytesSent = 0;         // 发送字节数
        uint64_t bytesReceived = 0;     // 接收字节数
        uint32_t framesSent = 0;        // 发送帧数
        uint32_t framesReceived = 0;    // 接收帧数
        uint32_t retransmissions = 0;   // 重传次数
        uint32_t checksumErrors = 0;    // 校验错误数
        DWORD startTime = 0;            // 开始时间
        DWORD endTime = 0;              // 结束时间
    };

    // 日志级别
    enum class LogLevel {
        LOG_DEBUG = 0,                  // 调试信息
        LOG_INFO,                       // 一般信息
        LOG_WARNING,                    // 警告
        LOG_ERROR                       // 错误
    };

    // 日志条目
    struct LogEntry {
        SYSTEMTIME timestamp;           // 时间戳
        LogLevel level;                 // 日志级别
        CString module;                 // 模块名称
        CString port;                   // 端口标识
        CString message;                // 消息内容
        DWORD taskId = 0;               // 任务ID
    };

} // namespace PortMaster