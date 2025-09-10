#pragma once

#include "../Transport/ITransport.h"
#include "json.hpp"
#include <string>
#include <map>
#include <vector>

using json = nlohmann::json;


// 配置管理器 - 处理JSON格式配置文件
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager();
    
    // 配置文件操作
    bool LoadConfig(const std::string& filePath = "");
    bool SaveConfig(const std::string& filePath = "") const;
    bool ResetToDefaults();
    
    // 传输配置
    bool SaveTransportConfig(const std::string& transportType, const TransportConfig& config);
    TransportConfig LoadTransportConfig(const std::string& transportType) const;
    std::vector<std::string> GetSavedTransportTypes() const;
    
    // 通用配置项
    void SetValue(const std::string& section, const std::string& key, const std::string& value);
    void SetValue(const std::string& section, const std::string& key, int value);
    void SetValue(const std::string& section, const std::string& key, bool value);
    void SetValue(const std::string& section, const std::string& key, double value);
    
    std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;
    int GetInt(const std::string& section, const std::string& key, int defaultValue = 0) const;
    bool GetBool(const std::string& section, const std::string& key, bool defaultValue = false) const;
    double GetDouble(const std::string& section, const std::string& key, double defaultValue = 0.0) const;
    
    // 应用程序特定配置
    struct AppConfig
    {
        // 窗口设置
        int windowX = 100;
        int windowY = 100;
        int windowWidth = 800;
        int windowHeight = 600;
        bool windowMaximized = false;
        
        // 鍙潬浼犺緭璁剧疆
        int ackTimeoutMs = 1000;
        int maxRetries = 3;
        size_t maxPayloadSize = 1024;
        std::string receiveDirectory;
        
        // 界面设置
        bool hexViewEnabled = true;
        bool textViewEnabled = true;
        bool showTimestamp = true;
        
        // 日志设置
        bool enableLogging = true;
        std::string logDirectory;
        int maxLogFiles = 10;
        
        // 测试设置
        bool autoTest = false;
        std::string testDataFile;
    };
    
    AppConfig GetAppConfig() const;
    void SetAppConfig(const AppConfig& config);
    
    // 閰嶇疆鏂囦欢璺緞绠＄悊
    static std::string GetDefaultConfigPath();
    static std::string GetUserConfigPath();
    std::string GetCurrentConfigPath() const { return m_configFilePath; }

private:
    std::string m_configFilePath;
    mutable json m_config;
    
    // JSON 操作方法（使用nlohmann/json）
    bool ParseJsonFile(const std::string& filePath);
    bool WriteJsonFile(const std::string& filePath) const;
    bool IsNumeric(const std::string& str) const;
    
    // 璺緞杈呭姪鏂规硶
    // 路径辅助方法
    bool EnsureDirectoryExists(const std::string& filePath) const;
    std::string GetSectionKey(const std::string& section, const std::string& key) const;
    
    // 默认值设置
    void SetDefaultValues();
    TransportConfig GetDefaultTransportConfig(const std::string& transportType) const;
};