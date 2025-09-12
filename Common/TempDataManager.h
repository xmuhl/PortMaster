#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <cstdint>

// 前置声明
namespace std {
    namespace filesystem {
        class path;
    }
}

/**
 * @brief 临时数据管理器 - 自动缓存文件管理和清理机制
 * 
 * 功能特性：
 * - 自动缓存管理：接收数据自动缓存到临时目录
 * - 智能存储策略：小文件内存缓存，大文件临时文件缓存
 * - 自动过期清理：30分钟自动过期机制
 * - 程序退出清理：析构函数自动清理所有临时文件
 * - 线程安全设计：支持多线程并发访问
 * 
 * 缓存配置：
 * - 默认内存缓存限制：100MB
 * - 大文件阈值：10MB（超过则使用临时文件）
 * - 自动过期时间：30分钟
 * - 临时目录：系统临时目录/PortMaster_Cache
 */
class TempDataManager
{
public:
    // 文件元数据管理结构
    struct FileMetadata {
        std::string fileName;           // 文件名
        size_t fileSize;               // 文件大小（字节）
        std::chrono::system_clock::time_point createTime;  // 创建时间
        std::chrono::system_clock::time_point lastAccess;  // 最后访问时间
        uint32_t checksum;             // CRC32校验和
        bool isMemoryCache;            // 是否为内存缓存
        std::string tempFilePath;      // 临时文件路径（文件缓存时使用）
        
        FileMetadata() : fileSize(0), checksum(0), isMemoryCache(true) {}
    };
    
    // 缓存配置结构
    struct CacheConfig {
        size_t maxMemoryCache;         // 最大内存缓存大小（字节）
        size_t largeFileThreshold;     // 大文件阈值（字节）
        std::chrono::minutes autoExpireTime;  // 自动过期时间
        std::string tempDirectory;     // 临时目录路径
        
        CacheConfig() 
            : maxMemoryCache(100 * 1024 * 1024)  // 100MB
            , largeFileThreshold(10 * 1024 * 1024)  // 10MB
            , autoExpireTime(30)  // 30分钟
            , tempDirectory("PortMaster_Cache") {}
    };

public:
    /**
     * @brief 构造函数 - 初始化临时数据管理器
     * @param config 缓存配置参数
     */
    explicit TempDataManager(const CacheConfig& config = CacheConfig());
    
    /**
     * @brief 析构函数 - 自动清理所有临时文件和内存缓存
     */
    ~TempDataManager();
    
    // 禁用拷贝构造和赋值操作（确保单例管理）
    TempDataManager(const TempDataManager&) = delete;
    TempDataManager& operator=(const TempDataManager&) = delete;
    
    /**
     * @brief 缓存数据到临时存储
     * @param data 要缓存的数据
     * @param identifier 数据标识符（用于后续检索）
     * @return 缓存成功返回true，失败返回false
     */
    bool CacheData(const std::vector<uint8_t>& data, const std::string& identifier);
    
    /**
     * @brief 从缓存中检索数据
     * @param identifier 数据标识符
     * @param data 输出参数，存储检索到的数据
     * @return 检索成功返回true，失败返回false
     */
    bool RetrieveData(const std::string& identifier, std::vector<uint8_t>& data);
    
    /**
     * @brief 检查缓存中是否存在指定数据
     * @param identifier 数据标识符
     * @return 存在返回true，不存在返回false
     */
    bool HasCachedData(const std::string& identifier) const;
    
    /**
     * @brief 移除指定的缓存数据
     * @param identifier 数据标识符
     * @return 移除成功返回true，失败返回false
     */
    bool RemoveCachedData(const std::string& identifier);
    
    /**
     * @brief 清理过期的缓存数据
     * @return 清理的数据项数量
     */
    size_t CleanupExpiredData();
    
    /**
     * @brief 清理所有缓存数据
     */
    void ClearAllCache();
    
    /**
     * @brief 获取缓存统计信息
     * @param totalItems 总缓存项数
     * @param memoryUsage 内存使用量（字节）
     * @param diskUsage 磁盘使用量（字节）
     */
    void GetCacheStatistics(size_t& totalItems, size_t& memoryUsage, size_t& diskUsage) const;
    
    /**
     * @brief 获取临时目录路径
     * @return 临时目录的完整路径
     */
    std::string GetTempDirectory() const;
    
    /**
     * @brief 设置缓存配置
     * @param config 新的缓存配置
     */
    void SetCacheConfig(const CacheConfig& config);
    
    /**
     * @brief 获取当前缓存配置
     * @return 当前缓存配置
     */
    const CacheConfig& GetCacheConfig() const;

private:
    /**
     * @brief 初始化临时目录
     * @return 初始化成功返回true，失败返回false
     */
    bool InitializeTempDirectory();
    
    /**
     * @brief 生成临时文件路径
     * @param identifier 数据标识符
     * @return 临时文件的完整路径
     */
    std::string GenerateTempFilePath(const std::string& identifier) const;
    
    /**
     * @brief 计算数据的CRC32校验和
     * @param data 要计算校验和的数据
     * @return CRC32校验和值
     */
    uint32_t CalculateCRC32(const std::vector<uint8_t>& data) const;
    
    /**
     * @brief 检查数据是否应该使用文件缓存
     * @param dataSize 数据大小
     * @return 应该使用文件缓存返回true，否则返回false
     */
    bool ShouldUseFileCache(size_t dataSize) const;
    
    /**
     * @brief 写入数据到临时文件
     * @param filePath 文件路径
     * @param data 要写入的数据
     * @return 写入成功返回true，失败返回false
     */
    bool WriteToTempFile(const std::string& filePath, const std::vector<uint8_t>& data) const;
    
    /**
     * @brief 从临时文件读取数据
     * @param filePath 文件路径
     * @param data 输出参数，存储读取的数据
     * @return 读取成功返回true，失败返回false
     */
    bool ReadFromTempFile(const std::string& filePath, std::vector<uint8_t>& data) const;
    
    /**
     * @brief 删除临时文件
     * @param filePath 文件路径
     * @return 删除成功返回true，失败返回false
     */
    bool DeleteTempFile(const std::string& filePath) const;
    
    /**
     * @brief 检查缓存项是否过期
     * @param metadata 文件元数据
     * @return 过期返回true，未过期返回false
     */
    bool IsExpired(const FileMetadata& metadata) const;
    
    /**
     * @brief 更新缓存项的访问时间
     * @param identifier 数据标识符
     */
    void UpdateAccessTime(const std::string& identifier);

private:
    // 缓存配置
    CacheConfig m_config;
    
    // 内存缓存存储
    std::unordered_map<std::string, std::vector<uint8_t>> m_memoryCache;
    
    // 文件元数据映射
    std::unordered_map<std::string, FileMetadata> m_metadataMap;
    
    // 临时目录路径
    std::string m_tempDirectoryPath;
    
    // 线程安全保护
    mutable std::mutex m_cacheMutex;
    
    // 当前内存使用量统计
    size_t m_currentMemoryUsage;
    
    // 初始化状态标志
    bool m_initialized;
};

/**
 * @brief 临时数据管理器单例访问接口
 * @return 临时数据管理器实例的引用
 */
TempDataManager& GetTempDataManager();