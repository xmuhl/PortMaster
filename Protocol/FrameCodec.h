#pragma once
#pragma execution_character_set("utf-8")

#include <vector>
#include <cstdint>
#include <string>

// 帧类型定义
enum class FrameType : uint8_t
{
    FRAME_START = 0x01,    // 开始帧
    FRAME_DATA  = 0x02,    // 数据帧
    FRAME_END   = 0x03,    // 结束帧
    FRAME_ACK   = 0x10,    // 确认帧
    FRAME_NAK   = 0x11,    // 否定帧
    FRAME_HEARTBEAT = 0x20, // 心跳帧
    FRAME_INVALID = 0xFF    // 无效帧
};

// 帧头结构
#pragma pack(push, 1)
struct FrameHeader
{
    uint16_t magic;         // 魔数 0xAA55
    uint8_t type;          // 帧类型
    uint16_t sequence;     // 序列号
    uint16_t length;       // 数据长度
    uint32_t crc32;        // CRC32校验值
};

struct FrameTail
{
    uint16_t magic;        // 尾部魔数 0x55AA
};
#pragma pack(pop)

// 帧数据结构
struct Frame
{
    FrameType type;
    uint16_t sequence;
    std::vector<uint8_t> payload;
    uint32_t crc32;
    bool valid;
    
    Frame() : type(FrameType::FRAME_INVALID), sequence(0), crc32(0), valid(false) {}
};

// START帧元数据
struct StartMetadata
{
    uint8_t version;           // 协议版本
    uint8_t flags;            // 标志位
    std::string fileName;     // 文件名
    uint64_t fileSize;        // 文件大小
    uint64_t modifyTime;      // 修改时间
    uint16_t sessionId;       // 会话ID
    
    StartMetadata() : version(1), flags(0), fileSize(0), modifyTime(0), sessionId(0) {}
};

// 帧编解码器
class FrameCodec
{
public:
    static const uint16_t HEADER_MAGIC = 0xAA55;
    static const uint16_t TAIL_MAGIC = 0x55AA;
    static const size_t MAX_PAYLOAD_SIZE = 1024;
    static const size_t MIN_FRAME_SIZE = sizeof(FrameHeader) + sizeof(FrameTail);
    
public:
    FrameCodec();
    ~FrameCodec();
    
    // 编码帧
    std::vector<uint8_t> EncodeFrame(FrameType type, uint16_t sequence, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> EncodeStartFrame(uint16_t sequence, const StartMetadata& metadata);
    std::vector<uint8_t> EncodeDataFrame(uint16_t sequence, const std::vector<uint8_t>& data);
    std::vector<uint8_t> EncodeEndFrame(uint16_t sequence);
    std::vector<uint8_t> EncodeAckFrame(uint16_t sequence);
    std::vector<uint8_t> EncodeNakFrame(uint16_t sequence);
    std::vector<uint8_t> EncodeHeartbeatFrame(uint16_t sequence);
    
    // 解码帧
    Frame DecodeFrame(const std::vector<uint8_t>& data);
    bool DecodeStartMetadata(const std::vector<uint8_t>& payload, StartMetadata& metadata);
    
    // 添加数据到缓冲区并尝试解析帧
    void AppendData(const std::vector<uint8_t>& data);
    bool TryGetFrame(Frame& frame);
    void ClearBuffer();
    size_t GetBufferSize() const { return m_buffer.size(); }
    
    // 设置最大负载大小
    void SetMaxPayloadSize(size_t size);
    size_t GetMaxPayloadSize() const { return m_maxPayloadSize; }
    
    // CRC32计算
    static uint32_t CalculateCRC32(const uint8_t* data, size_t length);
    static bool VerifyCRC32(const uint8_t* data, size_t length, uint32_t crc);
    
private:
    // 内部辅助函数
    bool FindFrameStart(size_t& startPos);
    bool ExtractFrame(size_t startPos, Frame& frame);
    std::vector<uint8_t> SerializeStartMetadata(const StartMetadata& metadata);
    bool DeserializeStartMetadata(const uint8_t* data, size_t size, StartMetadata& metadata);
    
private:
    std::vector<uint8_t> m_buffer;     // 接收缓冲区
    size_t m_maxPayloadSize;           // 最大负载大小
    static uint32_t s_crcTable[256];   // CRC查找表
    static bool s_crcTableInit;        // CRC表初始化标志
    
    // 初始化CRC表
    static void InitCRCTable();
};