#pragma execution_character_set("utf-8")

#include "pch.h"
#include "PortDetector.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// 静态成员初始化
bool PortDetector::s_initialized = false;
std::vector<DeviceInfo> PortDetector::s_cachedDevices = {};

// GUID定义
static const GUID GUID_DEVCLASS_PORTS = { 0x4d36e978, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
static const GUID GUID_DEVINTERFACE_PARALLEL = { 0x97f76ef0, 0xf883, 0x11d0, { 0xaf, 0x1f, 0x00, 0x00, 0xf8, 0x00, 0x84, 0x5c } };
static const GUID GUID_CLASS_I82930_BULK = { 0x28d78fad, 0x5a12, 0x11d1, { 0xae, 0x5b, 0x00, 0x00, 0xf8, 0x03, 0xa8, 0xc2 } };

// ==================== 公共接口实现 ====================

std::vector<DeviceInfo> PortDetector::EnumerateAllDevices()
{
	if (!InitializeEnvironment())
	{
		return {};
	}

	// 清理缓存
	s_cachedDevices.clear();

	Logger::LogDebug("[PortDetector] 开始枚举所有设备");

	// 枚举各种类型设备
	auto serialDevices = EnumerateSerialPorts();
	auto parallelDevices = EnumerateParallelPorts();
	auto usbDevices = EnumerateUsbPrintDevices();

	Logger::LogDebug("[PortDetector] 设备枚举完成 - 串口:" + std::to_string(serialDevices.size()) +
		" 并口:" + std::to_string(parallelDevices.size()) +
		" USB:" + std::to_string(usbDevices.size()));

	// 合并结果
	s_cachedDevices.insert(s_cachedDevices.end(), serialDevices.begin(), serialDevices.end());
	s_cachedDevices.insert(s_cachedDevices.end(), parallelDevices.begin(), parallelDevices.end());
	s_cachedDevices.insert(s_cachedDevices.end(), usbDevices.begin(), usbDevices.end());

	return s_cachedDevices;
}

std::vector<DeviceInfo> PortDetector::EnumerateDevicesByType(PortType portType)
{
	switch (portType)
	{
	case PortType::PORT_TYPE_SERIAL:
		return EnumerateSerialPorts();
	case PortType::PORT_TYPE_PARALLEL:
		return EnumerateParallelPorts();
	case PortType::PORT_TYPE_USB_PRINT:
		return EnumerateUsbPrintDevices();
	default:
		return {};
	}
}

std::vector<DeviceInfo> PortDetector::EnumerateSerialPorts()
{
	if (!InitializeEnvironment())
	{
		return {};
	}

	std::vector<DeviceInfo> devices;

	// 方法1：使用SetupDi枚举Ports类设备
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_DEVCLASS_PORTS,
		NULL,
		NULL,
		DIGCF_PRESENT
	);

	if (deviceInfoSet != INVALID_HANDLE_VALUE)
	{
		EnumerateDevicesInternal(deviceInfoSet, nullptr, &GUID_DEVCLASS_PORTS, devices, PortType::PORT_TYPE_SERIAL);
		SetupDiDestroyDeviceInfoList(deviceInfoSet);
	}

	// 方法2：遍历COM1-256作为备选方案（兼容旧系统或虚拟端口）
	for (int i = 1; i <= 256; i++)
	{
		std::string portName = "COM" + std::to_string(i);

		// 检查是否已存在
		bool found = false;
		for (const auto& device : devices)
		{
			if (device.portName == portName)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			// 尝试打开端口以验证存在性
			HANDLE hPort = TryOpenDeviceHandle(portName, PortType::PORT_TYPE_SERIAL, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);

			if (hPort != INVALID_HANDLE_VALUE)
			{
				DeviceInfo device;
				device.portType = PortType::PORT_TYPE_SERIAL;
				device.portName = portName;
				device.friendlyName = portName;
				device.status = PortStatus::Available;
				device.isConnected = true;  // 能打开说明存在连接
				device.isConfigured = true;
				device.isDisabled = false;

				devices.push_back(device);
				CloseHandle(hPort);
			}
		}
	}

	return devices;
}

std::vector<DeviceInfo> PortDetector::EnumerateParallelPorts()
{
	if (!InitializeEnvironment())
	{
		return {};
	}

	std::vector<DeviceInfo> devices;

	// 使用SetupDi枚举并行端口设备
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_PARALLEL,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
	);

	if (deviceInfoSet != INVALID_HANDLE_VALUE)
	{
		EnumerateDevicesInternal(deviceInfoSet, &GUID_DEVINTERFACE_PARALLEL, nullptr, devices, PortType::PORT_TYPE_PARALLEL);
		SetupDiDestroyDeviceInfoList(deviceInfoSet);
	}

	// 备选方案：检查LPT1-LPT3
	for (int i = 1; i <= 3; i++)
	{
		std::string portName = "LPT" + std::to_string(i);

		bool found = false;
		for (const auto& device : devices)
		{
			if (device.portName == portName)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			HANDLE hPort = TryOpenDeviceHandle(portName, PortType::PORT_TYPE_PARALLEL, GENERIC_READ | GENERIC_WRITE, 0);

			if (hPort != INVALID_HANDLE_VALUE)
			{
				DeviceInfo device;
				device.portType = PortType::PORT_TYPE_PARALLEL;
				device.portName = portName;
				device.friendlyName = portName;
				device.status = PortStatus::Available;
				device.isConnected = true;
				device.isConfigured = true;
				device.isDisabled = false;

				devices.push_back(device);
				CloseHandle(hPort);
			}
		}
	}

	return devices;
}

std::vector<DeviceInfo> PortDetector::EnumerateUsbPrintDevices()
{
	if (!InitializeEnvironment())
	{
		return {};
	}

	std::vector<DeviceInfo> devices;

	Logger::LogDebug("[PortDetector] 开始枚举USB打印设备");

	// 使用SetupDi枚举USB打印设备
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_CLASS_I82930_BULK,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
	);

	if (deviceInfoSet != INVALID_HANDLE_VALUE)
	{
		Logger::LogDebug("[PortDetector] 成功创建USB设备信息集");
		EnumerateDevicesInternal(deviceInfoSet, &GUID_CLASS_I82930_BULK, nullptr, devices, PortType::PORT_TYPE_USB_PRINT);
		SetupDiDestroyDeviceInfoList(deviceInfoSet);
		Logger::LogDebug("[PortDetector] USB设备枚举完成，找到 " + std::to_string(devices.size()) + " 个设备");
	}
	else
	{
		Logger::LogError("[PortDetector] 无法创建USB设备信息集");
	}

	return devices;
}

PortStatus PortDetector::CheckDeviceStatus(const std::string& portName)
{
	auto device = FindDeviceByPortName(portName);
	if (device.portName.empty())
	{
		return PortStatus::Unknown;
	}

	return device.status;
}

bool PortDetector::IsDeviceConnected(const DeviceInfo& device)
{
	if (!device.deviceInstanceId.empty())
	{
		PortStatus status = DetectDeviceConnectionStatus(device.deviceInstanceId);
		return status == PortStatus::Connected || status == PortStatus::Available;
	}

	// 备选方案：尝试打开设备
	HANDLE hDevice = TryOpenDeviceHandle(device.portName, device.portType);
	bool connected = (hDevice != INVALID_HANDLE_VALUE);

	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
	}

	return connected;
}

bool PortDetector::QuickCheckDevice(const std::string& portName, PortType portType)
{
	// 快速检测：尝试打开端口
	HANDLE hDevice = TryOpenDeviceHandle(portName, portType);
	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
		return true;
	}

	return false;
}

bool PortDetector::GetDeviceDetails(DeviceInfo& device)
{
	// 【简化实现】基础信息已在EnumerateDevicesInternal中获取
	device.isConnected = !device.friendlyName.empty();
	device.isConfigured = !device.hardwareId.empty();
	device.isDisabled = false;

	return true;
}

DeviceInfo PortDetector::FindDeviceByPortName(const std::string& portName)
{
	// 先检查缓存
	for (const auto& device : s_cachedDevices)
	{
		if (device.portName == portName)
		{
			return device;
		}
	}

	// 缓存中没有，重新枚举并查找
	auto allDevices = EnumerateAllDevices();
	for (const auto& device : allDevices)
	{
		if (device.portName == portName)
		{
			return device;
		}
	}

	// 未找到，返回默认对象
	return DeviceInfo();
}

std::string PortDetector::StatusToString(PortStatus status)
{
	switch (status)
	{
	case PortStatus::Unknown:
		return "未知";
	case PortStatus::Available:
		return "可用";
	case PortStatus::Connected:
		return "已连接";
	case PortStatus::Busy:
		return "忙碌";
	case PortStatus::Offline:
		return "离线";
	case PortStatus::Error:
		return "错误";
	default:
		return "未知";
	}
}

std::string PortDetector::PortTypeToString(PortType portType)
{
	switch (portType)
	{
	case PortType::PORT_TYPE_SERIAL:
		return "串口";
	case PortType::PORT_TYPE_PARALLEL:
		return "并口";
	case PortType::PORT_TYPE_USB_PRINT:
		return "USB";
	case PortType::PORT_TYPE_NETWORK_PRINT:
		return "网络";
	case PortType::PORT_TYPE_LOOPBACK:
		return "回路";
	default:
		return "未知";
	}
}

// ==================== 内部辅助方法实现 ====================

bool PortDetector::InitializeEnvironment()
{
	if (s_initialized)
	{
		return true;
	}

	// 初始化日志系统
	Logger::Initialize("PortMaster_debug.log");
	Logger::LogDebug("[PortDetector] 环境初始化");

	// 初始化其他资源（如果有）
	s_initialized = true;
	return true;
}

void PortDetector::CleanupEnvironment()
{
	s_cachedDevices.clear();
	s_initialized = false;
}

bool PortDetector::EnumerateDevicesInternal(
	HDEVINFO deviceInfoSet,
	const GUID* deviceInterfaceGuid,
	const GUID* deviceClassGuid,
	std::vector<DeviceInfo>& devices,
	PortType portType)
{
	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	DWORD deviceIndex = 0;
	while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData))
	{
		deviceIndex++;

		DeviceInfo device;
		device.portType = portType;

		// 获取友好名称
		GetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_FRIENDLYNAME, device.friendlyName);

		// 获取设备描述
		GetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, device.description);

		// 获取硬件ID
		GetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, device.hardwareId);

		// 获取制造商
		GetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_MFG, device.manufacturer);

		// 【简化实现】跳过复杂的设备实例ID获取
		// 实际项目中可以使用CM_Get_Device_ID获取

		// 对于需要设备接口路径的设备（如USB），获取详细路径
		if (deviceInterfaceGuid != nullptr)
		{
			// 枚举设备接口
			SP_DEVICE_INTERFACE_DATA interfaceData;
			interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

			DWORD interfaceIndex = 0;
			while (SetupDiEnumDeviceInterfaces(deviceInfoSet, &deviceInfoData, deviceInterfaceGuid, interfaceIndex, &interfaceData))
			{
				interfaceIndex++;

				// 获取设备接口详细信息的缓冲区大小
				DWORD detailSize = 0;
				SP_DEVICE_INTERFACE_DETAIL_DATA* pDetailData = nullptr;

				if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &interfaceData, nullptr, 0, &detailSize, nullptr))
				{
					DWORD error = GetLastError();
					if (error == ERROR_INSUFFICIENT_BUFFER && detailSize > 0)
					{
						// 分配缓冲区
						pDetailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(new BYTE[detailSize]);
						pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

						// 获取详细信息
						if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &interfaceData, pDetailData, detailSize, nullptr, nullptr))
						{
							// 将WCHAR设备路径转换为UTF-8字符串
							LPCWSTR wideStr = pDetailData->DevicePath;
							int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
							if (sizeNeeded > 0)
							{
								std::vector<char> utf8Buffer(sizeNeeded);
								WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Buffer.data(), sizeNeeded, nullptr, nullptr);
								device.devicePath = utf8Buffer.data();
								Logger::LogDebug("[PortDetector] 获取设备路径: " + device.devicePath);
							}
						}

						// 释放缓冲区
						delete[] pDetailData;
					}
				}

				// 成功获取路径后退出循环（每个设备接口只取第一个）
				if (!device.devicePath.empty())
				{
					break;
				}
			}
		}

		// 提取端口名称
		if (!device.friendlyName.empty())
		{
			// 从友好名称中提取端口名（如从"CH340 (COM3)"中提取"COM3"）
			size_t pos = device.friendlyName.find('(');
			if (pos != std::string::npos)
			{
				size_t endPos = device.friendlyName.find(')', pos);
				if (endPos != std::string::npos)
				{
					device.portName = device.friendlyName.substr(pos + 1, endPos - pos - 1);
				}
			}
			else
			{
				// 备选：从硬件ID中提取
				device.portName = ExtractPortNameFromPath(device.hardwareId, portType);
			}
		}

		// 如果仍未找到端口名，尝试从设备描述或设备路径中提取
		if (device.portName.empty())
		{
			// 对于USB设备，优先使用devicePath进行端口提取
			if (portType == PortType::PORT_TYPE_USB_PRINT && !device.devicePath.empty())
			{
				Logger::LogDebug("[PortDetector] 从设备路径提取USB端口号");
				device.portName = ExtractPortNameFromPath(device.devicePath, portType);
			}
			else
			{
				device.portName = ExtractPortNameFromPath(device.description, portType);
			}
		}

		Logger::LogDebug("[PortDetector] 设备信息 - 端口名:" + device.portName +
			" 友好名:" + device.friendlyName +
			" 硬件ID:" + device.hardwareId +
			" 设备路径:" + device.devicePath);

		// 过滤空端口名
		if (device.portName.empty())
		{
			continue;
		}

		// 【简化实现】基础状态检测
		device.status = PortStatus::Available;
		device.isConnected = true;
		device.isConfigured = !device.hardwareId.empty();
		device.isDisabled = false;

		devices.push_back(device);
	}

	return true;
}

bool PortDetector::GetDeviceRegistryProperty(
	HDEVINFO deviceInfoSet,
	PSP_DEVINFO_DATA deviceInfoData,
	DWORD property,
	std::string& value)
{
	DWORD dataType = 0;
	DWORD bufferSize = 0;
	std::vector<BYTE> buffer;

	// 第一次调用获取所需缓冲区大小
	if (!SetupDiGetDeviceRegistryProperty(
		deviceInfoSet,
		deviceInfoData,
		property,
		&dataType,
		nullptr,
		0,
		&bufferSize))
	{
		DWORD error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
		{
			return false;
		}
	}

	// 分配缓冲区
	buffer.resize(bufferSize);

	// 第二次调用获取实际数据
	if (!SetupDiGetDeviceRegistryProperty(
		deviceInfoSet,
		deviceInfoData,
		property,
		&dataType,
		buffer.data(),
		bufferSize,
		&bufferSize))
	{
		return false;
	}

	// 转换为UTF-8字符串
	if (dataType == REG_SZ || dataType == REG_EXPAND_SZ)
	{
		// 宽字符转换为UTF-8
		LPCWSTR wideStr = reinterpret_cast<LPCWSTR>(buffer.data());
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
		if (sizeNeeded > 0)
		{
			std::vector<char> utf8Buffer(sizeNeeded);
			WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Buffer.data(), sizeNeeded, nullptr, nullptr);
			value = utf8Buffer.data();
		}
	}
	else if (dataType == REG_MULTI_SZ)
	{
		// 多字符串，取第一个
		LPCWSTR wideStr = reinterpret_cast<LPCWSTR>(buffer.data());
		size_t len = 0;
		while (wideStr[len] != L'\0')
		{
			len++;
		}
		if (len > 0)
		{
			int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, len + 1, nullptr, 0, nullptr, nullptr);
			if (sizeNeeded > 0)
			{
				std::vector<char> utf8Buffer(sizeNeeded);
				WideCharToMultiByte(CP_UTF8, 0, wideStr, len + 1, utf8Buffer.data(), sizeNeeded, nullptr, nullptr);
				value = utf8Buffer.data();
			}
		}
	}

	return true;
}

std::string PortDetector::ExtractPortNameFromPath(const std::string& devicePath, PortType portType)
{
	if (devicePath.empty())
	{
		return "";
	}

	switch (portType)
	{
	case PortType::PORT_TYPE_SERIAL:
	{
		// 查找COMx模式
		size_t pos = devicePath.find("COM");
		if (pos != std::string::npos)
		{
			size_t endPos = pos + 3;
			while (endPos < devicePath.size() && isdigit(devicePath[endPos]))
			{
				endPos++;
			}
			return devicePath.substr(pos, endPos - pos);
		}
		break;
	}

	case PortType::PORT_TYPE_PARALLEL:
	{
		// 查找LPTx模式
		size_t pos = devicePath.find("LPT");
		if (pos != std::string::npos)
		{
			size_t endPos = pos + 3;
			while (endPos < devicePath.size() && isdigit(devicePath[endPos]))
			{
				endPos++;
			}
			return devicePath.substr(pos, endPos - pos);
		}
		break;
	}

	case PortType::PORT_TYPE_USB_PRINT:
	{
		// 【关键修复】从注册表获取真实的USB端口号
		Logger::LogDebug("[PortDetector] USB端口提取，设备路径: " + devicePath);
		int portNumber = GetUsbPortNumberFromRegistry(devicePath);

		if (portNumber > 0)
		{
			// 格式化为USB001、USB002等
			char buffer[16];
			sprintf_s(buffer, "USB%03d", portNumber);
			Logger::LogDebug("[PortDetector] USB端口号: " + std::string(buffer));
			return buffer;
		}

		Logger::LogDebug("[PortDetector] 注册表查询失败，使用回退方案");

		// 回退方案：如果注册表查询失败，尝试从友好名称中提取
		size_t pos = devicePath.find("USB");
		if (pos != std::string::npos)
		{
			size_t endPos = pos + 3;
			while (endPos < devicePath.size() && isdigit(devicePath[endPos]))
			{
				endPos++;
			}
			if (endPos > pos + 3)
			{
				Logger::LogDebug("[PortDetector] 从设备路径提取端口号: " + devicePath.substr(pos, endPos - pos));
				return devicePath.substr(pos, endPos - pos);
			}
		}

		Logger::LogError("[PortDetector] 警告：无法获取USB端口号");
		return "";  // 无法获取端口号，返回空字符串
	}
	}

	return "";
}

PortStatus PortDetector::DetectDeviceConnectionStatus(const std::string& deviceInstanceId)
{
	if (deviceInstanceId.empty())
	{
		return PortStatus::Unknown;
	}

	// 【简化实现】Windows 7兼容性问题，简化状态检测
	// 实际项目中可以使用WMI或其他方法
	return PortStatus::Available;
}

HANDLE PortDetector::TryOpenDeviceHandle(
	const std::string& portName,
	PortType portType,
	DWORD accessMode,
	DWORD shareMode)
{
	// 【简化实现】统一使用\\.\前缀
	std::string devicePath = "\\\\.\\" + portName;

	// 尝试打开设备
	HANDLE hDevice = CreateFileA(
		devicePath.c_str(),
		accessMode,
		shareMode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	return hDevice;
}

// ==================== USB端口号查询方法 ====================

int PortDetector::GetUsbPortNumberFromRegistry(const std::string& devicePath)
{
	if (devicePath.empty())
	{
		Logger::LogError("[PortDetector] 错误：设备路径为空");
		return -1;
	}

	Logger::LogDebug("[PortDetector] 开始查询注册表获取USB端口号");

	// 1. 构造注册表路径
	const char* guidStr = "{28d78fad-5a12-11d1-ae5b-0000f803a8c2}";
	std::string regPath = "SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\";
	regPath += guidStr;

	// 2. 转换设备路径格式 (\\?\ -> ##?#)
	std::string transformedPath = devicePath;
	size_t pos = 0;
	while ((pos = transformedPath.find("\\\\?\\", pos)) != std::string::npos)
	{
		transformedPath.replace(pos, 4, "##?#");
		pos += 4;
	}

	regPath += "\\" + transformedPath;
	regPath += "\\#\\Device Parameters";

	Logger::LogDebug("[PortDetector] 查询注册表路径: " + regPath);

	// 3. 打开注册表键
	HKEY hKey = NULL;
	LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);

	if (result != ERROR_SUCCESS)
	{
		Logger::LogError("[PortDetector] 无法打开注册表键: " + regPath);
		return -1;
	}

	// 4. 读取Port Number
	DWORD portNumber = 0;
	DWORD dataSize = sizeof(DWORD);
	DWORD dataType = REG_DWORD;

	result = RegQueryValueExA(hKey, "Port Number", NULL, &dataType,
		(LPBYTE)&portNumber, &dataSize);

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS)
	{
		Logger::LogError("[PortDetector] 无法读取Port Number");
		return -1;
	}

	Logger::LogDebug("[PortDetector] 成功读取Port Number: " + std::to_string(portNumber));
	return static_cast<int>(portNumber);
}
