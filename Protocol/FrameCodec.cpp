#pragma execution_character_set("utf-8")

#include "pch.h"
#include "FrameCodec.h"
#include "../Common/CommonTypes.h"
#include <algorithm>
#include <cstring>

// 静态成员初始化
uint32_t FrameCodec::s_crcTable[256];
bool FrameCodec::s_crcTableInit = false;

FrameCodec::FrameCodec()
    : m_maxPayloadSize(MAX_PAYLOAD_SIZE)
{
    if (!s_crcTableInit)
    {
        InitCRCTable();
    }
}

FrameCodec::~FrameCodec()
{
}

void FrameCodec::InitCRCTable()
{
    const uint32_t polynomial = 0xEDB88320;
    
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ polynomial;
            }
            else
            {
                crc >>= 1;
            }
        }
        s_crcTable[i] = crc;
    }
    
    s_crcTableInit = true;
}

uint32_t FrameCodec::CalculateCRC32(const uint8_t* data, size_t length)
{
    if (!s_crcTableInit)
    {
        InitCRCTable();
    }
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++)
    {
        uint8_t index = (uint8_t)(crc ^ data[i]);
        crc = (crc >> 8) ^ s_crcTable[index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool FrameCodec::VerifyCRC32(const uint8_t* data, size_t length, uint32_t crc)
{
    return CalculateCRC32(data, length) == crc;
}

std::vector<uint8_t> FrameCodec::EncodeFrame(FrameType type, uint16_t sequence, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> frame;
    
    // 确保负载不超过最大限制
    size_t payloadSize = (std::min)(payload.size(), m_maxPayloadSize);
    
    // 预留空间
    frame.reserve(sizeof(FrameHeader) + payloadSize + sizeof(FrameTail));
    
    // 构建帧头
    FrameHeader header;
    header.magic = HEADER_MAGIC;
    header.type = static_cast<uint8_t>(type);
    header.sequence = sequence;
    header.length = static_cast<uint16_t>(payloadSize);
    
    // 计算CRC32（类型+序号+长度+数据）
    std::vector<uint8_t> crcData;
    crcData.push_back(header.type);
    crcData.push_back(header.sequence & 0xFF);
    crcData.push_back((header.sequence >> 8) & 0xFF);
    crcData.push_back(header.length & 0xFF);
    crcData.push_back((header.length >> 8) & 0xFF);
    crcData.insert(crcData.end(), payload.begin(), payload.begin() + payloadSize);
    
    header.crc32 = CalculateCRC32(crcData.data(), crcData.size());
    
    // 写入帧头
    frame.insert(frame.end(), (uint8_t*)&header, (uint8_t*)&header + sizeof(header));
    
    // 写入负载
    frame.insert(frame.end(), payload.begin(), payload.begin() + payloadSize);
    
    // 写入帧尾
    FrameTail tail;
    tail.magic = TAIL_MAGIC;
    frame.insert(frame.end(), (uint8_t*)&tail, (uint8_t*)&tail + sizeof(tail));
    
    return frame;
}

std::vector<uint8_t> FrameCodec::EncodeStartFrame(uint16_t sequence, const StartMetadata& metadata)
{
    std::vector<uint8_t> payload = SerializeStartMetadata(metadata);
    return EncodeFrame(FrameType::FRAME_START, sequence, payload);
}

std::vector<uint8_t> FrameCodec::EncodeDataFrame(uint16_t sequence, const std::vector<uint8_t>& data)
{
    return EncodeFrame(FrameType::FRAME_DATA, sequence, data);
}

std::vector<uint8_t> FrameCodec::EncodeEndFrame(uint16_t sequence)
{
    std::vector<uint8_t> empty;
    return EncodeFrame(FrameType::FRAME_END, sequence, empty);
}

std::vector<uint8_t> FrameCodec::EncodeAckFrame(uint16_t sequence)
{
    std::vector<uint8_t> empty;
    return EncodeFrame(FrameType::FRAME_ACK, sequence, empty);
}

std::vector<uint8_t> FrameCodec::EncodeNakFrame(uint16_t sequence)
{
    std::vector<uint8_t> empty;
    return EncodeFrame(FrameType::FRAME_NAK, sequence, empty);
}

std::vector<uint8_t> FrameCodec::EncodeHeartbeatFrame(uint16_t sequence)
{
    std::vector<uint8_t> empty;
    return EncodeFrame(FrameType::FRAME_HEARTBEAT, sequence, empty);
}

Frame FrameCodec::DecodeFrame(const std::vector<uint8_t>& data)
{
    Frame frame;
    
    if (data.size() < MIN_FRAME_SIZE)
    {
        frame.valid = false;
        return frame;
    }
    
    // 读取帧头
    const FrameHeader* header = reinterpret_cast<const FrameHeader*>(data.data());
    
    // 验证魔数
    if (header->magic != HEADER_MAGIC)
    {
        frame.valid = false;
        return frame;
    }
    
    // 验证长度
    if (data.size() < sizeof(FrameHeader) + header->length + sizeof(FrameTail))
    {
        frame.valid = false;
        return frame;
    }
    
    // 验证帧尾魔数
    const FrameTail* tail = reinterpret_cast<const FrameTail*>(
        data.data() + sizeof(FrameHeader) + header->length);
    
    if (tail->magic != TAIL_MAGIC)
    {
        frame.valid = false;
        return frame;
    }
    
    // 验证CRC32
    std::vector<uint8_t> crcData;
    crcData.push_back(header->type);
    crcData.push_back(header->sequence & 0xFF);
    crcData.push_back((header->sequence >> 8) & 0xFF);
    crcData.push_back(header->length & 0xFF);
    crcData.push_back((header->length >> 8) & 0xFF);
    
    if (header->length > 0)
    {
        const uint8_t* payloadPtr = data.data() + sizeof(FrameHeader);
        crcData.insert(crcData.end(), payloadPtr, payloadPtr + header->length);
    }
    
    if (!VerifyCRC32(crcData.data(), crcData.size(), header->crc32))
    {
        frame.valid = false;
        return frame;
    }
    
    // 填充帧信息
    frame.type = static_cast<FrameType>(header->type);
    frame.sequence = header->sequence;
    frame.crc32 = header->crc32;
    frame.valid = true;
    
    if (header->length > 0)
    {
        const uint8_t* payloadPtr = data.data() + sizeof(FrameHeader);
        frame.payload.assign(payloadPtr, payloadPtr + header->length);
    }
    
    return frame;
}

bool FrameCodec::DecodeStartMetadata(const std::vector<uint8_t>& payload, StartMetadata& metadata)
{
    if (payload.empty())
    {
        return false;
    }
    
    return DeserializeStartMetadata(payload.data(), payload.size(), metadata);
}

void FrameCodec::AppendData(const std::vector<uint8_t>& data)
{
    m_buffer.insert(m_buffer.end(), data.begin(), data.end());
}

bool FrameCodec::TryGetFrame(Frame& frame)
{
    size_t startPos = 0;
    
    if (FindFrameStart(startPos))
    {
        if (ExtractFrame(startPos, frame))
        {
            return true;
        }
    }
    
    return false;
}

void FrameCodec::ClearBuffer()
{
    m_buffer.clear();
}

void FrameCodec::SetMaxPayloadSize(size_t size)
{
    m_maxPayloadSize = size;
}

bool FrameCodec::FindFrameStart(size_t& startPos)
{
    if (m_buffer.size() < sizeof(FrameHeader))
    {
        return false;
    }
    
    for (size_t i = 0; i <= m_buffer.size() - sizeof(FrameHeader); i++)
    {
        uint16_t magic = *(uint16_t*)(&m_buffer[i]);
        if (magic == HEADER_MAGIC)
        {
            startPos = i;
            return true;
        }
    }
    
    // 未找到帧头，清理无效数据
    if (m_buffer.size() > sizeof(FrameHeader))
    {
        m_buffer.erase(m_buffer.begin(), m_buffer.end() - sizeof(FrameHeader) + 1);
    }
    
    return false;
}

bool FrameCodec::ExtractFrame(size_t startPos, Frame& frame)
{
    if (m_buffer.size() - startPos < sizeof(FrameHeader))
    {
        return false;
    }
    
    const FrameHeader* header = reinterpret_cast<const FrameHeader*>(&m_buffer[startPos]);
    
    size_t frameSize = sizeof(FrameHeader) + header->length + sizeof(FrameTail);
    
    if (m_buffer.size() - startPos < frameSize)
    {
        // 数据不完整
        return false;
    }
    
    // 提取完整帧
    std::vector<uint8_t> frameData(m_buffer.begin() + startPos, 
                                   m_buffer.begin() + startPos + frameSize);
    
    // 解码帧
    frame = DecodeFrame(frameData);
    
    if (frame.valid)
    {
        // 从缓冲区移除已处理的帧
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + startPos + frameSize);
        return true;
    }
    else
    {
        // 无效帧，跳过帧头继续查找
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + startPos + sizeof(uint16_t));
        return false;
    }
}

std::vector<uint8_t> FrameCodec::SerializeStartMetadata(const StartMetadata& metadata)
{
    std::vector<uint8_t> data;
    
    // 版本号
    data.push_back(metadata.version);
    
    // 标志位
    data.push_back(metadata.flags);
    
    // 文件名长度和内容
    uint16_t nameLen = static_cast<uint16_t>(metadata.fileName.length());
    data.push_back(nameLen & 0xFF);
    data.push_back((nameLen >> 8) & 0xFF);
    
    if (nameLen > 0)
    {
        data.insert(data.end(), metadata.fileName.begin(), metadata.fileName.end());
    }
    
    // 文件大小（8字节小端）
    for (int i = 0; i < 8; i++)
    {
        data.push_back((metadata.fileSize >> (i * 8)) & 0xFF);
    }
    
    // 修改时间（8字节小端）
    for (int i = 0; i < 8; i++)
    {
        data.push_back((metadata.modifyTime >> (i * 8)) & 0xFF);
    }
    
    // 会话ID
    data.push_back(metadata.sessionId & 0xFF);
    data.push_back((metadata.sessionId >> 8) & 0xFF);
    
    return data;
}

bool FrameCodec::DeserializeStartMetadata(const uint8_t* data, size_t size, StartMetadata& metadata)
{
    if (size < 4) // 最小长度：版本+标志+文件名长度
    {
        return false;
    }
    
    size_t offset = 0;
    
    // 版本号
    metadata.version = data[offset++];
    
    // 标志位
    metadata.flags = data[offset++];
    
    // 文件名长度
    uint16_t nameLen = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    
    if (offset + nameLen > size)
    {
        return false;
    }
    
    // 文件名
    if (nameLen > 0)
    {
        metadata.fileName.assign((const char*)&data[offset], nameLen);
        offset += nameLen;
    }
    
    if (offset + 18 > size) // 8字节文件大小 + 8字节修改时间 + 2字节会话ID
    {
        return false;
    }
    
    // 文件大小
    metadata.fileSize = 0;
    for (int i = 0; i < 8; i++)
    {
        metadata.fileSize |= (uint64_t)data[offset++] << (i * 8);
    }
    
    // 修改时间
    metadata.modifyTime = 0;
    for (int i = 0; i < 8; i++)
    {
        metadata.modifyTime |= (uint64_t)data[offset++] << (i * 8);
    }
    
    // 会话ID
    metadata.sessionId = data[offset] | (data[offset + 1] << 8);
    
    return true;
}