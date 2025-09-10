#include "pch.h"
#include "FrameCodec.h"
#include "CRC32.h"

FrameCodec::FrameCodec(size_t maxPayloadSize)
    : m_maxPayloadSize(maxPayloadSize)
{
}

std::vector<uint8_t> FrameCodec::EncodeFrame(const Frame& frame)
{
    std::vector<uint8_t> buffer;
    
    // 帧格式：包头(2B) + 类型(1B) + 序号(2B) + 长度(2B) + CRC32(4B) + 数据(0..N) + 包尾(2B)
    
    // 1. 包头 (0xAA55)
    WriteUint16LE(buffer, FRAME_HEADER);
    
    // 2. 类型 (1B)
    buffer.push_back(static_cast<uint8_t>(frame.type));
    
    // 3. 序号 (2B, 小端)
    WriteUint16LE(buffer, frame.sequence);
    
    // 4. 长度 (2B, 小端) - 负载数据长度
    WriteUint16LE(buffer, static_cast<uint16_t>(frame.payload.size()));
    
    // 5. 计算CRC32 (覆盖类型|序号|长度|数据)
    uint32_t crc = CalculateFrameCrc(frame.type, frame.sequence, frame.payload);
    WriteUint32LE(buffer, crc);
    
    // 6. 数据 (0..N)
    buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());
    
    // 7. 包尾 (0x55AA)
    WriteUint16LE(buffer, FRAME_FOOTER);
    
    return buffer;
}

FrameCodec::DecodeResult FrameCodec::DecodeFrame(const std::vector<uint8_t>& buffer, 
                                                size_t& consumedBytes, Frame& frame)
{
    consumedBytes = 0;
    
    // 最小帧大小检查
    if (buffer.size() < MIN_FRAME_SIZE)
    {
        return DecodeResult::Incomplete;
    }
    
    // 查找帧头
    size_t headerPos = FindFrameHeader(buffer, 0);
    if (headerPos == std::string::npos)
    {
        consumedBytes = buffer.size(); // 没有找到帧头，丢弃所有数据
        return DecodeResult::InvalidFrame;
    }
    
    // 如果帧头不在开始位置，跳过前面的无效数据
    if (headerPos > 0)
    {
        consumedBytes = headerPos;
        return DecodeResult::InvalidFrame;
    }
    
    // 检查是否有足够的数据来解析帧头
    if (buffer.size() < headerPos + MIN_FRAME_SIZE)
    {
        return DecodeResult::Incomplete;
    }
    
    const uint8_t* data = buffer.data() + headerPos;
    
    // 解析帧头部分
    uint16_t header = ReadUint16LE(data + 0);      // 包头
    uint8_t type = data[2];                        // 类型
    uint16_t sequence = ReadUint16LE(data + 3);    // 序号
    uint16_t length = ReadUint16LE(data + 5);      // 负载长度
    uint32_t crc = ReadUint32LE(data + 7);         // CRC32
    
    // 验证帧头
    if (header != FRAME_HEADER)
    {
        consumedBytes = headerPos + 2; // 跳过这个假的帧头
        return DecodeResult::InvalidFrame;
    }
    
    // 检查负载大小
    if (length > m_maxPayloadSize)
    {
        consumedBytes = headerPos + MIN_FRAME_SIZE;
        return DecodeResult::PayloadTooLarge;
    }
    
    // 计算完整帧大小
    size_t totalFrameSize = MIN_FRAME_SIZE + length;
    if (buffer.size() < headerPos + totalFrameSize)
    {
        return DecodeResult::Incomplete;
    }
    
    // 检查帧尾
    uint16_t footer = ReadUint16LE(data + MIN_FRAME_SIZE - 2 + length);
    if (footer != FRAME_FOOTER)
    {
        consumedBytes = headerPos + MIN_FRAME_SIZE;
        return DecodeResult::InvalidFrame;
    }
    
    // 提取负载数据
    std::vector<uint8_t> payload;
    if (length > 0)
    {
        payload.assign(data + 11, data + 11 + length);
    }
    
    // 验证CRC
    uint32_t calculatedCrc = CalculateFrameCrc(static_cast<FrameType>(type), sequence, payload);
    if (crc != calculatedCrc)
    {
        consumedBytes = headerPos + totalFrameSize;
        return DecodeResult::CrcError;
    }
    
    // 构建解码后的帧
    frame.type = static_cast<FrameType>(type);
    frame.sequence = sequence;
    frame.payload = std::move(payload);
    frame.crc = crc;
    
    consumedBytes = headerPos + totalFrameSize;
    return DecodeResult::Success;
}

Frame FrameCodec::CreateStartFrame(uint16_t sequence, const StartMetadata& metadata)
{
    Frame frame(FRAME_START, sequence);
    frame.payload = metadata.Serialize();
    return frame;
}

Frame FrameCodec::CreateDataFrame(uint16_t sequence, const std::vector<uint8_t>& data)
{
    return Frame(FRAME_DATA, sequence, data);
}

Frame FrameCodec::CreateEndFrame(uint16_t sequence)
{
    return Frame(FRAME_END, sequence);
}

Frame FrameCodec::CreateAckFrame(uint16_t sequence)
{
    return Frame(FRAME_ACK, sequence);
}

Frame FrameCodec::CreateNakFrame(uint16_t sequence)
{
    return Frame(FRAME_NAK, sequence);
}

bool FrameCodec::ValidateFrame(const Frame& frame) const
{
    // 验证帧类型
    if (frame.type < FRAME_START || frame.type > FRAME_NAK)
        return false;
    
    // 验证负载大小
    if (frame.payload.size() > m_maxPayloadSize)
        return false;
    
    // 验证CRC
    uint32_t calculatedCrc = CalculateFrameCrc(frame.type, frame.sequence, frame.payload);
    return (frame.crc == calculatedCrc);
}

size_t FrameCodec::FindFrameHeader(const std::vector<uint8_t>& buffer, size_t startPos)
{
    if (buffer.size() < startPos + 2)
    {
        return std::string::npos;
    }
    
    // 查找帧头标识 0xAA55 (小端序)
    for (size_t i = startPos; i <= buffer.size() - 2; ++i)
    {
        if (buffer[i] == 0x55 && buffer[i + 1] == 0xAA) // 小端序：0xAA55 -> 0x55, 0xAA
        {
            return i;
        }
    }
    
    return std::string::npos;
}

uint32_t FrameCodec::CalculateFrameCrc(FrameType type, uint16_t sequence, 
                                      const std::vector<uint8_t>& payload) const
{
    // CRC32覆盖：类型(1B) + 序号(2B) + 长度(2B) + 数据(0..N)
    std::vector<uint8_t> crcData;
    
    // 1. 类型 (1B)
    crcData.push_back(static_cast<uint8_t>(type));
    
    // 2. 序号 (2B, 小端)
    WriteUint16LE(crcData, sequence);
    
    // 3. 长度 (2B, 小端)
    WriteUint16LE(crcData, static_cast<uint16_t>(payload.size()));
    
    // 4. 数据 (0..N)
    crcData.insert(crcData.end(), payload.begin(), payload.end());
    
    // 使用CRC32算法计算校验码
    return CRC32::Calculate(crcData.data(), crcData.size());
}

void FrameCodec::WriteUint16LE(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void FrameCodec::WriteUint32LE(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void FrameCodec::WriteUint64LE(std::vector<uint8_t>& buffer, uint64_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
}

uint16_t FrameCodec::ReadUint16LE(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t FrameCodec::ReadUint32LE(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t FrameCodec::ReadUint64LE(const uint8_t* data)
{
    return static_cast<uint64_t>(data[0]) |
           (static_cast<uint64_t>(data[1]) << 8) |
           (static_cast<uint64_t>(data[2]) << 16) |
           (static_cast<uint64_t>(data[3]) << 24) |
           (static_cast<uint64_t>(data[4]) << 32) |
           (static_cast<uint64_t>(data[5]) << 40) |
           (static_cast<uint64_t>(data[6]) << 48) |
           (static_cast<uint64_t>(data[7]) << 56);
}

// StartMetadata 实现
std::vector<uint8_t> StartMetadata::Serialize() const
{
    std::vector<uint8_t> data;
    
    // 格式: ver(1B) + flags(1B) + window_size(1B) + name_len(2B) + name(UTF-8) + file_size(8B) + [mtime(8B)]
    
    // 1. 版本号 (1B)
    data.push_back(version);
    
    // 2. 标志位 (1B) - bit 0: 是否包含修改时间
    uint8_t flagsValue = flags;
    if (modifyTime != 0)
        flagsValue |= 0x01; // 设置bit 0表示包含修改时间
    data.push_back(flagsValue);
    
    // 3. 滑动窗口大小 (1B)
    data.push_back(window_size);
    
    // 4. 文件名转换为UTF-8字节
    std::string filenameUtf8 = filename; // 假设已经是UTF-8编码
    uint16_t nameLen = static_cast<uint16_t>(filenameUtf8.size());
    
    // 5. 文件名长度 (2B, 小端)
    FrameCodec::WriteUint16LE(data, nameLen);
    
    // 6. 文件名 (UTF-8)
    data.insert(data.end(), filenameUtf8.begin(), filenameUtf8.end());
    
    // 7. 文件大小 (8B, 小端)
    FrameCodec::WriteUint64LE(data, fileSize);
    
    // 8. 修改时间 (8B, 小端) - 可选
    if (modifyTime != 0)
    {
        FrameCodec::WriteUint64LE(data, modifyTime);
    }
    
    return data;
}

bool StartMetadata::Deserialize(const std::vector<uint8_t>& data)
{
    if (data.size() < 5) // 最小大小：ver(1) + flags(1) + window_size(1) + name_len(2)
        return false;
    
    size_t offset = 0;
    
    // 1. 版本号 (1B)
    version = data[offset++];
    
    // 2. 标志位 (1B)
    flags = data[offset++];
    bool hasModifyTime = (flags & 0x01) != 0;
    
    // 3. 滑动窗口大小 (1B)
    window_size = data[offset++];
    if (window_size == 0) window_size = 1; // 确保至少为1
    
    // 4. 文件名长度 (2B, 小端)
    if (offset + 2 > data.size())
        return false;
    uint16_t nameLen = FrameCodec::ReadUint16LE(data.data() + offset);
    offset += 2;
    
    // 5. 文件名 (UTF-8)
    if (offset + nameLen > data.size())
        return false;
    filename.assign(data.begin() + offset, data.begin() + offset + nameLen);
    offset += nameLen;
    
    // 6. 文件大小 (8B, 小端)
    if (offset + 8 > data.size())
        return false;
    fileSize = FrameCodec::ReadUint64LE(data.data() + offset);
    offset += 8;
    
    // 7. 修改时间 (8B, 小端) - 可选
    if (hasModifyTime)
    {
        if (offset + 8 > data.size())
            return false;
        modifyTime = FrameCodec::ReadUint64LE(data.data() + offset);
        offset += 8;
    }
    else
    {
        modifyTime = 0;
    }
    
    return true;
}