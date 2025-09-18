#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "ConfigStore.h"

namespace PortMaster {

    ConfigStore::ConfigStore() {
        // 确定配置文件路径：优先程序目录，回退到LocalAppData
        CString programDir = GetProgramDirectory();
        CString testPath = programDir + _T("\\PortMaster.config");

        // 测试程序目录是否可写
        HANDLE hTest = CreateFile(testPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (hTest != INVALID_HANDLE_VALUE) {
            CloseHandle(hTest);
            DeleteFile(testPath);
            configFilePath_ = programDir + _T("\\PortMaster.config");
        } else {
            // 回退到LocalAppData
            CString localAppData = GetLocalAppDataPath();
            EnsureDirectoryExists(localAppData + _T("\\PortMaster"));
            configFilePath_ = localAppData + _T("\\PortMaster\\PortMaster.config");
        }
    }

    ConfigStore::~ConfigStore() {
    }

    bool ConfigStore::Load(AppConfig& config) {
        std::lock_guard<std::mutex> lock(configMutex_);

        try {
            // 检查配置文件是否存在
            HANDLE hFile = CreateFile(configFilePath_, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                // 文件不存在，使用默认配置
                SetDefaults(config);
                return Save(config);
            }

            // 读取文件内容
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
                CloseHandle(hFile);
                SetDefaults(config);
                return Save(config);
            }

            std::vector<char> buffer(fileSize + 1);
            DWORD bytesRead = 0;
            if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL)) {
                CloseHandle(hFile);
                SetDefaults(config);
                return Save(config);
            }

            CloseHandle(hFile);
            buffer[bytesRead] = '\0';

            // 解析JSON（临时简化实现）
            // TODO: 实现完整的JSON解析
            SetDefaults(config);
            return true;

        } catch (const std::exception&) {
            // JSON解析错误，使用默认配置
            SetDefaults(config);
            return Save(config);
        }
    }

    bool ConfigStore::Save(const AppConfig& config) {
        std::lock_guard<std::mutex> lock(configMutex_);

        try {
            // 验证配置
            if (!Validate(config)) {
                return false;
            }

            // 转换为JSON（临时简化实现）
            // TODO: 实现完整的JSON序列化
            std::string jsonStr = "{ \"version\": \"1.0\" }";

            // 确保目录存在
            CString directory = configFilePath_.Left(configFilePath_.ReverseFind('\\'));
            EnsureDirectoryExists(directory);

            // 写入文件
            HANDLE hFile = CreateFile(configFilePath_, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                return false;
            }

            DWORD bytesWritten = 0;
            BOOL result = WriteFile(hFile, jsonStr.c_str(), static_cast<DWORD>(jsonStr.length()), &bytesWritten, NULL);
            CloseHandle(hFile);

            return result && (bytesWritten == jsonStr.length());

        } catch (const std::exception&) {
            return false;
        }
    }

    CString ConfigStore::GetConfigPath() const {
        return configFilePath_;
    }

    void ConfigStore::SetDefaults(AppConfig& config) const {
        config.ports.clear();

        // 默认串口配置
        PortConfig serialConfig;
        serialConfig.portName = _T("COM1");
        serialConfig.baudRate = 9600;
        serialConfig.timeout = 5000;
        config.ports.push_back(serialConfig);

        // 默认可靠传输配置
        config.reliableDefaults.windowSize = 4;
        config.reliableDefaults.timeoutMs = 500;
        config.reliableDefaults.maxRetries = 3;
        config.reliableDefaults.enableChecksum = true;

        // 默认UI配置
        config.ui.lastSelectedPort = _T("COM1");
        config.ui.enableReliableMode = true;
        config.ui.windowRect = {100, 100, 900, 700};
        config.ui.hexViewEnabled = false;

        // 默认日志配置
        config.logging.minLevel = LogLevel::LOG_INFO;
        config.logging.enableFileOutput = true;
        config.logging.logDirectory = GetProgramDirectory() + _T("\\Logs");
        config.logging.maxLogFiles = 10;

        // 默认网络配置
        config.network.defaultPort = 9100;
        config.network.connectionTimeout = 5000;
        config.network.enableIPP = false;
    }

    bool ConfigStore::Validate(const AppConfig& config) const {
        // TODO: 实现配置验证逻辑
        // 检查端口名称格式、波特率范围、超时值等

        // 基础验证
        if (config.reliableDefaults.windowSize == 0 || config.reliableDefaults.windowSize > 64) {
            return false;
        }

        if (config.reliableDefaults.timeoutMs < 100 || config.reliableDefaults.timeoutMs > 60000) {
            return false;
        }

        return true;
    }

    bool ConfigStore::Backup() {
        std::lock_guard<std::mutex> lock(configMutex_);

        CString backupPath = configFilePath_ + _T(".bak");
        return CopyFile(configFilePath_, backupPath, FALSE);
    }

    bool ConfigStore::Restore() {
        std::lock_guard<std::mutex> lock(configMutex_);

        CString backupPath = configFilePath_ + _T(".bak");
        return CopyFile(backupPath, configFilePath_, FALSE);
    }

    // JSON序列化方法（临时简化实现）
    CString ConfigStore::ConfigToJson(const AppConfig& config) const {
        // TODO: 实现完整的JSON序列化
        CString json;
        json.Format(_T("{\n")
                   _T("  \"version\": \"1.0\",\n")
                   _T("  \"reliableDefaults\": {\n")
                   _T("    \"windowSize\": %u,\n")
                   _T("    \"timeoutMs\": %u,\n")
                   _T("    \"maxRetries\": %u,\n")
                   _T("    \"enableChecksum\": %s\n")
                   _T("  }\n")
                   _T("}"),
                   config.reliableDefaults.windowSize,
                   config.reliableDefaults.timeoutMs,
                   config.reliableDefaults.maxRetries,
                   config.reliableDefaults.enableChecksum ? _T("true") : _T("false"));
        return json;
    }

    bool ConfigStore::JsonToConfig(const CString& jsonStr, AppConfig& config) const {
        UNREFERENCED_PARAMETER(jsonStr);
        // TODO: 实现完整的JSON解析
        // 临时使用默认配置
        SetDefaults(config);
        return true;
    }

    // 路径管理方法
    CString ConfigStore::GetProgramDirectory() const {
        TCHAR modulePath[MAX_PATH];
        GetModuleFileName(NULL, modulePath, MAX_PATH);

        CString path(modulePath);
        int pos = path.ReverseFind('\\');
        if (pos > 0) {
            return path.Left(pos);
        }

        return _T(".");
    }

    CString ConfigStore::GetLocalAppDataPath() const {
        TCHAR path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            return CString(path);
        }

        return _T("C:\\Users\\Public");
    }

    bool ConfigStore::EnsureDirectoryExists(const CString& path) const {
        DWORD attrs = GetFileAttributes(path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return true;
        }

        return CreateDirectory(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    }

} // namespace PortMaster

