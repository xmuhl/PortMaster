#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <filesystem>

// 文件元数据结构
struct FileMetadata {
    std::string filename;
    size_t fileSize;
    std::chrono::system_clock::time_point timestamp;
    std::string checksum;
    std::string sessionId;
};

// 缓存文件数据结构
struct CachedFileData {
    std::vector<uint8_t> data;
    FileMetadata metadata;
    std::chrono::steady_clock::time_point cacheTime;
    bool isComplete;
    std::string tempFilePath;  // 临时文件路径（用于大文件）
};

// 临时数据管理器类
class TempDataManager {
public:
    TempDataManager();
    ~TempDataManager();

    // 数据缓存管理
    bool CacheReceivedData(const std::string& sessionId, 
                          const std::vector<uint8_t>& data,
                          const FileMetadata& metadata);
    
    // 获取缓存数据
    std::shared_ptr<CachedFileData> GetCachedData(const std::string& sessionId);
    
    // 缓存清理
    void ClearCache(const std::string& sessionId);
    void ClearAllCache();
    void ClearExpiredCache(std::chrono::minutes maxAge = std::chrono::minutes(30));
    
    // 内存管理
    size_t GetCacheSize() const;
    void SetMaxCacheSize(size_t maxSize);
    
    // 获取缓存文件列表
    std::vector<std::string> GetCachedSessionIds() const;
    
    // 程序退出时的清理
    void CleanupOnExit();
    
private:
    // 生成临时文件路径
    std::string GenerateTempFilePath(const std::string& sessionId, const std::string& filename);
    
    // 计算数据校验和
    std::string CalculateChecksum(const std::vector<uint8_t>& data);
    
    // 创建临时目录
    bool CreateTempDirectory();
    
    // 清理临时文件
    void CleanupTempFiles();
    
private:
    std::unordered_map<std::string, std::shared_ptr<CachedFileData>> m_cache;
    mutable std::mutex m_cacheMutex;
    size_t m_maxCacheSize = 100 * 1024 * 1024; // 100MB默认限制
    std::string m_tempDirectory;
    size_t m_tempFileThreshold = 10 * 1024 * 1024; // 10MB阈值，超过则使用临时文件
};