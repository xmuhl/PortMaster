#pragma once

#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "CommonTypes.h"

namespace PortMaster {

    /// <summary>
    /// 应用配置数据结构
    /// 包含所有持久化配置项
    /// </summary>
    struct AppConfig {
        // 端口配置列表
        std::vector<PortConfig> ports;

        // 默认可靠传输配置
        ReliableConfig reliableDefaults;

        // UI状态配置
        struct {
            CString lastSelectedPort;      // 最后选择的端口
            bool enableReliableMode = true; // 启用可靠模式
            RECT windowRect = {0, 0, 800, 600}; // 窗口位置大小
            bool hexViewEnabled = false;   // 十六进制视图
        } ui;

        // 日志配置
        struct {
            LogLevel minLevel = LogLevel::LOG_INFO; // 最小日志级别
            bool enableFileOutput = true;  // 启用文件输出
            CString logDirectory;          // 日志目录
            DWORD maxLogFiles = 10;        // 最大日志文件数
        } logging;

        // 网络配置
        struct {
            WORD defaultPort = 9100;       // 默认网络端口
            DWORD connectionTimeout = 5000; // 连接超时
            bool enableIPP = false;        // 启用IPP协议
        } network;
    };

    /// <summary>
    /// 配置存储管理器
    /// 负责应用配置的加载、保存和默认值管理
    /// </summary>
    class ConfigStore {
    public:
        ConfigStore();
        virtual ~ConfigStore();

        /// <summary>
        /// 加载应用配置
        /// </summary>
        /// <param name="config">输出配置结构</param>
        /// <returns>成功返回true，失败返回false</returns>
        bool Load(AppConfig& config);

        /// <summary>
        /// 保存应用配置
        /// </summary>
        /// <param name="config">输入配置结构</param>
        /// <returns>成功返回true，失败返回false</returns>
        bool Save(const AppConfig& config);

        /// <summary>
        /// 获取配置文件路径
        /// </summary>
        /// <returns>配置文件完整路径</returns>
        CString GetConfigPath() const;

        /// <summary>
        /// 重置为默认配置
        /// </summary>
        /// <param name="config">输出默认配置</param>
        void SetDefaults(AppConfig& config) const;

        /// <summary>
        /// 验证配置有效性
        /// </summary>
        /// <param name="config">要验证的配置</param>
        /// <returns>配置有效返回true</returns>
        bool Validate(const AppConfig& config) const;

        /// <summary>
        /// 备份当前配置
        /// </summary>
        /// <returns>成功返回true</returns>
        bool Backup();

        /// <summary>
        /// 从备份恢复配置
        /// </summary>
        /// <returns>成功返回true</returns>
        bool Restore();

    private:
        // JSON序列化支持（临时简化实现）
        CString ConfigToJson(const AppConfig& config) const;
        bool JsonToConfig(const CString& jsonStr, AppConfig& config) const;

        // 路径管理
        CString GetProgramDirectory() const;
        CString GetLocalAppDataPath() const;
        bool EnsureDirectoryExists(const CString& path) const;

        // 成员变量
        CString configFilePath_;                    // 配置文件路径
        mutable std::mutex configMutex_;            // 配置访问锁
    };

} // namespace PortMaster