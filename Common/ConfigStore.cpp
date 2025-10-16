#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ConfigStore.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ShlObj.h>
#include <direct.h>

#pragma comment(lib, "shell32.lib")

// 静态成员初始化
std::unique_ptr<ConfigStore> ConfigStore::s_instance = nullptr;
std::mutex ConfigStore::s_instanceMutex;

// 构造函数
ConfigStore::ConfigStore()
	: m_autoSaveEnabled(true)
	, m_autoSaveInterval(30)
	, m_autoSaveTimer(nullptr)
{
	// 确定配置文件路径
	m_configFilePath = FindConfigPath();
	m_backupFilePath = m_configFilePath + ".backup";

	// 加载配置
	if (!LoadConfig())
	{
		// 加载失败时使用默认配置
		m_config = PortMasterConfig();
		ValidateConfig();
	}

	// 启动自动保存
	if (m_autoSaveEnabled)
	{
		EnableAutoSave(true);
	}
}

// 析构函数
ConfigStore::~ConfigStore()
{
	// 停止自动保存
	EnableAutoSave(false);

	// 保存当前配置
	SaveConfig();
}

// 获取单例实例
ConfigStore& ConfigStore::GetInstance()
{
	std::lock_guard<std::mutex> lock(s_instanceMutex);
	if (!s_instance)
	{
		s_instance = std::make_unique<ConfigStore>();
	}
	return *s_instance;
}

// 加载配置
bool ConfigStore::LoadConfig()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 首先尝试从主配置文件加载
	if (LoadConfigFromFile(m_configFilePath))
	{
		return true;
	}

	// 主配置文件失败，尝试从备份加载
	if (LoadConfigFromFile(m_backupFilePath))
	{
		// 从备份恢复成功，保存到主配置文件
		SaveConfigToFile(m_configFilePath);
		return true;
	}

	return false;
}

// 保存配置
bool ConfigStore::SaveConfig()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 先备份当前配置
	BackupConfig();

	// 保存到主配置文件
	bool success = SaveConfigToFile(m_configFilePath);

	if (success && m_configChangedCallback)
	{
		m_configChangedCallback("配置已保存");
	}

	return success;
}

// 另存为配置
bool ConfigStore::SaveConfigAs(const std::string& filePath)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return SaveConfigToFile(filePath);
}

// 获取配置
const PortMasterConfig& ConfigStore::GetConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config;
}

// 设置配置
void ConfigStore::SetConfig(const PortMasterConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("配置已更新");
	}
}

// 获取应用配置
const AppConfig& ConfigStore::GetAppConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.app;
}

// 获取串口配置
const SerialConfig& ConfigStore::GetSerialConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.serial;
}

// 获取并口配置
const ParallelPortConfig& ConfigStore::GetParallelConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.parallel;
}

// 获取USB配置
const UsbPrintConfig& ConfigStore::GetUsbConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.usb;
}

// 获取网络配置
const NetworkPrintConfig& ConfigStore::GetNetworkConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.network;
}

// 获取回路测试配置
const LoopbackTestConfig& ConfigStore::GetLoopbackConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.loopback;
}

// 获取协议配置
const ReliableProtocolConfig& ConfigStore::GetProtocolConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.protocol;
}

// 获取UI配置
const UIConfig& ConfigStore::GetUIConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.ui;
}

// 设置应用配置
void ConfigStore::SetAppConfig(const AppConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.app = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("应用配置已更新");
	}
}

// 设置串口配置
void ConfigStore::SetSerialConfig(const SerialConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.serial = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("串口配置已更新");
	}
}

// 设置并口配置
void ConfigStore::SetParallelConfig(const ParallelPortConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.parallel = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("并口配置已更新");
	}
}

// 设置USB配置
void ConfigStore::SetUsbConfig(const UsbPrintConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.usb = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("USB配置已更新");
	}
}

// 设置网络配置
void ConfigStore::SetNetworkConfig(const NetworkPrintConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.network = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("网络配置已更新");
	}
}

// 设置回路测试配置
void ConfigStore::SetLoopbackConfig(const LoopbackTestConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.loopback = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("回路测试配置已更新");
	}
}

// 设置协议配置
void ConfigStore::SetProtocolConfig(const ReliableProtocolConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.protocol = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("协议配置已更新");
	}
}

// 设置UI配置
void ConfigStore::SetUIConfig(const UIConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.ui = config;

	if (m_configChangedCallback)
	{
		m_configChangedCallback("UI配置已更新");
	}
}

// 获取配置文件路径
std::string ConfigStore::GetConfigFilePath() const
{
	return m_configFilePath;
}

// 获取配置目录
std::string ConfigStore::GetConfigDirectory() const
{
	size_t pos = m_configFilePath.find_last_of("\\/");
	if (pos != std::string::npos)
	{
		return m_configFilePath.substr(0, pos);
	}
	return "";
}

// 获取备份文件路径
std::string ConfigStore::GetBackupFilePath() const
{
	return m_backupFilePath;
}

// 验证配置
bool ConfigStore::ValidateConfig()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	bool isValid = true;

	// 验证应用配置
	if (m_config.app.version.empty())
	{
		m_config.app.version = "1.0.0";
		isValid = false;
	}

	if (!ValidateRange(m_config.app.logLevel, 0, 3))
	{
		m_config.app.logLevel = 2;
		isValid = false;
	}

	// 验证串口配置
	if (!ValidatePortName(m_config.serial.portName, "串口"))
	{
		m_config.serial.portName = "COM1";
		isValid = false;
	}

	// 验证并口配置
	if (!ValidatePortName(m_config.parallel.portName, "并口"))
	{
		m_config.parallel.portName = "LPT1";
		isValid = false;
	}

	// 验证USB配置
	if (m_config.usb.deviceName.empty())
	{
		m_config.usb.deviceName = "USB001";
		isValid = false;
	}

	// 验证网络配置
	if (!ValidateIPAddress(m_config.network.hostname) && m_config.network.hostname.find('.') != std::string::npos)
	{
		m_config.network.hostname = "192.168.1.100";
		isValid = false;
	}

	if (!ValidateRange(static_cast<int>(m_config.network.port), 1, 65535))
	{
		m_config.network.port = 9100;
		isValid = false;
	}

	// 验证回路测试配置
	if (!ValidateRange(static_cast<int>(m_config.loopback.errorRate), 0, 100))
	{
		m_config.loopback.errorRate = 0;
		isValid = false;
	}

	if (!ValidateRange(static_cast<int>(m_config.loopback.packetLossRate), 0, 100))
	{
		m_config.loopback.packetLossRate = 0;
		isValid = false;
	}

	// 验证协议配置
	if (!ValidateRange(m_config.protocol.windowSize, 1, 256))
	{
		m_config.protocol.windowSize = 4;
		isValid = false;
	}

	if (m_config.protocol.maxPayloadSize < 64 || m_config.protocol.maxPayloadSize > 4096)
	{
		m_config.protocol.maxPayloadSize = 1024;
		isValid = false;
	}

	// 验证UI配置
	if (m_config.ui.windowWidth < 400)
	{
		m_config.ui.windowWidth = 1000;
		isValid = false;
	}

	if (m_config.ui.windowHeight < 300)
	{
		m_config.ui.windowHeight = 700;
		isValid = false;
	}

	return isValid;
}

// 修复配置
bool ConfigStore::RepairConfig()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 尝试从备份恢复
	if (RestoreFromBackup())
	{
		return true;
	}

	// 备份也无法恢复，重置为默认值
	ResetToDefaults();
	return false;
}

// 重置为默认值
void ConfigStore::ResetToDefaults()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config = PortMasterConfig();

	if (m_configChangedCallback)
	{
		m_configChangedCallback("配置已重置为默认值");
	}
}

// 添加最近文件
void ConfigStore::AddRecentFile(const std::string& filePath)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 移除重复项
	auto it = std::find(m_config.ui.recentFiles.begin(), m_config.ui.recentFiles.end(), filePath);
	if (it != m_config.ui.recentFiles.end())
	{
		m_config.ui.recentFiles.erase(it);
	}

	// 添加到开头
	m_config.ui.recentFiles.insert(m_config.ui.recentFiles.begin(), filePath);

	// 限制最大数量
	if (m_config.ui.recentFiles.size() > static_cast<size_t>(m_config.ui.maxRecentFiles))
	{
		m_config.ui.recentFiles.resize(m_config.ui.maxRecentFiles);
	}

	if (m_configChangedCallback)
	{
		m_configChangedCallback("最近文件列表已更新");
	}
}

// 移除最近文件
void ConfigStore::RemoveRecentFile(const std::string& filePath)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = std::find(m_config.ui.recentFiles.begin(), m_config.ui.recentFiles.end(), filePath);
	if (it != m_config.ui.recentFiles.end())
	{
		m_config.ui.recentFiles.erase(it);

		if (m_configChangedCallback)
		{
			m_configChangedCallback("最近文件已移除");
		}
	}
}

// 清空最近文件
void ConfigStore::ClearRecentFiles()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.ui.recentFiles.clear();

	if (m_configChangedCallback)
	{
		m_configChangedCallback("最近文件列表已清空");
	}
}

// 获取最近文件列表
std::vector<std::string> ConfigStore::GetRecentFiles() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_config.ui.recentFiles;
}

// 导出配置
bool ConfigStore::ExportConfig(const std::string& filePath) const
{
	return SaveConfigToFile(filePath);
}

// 导入配置
bool ConfigStore::ImportConfig(const std::string& filePath)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	PortMasterConfig backupConfig = m_config;

	if (LoadConfigFromFile(filePath))
	{
		// 验证导入的配置
		if (ValidateConfig())
		{
			if (m_configChangedCallback)
			{
				m_configChangedCallback("配置导入成功");
			}
			return true;
		}
		else
		{
			// 导入的配置无效，恢复备份
			m_config = backupConfig;
			return false;
		}
	}

	return false;
}

// 启用自动保存
void ConfigStore::EnableAutoSave(bool enable)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_autoSaveEnabled == enable)
	{
		return;
	}

	m_autoSaveEnabled = enable;

	if (enable)
	{
		// 启动定时器
		if (m_autoSaveTimer)
		{
			KillTimer(nullptr, reinterpret_cast<UINT_PTR>(m_autoSaveTimer));
		}

		m_autoSaveTimer = reinterpret_cast<HANDLE>(SetTimer(nullptr, 0, m_autoSaveInterval * 1000, AutoSaveTimerProc));
	}
	else
	{
		// 停止定时器
		if (m_autoSaveTimer)
		{
			KillTimer(nullptr, reinterpret_cast<UINT_PTR>(m_autoSaveTimer));
			m_autoSaveTimer = nullptr;
		}
	}
}

// 检查自动保存是否启用
bool ConfigStore::IsAutoSaveEnabled() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_autoSaveEnabled;
}

// 设置自动保存间隔
void ConfigStore::SetAutoSaveInterval(int seconds)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_autoSaveInterval = seconds;

	// 重新启动定时器
	if (m_autoSaveEnabled)
	{
		EnableAutoSave(false);
		EnableAutoSave(true);
	}
}

// 获取自动保存间隔
int ConfigStore::GetAutoSaveInterval() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_autoSaveInterval;
}

// 设置配置变更回调
void ConfigStore::SetConfigChangedCallback(ConfigChangedCallback callback)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_configChangedCallback = callback;
}

// 从文件加载配置
bool ConfigStore::LoadConfigFromFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	// 读取文件内容
	std::string content((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();

	// 解析JSON
	return DeserializeFromJson(content);
}

// 保存配置到文件
bool ConfigStore::SaveConfigToFile(const std::string& filePath) const
{
	// 确保目录存在
	std::string directory = filePath.substr(0, filePath.find_last_of("\\/"));
	if (!CreateConfigDirectory(directory))
	{
		return false;
	}

	// 序列化为JSON
	std::string jsonContent = SerializeToJson();

	// 写入文件
	std::ofstream file(filePath, std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	file << jsonContent;
	file.close();

	return true;
}

// 查找配置路径
std::string ConfigStore::FindConfigPath() const
{
	// 获取程序所在目录
	char exePath[MAX_PATH];
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);

	std::string programDir = exePath;
	size_t pos = programDir.find_last_of("\\/");
	if (pos != std::string::npos)
	{
		programDir = programDir.substr(0, pos);
	}

	std::string configFile = programDir + "\\PortMaster.json";

	// 尝试在程序目录创建配置文件
	std::ofstream testFile(configFile, std::ios::app);
	if (testFile.is_open())
	{
		testFile.close();
		return configFile;
	}

	// 程序目录无写权限，使用%LOCALAPPDATA%
	char localAppData[MAX_PATH];
	if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData) == S_OK)
	{
		std::string appDataDir = std::string(localAppData) + "\\PortMaster";
		CreateConfigDirectory(appDataDir);
		return appDataDir + "\\PortMaster.json";
	}

	// 最后的回退选项
	return "PortMaster.json";
}

// 创建配置目录
bool ConfigStore::CreateConfigDirectory(const std::string& path) const
{
	if (path.empty())
	{
		return false;
	}

	// 检查目录是否已存在
	DWORD dwAttrib = GetFileAttributesA(path.c_str());
	if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		return true;
	}

	// 创建目录
	return _mkdir(path.c_str()) == 0 || errno == EEXIST;
}

// 备份配置
bool ConfigStore::BackupConfig() const
{
	if (m_configFilePath.empty() || m_backupFilePath.empty())
	{
		return false;
	}

	return CopyFileA(m_configFilePath.c_str(), m_backupFilePath.c_str(), FALSE) != 0;
}

// 从备份恢复
bool ConfigStore::RestoreFromBackup()
{
	if (m_backupFilePath.empty())
	{
		return false;
	}

	return LoadConfigFromFile(m_backupFilePath);
}

// 序列化为JSON
std::string ConfigStore::SerializeToJson() const
{
	std::stringstream ss;

	ss << "{\n";
	ss << "  \"version\": \"1.0\",\n";
	ss << "  \"app\": {\n";
	ss << "    \"version\": \"" << EscapeJsonString(m_config.app.version) << "\",\n";
	ss << "    \"language\": \"" << EscapeJsonString(m_config.app.language) << "\",\n";
	ss << "    \"enableLogging\": " << BoolToString(m_config.app.enableLogging) << ",\n";
	ss << "    \"logLevel\": " << IntToString(m_config.app.logLevel) << ",\n";
	ss << "    \"autoSave\": " << BoolToString(m_config.app.autoSave) << ",\n";
	ss << "    \"autoSaveInterval\": " << IntToString(m_config.app.autoSaveInterval) << "\n";
	ss << "  },\n";

	ss << "  \"serial\": {\n";
	ss << "    \"portName\": \"" << EscapeJsonString(m_config.serial.portName) << "\",\n";
	ss << "    \"baudRate\": " << DwordToString(m_config.serial.baudRate) << ",\n";
	ss << "    \"dataBits\": " << IntToString(m_config.serial.dataBits) << ",\n";
	ss << "    \"parity\": " << IntToString(m_config.serial.parity) << ",\n";
	ss << "    \"stopBits\": " << IntToString(m_config.serial.stopBits) << ",\n";
	ss << "    \"flowControl\": " << DwordToString(m_config.serial.flowControl) << ",\n";
	ss << "    \"readTimeout\": " << DwordToString(m_config.serial.readTimeout) << ",\n";
	ss << "    \"writeTimeout\": " << DwordToString(m_config.serial.writeTimeout) << ",\n";
	ss << "    \"reliableMode\": " << BoolToString(false) << "\n"; // 串口配置不再支持可靠模式
	ss << "  },\n";

	// 并口配置
	ss << "  \"parallel\": {\n";
	ss << "    \"portName\": \"" << EscapeJsonString(m_config.parallel.portName) << "\",\n";
	ss << "    \"deviceName\": \"" << EscapeJsonString(m_config.parallel.deviceName) << "\",\n";
	ss << "    \"readTimeout\": " << DwordToString(m_config.parallel.readTimeout) << ",\n";
	ss << "    \"writeTimeout\": " << DwordToString(m_config.parallel.writeTimeout) << ",\n";
	ss << "    \"enableBidirectional\": " << BoolToString(m_config.parallel.enableBidirectional) << ",\n";
	ss << "    \"checkStatus\": " << BoolToString(m_config.parallel.checkStatus) << ",\n";
	ss << "    \"statusCheckInterval\": " << DwordToString(m_config.parallel.statusCheckInterval) << ",\n";
	ss << "    \"bufferSize\": " << DwordToString(m_config.parallel.bufferSize) << "\n";
	ss << "  },\n";

	// USB配置
	ss << "  \"usb\": {\n";
	ss << "    \"portName\": \"" << EscapeJsonString(m_config.usb.portName) << "\",\n";
	ss << "    \"deviceName\": \"" << EscapeJsonString(m_config.usb.deviceName) << "\",\n";
	ss << "    \"deviceId\": \"" << EscapeJsonString(m_config.usb.deviceId) << "\",\n";
	ss << "    \"printerName\": \"" << EscapeJsonString(m_config.usb.printerName) << "\",\n";
	ss << "    \"readTimeout\": " << DwordToString(m_config.usb.readTimeout) << ",\n";
	ss << "    \"writeTimeout\": " << DwordToString(m_config.usb.writeTimeout) << ",\n";
	ss << "    \"bufferSize\": " << DwordToString(m_config.usb.bufferSize) << ",\n";
	ss << "    \"checkStatus\": " << BoolToString(m_config.usb.checkStatus) << ",\n";
	ss << "    \"statusCheckInterval\": " << DwordToString(m_config.usb.statusCheckInterval) << "\n";
	ss << "  },\n";

	// 网络配置
	ss << "  \"network\": {\n";
	ss << "    \"hostname\": \"" << EscapeJsonString(m_config.network.hostname) << "\",\n";
	ss << "    \"port\": " << IntToString(m_config.network.port) << ",\n";
	ss << "    \"protocol\": " << IntToString(static_cast<int>(m_config.network.protocol)) << ",\n";
	ss << "    \"queueName\": \"" << EscapeJsonString(m_config.network.queueName) << "\",\n";
	ss << "    \"userName\": \"" << EscapeJsonString(m_config.network.userName) << "\",\n";
	ss << "    \"connectTimeout\": " << DwordToString(m_config.network.connectTimeout) << ",\n";
	ss << "    \"sendTimeout\": " << DwordToString(m_config.network.sendTimeout) << ",\n";
	ss << "    \"receiveTimeout\": " << DwordToString(m_config.network.receiveTimeout) << ",\n";
	ss << "    \"enableReconnect\": " << BoolToString(m_config.network.enableReconnect) << ",\n";
	ss << "    \"maxReconnectAttempts\": " << IntToString(m_config.network.maxReconnectAttempts) << "\n";
	ss << "  },\n";

	// 回路测试配置
	ss << "  \"loopback\": {\n";
	ss << "    \"delayMs\": " << DwordToString(m_config.loopback.delayMs) << ",\n";
	ss << "    \"errorRate\": " << DwordToString(m_config.loopback.errorRate) << ",\n";
	ss << "    \"packetLossRate\": " << DwordToString(m_config.loopback.packetLossRate) << ",\n";
	ss << "    \"enableJitter\": " << BoolToString(m_config.loopback.enableJitter) << ",\n";
	ss << "    \"jitterMaxMs\": " << DwordToString(m_config.loopback.jitterMaxMs) << ",\n";
	ss << "    \"maxQueueSize\": " << DwordToString(m_config.loopback.maxQueueSize) << ",\n";
	ss << "    \"autoTest\": " << BoolToString(m_config.loopback.autoTest) << ",\n";
	ss << "    \"reliableMode\": " << BoolToString(m_config.loopback.reliableMode) << "\n";
	ss << "  },\n";

	// 可靠协议配置
	ss << "  \"protocol\": {\n";
	ss << "    \"version\": " << IntToString(m_config.protocol.version) << ",\n";
	ss << "    \"windowSize\": " << IntToString(m_config.protocol.windowSize) << ",\n";
	ss << "    \"maxRetries\": " << IntToString(m_config.protocol.maxRetries) << ",\n";
	ss << "    \"timeoutBase\": " << DwordToString(m_config.protocol.timeoutBase) << ",\n";
	ss << "    \"timeoutMax\": " << DwordToString(m_config.protocol.timeoutMax) << ",\n";
	ss << "    \"heartbeatInterval\": " << DwordToString(m_config.protocol.heartbeatInterval) << ",\n";
	ss << "    \"maxPayloadSize\": " << DwordToString(m_config.protocol.maxPayloadSize) << ",\n";
	ss << "    \"enableCompression\": " << BoolToString(m_config.protocol.enableCompression) << ",\n";
	ss << "    \"enableEncryption\": " << BoolToString(m_config.protocol.enableEncryption) << ",\n";
	ss << "    \"encryptionKey\": \"" << EscapeJsonString(m_config.protocol.encryptionKey) << "\"\n";
	ss << "  },\n";

	ss << "  \"ui\": {\n";
	ss << "    \"windowX\": " << IntToString(m_config.ui.windowX) << ",\n";
	ss << "    \"windowY\": " << IntToString(m_config.ui.windowY) << ",\n";
	ss << "    \"windowWidth\": " << IntToString(m_config.ui.windowWidth) << ",\n";
	ss << "    \"windowHeight\": " << IntToString(m_config.ui.windowHeight) << ",\n";
	ss << "    \"maximized\": " << BoolToString(m_config.ui.maximized) << ",\n";
	ss << "    \"hexDisplay\": " << BoolToString(m_config.ui.hexDisplay) << ",\n";
	ss << "    \"autoScroll\": " << BoolToString(m_config.ui.autoScroll) << ",\n";
	ss << "    \"wordWrap\": " << BoolToString(m_config.ui.wordWrap) << ",\n";
	ss << "    \"lastPortType\": \"" << EscapeJsonString(m_config.ui.lastPortType) << "\",\n";
	ss << "    \"lastPortName\": \"" << EscapeJsonString(m_config.ui.lastPortName) << "\",\n";
	ss << "    \"recentFiles\": [";

	for (size_t i = 0; i < m_config.ui.recentFiles.size(); ++i)
	{
		if (i > 0) ss << ",";
		ss << "\"" << EscapeJsonString(m_config.ui.recentFiles[i]) << "\"";
	}

	ss << "],\n";
	ss << "    \"maxRecentFiles\": " << IntToString(m_config.ui.maxRecentFiles) << "\n";
	ss << "  }\n";
	ss << "}\n";

	return ss.str();
}

// 从JSON反序列化
bool ConfigStore::DeserializeFromJson(const std::string& jsonStr)
{
	try
	{
		// 简化的JSON解析实现
		// 解析应用配置
		std::string appSection = GetJsonObject(jsonStr, "app");
		if (!appSection.empty())
		{
			m_config.app.version = GetJsonValue(appSection, "version");
			m_config.app.language = GetJsonValue(appSection, "language");
			m_config.app.enableLogging = StringToBool(GetJsonValue(appSection, "enableLogging"));
			m_config.app.logLevel = StringToInt(GetJsonValue(appSection, "logLevel"));
			m_config.app.autoSave = StringToBool(GetJsonValue(appSection, "autoSave"));
			m_config.app.autoSaveInterval = StringToInt(GetJsonValue(appSection, "autoSaveInterval"));
		}

		// 解析串口配置
		std::string serialSection = GetJsonObject(jsonStr, "serial");
		if (!serialSection.empty())
		{
			m_config.serial.portName = GetJsonValue(serialSection, "portName");
			m_config.serial.baudRate = StringToDword(GetJsonValue(serialSection, "baudRate"));
			m_config.serial.dataBits = static_cast<BYTE>(StringToInt(GetJsonValue(serialSection, "dataBits")));
			m_config.serial.parity = static_cast<BYTE>(StringToInt(GetJsonValue(serialSection, "parity")));
			m_config.serial.stopBits = static_cast<BYTE>(StringToInt(GetJsonValue(serialSection, "stopBits")));
			m_config.serial.flowControl = StringToDword(GetJsonValue(serialSection, "flowControl"));
			m_config.serial.readTimeout = StringToDword(GetJsonValue(serialSection, "readTimeout"));
			m_config.serial.writeTimeout = StringToDword(GetJsonValue(serialSection, "writeTimeout"));
			// m_config.serial.reliableMode = StringToBool(GetJsonValue(serialSection, "reliableMode")); // 串口配置不再支持可靠模式
		}

		// 解析UI配置
		std::string uiSection = GetJsonObject(jsonStr, "ui");
		if (!uiSection.empty())
		{
			m_config.ui.windowX = StringToInt(GetJsonValue(uiSection, "windowX"));
			m_config.ui.windowY = StringToInt(GetJsonValue(uiSection, "windowY"));
			m_config.ui.windowWidth = StringToInt(GetJsonValue(uiSection, "windowWidth"));
			m_config.ui.windowHeight = StringToInt(GetJsonValue(uiSection, "windowHeight"));
			m_config.ui.maximized = StringToBool(GetJsonValue(uiSection, "maximized"));
			m_config.ui.hexDisplay = StringToBool(GetJsonValue(uiSection, "hexDisplay"));
			m_config.ui.autoScroll = StringToBool(GetJsonValue(uiSection, "autoScroll"));
			m_config.ui.wordWrap = StringToBool(GetJsonValue(uiSection, "wordWrap"));
			m_config.ui.lastPortType = GetJsonValue(uiSection, "lastPortType");
			m_config.ui.lastPortName = GetJsonValue(uiSection, "lastPortName");
			m_config.ui.recentFiles = GetJsonArray(uiSection, "recentFiles");
			m_config.ui.maxRecentFiles = StringToInt(GetJsonValue(uiSection, "maxRecentFiles"));
		}

		// 解析并口配置
		std::string parallelSection = GetJsonObject(jsonStr, "parallel");
		if (!parallelSection.empty())
		{
			m_config.parallel.portName = GetJsonValue(parallelSection, "portName");
			m_config.parallel.deviceName = GetJsonValue(parallelSection, "deviceName");
			m_config.parallel.readTimeout = StringToDword(GetJsonValue(parallelSection, "readTimeout"));
			m_config.parallel.writeTimeout = StringToDword(GetJsonValue(parallelSection, "writeTimeout"));
			m_config.parallel.enableBidirectional = StringToBool(GetJsonValue(parallelSection, "enableBidirectional"));
			m_config.parallel.checkStatus = StringToBool(GetJsonValue(parallelSection, "checkStatus"));
			m_config.parallel.statusCheckInterval = StringToDword(GetJsonValue(parallelSection, "statusCheckInterval"));
			m_config.parallel.bufferSize = StringToDword(GetJsonValue(parallelSection, "bufferSize"));
		}

		// 解析USB配置
		std::string usbSection = GetJsonObject(jsonStr, "usb");
		if (!usbSection.empty())
		{
			m_config.usb.portName = GetJsonValue(usbSection, "portName");
			m_config.usb.deviceName = GetJsonValue(usbSection, "deviceName");
			m_config.usb.deviceId = GetJsonValue(usbSection, "deviceId");
			m_config.usb.printerName = GetJsonValue(usbSection, "printerName");
			m_config.usb.readTimeout = StringToDword(GetJsonValue(usbSection, "readTimeout"));
			m_config.usb.writeTimeout = StringToDword(GetJsonValue(usbSection, "writeTimeout"));
			m_config.usb.bufferSize = StringToDword(GetJsonValue(usbSection, "bufferSize"));
			m_config.usb.checkStatus = StringToBool(GetJsonValue(usbSection, "checkStatus"));
			m_config.usb.statusCheckInterval = StringToDword(GetJsonValue(usbSection, "statusCheckInterval"));
		}

		// 解析网络配置
		std::string networkSection = GetJsonObject(jsonStr, "network");
		if (!networkSection.empty())
		{
			m_config.network.hostname = GetJsonValue(networkSection, "hostname");
			m_config.network.port = static_cast<WORD>(StringToInt(GetJsonValue(networkSection, "port")));
			m_config.network.protocol = static_cast<NetworkPrintProtocol>(StringToInt(GetJsonValue(networkSection, "protocol")));
			m_config.network.queueName = GetJsonValue(networkSection, "queueName");
			m_config.network.userName = GetJsonValue(networkSection, "userName");
			m_config.network.connectTimeout = StringToDword(GetJsonValue(networkSection, "connectTimeout"));
			m_config.network.sendTimeout = StringToDword(GetJsonValue(networkSection, "sendTimeout"));
			m_config.network.receiveTimeout = StringToDword(GetJsonValue(networkSection, "receiveTimeout"));
			m_config.network.enableReconnect = StringToBool(GetJsonValue(networkSection, "enableReconnect"));
			m_config.network.maxReconnectAttempts = StringToInt(GetJsonValue(networkSection, "maxReconnectAttempts"));
		}

		// 解析回路测试配置
		std::string loopbackSection = GetJsonObject(jsonStr, "loopback");
		if (!loopbackSection.empty())
		{
			m_config.loopback.delayMs = StringToDword(GetJsonValue(loopbackSection, "delayMs"));
			m_config.loopback.errorRate = StringToDword(GetJsonValue(loopbackSection, "errorRate"));
			m_config.loopback.packetLossRate = StringToDword(GetJsonValue(loopbackSection, "packetLossRate"));
			m_config.loopback.enableJitter = StringToBool(GetJsonValue(loopbackSection, "enableJitter"));
			m_config.loopback.jitterMaxMs = StringToDword(GetJsonValue(loopbackSection, "jitterMaxMs"));
			m_config.loopback.maxQueueSize = StringToDword(GetJsonValue(loopbackSection, "maxQueueSize"));
			m_config.loopback.autoTest = StringToBool(GetJsonValue(loopbackSection, "autoTest"));
			m_config.loopback.reliableMode = StringToBool(GetJsonValue(loopbackSection, "reliableMode"));
		}

		// 解析可靠协议配置
		std::string protocolSection = GetJsonObject(jsonStr, "protocol");
		if (!protocolSection.empty())
		{
			m_config.protocol.version = static_cast<BYTE>(StringToInt(GetJsonValue(protocolSection, "version")));
			m_config.protocol.windowSize = static_cast<WORD>(StringToInt(GetJsonValue(protocolSection, "windowSize")));
			m_config.protocol.maxRetries = static_cast<WORD>(StringToInt(GetJsonValue(protocolSection, "maxRetries")));
			m_config.protocol.timeoutBase = StringToDword(GetJsonValue(protocolSection, "timeoutBase"));
			m_config.protocol.timeoutMax = StringToDword(GetJsonValue(protocolSection, "timeoutMax"));
			m_config.protocol.heartbeatInterval = StringToDword(GetJsonValue(protocolSection, "heartbeatInterval"));
			m_config.protocol.maxPayloadSize = StringToDword(GetJsonValue(protocolSection, "maxPayloadSize"));
			m_config.protocol.enableCompression = StringToBool(GetJsonValue(protocolSection, "enableCompression"));
			m_config.protocol.enableEncryption = StringToBool(GetJsonValue(protocolSection, "enableEncryption"));
			m_config.protocol.encryptionKey = GetJsonValue(protocolSection, "encryptionKey");
		}

		return true;
	}
	catch (...)
	{
		return false;
	}
}

// JSON字符串转义
std::string ConfigStore::EscapeJsonString(const std::string& str) const
{
	std::string result;
	for (char c : str)
	{
		switch (c)
		{
		case '"': result += "\\\""; break;
		case '\\': result += "\\\\"; break;
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default: result += c; break;
		}
	}
	return result;
}

// JSON字符串反转义
std::string ConfigStore::UnescapeJsonString(const std::string& str) const
{
	std::string result;
	for (size_t i = 0; i < str.length(); ++i)
	{
		if (str[i] == '\\' && i + 1 < str.length())
		{
			switch (str[i + 1])
			{
			case '"': result += '"'; i++; break;
			case '\\': result += '\\'; i++; break;
			case 'n': result += '\n'; i++; break;
			case 'r': result += '\r'; i++; break;
			case 't': result += '\t'; i++; break;
			default: result += str[i]; break;
			}
		}
		else
		{
			result += str[i];
		}
	}
	return result;
}

// 获取JSON值
std::string ConfigStore::GetJsonValue(const std::string& json, const std::string& key) const
{
	std::string searchKey = "\"" + key + "\"";
	size_t keyPos = json.find(searchKey);
	if (keyPos == std::string::npos)
	{
		return "";
	}

	size_t colonPos = json.find(':', keyPos);
	if (colonPos == std::string::npos)
	{
		return "";
	}

	// 跳过空白字符
	size_t valueStart = colonPos + 1;
	while (valueStart < json.length() && isspace(json[valueStart]))
	{
		valueStart++;
	}

	if (valueStart >= json.length())
	{
		return "";
	}

	// 确定值的结束位置
	size_t valueEnd = valueStart;
	if (json[valueStart] == '"')
	{
		// 字符串值
		valueStart++; // 跳过开头的引号
		valueEnd = json.find('"', valueStart);
		if (valueEnd == std::string::npos)
		{
			return "";
		}
		return UnescapeJsonString(json.substr(valueStart, valueEnd - valueStart));
	}
	else
	{
		// 数值或布尔值
		while (valueEnd < json.length() && json[valueEnd] != ',' && json[valueEnd] != '}' && !isspace(json[valueEnd]))
		{
			valueEnd++;
		}
		return json.substr(valueStart, valueEnd - valueStart);
	}
}

// 获取JSON对象
std::string ConfigStore::GetJsonObject(const std::string& json, const std::string& key) const
{
	std::string searchKey = "\"" + key + "\"";
	size_t keyPos = json.find(searchKey);
	if (keyPos == std::string::npos)
	{
		return "";
	}

	size_t colonPos = json.find(':', keyPos);
	if (colonPos == std::string::npos)
	{
		return "";
	}

	size_t braceStart = json.find('{', colonPos);
	if (braceStart == std::string::npos)
	{
		return "";
	}

	// 查找匹配的右大括号
	int braceCount = 1;
	size_t braceEnd = braceStart + 1;
	while (braceEnd < json.length() && braceCount > 0)
	{
		if (json[braceEnd] == '{')
		{
			braceCount++;
		}
		else if (json[braceEnd] == '}')
		{
			braceCount--;
		}
		braceEnd++;
	}

	if (braceCount != 0)
	{
		return "";
	}

	return json.substr(braceStart, braceEnd - braceStart);
}

// 获取JSON数组
std::vector<std::string> ConfigStore::GetJsonArray(const std::string& json, const std::string& key) const
{
	std::vector<std::string> result;

	std::string searchKey = "\"" + key + "\"";
	size_t keyPos = json.find(searchKey);
	if (keyPos == std::string::npos)
	{
		return result;
	}

	size_t colonPos = json.find(':', keyPos);
	if (colonPos == std::string::npos)
	{
		return result;
	}

	size_t arrayStart = json.find('[', colonPos);
	if (arrayStart == std::string::npos)
	{
		return result;
	}

	size_t arrayEnd = json.find(']', arrayStart);
	if (arrayEnd == std::string::npos)
	{
		return result;
	}

	std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

	// 解析数组元素
	size_t pos = 0;
	while (pos < arrayContent.length())
	{
		// 跳过空白字符
		while (pos < arrayContent.length() && isspace(arrayContent[pos]))
		{
			pos++;
		}

		if (pos >= arrayContent.length())
		{
			break;
		}

		if (arrayContent[pos] == '"')
		{
			// 字符串元素
			pos++; // 跳过开头引号
			size_t endQuote = arrayContent.find('"', pos);
			if (endQuote != std::string::npos)
			{
				result.push_back(UnescapeJsonString(arrayContent.substr(pos, endQuote - pos)));
				pos = endQuote + 1;

				// 查找下一个逗号
				pos = arrayContent.find(',', pos);
				if (pos != std::string::npos)
				{
					pos++;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	return result;
}

// 布尔值转字符串
std::string ConfigStore::BoolToString(bool value) const
{
	return value ? "true" : "false";
}

// 字符串转布尔值
bool ConfigStore::StringToBool(const std::string& str) const
{
	return str == "true" || str == "1";
}

// 整数转字符串
std::string ConfigStore::IntToString(int value) const
{
	return std::to_string(value);
}

// 字符串转整数
int ConfigStore::StringToInt(const std::string& str) const
{
	if (str.empty())
	{
		return 0;
	}
	return std::stoi(str);
}

// DWORD转字符串
std::string ConfigStore::DwordToString(DWORD value) const
{
	return std::to_string(value);
}

// 字符串转DWORD
DWORD ConfigStore::StringToDword(const std::string& str) const
{
	if (str.empty())
	{
		return 0;
	}
	return static_cast<DWORD>(std::stoul(str));
}

// 验证端口名
bool ConfigStore::ValidatePortName(const std::string& portName, const std::string& type) const
{
	if (portName.empty())
	{
		return false;
	}

	if (type == "串口")
	{
		return portName.length() >= 4 && portName.substr(0, 3) == "COM";
	}
	else if (type == "并口")
	{
		return portName.length() >= 4 && portName.substr(0, 3) == "LPT";
	}

	return true;
}

// 验证IP地址
bool ConfigStore::ValidateIPAddress(const std::string& ip) const
{
	if (ip.empty())
	{
		return false;
	}

	// 简单的IP地址验证
	int dotCount = 0;
	for (char c : ip)
	{
		if (c == '.')
		{
			dotCount++;
		}
		else if (!isdigit(c))
		{
			return false;
		}
	}

	return dotCount == 3;
}

// 验证范围
bool ConfigStore::ValidateRange(int value, int min, int max) const
{
	return value >= min && value <= max;
}

// 自动保存定时器回调
void CALLBACK ConfigStore::AutoSaveTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	ConfigStore::GetInstance().TriggerAutoSave();
}

// 触发自动保存
void ConfigStore::TriggerAutoSave()
{
	SaveConfig();
}