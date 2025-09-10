#include "pch.h"
#include "RingBuffer.h"
#include <algorithm>
#include <cstring>

RingBuffer::RingBuffer(size_t initialSize)
    : m_buffer(ValidateBufferSize(initialSize) ? initialSize : DEFAULT_INITIAL_SIZE)
    , m_readPos(0)
    , m_writePos(0)
    , m_dataSize(0)
    , m_autoExpand(true)
    , m_maxSize(DEFAULT_MAX_SIZE) // 使用安全的默认配置
{
    // SOLID-S: 构造函数参数验证确保安全性
    if (!ValidateBufferSize(initialSize)) {
        // 记录警告但不抛出异常，使用安全默认值
        // 可通过日志系统记录配置问题
    }
}

RingBuffer::~RingBuffer()
{
}

size_t RingBuffer::Write(const uint8_t* data, size_t length)
{
    if (!data || length == 0) return 0;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // SOLID-S: 检查并自动扩展缓冲区
    if (m_autoExpand && FreeSpace() < length) {
        if (!ExpandIfNeeded(m_dataSize + length)) {
            return 0; // 扩展失败
        }
    }
    
    size_t availableSpace = FreeSpace();
    size_t writeSize = std::min(length, availableSpace);
    
    if (writeSize == 0) return 0;
    
    // DRY: 处理环形写入的两种情况
    size_t firstPart = std::min(writeSize, m_buffer.size() - m_writePos);
    memcpy(&m_buffer[m_writePos], data, firstPart);
    
    if (firstPart < writeSize) {
        // 需要从缓冲区开始处继续写入
        size_t secondPart = writeSize - firstPart;
        memcpy(&m_buffer[0], data + firstPart, secondPart);
    }
    
    AdvanceWritePos(writeSize);
    return writeSize;
}

size_t RingBuffer::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

size_t RingBuffer::Read(uint8_t* buffer, size_t maxLength)
{
    if (!buffer || maxLength == 0) return 0;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableData = Available();
    size_t readSize = std::min(maxLength, availableData);
    
    if (readSize == 0) return 0;
    
    // DRY: 处理环形读取的两种情况
    size_t firstPart = std::min(readSize, m_buffer.size() - m_readPos);
    memcpy(buffer, &m_buffer[m_readPos], firstPart);
    
    if (firstPart < readSize) {
        // 需要从缓冲区开始处继续读取
        size_t secondPart = readSize - firstPart;
        memcpy(buffer + firstPart, &m_buffer[0], secondPart);
    }
    
    AdvanceReadPos(readSize);
    return readSize;
}

size_t RingBuffer::Read(std::vector<uint8_t>& buffer, size_t maxLength)
{
    buffer.resize(maxLength);
    size_t bytesRead = Read(buffer.data(), maxLength);
    buffer.resize(bytesRead);
    return bytesRead;
}

size_t RingBuffer::Available() const
{
    return m_dataSize.load();
}

size_t RingBuffer::FreeSpace() const
{
    return m_buffer.size() - Available();
}

size_t RingBuffer::Capacity() const
{
    return m_buffer.size();
}

bool RingBuffer::IsEmpty() const
{
    return Available() == 0;
}

bool RingBuffer::IsFull() const
{
    return Available() == Capacity();
}

void RingBuffer::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_readPos = m_writePos = 0;
    m_dataSize = 0;
}

bool RingBuffer::Resize(size_t newSize)
{
    if (newSize == 0) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t currentData = Available();
    if (currentData > newSize) {
        return false; // 新尺寸太小，无法保存现有数据
    }
    
    // SOLID-S: 保存现有数据并调整大小
    std::vector<uint8_t> newBuffer(newSize);
    
    if (currentData > 0) {
        // 将现有数据复制到新缓冲区
        if (m_readPos <= m_writePos) {
            memcpy(&newBuffer[0], &m_buffer[m_readPos], currentData);
        } else {
            size_t firstPart = m_buffer.size() - m_readPos;
            memcpy(&newBuffer[0], &m_buffer[m_readPos], firstPart);
            memcpy(&newBuffer[firstPart], &m_buffer[0], currentData - firstPart);
        }
    }
    
    m_buffer = std::move(newBuffer);
    m_readPos = 0;
    m_writePos = currentData;
    
    return true;
}

void RingBuffer::SetAutoExpand(bool enable, size_t maxSize)
{
    m_autoExpand = enable;
    if (maxSize > 0)
        m_maxSize = maxSize;
}

uint8_t RingBuffer::Peek(size_t offset) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (offset >= Available()) {
        return 0; // 超出可用数据范围
    }
    
    // SOLID-S: 计算对应的环形位置
    size_t pos = (m_readPos + offset) % m_buffer.size();
    return m_buffer[pos];
}

size_t RingBuffer::Peek(uint8_t* buffer, size_t length, size_t offset) const
{
    if (!buffer || length == 0) return 0;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableData = Available();
    if (offset >= availableData) return 0;
    
    size_t readableSize = std::min(length, availableData - offset);
    size_t startPos = (m_readPos + offset) % m_buffer.size();
    
    // DRY: 处理环形读取
    size_t firstPart = std::min(readableSize, m_buffer.size() - startPos);
    memcpy(buffer, &m_buffer[startPos], firstPart);
    
    if (firstPart < readableSize) {
        size_t secondPart = readableSize - firstPart;
        memcpy(buffer + firstPart, &m_buffer[0], secondPart);
    }
    
    return readableSize;
}

size_t RingBuffer::Skip(size_t length)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableData = Available();
    size_t skipSize = std::min(length, availableData);
    
    if (skipSize > 0) {
        AdvanceReadPos(skipSize);
    }
    
    return skipSize;
}

size_t RingBuffer::Find(uint8_t byte, size_t startOffset) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableData = Available();
    if (startOffset >= availableData) return std::string::npos;
    
    // KISS: 简单的线性查找
    for (size_t i = startOffset; i < availableData; i++) {
        size_t pos = (m_readPos + i) % m_buffer.size();
        if (m_buffer[pos] == byte) {
            return i;
        }
    }
    
    return std::string::npos;
}

size_t RingBuffer::Find(const uint8_t* pattern, size_t patternLength, size_t startOffset) const
{
    if (!pattern || patternLength == 0) return std::string::npos;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableData = Available();
    if (startOffset >= availableData || patternLength > availableData - startOffset) {
        return std::string::npos;
    }
    
    // KISS: 简单的模式匹配
    for (size_t i = startOffset; i <= availableData - patternLength; i++) {
        bool found = true;
        for (size_t j = 0; j < patternLength; j++) {
            size_t pos = (m_readPos + i + j) % m_buffer.size();
            if (m_buffer[pos] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    
    return std::string::npos;
}

bool RingBuffer::ExpandIfNeeded(size_t requiredSize)
{
    if (!m_autoExpand || requiredSize <= m_buffer.size()) {
        return requiredSize <= m_buffer.size();
    }
    
    // YAGNI: 计算新尺寸（双倍扩展策略）
    size_t newSize = m_buffer.size();
    while (newSize < requiredSize) {
        newSize *= 2;
        if (m_maxSize > 0 && newSize > m_maxSize) {
            newSize = m_maxSize;
            break;
        }
    }
    
    if (newSize < requiredSize) return false; // 超过最大限制
    
    // SOLID-S: 保存现有数据并扩展缓冲区
    std::vector<uint8_t> newBuffer(newSize);
    size_t dataSize = Available();
    
    if (dataSize > 0) {
        // 将现有数据复制到新缓冲区的开始处
        if (m_readPos <= m_writePos) {
            memcpy(&newBuffer[0], &m_buffer[m_readPos], dataSize);
        } else {
            // 数据分两段
            size_t firstPart = m_buffer.size() - m_readPos;
            memcpy(&newBuffer[0], &m_buffer[m_readPos], firstPart);
            memcpy(&newBuffer[firstPart], &m_buffer[0], dataSize - firstPart);
        }
    }
    
    // 更新缓冲区和指针
    m_buffer = std::move(newBuffer);
    m_readPos = 0;
    m_writePos = dataSize;
    
    return true;
}

size_t RingBuffer::GetContiguousWriteSize() const
{
    // SOLID-S: 计算从当前写位置到缓冲区末尾的连续空间
    if (m_writePos >= m_readPos || IsEmpty()) {
        return m_buffer.size() - m_writePos;
    } else {
        return m_readPos - m_writePos;
    }
}

size_t RingBuffer::GetContiguousReadSize() const
{
    // SOLID-S: 计算从当前读位置到数据末尾的连续数据大小
    if (IsEmpty()) return 0;
    
    if (m_readPos < m_writePos) {
        return m_writePos - m_readPos;
    } else {
        return m_buffer.size() - m_readPos;
    }
}

void RingBuffer::AdvanceWritePos(size_t length)
{
    // SOLID-S: 线程安全的写指针推进
    m_writePos = (m_writePos + length) % m_buffer.size();
    m_dataSize += length;
}

void RingBuffer::AdvanceReadPos(size_t length)
{
    // SOLID-S: 线程安全的读指针推进
    m_readPos = (m_readPos + length) % m_buffer.size();
    m_dataSize -= length;
}

// SOLID-S: 配置安全验证实现
bool RingBuffer::ValidateBufferSize(size_t size)
{
    return size >= MIN_BUFFER_SIZE && size <= MAX_BUFFER_SIZE && 
           (size & (size - 1)) == 0; // 检查是否为2的幂（可选优化）
}

bool RingBuffer::ValidateMaxSize(size_t maxSize) const
{
    return maxSize >= m_buffer.size() && maxSize <= MAX_BUFFER_SIZE;
}