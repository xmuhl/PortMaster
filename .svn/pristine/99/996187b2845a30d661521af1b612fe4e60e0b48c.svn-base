#pragma once

#include <vector>
#include <cstdint>
#include <string>

// 帧类型定义
enum FrameType
{
    FRAME_START = 0x01,   // 开始帧
    FRAME_DATA = 0x02,    // 数据帧
    FRAME_END = 0x03,     // 结束帧
    FRAME_ACK = 0x10,     // 确认帧
    FRAME_NAK = 0x11      // 否定确认帧
};

// 帧结构
struct Frame
{
    FrameType type;                // 帧类型
    uint16_t sequence;             // 序列号
    std::vector<uint8_t> payload;  // 负载数据
    uint32_t crc;                  // CRC校验
    
    Frame() : type(FRAME_DATA), sequence(0), crc(0) {}
    Frame(FrameType t, uint16_t seq) : type(t), sequence(seq), crc(0) {}
    Frame(FrameType t, uint16_t seq, const std::vector<uint8_t>& data) 
        : type(t), sequence(seq), payload(data), crc(0) {}
};

// START帧元数据结构
struct StartMetadata
{
    uint8_t version;           // 版本号
    uint8_t flags;             // 标志位
    uint8_t window_size;       // 滑动窗口大小（1-255）
    std::string filename;      // 文件名（UTF-8）
    uint64_t fileSize;         // 文件大小
    uint64_t modifyTime;       // 修改时间（可选）
    
    StartMetadata() 
        : version(1), flags(0), window_size(8), fileSize(0), modifyTime(0) {}
    
    std::vector<uint8_t> Serialize() const;
    bool Deserialize(const std::vector<uint8_t>& data);
};

// 帧编解码器
class FrameCodec
{
public:
    static const uint16_t FRAME_HEADER = 0xAA55;  // 包头
    static const uint16_t FRAME_FOOTER = 0x55AA;  // 包尾
    static const size_t DEFAULT_MAX_PAYLOAD = 1024; // 默认最大负载
    static const size_t MIN_FRAME_SIZE = 13;      // 最小帧大小（头+类型+序号+长度+CRC+尾）

    FrameCodec(size_t maxPayloadSize = DEFAULT_MAX_PAYLOAD);

    // 编码帧为字节流
    std::vector<uint8_t> EncodeFrame(const Frame& frame);
    
    // 从字节流解码帧
    enum class DecodeResult
    {
        Success,        // 成功解码
        Incomplete,     // 数据不完整，需要更多数据
        InvalidFrame,   // 帧格式无效
        CrcError,       // CRC校验错误
        PayloadTooLarge // 负载过大
    };
    
    DecodeResult DecodeFrame(const std::vector<uint8_t>& buffer, size_t& consumedBytes, Frame& frame);
    
    // 创建特定类型的帧
    static Frame CreateStartFrame(uint16_t sequence, const StartMetadata& metadata);
    static Frame CreateDataFrame(uint16_t sequence, const std::vector<uint8_t>& data);
    static Frame CreateEndFrame(uint16_t sequence);
    static Frame CreateAckFrame(uint16_t sequence);
    static Frame CreateNakFrame(uint16_t sequence);
    
    // 验证帧的有效性
    bool ValidateFrame(const Frame& frame) const;
    
    // 获取/设置最大负载大小
    size_t GetMaxPayloadSize() const { return m_maxPayloadSize; }
    void SetMaxPayloadSize(size_t size) { m_maxPayloadSize = size; }
    
    // 同步查找：在数据流中查找帧头
    static size_t FindFrameHeader(const std::vector<uint8_t>& buffer, size_t startPos = 0);
    
    // 字节序转换工具（公共接口）
    static void WriteUint16LE(std::vector<uint8_t>& buffer, uint16_t value);
    static void WriteUint32LE(std::vector<uint8_t>& buffer, uint32_t value);
    static void WriteUint64LE(std::vector<uint8_t>& buffer, uint64_t value);
    static uint16_t ReadUint16LE(const uint8_t* data);
    static uint32_t ReadUint32LE(const uint8_t* data);
    static uint64_t ReadUint64LE(const uint8_t* data);

private:
    size_t m_maxPayloadSize;
    
    // 计算帧的CRC（不包括帧头和帧尾）
    uint32_t CalculateFrameCrc(FrameType type, uint16_t sequence, 
                               const std::vector<uint8_t>& payload) const;
};