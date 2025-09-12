#include "pch.h"
#include "TempDataManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>

extern void WriteDebugLog(const char* message);

TempDataManager::TempDataManager() {
    CreateTempDirectory();
    WriteDebugLog("[TempDataManager] 临时数据管理器初始化完成");
}

TempDataManager::~TempDataManager() {
    CleanupOnExit();
    WriteDebugLog("[TempDataManager] 临时数据管理器析构完成");
}

bool TempDataManager::CacheReceivedData(const std::string& sessionId, 
                                       const std::vector<uint8_t>& data,
                                       const FileMetadata& metadata) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    try {
        // 检查缓存大小限制
        if (data.size() > m_maxCacheSize) {
            WriteDebugLog("[TempDataManager] 数据大小超过缓存限制");
            return false;
        }
        
        auto cachedData = std::make_shared<CachedFileData>();
        cachedData->metadata = metadata;
        cachedData->metadata.sessionId = sessionId;
        cachedData->cacheTime = std::chrono::steady_clock::now();
        cachedData->isComplete = true;
        
        // 如果数据大小超过阈值，使用临时文件
        if (data.size() > m_tempFileThreshold) {
            std::string tempFilePath = GenerateTempFilePath(sessionId, metadata.filename);
            
            std::ofstream tempFile(tempFilePath, std::ios::binary);
            if (!tempFile.is_open()) {
                WriteDebugLog("[TempDataManager] 无法创建临时文件");
                return false;
            }
            
            tempFile.write(reinterpret_cast<const char*>(data.data()), data.size());
            tempFile.close();
            
            if (tempFile.fail()) {
                WriteDebugLog("[TempDataManager] 临时文件写入失败");
                std::filesystem::remove(tempFilePath);
                return false;
            }
            
            cachedData->tempFilePath = tempFilePath;
            WriteDebugLog(("[TempDataManager] 大文件已缓存到临时文件: " + tempFilePath).c_str());
        } else {
            // 小文件直接缓存到内存
            cachedData->data = data;
            WriteDebugLog(("[TempDataManager] 文件已缓存到内存: " + metadata.filename).c_str());
        }
        
        // 计算校验和
        cachedData->metadata.checksum = CalculateChecksum(data);
        
        // 存储到缓存映射
        m_cache[sessionId] = cachedData;
        
        WriteDebugLog(("[TempDataManager] 数据缓存成功: " + sessionId + ", 文件: " + metadata.filename).c_str());
        return true;
        
    } catch (const std::exception& e) {
        WriteDebugLog(("[TempDataManager] 缓存数据异常: " + std::string(e.what())).c_str());
        return false;
    }
}

std::shared_ptr<CachedFileData> TempDataManager::GetCachedData(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_cache.find(sessionId);
    if (it == m_cache.end()) {
        return nullptr;
    }
    
    auto cachedData = it->second;
    
    // 如果数据在临时文件中，需要读取
    if (!cachedData->tempFilePath.empty() && cachedData->data.empty()) {
        try {
            std::ifstream tempFile(cachedData->tempFilePath, std::ios::binary);
            if (!tempFile.is_open()) {
                WriteDebugLog(("[TempDataManager] 无法读取临时文件: " + cachedData->tempFilePath).c_str());
                return nullptr;
            }
            
            // 获取文件大小
            tempFile.seekg(0, std::ios::end);
            size_t fileSize = tempFile.tellg();
            tempFile.seekg(0, std::ios::beg);
            
            // 读取数据
            cachedData->data.resize(fileSize);
            tempFile.read(reinterpret_cast<char*>(cachedData->data.data()), fileSize);
            tempFile.close();
            
            if (tempFile.fail()) {
                WriteDebugLog(("[TempDataManager] 临时文件读取失败: " + cachedData->tempFilePath).c_str());
                cachedData->data.clear();
                return nullptr;
            }
            
        } catch (const std::exception& e) {
            WriteDebugLog(("[TempDataManager] 读取临时文件异常: " + std::string(e.what())).c_str());
            return nullptr;
        }
    }
    
    return cachedData;
}

void TempDataManager::ClearCache(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_cache.find(sessionId);
    if (it != m_cache.end()) {
        // 删除临时文件
        if (!it->second->tempFilePath.empty()) {
            try {
                std::filesystem::remove(it->second->tempFilePath);
                WriteDebugLog(("[TempDataManager] 临时文件已删除: " + it->second->tempFilePath).c_str());
            } catch (const std::exception& e) {
                WriteDebugLog(("[TempDataManager] 删除临时文件失败: " + std::string(e.what())).c_str());
            }
        }
        
        m_cache.erase(it);
        WriteDebugLog(("[TempDataManager] 缓存已清理: " + sessionId).c_str());
    }
}

void TempDataManager::ClearAllCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    // 删除所有临时文件
    for (const auto& pair : m_cache) {
        if (!pair.second->tempFilePath.empty()) {
            try {
                std::filesystem::remove(pair.second->tempFilePath);
            } catch (const std::exception& e) {
                WriteDebugLog(("[TempDataManager] 删除临时文件失败: " + std::string(e.what())).c_str());
            }
        }
    }
    
    m_cache.clear();
    WriteDebugLog("[TempDataManager] 所有缓存已清理");
}

void TempDataManager::ClearExpiredCache(std::chrono::minutes maxAge) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = m_cache.begin();
    
    while (it != m_cache.end()) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second->cacheTime);
        
        if (age > maxAge) {
            // 删除临时文件
            if (!it->second->tempFilePath.empty()) {
                try {
                    std::filesystem::remove(it->second->tempFilePath);
                } catch (const std::exception& e) {
                    WriteDebugLog(("[TempDataManager] 删除过期临时文件失败: " + std::string(e.what())).c_str());
                }
            }
            
            WriteDebugLog(("[TempDataManager] 过期缓存已清理: " + it->first).c_str());
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

size_t TempDataManager::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    size_t totalSize = 0;
    for (const auto& pair : m_cache) {
        totalSize += pair.second->data.size();
    }
    
    return totalSize;
}

void TempDataManager::SetMaxCacheSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_maxCacheSize = maxSize;
    WriteDebugLog(("[TempDataManager] 最大缓存大小设置为: " + std::to_string(maxSize)).c_str());
}

std::vector<std::string> TempDataManager::GetCachedSessionIds() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    std::vector<std::string> sessionIds;
    for (const auto& pair : m_cache) {
        sessionIds.push_back(pair.first);
    }
    
    return sessionIds;
}

void TempDataManager::CleanupOnExit() {
    WriteDebugLog("[TempDataManager] 开始程序退出清理");
    
    // 清理所有缓存和临时文件
    ClearAllCache();
    
    // 清理临时目录（如果为空）
    CleanupTempFiles();
    
    WriteDebugLog("[TempDataManager] 程序退出清理完成");
}

std::string TempDataManager::GenerateTempFilePath(const std::string& sessionId, const std::string& filename) {
    // 生成基于会话ID和时间戳的唯一临时文件名
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::string tempFileName = "cache_" + sessionId + "_" + std::to_string(timestamp);
    
    // 保留原文件扩展名
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos) {
        tempFileName += filename.substr(dotPos);
    }
    
    return m_tempDirectory + "\\" + tempFileName;
}

std::string TempDataManager::CalculateChecksum(const std::vector<uint8_t>& data) {
    // 简单的CRC32校验和计算
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    crc ^= 0xFFFFFFFF;
    
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << crc;
    return ss.str();
}

bool TempDataManager::CreateTempDirectory() {
    try {
        // 获取系统临时目录
        char tempPath[MAX_PATH];
        DWORD result = GetTempPathA(MAX_PATH, tempPath);
        
        if (result == 0 || result > MAX_PATH) {
            // 如果获取失败，使用当前目录下的temp子目录
            m_tempDirectory = ".\\temp\\PortMaster_Cache";
        } else {
            m_tempDirectory = std::string(tempPath) + "PortMaster_Cache";
        }
        
        // 创建目录
        std::filesystem::create_directories(m_tempDirectory);
        
        WriteDebugLog(("[TempDataManager] 临时目录创建成功: " + m_tempDirectory).c_str());
        return true;
        
    } catch (const std::exception& e) {
        WriteDebugLog(("[TempDataManager] 创建临时目录失败: " + std::string(e.what())).c_str());
        m_tempDirectory = ".\\temp";
        return false;
    }
}

void TempDataManager::CleanupTempFiles() {
    try {
        if (std::filesystem::exists(m_tempDirectory)) {
            // 删除临时目录中的所有文件
            for (const auto& entry : std::filesystem::directory_iterator(m_tempDirectory)) {
                if (entry.is_regular_file()) {
                    std::filesystem::remove(entry.path());
                }
            }
            
            // 如果目录为空，删除目录
            if (std::filesystem::is_empty(m_tempDirectory)) {
                std::filesystem::remove(m_tempDirectory);
                WriteDebugLog(("[TempDataManager] 临时目录已删除: " + m_tempDirectory).c_str());
            }
        }
    } catch (const std::exception& e) {
        WriteDebugLog(("[TempDataManager] 清理临时文件失败: " + std::string(e.what())).c_str());
    }
}