#include "pch.h"
#include "CRC32.h"

uint32_t CRC32::s_table[256];
bool CRC32::s_tableInitialized = false;

CRC32::CRC32()
    : m_crc(0xFFFFFFFF)
{
    if (!s_tableInitialized)
        InitializeTable();
}

uint32_t CRC32::Calculate(const uint8_t* data, size_t length)
{
    CRC32 crc;
    crc.Update(data, length);
    return crc.GetValue();
}

uint32_t CRC32::Calculate(const std::vector<uint8_t>& data)
{
    return Calculate(data.data(), data.size());
}

void CRC32::Reset()
{
    m_crc = 0xFFFFFFFF;
}

void CRC32::Update(const uint8_t* data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        Update(data[i]);
    }
}

void CRC32::Update(const std::vector<uint8_t>& data)
{
    Update(data.data(), data.size());
}

void CRC32::Update(uint8_t byte)
{
    m_crc = s_table[(m_crc ^ byte) & 0xFF] ^ (m_crc >> 8);
}

uint32_t CRC32::GetValue() const
{
    return m_crc ^ 0xFFFFFFFF;
}

bool CRC32::Verify(const uint8_t* data, size_t length, uint32_t expectedCrc)
{
    return Calculate(data, length) == expectedCrc;
}

bool CRC32::Verify(const std::vector<uint8_t>& data, uint32_t expectedCrc)
{
    return Calculate(data) == expectedCrc;
}

void CRC32::InitializeTable()
{
    const uint32_t polynomial = 0xEDB88320;
    
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ polynomial;
            else
                crc >>= 1;
        }
        s_table[i] = crc;
    }
    
    s_tableInitialized = true;
}