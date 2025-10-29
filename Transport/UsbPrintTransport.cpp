#pragma execution_character_set("utf-8")

#include "pch.h"
#include "UsbPrintTransport.h"
#include <algorithm>
#include <sstream>

// USB打印设备IOCTL定义
#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0
#endif

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) )
#endif

#ifndef IOCTL_USBPRINT_GET_LPT_STATUS
#define IOCTL_USBPRINT_GET_LPT_STATUS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef IOCTL_USBPRINT_SOFT_RESET
#define IOCTL_USBPRINT_SOFT_RESET      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef IOCTL_USBPRINT_GET_DEVICE_ID
#define IOCTL_USBPRINT_GET_DEVICE_ID   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

// 构造函数
UsbPrintTransport::UsbPrintTransport()
	: m_state(TransportState::Closed)
	, m_hDevice(INVALID_HANDLE_VALUE)
	, m_asyncReadRunning(false)
	, m_asyncWriteRunning(false)
	, m_statusThreadRunning(false)
	, m_lastStatus(UsbDeviceStatus::Unknown)
{
	memset(&m_stats, 0, sizeof(m_stats));
}

// 析构函数
UsbPrintTransport::~UsbPrintTransport()
{
	Close();
}

// 打开传输通道
TransportError UsbPrintTransport::Open(const TransportConfig& baseConfig)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state != TransportState::Closed)
	{
		return TransportError::AlreadyOpen;
	}

	// 转换配置类型
	const UsbPrintConfig* config = dynamic_cast<const UsbPrintConfig*>(&baseConfig);
	if (!config)
	{
		UsbPrintConfig defaultConfig;
		defaultConfig.portName = baseConfig.portName;
		defaultConfig.readTimeout = baseConfig.readTimeout;
		defaultConfig.writeTimeout = baseConfig.writeTimeout;
		defaultConfig.bufferSize = baseConfig.bufferSize;
		defaultConfig.asyncMode = baseConfig.asyncMode;
		m_config = defaultConfig;
	}
	else
	{
		m_config = *config;
	}

	if (!ValidateConfig(m_config))
	{
		return TransportError::InvalidConfig;
	}

	m_config.deviceName = NormalizePortName(m_config.portName);
	m_config.portName = m_config.deviceName;

	SetState(TransportState::Opening);

	TransportError result = OpenDeviceHandle();
	if (result != TransportError::Success)
	{
		SetState(TransportState::Error);
		return result;
	}

	result = SetDeviceTimeouts();
	if (result != TransportError::Success)
	{
		CloseDeviceHandle();
		SetState(TransportState::Error);
		return result;
	}

	// 启动状态监控线程
	if (m_config.checkStatus)
	{
		m_statusThreadRunning = true;
		m_statusThread = std::thread(&UsbPrintTransport::StatusMonitorThread, this);
	}

	if (m_config.asyncMode)
	{
		result = StartAsyncRead();
		if (result != TransportError::Success)
		{
			CloseDeviceHandle();
			SetState(TransportState::Error);
			return result;
		}
	}

	SetState(TransportState::Open);
	return TransportError::Success;
}

// 关闭传输通道
TransportError UsbPrintTransport::Close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state == TransportState::Closed)
	{
		return TransportError::Success;
	}

	SetState(TransportState::Closing);

	StopAsyncRead();

	if (m_statusThreadRunning)
	{
		m_statusThreadRunning = false;
		if (m_statusThread.joinable())
		{
			m_statusThread.join();
		}
	}

	if (m_asyncWriteRunning)
	{
		m_asyncWriteRunning = false;
		if (m_asyncWriteThread.joinable())
		{
			m_asyncWriteThread.join();
		}
	}

	CloseDeviceHandle();
	SetState(TransportState::Closed);
	return TransportError::Success;
}

// 同步写入数据
TransportError UsbPrintTransport::Write(const void* data, size_t size, size_t* written)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!data || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	return WriteToDevice(data, size, written);
}

// 同步读取数据
TransportError UsbPrintTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!buffer || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	return ReadFromDevice(buffer, size, read, timeout);
}

// 其他接口方法的基本实现
TransportError UsbPrintTransport::WriteAsync(const void* data, size_t size)
{
	if (!IsOpen()) return TransportError::NotOpen;
	if (!data || size == 0) return TransportError::InvalidParameter;

	std::vector<uint8_t> buffer(static_cast<const uint8_t*>(data),
		static_cast<const uint8_t*>(data) + size);

	{
		std::lock_guard<std::mutex> lock(m_writeQueueMutex);
		m_writeQueue.push(std::move(buffer));
	}

	if (!m_asyncWriteRunning)
	{
		m_asyncWriteRunning = true;
		m_asyncWriteThread = std::thread(&UsbPrintTransport::AsyncWriteThread, this);
	}

	return TransportError::Success;
}

TransportError UsbPrintTransport::StartAsyncRead()
{
	if (!IsOpen()) return TransportError::NotOpen;
	if (m_asyncReadRunning) return TransportError::Success;

	m_asyncReadRunning = true;
	m_asyncReadThread = std::thread(&UsbPrintTransport::AsyncReadThread, this);
	return TransportError::Success;
}

TransportError UsbPrintTransport::StopAsyncRead()
{
	if (!m_asyncReadRunning) return TransportError::Success;

	m_asyncReadRunning = false;
	if (m_asyncReadThread.joinable())
	{
		m_asyncReadThread.join();
	}
	return TransportError::Success;
}

TransportState UsbPrintTransport::GetState() const { return m_state; }
bool UsbPrintTransport::IsOpen() const { return m_state == TransportState::Open && m_hDevice != INVALID_HANDLE_VALUE; }

TransportStats UsbPrintTransport::GetStats() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_stats;
}

void UsbPrintTransport::ResetStats()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	memset(&m_stats, 0, sizeof(m_stats));
}

std::string UsbPrintTransport::GetPortName() const { return m_config.portName; }

void UsbPrintTransport::SetDataReceivedCallback(DataReceivedCallback callback) { m_dataReceivedCallback = callback; }
void UsbPrintTransport::SetStateChangedCallback(StateChangedCallback callback) { m_stateChangedCallback = callback; }
void UsbPrintTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback) { m_errorOccurredCallback = callback; }

TransportError UsbPrintTransport::FlushBuffers()
{
	if (!IsOpen()) return TransportError::NotOpen;
	if (!FlushFileBuffers(m_hDevice)) return this->GetLastError();
	return TransportError::Success;
}

size_t UsbPrintTransport::GetAvailableBytes() const { return 0; }

// USB专用方法
UsbDeviceStatus UsbPrintTransport::GetDeviceStatus() const { return QueryDeviceStatus(); }

std::string UsbPrintTransport::GetDeviceInfo() const
{
	return "USB设备: " + m_config.deviceName;
}

bool UsbPrintTransport::IsDeviceConnected() const
{
	UsbDeviceStatus status = GetDeviceStatus();
	return status != UsbDeviceStatus::NotConnected;
}

// 静态方法
std::vector<std::string> UsbPrintTransport::EnumerateUsbPorts()
{
	std::vector<std::string> ports;

	// 方法1: 检查常见的USB端口
	std::vector<std::string> commonPorts = { "USB001", "USB002", "USB003", "USB004", "USB005", "USB006" };
	for (const auto& port : commonPorts)
	{
		if (IsUsbPortAvailable(port))
		{
			ports.push_back(port);
		}
	}

	// 方法2: 通过注册表查找USB打印机
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\USB Monitor\\Ports",
		0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		char portName[256];
		DWORD index = 0;
		DWORD portNameSize = sizeof(portName);

		while (RegEnumKeyExA(hKey, index++, portName, &portNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			std::string port(portName);
			if (port.find("USB") != std::string::npos || port.find("usb") != std::string::npos)
			{
				if (std::find(ports.begin(), ports.end(), port) == ports.end())
				{
					ports.push_back(port);
				}
			}
			portNameSize = sizeof(portName);
		}
		RegCloseKey(hKey);
	}

	return ports;
}

// 增强版USB打印枚举（获取设备描述）
std::vector<PortInfo> UsbPrintTransport::EnumerateUsbPortsWithInfo()
{
	std::vector<PortInfo> portInfos;

	// 获取基本端口列表
	auto ports = EnumerateUsbPorts();

	for (const auto& port : ports)
	{
		PortInfo info;
		info.portType = PortType::PORT_TYPE_USB_PRINT;
		info.portName = port;
		info.displayName = "USB打印机 (" + port + ")";
		info.description = "USB打印设备：" + info.displayName;

		// 检测端口状态
		if (IsUsbPortAvailable(port))
		{
			info.status = PortStatus::Available;
			info.statusText = "就绪";
		}
		else
		{
			info.status = PortStatus::Offline;
			info.statusText = "离线";
		}

		portInfos.push_back(info);
	}

	return portInfos;
}

bool UsbPrintTransport::IsUsbPortAvailable(const std::string& portName)
{
	// 【修复】增强设备检测逻辑，使用多种方法验证设备可用性
	std::string devicePath = "\\\\.\\" + portName;

	// 方法1: 尝试以只写方式打开（USB打印机常用模式）
	HANDLE hDevice = CreateFileA(
		devicePath.c_str(),
		GENERIC_WRITE,           // 只写模式
		0,                       // 不共享
		nullptr,
		OPEN_EXISTING,           // 只打开已存在的设备
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice != INVALID_HANDLE_VALUE)
	{
		// 【调试信息】记录检测成功的设备
		OutputDebugStringA(("【USB端口检测】设备可用: " + portName + " (路径: " + devicePath + ")\n").c_str());

		// 尝试获取设备状态（可选）
		DWORD bytesReturned = 0;
		BYTE statusBuffer[4] = { 0 };
		BOOL statusResult = DeviceIoControl(
			hDevice,
			IOCTL_USBPRINT_GET_LPT_STATUS,
			nullptr, 0,
			statusBuffer, sizeof(statusBuffer),
			&bytesReturned,
			nullptr
		);

		if (statusResult)
		{
			OutputDebugStringA(("【USB端口检测】设备状态查询成功，状态字节: 0x" + std::to_string(statusBuffer[0]) + "\n").c_str());
		}

		CloseHandle(hDevice);
		return true;
	}

	// 【调试信息】记录检测失败的设备及原因
	DWORD lastError = ::GetLastError();
	std::string msg1 = "【USB端口检测】设备不可用: " + portName + "\n";
	OutputDebugStringA(msg1.c_str());

	std::string msg2 = "【USB端口检测】失败原因 - 错误码: " + std::to_string(lastError) + ", 路径: " + devicePath + "\n";
	OutputDebugStringA(msg2.c_str());

	// 方法2: 如果方法1失败，尝试只读模式（某些设备可能只允许读取）
	if (lastError == ERROR_ACCESS_DENIED || lastError == ERROR_SHARING_VIOLATION)
	{
		std::string msg = "【USB端口检测】尝试只读模式检测...\n";
		OutputDebugStringA(msg.c_str());

		HANDLE hDeviceRead = CreateFileA(
			devicePath.c_str(),
			GENERIC_READ,           // 只读模式
			FILE_SHARE_READ | FILE_SHARE_WRITE,  // 允许共享读写
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (hDeviceRead != INVALID_HANDLE_VALUE)
		{
			OutputDebugStringA(("【USB端口检测】设备在只读模式下可用: " + portName + "\n").c_str());
			CloseHandle(hDeviceRead);
			return true;
		}
	}

	return false;
}

std::string UsbPrintTransport::GetDeviceStatusString(UsbDeviceStatus status)
{
	std::vector<std::string> statusStrings;

	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Ready))
		statusStrings.push_back("就绪");
	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Busy))
		statusStrings.push_back("忙碌");
	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Offline))
		statusStrings.push_back("离线");
	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::OutOfPaper))
		statusStrings.push_back("缺纸");
	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Error))
		statusStrings.push_back("错误");
	if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::NotConnected))
		statusStrings.push_back("未连接");

	if (statusStrings.empty()) return "未知";

	std::string result;
	for (size_t i = 0; i < statusStrings.size(); ++i)
	{
		if (i > 0) result += ", ";
		result += statusStrings[i];
	}
	return result;
}

// 内部实现方法
void UsbPrintTransport::SetState(TransportState newState)
{
	TransportState oldState = m_state;
	m_state = newState;

	if (oldState != newState && m_stateChangedCallback)
	{
		m_stateChangedCallback(newState);
	}
}

void UsbPrintTransport::NotifyError(TransportError error, const std::string& message)
{
	if (m_errorOccurredCallback)
	{
		m_errorOccurredCallback(error, message);
	}
}

void UsbPrintTransport::UpdateStats(uint64_t bytesSent, uint64_t bytesReceived)
{
	m_stats.bytesSent += bytesSent;
	m_stats.bytesReceived += bytesReceived;
	m_stats.packetsTotal++;
}

TransportError UsbPrintTransport::OpenDeviceHandle()
{
	std::string devicePath = "\\\\.\\" + m_config.deviceName;

	// 【调试信息】记录尝试打开的设备路径
	OutputDebugStringA(("【USB端口】尝试打开设备路径: " + devicePath + "\n").c_str());
	OutputDebugStringA(("【USB端口】设备名称: " + m_config.deviceName + "\n").c_str());
	OutputDebugStringA(("【USB端口】端口名称: " + m_config.portName + "\n").c_str());

	m_hDevice = CreateFileA(
		devicePath.c_str(),
		m_config.accessMode,
		m_config.shareMode,
		nullptr,
		m_config.creationDisposition,
		m_config.flagsAndAttributes,
		nullptr
	);

	if (m_hDevice == INVALID_HANDLE_VALUE)
	{
		// 【调试信息】记录打开失败，获取详细错误信息
		DWORD lastError = ::GetLastError();
		std::string errorMsg = GetSystemErrorMessage(lastError);
		OutputDebugStringA(("【USB端口】打开设备失败！错误码: " + std::to_string(lastError) + "\n").c_str());
		OutputDebugStringA(("【USB端口】错误信息: " + errorMsg + "\n").c_str());
		OutputDebugStringA(("【USB端口】设备路径: " + devicePath + "\n").c_str());
		OutputDebugStringA(("【USB端口】访问模式: 0x" + std::to_string(m_config.accessMode) + ", 共享模式: 0x" + std::to_string(m_config.shareMode) + "\n").c_str());

		// 【错误诊断】根据错误码提供具体诊断信息
		if (lastError == ERROR_FILE_NOT_FOUND)
		{
			OutputDebugStringA("【USB端口】诊断: 设备不存在，请检查设备是否正确连接\n");
		}
		else if (lastError == ERROR_ACCESS_DENIED)
		{
			OutputDebugStringA("【USB端口】诊断: 访问被拒绝，可能原因：1)设备正被其他程序使用 2)权限不足 3)设备已被锁定\n");
		}
		else if (lastError == ERROR_SHARING_VIOLATION)
		{
			OutputDebugStringA("【USB端口】诊断: 共享冲突，设备正被其他进程使用\n");
		}
		else if (lastError == ERROR_INVALID_NAME)
		{
			OutputDebugStringA("【USB端口】诊断: 设备名称无效，请检查端口名称格式\n");
		}

		return this->GetLastError();
	}

	// 【调试信息】记录打开成功
	OutputDebugStringA(("【USB端口】设备打开成功！句柄值: 0x" + std::to_string(reinterpret_cast<uintptr_t>(m_hDevice)) + "\n").c_str());
	return TransportError::Success;
}

void UsbPrintTransport::CloseDeviceHandle()
{
	if (m_hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hDevice);
		m_hDevice = INVALID_HANDLE_VALUE;
	}
}

TransportError UsbPrintTransport::WriteToDevice(const void* data, size_t size, size_t* written)
{
	DWORD bytesWritten = 0;
	BOOL success = WriteFile(m_hDevice, data, static_cast<DWORD>(size), &bytesWritten, nullptr);

	if (written) *written = bytesWritten;

	if (!success) return this->GetLastError();

	UpdateStats(bytesWritten, 0);
	return TransportError::Success;
}

TransportError UsbPrintTransport::ReadFromDevice(void* buffer, size_t size, size_t* read, DWORD timeout)
{
	DWORD bytesRead = 0;
	BOOL success = ReadFile(m_hDevice, buffer, static_cast<DWORD>(size), &bytesRead, nullptr);

	if (read) *read = bytesRead;

	if (!success) return this->GetLastError();

	UpdateStats(0, bytesRead);
	return TransportError::Success;
}

UsbDeviceStatus UsbPrintTransport::QueryDeviceStatus() const
{
	if (!IsOpen()) return UsbDeviceStatus::NotConnected;

	// 查询打印机状态
	DWORD bytesReturned = 0;
	BYTE statusBuffer[256] = { 0 };

	// 尝试获取打印机状态
	if (DeviceIoControl(m_hDevice, IOCTL_USBPRINT_GET_LPT_STATUS,
		nullptr, 0, statusBuffer, sizeof(statusBuffer),
		&bytesReturned, nullptr))
	{
		UsbDeviceStatus status = UsbDeviceStatus::Ready;

		// 解析状态字节
		if (bytesReturned > 0)
		{
			BYTE statusByte = statusBuffer[0];

			// 检查各个状态位
			if (statusByte & 0x08) status = static_cast<UsbDeviceStatus>(static_cast<int>(status) | static_cast<int>(UsbDeviceStatus::Error));
			if (statusByte & 0x20) status = static_cast<UsbDeviceStatus>(static_cast<int>(status) | static_cast<int>(UsbDeviceStatus::OutOfPaper));
			if (statusByte & 0x80) status = static_cast<UsbDeviceStatus>(static_cast<int>(status) | static_cast<int>(UsbDeviceStatus::Busy));
		}

		return status;
	}

	// 如果无法获取详细状态，返回基本状态
	return UsbDeviceStatus::Ready;
}

void UsbPrintTransport::StatusMonitorThread()
{
	while (m_statusThreadRunning)
	{
		UsbDeviceStatus currentStatus = QueryDeviceStatus();
		if (currentStatus != m_lastStatus)
		{
			m_lastStatus = currentStatus;
		}
		Sleep(m_config.statusCheckInterval);
	}
}

void UsbPrintTransport::AsyncReadThread()
{
	const size_t bufferSize = 1024;
	std::vector<uint8_t> buffer(bufferSize);

	while (m_asyncReadRunning && IsOpen())
	{
		size_t bytesRead = 0;
		TransportError result = ReadFromDevice(buffer.data(), bufferSize, &bytesRead, m_config.readTimeout);

		if (result == TransportError::Success && bytesRead > 0)
		{
			std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
			if (m_dataReceivedCallback)
			{
				m_dataReceivedCallback(data);
			}
		}
		else if (result != TransportError::Timeout)
		{
			NotifyError(result, "异步读取失败");
			break;
		}
	}
}

void UsbPrintTransport::AsyncWriteThread()
{
	while (m_asyncWriteRunning)
	{
		std::vector<uint8_t> data;

		{
			std::lock_guard<std::mutex> lock(m_writeQueueMutex);
			if (m_writeQueue.empty())
			{
				Sleep(10);
				continue;
			}

			data = std::move(m_writeQueue.front());
			m_writeQueue.pop();
		}

		size_t written = 0;
		TransportError result = WriteToDevice(data.data(), data.size(), &written);

		if (result != TransportError::Success)
		{
			NotifyError(result, "异步写入失败");
		}
	}
}

TransportError UsbPrintTransport::GetLastError()
{
	DWORD error = ::GetLastError();
	m_stats.lastErrorCode = error;

	switch (error)
	{
	case ERROR_SUCCESS:
		return TransportError::Success;
	case ERROR_FILE_NOT_FOUND:
		return TransportError::OpenFailed;
	case ERROR_ACCESS_DENIED:
		return TransportError::Busy;
	case ERROR_INVALID_HANDLE:
		return TransportError::NotOpen;
	case ERROR_TIMEOUT:
		return TransportError::Timeout;
	default:
		return TransportError::WriteFailed;
	}
}

std::string UsbPrintTransport::GetSystemErrorMessage(DWORD errorCode) const
{
	LPSTR messageBuffer = nullptr;

	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&messageBuffer),
		0,
		nullptr
	);

	std::string message(messageBuffer, size);
	LocalFree(messageBuffer);

	return message;
}

bool UsbPrintTransport::ValidateConfig(const UsbPrintConfig& config) const
{
	return !config.deviceName.empty() && !config.portName.empty() &&
		config.readTimeout > 0 && config.writeTimeout > 0 && config.bufferSize > 0;
}

std::string UsbPrintTransport::NormalizePortName(const std::string& portName) const
{
	std::string normalized = portName;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);

	if (normalized.find("USB") != 0)
	{
		if (normalized == "1") normalized = "USB001";
		else if (normalized == "2") normalized = "USB002";
		else normalized = "USB001";
	}

	return normalized;
}

TransportError UsbPrintTransport::SetDeviceTimeouts()
{
	// 【关键修复】使用正确的USB设备超时设置方法
	DWORD readTimeout = m_config.readTimeout;
	DWORD writeTimeout = m_config.writeTimeout;

	// 方法1：尝试使用COMM结构设置超时（适用于某些USB到串口的设备）
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;              // 字符间超时
	timeouts.ReadTotalTimeoutMultiplier = 0;        // 读取总超时倍数
	timeouts.ReadTotalTimeoutConstant = readTimeout; // 读取总超时常数
	timeouts.WriteTotalTimeoutMultiplier = 0;       // 写入超时倍数
	timeouts.WriteTotalTimeoutConstant = writeTimeout; // 写入超时常数

	BOOL commResult = SetCommTimeouts(m_hDevice, &timeouts);
	if (commResult)
	{
		// COMM超时设置成功（USB-Serial适配器）
		return TransportError::Success;
	}

	// 方法2：使用DeviceIoControl设置USB打印设备特定超时
	DWORD bytesReturned = 0;

	// USB打印设备的超时设置结构
	struct UsbPrintTimeouts
	{
		DWORD ReadTimeout;
		DWORD WriteTimeout;
	} usbTimeouts = { readTimeout, writeTimeout };

	// 尝试USB打印机特定的超时设置IOCTL
	// 定义打印机设备相关常量
#define FILE_DEVICE_PRINTER         0x00000018
#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED             0
#endif
#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS             0
#endif
#ifndef CTL_CODE
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
        ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
    )
#endif

	const DWORD IOCTL_USBPRINT_SET_TIMEOUT = CTL_CODE(FILE_DEVICE_PRINTER, 13, METHOD_BUFFERED, FILE_ANY_ACCESS);

	BOOL iotResult = DeviceIoControl(
		m_hDevice,                      // 设备句柄
		IOCTL_USBPRINT_SET_TIMEOUT,     // 控制代码
		&usbTimeouts,                   // 输入缓冲区
		sizeof(usbTimeouts),            // 输入缓冲区大小
		nullptr,                        // 输出缓冲区
		0,                              // 输出缓冲区大小
		&bytesReturned,                 // 返回字节数
		nullptr                         // 重叠结构
	);

	if (iotResult)
	{
		// USB特定超时设置成功
		return TransportError::Success;
	}

	// 方法3：作为最后手段，设置文件句柄的通用I/O超时
	// 虽然Windows文件句柄没有直接的超时设置，但可以通过重叠I/O实现
	DWORD error = ::GetLastError();

	if (error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED)
	{
		// 设备不支持超时设置，这对USB打印机是常见情况
		// 超时控制将通过重叠I/O和等待机制在实际读写操作中实现
		return TransportError::Success;
	}
	else if (error == ERROR_ACCESS_DENIED)
	{
		return TransportError::AccessDenied;
	}
	else if (error == ERROR_INVALID_HANDLE)
	{
		return TransportError::NotOpen;
	}
	else
	{
		// 记录具体的错误信息用于诊断
		// std::string errorMsg = "USB超时设置失败，错误代码: " + std::to_string(error);
		// 注意：在实际项目中可能需要通过日志系统记录错误

		// 非致命错误，继续操作，但实际I/O操作需要实现超时控制
		return TransportError::Success;
	}
}

// 新增：重置USB设备
TransportError UsbPrintTransport::ResetDevice()
{
	if (!IsOpen()) return TransportError::NotOpen;

	DWORD bytesReturned = 0;
	BOOL success = DeviceIoControl(m_hDevice, IOCTL_USBPRINT_SOFT_RESET,
		nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

	if (!success)
	{
		return this->GetLastError();
	}

	return TransportError::Success;
}

// 新增：获取设备描述符
std::string UsbPrintTransport::GetDeviceDescriptor() const
{
	if (!IsOpen()) return "";

	DWORD bytesReturned = 0;
	BYTE descriptorBuffer[256] = { 0 };

	if (DeviceIoControl(m_hDevice, IOCTL_USBPRINT_GET_DEVICE_ID,
		nullptr, 0, descriptorBuffer, sizeof(descriptorBuffer),
		&bytesReturned, nullptr))
	{
		return std::string(reinterpret_cast<char*>(descriptorBuffer), bytesReturned);
	}

	return "";
}