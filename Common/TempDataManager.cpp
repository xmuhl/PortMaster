#include "pch.h"
#include "TempDataManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <Windows.h>
#include <ShlObj.h>

// 外部调试日志函数声明
extern void WriteDebugLog(const char* message);

// CRC32查找表（用于校验和计算）
static const uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// 全局单例实例
static std::unique_ptr<TempDataManager> g_tempDataManager;
static std::mutex g_singletonMutex;

// 构造函数
TempDataManager::TempDataManager(const CacheConfig& config)
    : m_config(config)
    , m_currentMemoryUsage(0)
    , m_initialized(false)
{
    WriteDebugLog("[TempDataManager] 初始化临时数据管理器");
    
    // 初始化临时目录
    if (InitializeTempDirectory()) {
        m_initialized = true;
        WriteDebugLog("[TempDataManager] 临时数据管理器初始化成功");
    } else {
        WriteDebugLog("[ERROR] TempDataManager初始化失败：无法创建临时目录");
    }
}

// 析构函数
TempDataManager::~TempDataManager()
{
    WriteDebugLog("[TempDataManager] 开始清理临时数据管理器");
    
    try {
        // 清理所有缓存数据
        ClearAllCache();
        
        // 尝试删除临时目录（如果为空）
        if (!m_tempDirectoryPath.empty() && std::filesystem::exists(m_tempDirectoryPath)) {
            std::error_code ec;
            if (std::filesystem::is_empty(m_tempDirectoryPath, ec) && !ec) {
                std::filesystem::remove(m_tempDirectoryPath, ec);
                if (!ec) {
                    WriteDebugLog("[TempDataManager] 临时目录已清理");
                }
            }
        }
        
        WriteDebugLog("[TempDataManager] 临时数据管理器清理完成");
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] TempDataManager析构异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
    } catch (...) {
        WriteDebugLog("[ERROR] TempDataManager析构发生未知异常");
    }
}

// 缓存数据到临时存储
bool TempDataManager::CacheData(const std::vector<uint8_t>& data, const std::string& identifier)
{
    if (!m_initialized) {
        WriteDebugLog("[ERROR] TempDataManager未初始化，无法缓存数据");
        return false;
    }
    
    if (data.empty() || identifier.empty()) {
        WriteDebugLog("[ERROR] 缓存数据或标识符为空");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    try {
        // 检查是否已存在相同标识符的数据
        if (m_metadataMap.find(identifier) != m_metadataMap.end()) {
            WriteDebugLog("[WARNING] 标识符已存在，将覆盖原有数据");
            RemoveCachedData(identifier);
        }
        
        // 创建文件元数据
        FileMetadata metadata;
        metadata.fileName = identifier;
        metadata.fileSize = data.size();
        metadata.createTime = std::chrono::system_clock::now();
        metadata.lastAccess = metadata.createTime;
        metadata.checksum = CalculateCRC32(data);
        
        // 根据数据大小选择缓存策略
        if (ShouldUseFileCache(data.size())) {
            // 使用文件缓存
            metadata.isMemoryCache = false;
            metadata.tempFilePath = GenerateTempFilePath(identifier);
            
            if (!WriteToTempFile(metadata.tempFilePath, data)) {
                WriteDebugLog("[ERROR] 写入临时文件失败");
                return false;
            }
            
            WriteDebugLog("[INFO] 数据已缓存到临时文件");
        } else {
            // 使用内存缓存
            metadata.isMemoryCache = true;
            
            // 检查内存限制
            if (m_currentMemoryUsage + data.size() > m_config.maxMemoryCache) {
                // 尝试清理过期数据释放空间
                CleanupExpiredData();
                
                if (m_currentMemoryUsage + data.size() > m_config.maxMemoryCache) {
                    WriteDebugLog("[ERROR] 内存缓存空间不足");
                    return false;
                }
            }
            
            m_memoryCache[identifier] = data;
            m_currentMemoryUsage += data.size();
            
            WriteDebugLog("[INFO] 数据已缓存到内存");
        }
        
        // 保存元数据
        m_metadataMap[identifier] = metadata;
        
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 缓存数据异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 从缓存中检索数据
bool TempDataManager::RetrieveData(const std::string& identifier, std::vector<uint8_t>& data)
{
    if (!m_initialized) {
        WriteDebugLog("[ERROR] TempDataManager未初始化，无法检索数据");
        return false;
    }
    
    if (identifier.empty()) {
        WriteDebugLog("[ERROR] 数据标识符为空");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    try {
        // 查找元数据
        auto metaIt = m_metadataMap.find(identifier);
        if (metaIt == m_metadataMap.end()) {
            WriteDebugLog("[WARNING] 未找到指定标识符的缓存数据");
            return false;
        }
        
        FileMetadata& metadata = metaIt->second;
        
        // 检查是否过期
        if (IsExpired(metadata)) {
            WriteDebugLog("[WARNING] 缓存数据已过期，自动清理");
            RemoveCachedData(identifier);
            return false;
        }
        
        // 根据缓存类型检索数据
        if (metadata.isMemoryCache) {
            // 从内存缓存检索
            auto memIt = m_memoryCache.find(identifier);
            if (memIt == m_memoryCache.end()) {
                WriteDebugLog("[ERROR] 内存缓存数据不一致");
                return false;
            }
            
            data = memIt->second;
        } else {
            // 从文件缓存检索
            if (!ReadFromTempFile(metadata.tempFilePath, data)) {
                WriteDebugLog("[ERROR] 读取临时文件失败");
                return false;
            }
        }
        
        // 验证数据完整性
        uint32_t currentChecksum = CalculateCRC32(data);
        if (currentChecksum != metadata.checksum) {
            WriteDebugLog("[ERROR] 数据校验和不匹配，数据可能已损坏");
            return false;
        }
        
        // 更新访问时间
        UpdateAccessTime(identifier);
        
        WriteDebugLog("[INFO] 数据检索成功");
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 检索数据异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 检查缓存中是否存在指定数据
bool TempDataManager::HasCachedData(const std::string& identifier) const
{
    if (!m_initialized || identifier.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_metadataMap.find(identifier);
    if (it == m_metadataMap.end()) {
        return false;
    }
    
    // 检查是否过期
    return !IsExpired(it->second);
}

// 移除指定的缓存数据
bool TempDataManager::RemoveCachedData(const std::string& identifier)
{
    if (!m_initialized || identifier.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    try {
        auto metaIt = m_metadataMap.find(identifier);
        if (metaIt == m_metadataMap.end()) {
            return false;
        }
        
        const FileMetadata& metadata = metaIt->second;
        
        if (metadata.isMemoryCache) {
            // 清理内存缓存
            auto memIt = m_memoryCache.find(identifier);
            if (memIt != m_memoryCache.end()) {
                m_currentMemoryUsage -= memIt->second.size();
                m_memoryCache.erase(memIt);
            }
        } else {
            // 删除临时文件
            if (!metadata.tempFilePath.empty()) {
                DeleteTempFile(metadata.tempFilePath);
            }
        }
        
        // 移除元数据
        m_metadataMap.erase(metaIt);
        
        WriteDebugLog("[INFO] 缓存数据已移除");
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 移除缓存数据异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 清理过期的缓存数据
size_t TempDataManager::CleanupExpiredData()
{
    if (!m_initialized) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    size_t cleanedCount = 0;
    
    try {
        auto it = m_metadataMap.begin();
        while (it != m_metadataMap.end()) {
            if (IsExpired(it->second)) {
                const std::string& identifier = it->first;
                const FileMetadata& metadata = it->second;
                
                if (metadata.isMemoryCache) {
                    // 清理内存缓存
                    auto memIt = m_memoryCache.find(identifier);
                    if (memIt != m_memoryCache.end()) {
                        m_currentMemoryUsage -= memIt->second.size();
                        m_memoryCache.erase(memIt);
                    }
                } else {
                    // 删除临时文件
                    if (!metadata.tempFilePath.empty()) {
                        DeleteTempFile(metadata.tempFilePath);
                    }
                }
                
                it = m_metadataMap.erase(it);
                cleanedCount++;
            } else {
                ++it;
            }
        }
        
        if (cleanedCount > 0) {
            std::string logMsg = "[INFO] 清理过期缓存数据 " + std::to_string(cleanedCount) + " 项";
            WriteDebugLog(logMsg.c_str());
        }
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 清理过期数据异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
    }
    
    return cleanedCount;
}

// 清理所有缓存数据
void TempDataManager::ClearAllCache()
{
    if (!m_initialized) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    try {
        // 清理所有临时文件
        for (const auto& pair : m_metadataMap) {
            const FileMetadata& metadata = pair.second;
            if (!metadata.isMemoryCache && !metadata.tempFilePath.empty()) {
                DeleteTempFile(metadata.tempFilePath);
            }
        }
        
        // 清理内存缓存
        m_memoryCache.clear();
        m_metadataMap.clear();
        m_currentMemoryUsage = 0;
        
        WriteDebugLog("[INFO] 所有缓存数据已清理");
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 清理所有缓存异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
    }
}

// 获取缓存统计信息
void TempDataManager::GetCacheStatistics(size_t& totalItems, size_t& memoryUsage, size_t& diskUsage) const
{
    if (!m_initialized) {
        totalItems = memoryUsage = diskUsage = 0;
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    totalItems = m_metadataMap.size();
    memoryUsage = m_currentMemoryUsage;
    diskUsage = 0;
    
    // 计算磁盘使用量
    for (const auto& pair : m_metadataMap) {
        const FileMetadata& metadata = pair.second;
        if (!metadata.isMemoryCache) {
            diskUsage += metadata.fileSize;
        }
    }
}

// 获取临时目录路径
std::string TempDataManager::GetTempDirectory() const
{
    return m_tempDirectoryPath;
}

// 设置缓存配置
void TempDataManager::SetCacheConfig(const CacheConfig& config)
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_config = config;
    WriteDebugLog("[INFO] 缓存配置已更新");
}

// 获取当前缓存配置
const TempDataManager::CacheConfig& TempDataManager::GetCacheConfig() const
{
    return m_config;
}

// 初始化临时目录
bool TempDataManager::InitializeTempDirectory()
{
    try {
        // 获取系统临时目录
        wchar_t tempPath[MAX_PATH];
        DWORD result = GetTempPathW(MAX_PATH, tempPath);
        if (result == 0 || result > MAX_PATH) {
            WriteDebugLog("[ERROR] 获取系统临时目录失败");
            return false;
        }
        
        // 转换为多字节字符串
        char tempPathMB[MAX_PATH * 2];
        int convertResult = WideCharToMultiByte(CP_UTF8, 0, tempPath, -1, tempPathMB, sizeof(tempPathMB), nullptr, nullptr);
        if (convertResult == 0) {
            WriteDebugLog("[ERROR] 临时目录路径转换失败");
            return false;
        }
        
        // 构建完整的临时目录路径
        std::filesystem::path fullTempPath = std::filesystem::path(tempPathMB) / m_config.tempDirectory;
        m_tempDirectoryPath = fullTempPath.string();
        
        // 创建临时目录
        std::error_code ec;
        if (!std::filesystem::exists(fullTempPath, ec)) {
            if (!std::filesystem::create_directories(fullTempPath, ec)) {
                std::string errorMsg = "[ERROR] 创建临时目录失败: " + ec.message();
                WriteDebugLog(errorMsg.c_str());
                return false;
            }
        }
        
        std::string logMsg = "[INFO] 临时目录初始化成功: " + m_tempDirectoryPath;
        WriteDebugLog(logMsg.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 初始化临时目录异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 生成临时文件路径
std::string TempDataManager::GenerateTempFilePath(const std::string& identifier) const
{
    // 使用标识符的哈希值作为文件名，避免特殊字符问题
    std::hash<std::string> hasher;
    size_t hashValue = hasher(identifier);
    
    std::ostringstream oss;
    oss << "cache_" << std::hex << hashValue << ".tmp";
    
    std::filesystem::path filePath = std::filesystem::path(m_tempDirectoryPath) / oss.str();
    return filePath.string();
}

// 计算数据的CRC32校验和
uint32_t TempDataManager::CalculateCRC32(const std::vector<uint8_t>& data) const
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint8_t byte : data) {
        crc = CRC32_TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

// 检查数据是否应该使用文件缓存
bool TempDataManager::ShouldUseFileCache(size_t dataSize) const
{
    return dataSize >= m_config.largeFileThreshold;
}

// 写入数据到临时文件
bool TempDataManager::WriteToTempFile(const std::string& filePath, const std::vector<uint8_t>& data) const
{
    try {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            WriteDebugLog("[ERROR] 无法打开临时文件进行写入");
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!file.good()) {
            WriteDebugLog("[ERROR] 写入临时文件失败");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 写入临时文件异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 从临时文件读取数据
bool TempDataManager::ReadFromTempFile(const std::string& filePath, std::vector<uint8_t>& data) const
{
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            WriteDebugLog("[ERROR] 无法打开临时文件进行读取");
            return false;
        }
        
        // 获取文件大小
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fileSize <= 0) {
            WriteDebugLog("[ERROR] 临时文件大小无效");
            return false;
        }
        
        // 读取文件内容
        data.resize(static_cast<size_t>(fileSize));
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        
        if (!file.good()) {
            WriteDebugLog("[ERROR] 读取临时文件失败");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 读取临时文件异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 删除临时文件
bool TempDataManager::DeleteTempFile(const std::string& filePath) const
{
    try {
        std::error_code ec;
        if (std::filesystem::exists(filePath, ec) && !ec) {
            if (std::filesystem::remove(filePath, ec) && !ec) {
                return true;
            } else {
                std::string errorMsg = "[ERROR] 删除临时文件失败: " + ec.message();
                WriteDebugLog(errorMsg.c_str());
                return false;
            }
        }
        return true;  // 文件不存在也视为删除成功
        
    } catch (const std::exception& e) {
        std::string errorMsg = "[ERROR] 删除临时文件异常: ";
        errorMsg += e.what();
        WriteDebugLog(errorMsg.c_str());
        return false;
    }
}

// 检查缓存项是否过期
bool TempDataManager::IsExpired(const FileMetadata& metadata) const
{
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - metadata.lastAccess);
    return elapsed >= m_config.autoExpireTime;
}

// 更新缓存项的访问时间
void TempDataManager::UpdateAccessTime(const std::string& identifier)
{
    auto it = m_metadataMap.find(identifier);
    if (it != m_metadataMap.end()) {
        it->second.lastAccess = std::chrono::system_clock::now();
    }
}

// 临时数据管理器单例访问接口
TempDataManager& GetTempDataManager()
{
    std::lock_guard<std::mutex> lock(g_singletonMutex);
    
    if (!g_tempDataManager) {
        g_tempDataManager = std::make_unique<TempDataManager>();
    }
    
    return *g_tempDataManager;
}