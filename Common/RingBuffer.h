#pragma once

#include <vector>
#include <mutex>
#include <atomic>

// 鑷姩鎵╁睍鐜舰缂撳啿鍖?
class RingBuffer
{
public:
    // SOLID-S: 配置安全常量定义
    static constexpr size_t DEFAULT_INITIAL_SIZE = 4096;
    static constexpr size_t DEFAULT_MAX_SIZE = 1024 * 1024;        // 1MB
    static constexpr size_t MIN_BUFFER_SIZE = 512;                 // 最小限制
    static constexpr size_t MAX_BUFFER_SIZE = 16 * 1024 * 1024;   // 16MB最大限制
    static constexpr size_t SAFETY_MARGIN = 64;                   // 安全边距
    
    explicit RingBuffer(size_t initialSize = DEFAULT_INITIAL_SIZE);
    ~RingBuffer();
    
    // 鍩烘湰鎿嶄綔
    size_t Write(const uint8_t* data, size_t length);
    size_t Write(const std::vector<uint8_t>& data);
    size_t Read(uint8_t* buffer, size_t maxLength);
    size_t Read(std::vector<uint8_t>& buffer, size_t maxLength);
    
    // 鐘舵€佹煡璇?    // 状态查询
    size_t Available() const;           // 可读数据大小
    size_t FreeSpace() const;          // 可写空间大小
    size_t Capacity() const;           // 总容量
    bool IsEmpty() const;              // 是否为空
    bool IsFull() const;               // 是否已满
    
    // 管理操作
    void Clear();                      // 清空缓冲区
    bool Resize(size_t newSize);       // 调整大小
    void SetAutoExpand(bool enable, size_t maxSize = 0); // 设置自动扩展
    
    // SOLID-S: 配置安全验证
    static bool ValidateBufferSize(size_t size);        // 验证缓冲区大小
    bool ValidateMaxSize(size_t maxSize) const;         // 验证最大大小设置
    
    // 非阻塞操作
    uint8_t Peek(size_t offset = 0) const;  // 预览数据（不移除）
    size_t Peek(uint8_t* buffer, size_t length, size_t offset = 0) const;
    size_t Skip(size_t length);        // 跳过数据
    
    // 鏌ユ壘鎿嶄綔
    size_t Find(uint8_t byte, size_t startOffset = 0) const;
    size_t Find(const uint8_t* pattern, size_t patternLength, size_t startOffset = 0) const;

private:
    mutable std::mutex m_mutex;
    std::vector<uint8_t> m_buffer;
    size_t m_readPos;
    size_t m_writePos;
    std::atomic<size_t> m_dataSize;
    bool m_autoExpand;
    size_t m_maxSize;
    
    // 鍐呴儴杈呭姪鏂规硶
    bool ExpandIfNeeded(size_t requiredSize);
    size_t GetContiguousWriteSize() const;
    size_t GetContiguousReadSize() const;
    void AdvanceWritePos(size_t length);
    void AdvanceReadPos(size_t length);
};