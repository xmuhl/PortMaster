#include "pch.h"
#include "ConfigManager.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <shlobj.h>

using json = nlohmann::json;

// 私有辅助方法前向声明
std::string GetNestedValue(const json& j, const std::string& path, const std::string& defaultValue = "");
void SetNestedValue(json& j, const std::string& path, const json& value);

ConfigManager::ConfigManager()
{
    // 初始化为空JSON对象
    m_config = json::object();
    
    SetDefaultValues();
    
    // 尝试加载配置文件
    std::string configPath = GetUserConfigPath();
    if (!LoadConfig(configPath))
    {
        // 如果默认路径失败，尝试用户路径
        configPath = GetDefaultConfigPath();
        LoadConfig(configPath);
    }
}

ConfigManager::~ConfigManager()
{
    // 自动保存配置
    SaveConfig();
}

bool ConfigManager::LoadConfig(const std::string& filePath)
{
    if (filePath.empty())
    {
        m_configFilePath = GetDefaultConfigPath();
    }
    else
    {
        m_configFilePath = filePath;
    }
    
    // TODO: 实现JSON配置文件加载
    return ParseJsonFile(m_configFilePath);
}

bool ConfigManager::SaveConfig(const std::string& filePath) const
{
    std::string savePath = filePath.empty() ? m_configFilePath : filePath;
    return WriteJsonFile(savePath);
}

bool ConfigManager::ResetToDefaults()
{
    m_config = json::object();
    SetDefaultValues();
    return SaveConfig();
}

bool ConfigManager::SaveTransportConfig(const std::string& transportType, const TransportConfig& config)
{
    std::string section = "Transport_" + transportType;
    
    SetValue(section, "baudRate", config.baudRate);
    SetValue(section, "dataBits", config.dataBits);
    SetValue(section, "parity", config.parity);
    SetValue(section, "stopBits", config.stopBits);
    SetValue(section, "ipAddress", config.ipAddress);
    SetValue(section, "port", config.port);
    SetValue(section, "isServer", config.isServer);
    SetValue(section, "connectTimeoutMs", config.connectTimeoutMs);
    SetValue(section, "readTimeoutMs", config.readTimeoutMs);
    SetValue(section, "writeTimeoutMs", config.writeTimeoutMs);
    SetValue(section, "rxBufferSize", static_cast<int>(config.rxBufferSize));
    SetValue(section, "txBufferSize", static_cast<int>(config.txBufferSize));
    
    return true;
}

TransportConfig ConfigManager::LoadTransportConfig(const std::string& transportType) const
{
    std::string section = "Transport_" + transportType;
    TransportConfig config = GetDefaultTransportConfig(transportType);
    
    config.baudRate = GetInt(section, "baudRate", config.baudRate);
    config.dataBits = GetInt(section, "dataBits", config.dataBits);
    config.parity = GetInt(section, "parity", config.parity);
    config.stopBits = GetInt(section, "stopBits", config.stopBits);
    config.ipAddress = GetString(section, "ipAddress", config.ipAddress);
    config.port = GetInt(section, "port", config.port);
    config.isServer = GetBool(section, "isServer", config.isServer);
    config.connectTimeoutMs = GetInt(section, "connectTimeoutMs", config.connectTimeoutMs);
    config.readTimeoutMs = GetInt(section, "readTimeoutMs", config.readTimeoutMs);
    config.writeTimeoutMs = GetInt(section, "writeTimeoutMs", config.writeTimeoutMs);
    config.rxBufferSize = GetInt(section, "rxBufferSize", static_cast<int>(config.rxBufferSize));
    config.txBufferSize = GetInt(section, "txBufferSize", static_cast<int>(config.txBufferSize));
    
    return config;
}

std::vector<std::string> ConfigManager::GetSavedTransportTypes() const
{
    std::vector<std::string> types;
    
    try {
        for (auto it = m_config.begin(); it != m_config.end(); ++it) {
            const std::string& key = it.key();
            if (key.substr(0, 10) == "Transport_" && it.value().is_object()) {
                std::string type = key.substr(10); // 移除"Transport_"前缀
                types.push_back(type);
            }
        }
    } catch (...) {
        // 如果发生异常，返回空列表
    }
    
    return types;
}

void ConfigManager::SetValue(const std::string& section, const std::string& key, const std::string& value)
{
    std::string fullKey = GetSectionKey(section, key);
    SetNestedValue(m_config, fullKey, value);
}

void ConfigManager::SetValue(const std::string& section, const std::string& key, int value)
{
    std::string fullKey = GetSectionKey(section, key);
    SetNestedValue(m_config, fullKey, value);
}

void ConfigManager::SetValue(const std::string& section, const std::string& key, bool value)
{
    std::string fullKey = GetSectionKey(section, key);
    SetNestedValue(m_config, fullKey, value);
}

void ConfigManager::SetValue(const std::string& section, const std::string& key, double value)
{
    std::string fullKey = GetSectionKey(section, key);
    SetNestedValue(m_config, fullKey, value);
}

std::string ConfigManager::GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const
{
    std::string fullKey = GetSectionKey(section, key);
    return GetNestedValue(m_config, fullKey, defaultValue);
}

int ConfigManager::GetInt(const std::string& section, const std::string& key, int defaultValue) const
{
    std::string fullKey = GetSectionKey(section, key);
    std::string value = GetNestedValue(m_config, fullKey, "");
    if (value.empty())
        return defaultValue;
    
    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigManager::GetBool(const std::string& section, const std::string& key, bool defaultValue) const
{
    std::string fullKey = GetSectionKey(section, key);
    std::string value = GetNestedValue(m_config, fullKey, "");
    if (value.empty())
        return defaultValue;
    
    return (value == "true" || value == "1" || value == "yes");
}

double ConfigManager::GetDouble(const std::string& section, const std::string& key, double defaultValue) const
{
    std::string fullKey = GetSectionKey(section, key);
    std::string value = GetNestedValue(m_config, fullKey, "");
    if (value.empty())
        return defaultValue;
    
    try {
        return std::stod(value);
    } catch (...) {
        return defaultValue;
    }
}

ConfigManager::AppConfig ConfigManager::GetAppConfig() const
{
    AppConfig config;
    
    config.windowX = GetInt("Window", "x", config.windowX);
    config.windowY = GetInt("Window", "y", config.windowY);
    config.windowWidth = GetInt("Window", "width", config.windowWidth);
    config.windowHeight = GetInt("Window", "height", config.windowHeight);
    config.windowMaximized = GetBool("Window", "maximized", config.windowMaximized);
    
    config.ackTimeoutMs = GetInt("Protocol", "ackTimeoutMs", config.ackTimeoutMs);
    config.maxRetries = GetInt("Protocol", "maxRetries", config.maxRetries);
    config.maxPayloadSize = GetInt("Protocol", "maxPayloadSize", static_cast<int>(config.maxPayloadSize));
    config.receiveDirectory = GetString("Protocol", "receiveDirectory", config.receiveDirectory);
    
    config.hexViewEnabled = GetBool("UI", "hexViewEnabled", config.hexViewEnabled);
    config.textViewEnabled = GetBool("UI", "textViewEnabled", config.textViewEnabled);
    config.showTimestamp = GetBool("UI", "showTimestamp", config.showTimestamp);
    
    config.enableLogging = GetBool("Log", "enableLogging", config.enableLogging);
    config.logDirectory = GetString("Log", "logDirectory", config.logDirectory);
    config.maxLogFiles = GetInt("Log", "maxLogFiles", config.maxLogFiles);
    
    config.autoTest = GetBool("Test", "autoTest", config.autoTest);
    config.testDataFile = GetString("Test", "testDataFile", config.testDataFile);
    
    return config;
}

void ConfigManager::SetAppConfig(const AppConfig& config)
{
    SetValue("Window", "x", config.windowX);
    SetValue("Window", "y", config.windowY);
    SetValue("Window", "width", config.windowWidth);
    SetValue("Window", "height", config.windowHeight);
    SetValue("Window", "maximized", config.windowMaximized);
    
    SetValue("Protocol", "ackTimeoutMs", config.ackTimeoutMs);
    SetValue("Protocol", "maxRetries", config.maxRetries);
    SetValue("Protocol", "maxPayloadSize", static_cast<int>(config.maxPayloadSize));
    SetValue("Protocol", "receiveDirectory", config.receiveDirectory);
    
    SetValue("UI", "hexViewEnabled", config.hexViewEnabled);
    SetValue("UI", "textViewEnabled", config.textViewEnabled);
    SetValue("UI", "showTimestamp", config.showTimestamp);
    
    SetValue("Log", "enableLogging", config.enableLogging);
    SetValue("Log", "logDirectory", config.logDirectory);
    SetValue("Log", "maxLogFiles", config.maxLogFiles);
    
    SetValue("Test", "autoTest", config.autoTest);
    SetValue("Test", "testDataFile", config.testDataFile);
}

std::string ConfigManager::GetDefaultConfigPath()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    std::string path = exePath;
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos)
        path = path.substr(0, pos);
    
    return path + "\\PortMaster.config";
}

std::string ConfigManager::GetUserConfigPath()
{
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath)))
    {
        std::string path = appDataPath;
        path += "\\PortMaster";
        
        // 创建目录
        CreateDirectoryA(path.c_str(), NULL);
        
        return path + "\\PortMaster.config";
    }
    
    return GetDefaultConfigPath();
}

bool ConfigManager::ParseJsonFile(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open())
            return false;
        
        // 直接解析JSON到配置对象
        file >> m_config;
        
        return true;
    } catch (const json::parse_error&) {
        // JSON解析错误，使用默认配置
        m_config = json::object();
        return false;
    } catch (const std::ios_base::failure&) {
        // 文件I/O异常
        m_config = json::object();
        return false;
    } catch (const std::exception&) {
        // 其他标准异常
        m_config = json::object();
        return false;
    } catch (...) {
        // 所有其他异常
        m_config = json::object();
        return false;
    }
}

// 获取嵌套JSON值的辅助函数
std::string GetNestedValue(const json& j, const std::string& path, const std::string& defaultValue)
{
    try {
        json current = j;
        std::istringstream iss(path);
        std::string part;
        
        while (std::getline(iss, part, '.')) {
            if (current.contains(part)) {
                current = current[part];
            } else {
                return defaultValue;
            }
        }
        
        if (current.is_string()) {
            return current.get<std::string>();
        } else if (current.is_boolean()) {
            return current.get<bool>() ? "true" : "false";
        } else if (current.is_number()) {
            return std::to_string(current.get<double>());
        } else {
            return current.dump();
        }
    } catch (...) {
        return defaultValue;
    }
}

// 设置嵌套JSON值的辅助函数
void SetNestedValue(json& j, const std::string& path, const json& value)
{
    std::istringstream iss(path);
    std::string part;
    std::vector<std::string> parts;
    
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }
    
    json* current = &j;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        current = &(*current)[parts[i]];
    }
    
    (*current)[parts.back()] = value;
}

bool ConfigManager::WriteJsonFile(const std::string& filePath) const
{
    try {
        if (!EnsureDirectoryExists(filePath))
            return false;
        
        std::ofstream file(filePath);
        if (!file.is_open())
            return false;
        
        // 创建输出JSON对象，添加注释
        json output = m_config;
        output["_comment"] = "PortMaster Configuration File - Generated automatically";
        
        // 写入格式化的JSON
        file << output.dump(2); // 2个空格缩进
        
        return !file.fail();
    } catch (const json::exception&) {
        // JSON处理异常
        return false;
    } catch (const std::ios_base::failure&) {
        // 文件I/O异常
        return false;
    } catch (const std::exception&) {
        // 其他标准异常
        return false;
    } catch (...) {
        // 所有其他异常
        return false;
    }
}



bool ConfigManager::IsNumeric(const std::string& str) const
{
    if (str.empty()) 
        return false;
    
    size_t start = 0;
    if (str[0] == '-' || str[0] == '+')
        start = 1;
    
    if (start >= str.length())
        return false;
    
    bool hasDecimalPoint = false;
    for (size_t i = start; i < str.length(); ++i)
    {
        if (str[i] == '.')
        {
            if (hasDecimalPoint) 
                return false; // 多个小数点
            hasDecimalPoint = true;
        }
        else if (str[i] < '0' || str[i] > '9')
        {
            return false; // 非数字字符
        }
    }
    
    return true;
}

bool ConfigManager::EnsureDirectoryExists(const std::string& filePath) const
{
    size_t pos = filePath.find_last_of("\\/");
    if (pos == std::string::npos)
        return true;
    
    std::string directory = filePath.substr(0, pos);
    return CreateDirectoryA(directory.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string ConfigManager::GetSectionKey(const std::string& section, const std::string& key) const
{
    return section + "." + key;
}

void ConfigManager::SetDefaultValues()
{
    // 设置默认配置值
    SetValue("General", "version", "1.0.0");
    SetValue("General", "firstRun", true);
    
    // 窗口默认设置
    SetValue("Window", "x", 100);
    SetValue("Window", "y", 100);
    SetValue("Window", "width", 800);
    SetValue("Window", "height", 600);
    SetValue("Window", "maximized", false);
    
    // 协议默认设置
    SetValue("Protocol", "ackTimeoutMs", 1000);
    SetValue("Protocol", "maxRetries", 3);
    SetValue("Protocol", "maxPayloadSize", 1024);
    
    // 获取用户本地数据目录并设置接收目录默认值 (DRY: 统一路径管理)
    char localAppData[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK)
    {
        std::string receiveDir = std::string(localAppData) + "\\PortIO\\Recv";
        SetValue("Protocol", "receiveDirectory", receiveDir);
        
        std::string logDir = std::string(localAppData) + "\\PortMaster\\Logs";
        SetValue("Log", "logDirectory", logDir);
    }
    else
    {
        // 备用默认目录
        SetValue("Protocol", "receiveDirectory", ".\\Recv");
        SetValue("Log", "logDirectory", ".\\Logs");
    }
    
    // UI默认设置
    SetValue("UI", "hexViewEnabled", true);
    SetValue("UI", "textViewEnabled", true);
    SetValue("UI", "showTimestamp", true);
    
    // 日志默认设置
    SetValue("Log", "enableLogging", true);
    SetValue("Log", "maxLogFiles", 10);
}

TransportConfig ConfigManager::GetDefaultTransportConfig(const std::string& transportType) const
{
    TransportConfig config;
    
    if (transportType == "Serial")
    {
        config.baudRate = 9600;
        config.dataBits = 8;
        config.parity = 0;
        config.stopBits = 1;
    }
    else if (transportType == "TCP" || transportType == "UDP")
    {
        config.ipAddress = "127.0.0.1";
        config.port = 8080;
        config.isServer = false;
    }
    
    return config;
}
