#pragma once

#include <cstdint>
#include <vector>

// CRC32 璁＄畻绫伙紙IEEE 802.3 鏍囧噯锛屽椤瑰紡 0xEDB88320锛?
class CRC32
{
public:
    CRC32();
    
    // 闈欐€佹柟娉曡绠桟RC32
    static uint32_t Calculate(const uint8_t* data, size_t length);
    static uint32_t Calculate(const std::vector<uint8_t>& data);
    
    // 瀹炰緥鏂规硶澧為噺璁＄畻CRC32
    void Reset();
    void Update(const uint8_t* data, size_t length);
    void Update(const std::vector<uint8_t>& data);
    void Update(uint8_t byte);
    uint32_t GetValue() const;
    
    // 楠岃瘉CRC32
    static bool Verify(const uint8_t* data, size_t length, uint32_t expectedCrc);
    static bool Verify(const std::vector<uint8_t>& data, uint32_t expectedCrc);

private:
    uint32_t m_crc;
    static uint32_t s_table[256];
    static bool s_tableInitialized;
    
    static void InitializeTable();
};