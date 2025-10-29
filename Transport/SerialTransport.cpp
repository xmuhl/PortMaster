#pragma execution_character_set("utf-8")

#include "pch.h"
#include "SerialTransport.h"
#include "../Common/CommonTypes.h"
#include "../Common/PortDetector.h"
#include <sstream>
#include <algorithm>
#include <SetupAPI.h>
#include <initguid.h>
#include <devguid.h>

#pragma comment(lib, "setupapi.lib")

SerialTransport::SerialTransport()
	: m_hSerial(INVALID_HANDLE_VALUE)
	, m_state(TransportState::Closed)
	, m_stopReading(false)
	, m_readEvent(nullptr)
	, m_writeEvent(nullptr)
{
	m_stats = {};
	m_readBuffer.resize(4096);

	// 创建重叠事件
	m_readEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	m_writeEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	ZeroMemory(&m_readOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&m_writeOverlapped, sizeof(OVERLAPPED));

	m_readOverlapped.hEvent = m_readEvent;
	m_writeOverlapped.hEvent = m_writeEvent;
}

SerialTransport::~SerialTransport()
{
	Close();
}

TransportError SerialTransport::Open(const TransportConfig& config)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state != TransportState::Closed)
	{
		return TransportError::AlreadyOpen;
	}

	// 验证配置类型
	const SerialConfig* serialConfig = dynamic_cast<const SerialConfig*>(&config);
	if (!serialConfig)
	{
		return TransportError::InvalidConfig;
	}

	m_config = *serialConfig;

	// 【端口访问控制增强】设备预检测
	PortDetector::QuickCheckDevice(m_config.portName, PortType::PORT_TYPE_SERIAL);

	// 构建串口名称
	std::string portNameA = m_config.portName;
	std::wstring portNameW = std::wstring(portNameA.begin(), portNameA.end());

	// 【调试信息】记录原始端口名称
	std::string msg = "【串口】尝试打开串口: " + portNameA + "\n";
	OutputDebugStringA(msg.c_str());

	if (portNameW.find(L"COM") == std::wstring::npos)
	{
		portNameW = L"COM" + portNameW;
		// 使用WideCharToMultiByte进行正确转换，避免警告
		{
			int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string narrowStr(sizeNeeded, 0);
			WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, &narrowStr[0], sizeNeeded, nullptr, nullptr);
			OutputDebugStringA(("【串口】添加COM前缀: " + narrowStr + "\n").c_str());
		}
	}
	if (portNameW.find(L"\\\\.\\") == std::wstring::npos)
	{
		portNameW = L"\\\\.\\" + portNameW;
		// 使用WideCharToMultiByte进行正确转换，避免警告
		{
			int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string narrowStr(sizeNeeded, 0);
			WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, &narrowStr[0], sizeNeeded, nullptr, nullptr);
			OutputDebugStringA(("【串口】添加\\\\.\\前缀: " + narrowStr + "\n").c_str());
		}
	}

	// 打开串口
	// 使用WideCharToMultiByte进行正确转换，避免警告
	{
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string narrowStr(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, &narrowStr[0], sizeNeeded, nullptr, nullptr);
		OutputDebugStringA(("【串口】最终设备路径: " + narrowStr + "\n").c_str());
	}

	m_hSerial = CreateFileW(
		portNameW.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (m_hSerial == INVALID_HANDLE_VALUE)
	{
		// 【增强】获取详细错误信息
		DWORD lastError = ::GetLastError();
		std::string error = CommonUtils::GetLastErrorString();

		std::string msg1 = "【串口】打开串口失败！\n";
		std::string msg2 = "【串口】错误码: " + std::to_string(lastError) + "\n";
		std::string msg3 = "【串口】错误信息: " + error + "\n";
		// 使用WideCharToMultiByte进行正确转换，避免警告
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string msg4(sizeNeeded + 20, 0);  // 预留空间
		msg4 = "【串口】设备路径: ";
		WideCharToMultiByte(CP_UTF8, 0, portNameW.c_str(), -1, &msg4[msg4.length()], sizeNeeded, nullptr, nullptr);
		msg4 += "\n";

		OutputDebugStringA(msg1.c_str());
		OutputDebugStringA(msg2.c_str());
		OutputDebugStringA(msg3.c_str());
		OutputDebugStringA(msg4.c_str());

		// 【错误诊断】根据错误码提供具体建议
		if (lastError == ERROR_FILE_NOT_FOUND)
		{
			std::string msg = "【串口】诊断: 串口不存在，请检查：1)端口名称是否正确 2)设备是否正确连接 3)驱动程序是否安装\n";
			OutputDebugStringA(msg.c_str());
		}
		else if (lastError == ERROR_ACCESS_DENIED)
		{
			std::string msg = "【串口】诊断: 访问被拒绝，可能原因：1)端口正被其他程序使用 2)权限不足 3)设备已被锁定\n";
			OutputDebugStringA(msg.c_str());
		}
		else if (lastError == ERROR_SHARING_VIOLATION)
		{
			std::string msg = "【串口】诊断: 共享冲突，串口正被其他进程占用\n";
			OutputDebugStringA(msg.c_str());
		}

		ReportError(TransportError::OpenFailed, "Failed to open serial port: " + error);
		return TransportError::OpenFailed;
	}

	// 【调试信息】记录成功
	OutputDebugStringA(("【串口】串口打开成功！句柄: 0x" + std::to_string(reinterpret_cast<uintptr_t>(m_hSerial)) + "\n").c_str());

	// 设置串口参数
	if (!SetCommState(m_config))
	{
		CloseHandle(m_hSerial);
		m_hSerial = INVALID_HANDLE_VALUE;
		ReportError(TransportError::ConfigFailed, "Failed to configure serial port");
		return TransportError::ConfigFailed;
	}

	// 设置超时
	if (!SetCommTimeouts(m_config.readTimeout, m_config.writeTimeout))
	{
		CloseHandle(m_hSerial);
		m_hSerial = INVALID_HANDLE_VALUE;
		ReportError(TransportError::ConfigFailed, "Failed to set serial timeouts");
		return TransportError::ConfigFailed;
	}

	// 设置缓冲区大小
	if (!SetupComm(m_hSerial, 4096, 4096))
	{
		// 非致命错误，继续
	}

	// 清空缓冲区
	PurgeComm(m_hSerial, PURGE_TXCLEAR | PURGE_RXCLEAR);

	UpdateState(TransportState::Open);
	ResetStats();

	return TransportError::Success;
}

TransportError SerialTransport::Close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state == TransportState::Closed)
	{
		return TransportError::Success;
	}

	// 停止异步读取
	StopAsyncRead();

	// 关闭串口
	if (m_hSerial != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hSerial);
		m_hSerial = INVALID_HANDLE_VALUE;
	}

	// 释放事件句柄
	if (m_readEvent != nullptr)
	{
		CloseHandle(m_readEvent);
		m_readEvent = nullptr;
	}
	if (m_writeEvent != nullptr)
	{
		CloseHandle(m_writeEvent);
		m_writeEvent = nullptr;
	}

	UpdateState(TransportState::Closed);

	return TransportError::Success;
}

TransportError SerialTransport::Write(const void* data, size_t size, size_t* written)
{
	if (!data || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state != TransportState::Open)
	{
		return TransportError::NotOpen;
	}

	if (m_hSerial == INVALID_HANDLE_VALUE)
	{
		return TransportError::NotOpen;
	}

	// 【分类3.2修复】实现分段循环写入，支持超过4GB的数据
	size_t totalWritten = 0;
	const uint8_t* pData = static_cast<const uint8_t*>(data);

	while (totalWritten < size)
	{
		// 计算本次分块大小（不超过MAXDWORD）
		DWORD chunkSize = static_cast<DWORD>(
			(size - totalWritten > MAXDWORD)
			? MAXDWORD
			: (size - totalWritten)
		);

		DWORD bytesWritten = 0;
		BOOL result = FALSE;

		if (m_config.writeTimeout == 0)
		{
			// 非阻塞写入
			ResetEvent(m_writeEvent);
			m_writeOverlapped.Internal = 0;
			m_writeOverlapped.InternalHigh = 0;
			m_writeOverlapped.Offset = 0;
			m_writeOverlapped.OffsetHigh = 0;

			result = WriteFile(m_hSerial, pData + totalWritten, chunkSize, &bytesWritten, &m_writeOverlapped);

			if (!result && GetLastError() == ERROR_IO_PENDING)
			{
				// 等待写入完成
				if (GetOverlappedResult(m_hSerial, &m_writeOverlapped, &bytesWritten, TRUE))
				{
					result = TRUE;
				}
			}
		}
		else
		{
			// 阻塞写入
			result = WriteFile(m_hSerial, pData + totalWritten, chunkSize, &bytesWritten, nullptr);
		}

		if (!result)
		{
			std::string error = CommonUtils::GetLastErrorString();
			ReportError(TransportError::WriteFailed, "Write failed at offset " + std::to_string(totalWritten) + ": " + error);

			// 返回已写入的字节数
			if (written)
			{
				*written = totalWritten;
			}
			return TransportError::WriteFailed;
		}

		if (bytesWritten == 0)
		{
			// 写入停滞，避免无限循环
			ReportError(TransportError::WriteFailed, "Write returned 0 bytes");
			if (written)
			{
				*written = totalWritten;
			}
			return TransportError::WriteFailed;
		}

		totalWritten += bytesWritten;
		UpdateStats(bytesWritten, 0);
	}

	if (written)
	{
		*written = totalWritten;
	}

	return TransportError::Success;
}

TransportError SerialTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!buffer || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	// 【分类3.2修复】实现分段循环读取，支持超过4GB的数据
	size_t totalRead = 0;
	uint8_t* pBuffer = static_cast<uint8_t*>(buffer);

	while (totalRead < size)
	{
		// 计算本次分块大小（不超过MAXDWORD）
		DWORD chunkSize = static_cast<DWORD>(
			(size - totalRead > MAXDWORD)
			? MAXDWORD
			: (size - totalRead)
		);

		DWORD bytesRead = 0;

		if (m_config.asyncMode)
		{
			// 异步读取
			OVERLAPPED overlapped = { 0 };
			overlapped.hEvent = m_readEvent;

			if (!ReadFile(m_hSerial, pBuffer + totalRead, chunkSize, &bytesRead, &overlapped))
			{
				if (GetLastError() == ERROR_IO_PENDING)
				{
					// 等待读取完成
					DWORD waitResult = WaitForSingleObject(m_readEvent, timeout);
					if (waitResult == WAIT_OBJECT_0)
					{
						if (!GetOverlappedResult(m_hSerial, &overlapped, &bytesRead, FALSE))
						{
							m_stats.lastErrorCode = GetLastError();
							if (read)
								*read = totalRead;
							return TransportError::ReadFailed;
						}
					}
					else if (waitResult == WAIT_TIMEOUT)
					{
						CancelIoEx(m_hSerial, &overlapped);
						if (read)
							*read = totalRead;
						return TransportError::Timeout;
					}
					else
					{
						m_stats.lastErrorCode = GetLastError();
						if (read)
							*read = totalRead;
						return TransportError::ReadFailed;
					}
				}
				else
				{
					m_stats.lastErrorCode = GetLastError();
					if (read)
						*read = totalRead;
					return TransportError::ReadFailed;
				}
			}
		}
		else
		{
			// 同步读取
			if (!ReadFile(m_hSerial, pBuffer + totalRead, chunkSize, &bytesRead, NULL))
			{
				m_stats.lastErrorCode = GetLastError();
				if (read)
					*read = totalRead;
				return TransportError::ReadFailed;
			}
		}

		if (bytesRead == 0)
		{
			// 读取停滞（可能EOF），返回已读数据
			break;
		}

		totalRead += bytesRead;
		UpdateStats(0, bytesRead);
	}

	if (read)
	{
		*read = totalRead;
	}

	return TransportError::Success;
}

TransportError SerialTransport::WriteAsync(const void* data, size_t size)
{
	size_t written;
	return Write(data, size, &written);
}

TransportError SerialTransport::StartAsyncRead()
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (m_readThread.joinable())
	{
		return TransportError::Success;
	}

	m_stopReading = false;
	m_readThread = std::thread(&SerialTransport::AsyncReadThread, this);

	return TransportError::Success;
}

TransportError SerialTransport::StopAsyncRead()
{
	m_stopReading = true;

	if (m_readThread.joinable())
	{
		// 取消I/O操作
		if (m_hSerial != INVALID_HANDLE_VALUE)
		{
			CancelIoEx(m_hSerial, nullptr);
		}

		m_readThread.join();
	}

	return TransportError::Success;
}

void SerialTransport::AsyncReadThread()
{
	std::vector<uint8_t> buffer(m_config.bufferSize);

	while (!m_stopReading && IsOpen())
	{
		size_t bytesRead = 0;
		TransportError error = Read(buffer.data(), buffer.size(), &bytesRead, 100);

		if (error == TransportError::Success && bytesRead > 0)
		{
			if (m_dataReceivedCallback)
			{
				std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
				m_dataReceivedCallback(data);
			}
		}
		else if (error != TransportError::Timeout)
		{
			// 非超时错误
			ReportError(error, "异步读取失败");
			break;
		}
	}
}

bool SerialTransport::SetCommState(const SerialConfig& config)
{
	DCB dcb = { 0 };
	dcb.DCBlength = sizeof(DCB);

	if (!::GetCommState(m_hSerial, &dcb))
	{
		m_stats.lastErrorCode = GetLastError();
		return false;
	}

	dcb.BaudRate = config.baudRate;
	dcb.ByteSize = config.dataBits;
	dcb.Parity = config.parity;
	dcb.StopBits = config.stopBits;

	// 流控制设置
	dcb.fOutxCtsFlow = (config.flowControl & 0x01) ? TRUE : FALSE;  // CTS
	dcb.fOutxDsrFlow = (config.flowControl & 0x02) ? TRUE : FALSE;  // DSR
	dcb.fDtrControl = config.dtr ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
	dcb.fRtsControl = config.rts ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
	dcb.fOutX = config.xonXoff ? TRUE : FALSE;
	dcb.fInX = config.xonXoff ? TRUE : FALSE;
	dcb.fDsrSensitivity = config.dsrSensitivity ? TRUE : FALSE;

	dcb.fBinary = TRUE;
	dcb.fParity = (config.parity != NOPARITY) ? TRUE : FALSE;
	dcb.fNull = FALSE;
	dcb.fAbortOnError = FALSE;

	if (!::SetCommState(m_hSerial, &dcb))
	{
		m_stats.lastErrorCode = GetLastError();
		return false;
	}

	return true;
}

bool SerialTransport::SetCommTimeouts(DWORD readTimeout, DWORD writeTimeout)
{
	COMMTIMEOUTS timeouts = { 0 };

	// 读取超时设置
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutConstant = readTimeout;
	timeouts.ReadTotalTimeoutMultiplier = 0;

	// 写入超时设置
	timeouts.WriteTotalTimeoutConstant = writeTimeout;
	timeouts.WriteTotalTimeoutMultiplier = 0;

	if (!::SetCommTimeouts(m_hSerial, &timeouts))
	{
		m_stats.lastErrorCode = GetLastError();
		return false;
	}

	return true;
}

TransportState SerialTransport::GetState() const
{
	return m_state;
}

bool SerialTransport::IsOpen() const
{
	return m_state == TransportState::Open && m_hSerial != INVALID_HANDLE_VALUE;
}

TransportStats SerialTransport::GetStats() const
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	return m_stats;
}

void SerialTransport::ResetStats()
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	ZeroMemory(&m_stats, sizeof(m_stats));
}

std::string SerialTransport::GetPortName() const
{
	return m_config.portName;
}

void SerialTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
	m_dataReceivedCallback = callback;
}

void SerialTransport::SetStateChangedCallback(StateChangedCallback callback)
{
	m_stateChangedCallback = callback;
}

void SerialTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback)
{
	m_errorOccurredCallback = callback;
}

TransportError SerialTransport::FlushBuffers()
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!::PurgeComm(m_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR))
	{
		m_stats.lastErrorCode = GetLastError();
		return TransportError::WriteFailed;
	}

	return TransportError::Success;
}

size_t SerialTransport::GetAvailableBytes() const
{
	if (!IsOpen())
	{
		return 0;
	}

	COMSTAT comStat;
	DWORD errors;

	if (!::ClearCommError(m_hSerial, &errors, &comStat))
	{
		return 0;
	}

	return comStat.cbInQue;
}

void SerialTransport::UpdateState(TransportState newState)
{
	m_state = newState;
	if (m_stateChangedCallback)
	{
		m_stateChangedCallback(newState);
	}
}

void SerialTransport::ReportError(TransportError error, const std::string& message)
{
	if (m_errorOccurredCallback)
	{
		m_errorOccurredCallback(error, message);
	}
}

void SerialTransport::UpdateStats(size_t bytesSent, size_t bytesReceived)
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	m_stats.bytesSent += bytesSent;
	m_stats.bytesReceived += bytesReceived;
	m_stats.packetsTotal++;
}

std::vector<std::string> SerialTransport::EnumerateSerialPorts()
{
	std::vector<std::string> ports;

	// 使用SetupAPI枚举串口
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);

	if (hDevInfo != INVALID_HANDLE_VALUE)
	{
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++)
		{
			HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
			if (hKey != INVALID_HANDLE_VALUE)
			{
				char portName[256];
				DWORD size = sizeof(portName);
				DWORD type = 0;

				if (RegQueryValueExA(hKey, "PortName", NULL, &type, (LPBYTE)portName, &size) == ERROR_SUCCESS)
				{
					if (type == REG_SZ && strncmp(portName, "COM", 3) == 0)
					{
						ports.push_back(portName);
					}
				}

				RegCloseKey(hKey);
			}
		}

		SetupDiDestroyDeviceInfoList(hDevInfo);
	}

	// 按端口号排序
	std::sort(ports.begin(), ports.end(), [](const std::string& a, const std::string& b) {
		int numA = atoi(a.substr(3).c_str());
		int numB = atoi(b.substr(3).c_str());
		return numA < numB;
		});

	return ports;
}

// 增强版串口枚举（获取设备描述）
std::vector<PortInfo> SerialTransport::EnumerateSerialPortsWithInfo()
{
	std::vector<PortInfo> portInfos;

	// 使用SetupAPI枚举串口
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);

	if (hDevInfo != INVALID_HANDLE_VALUE)
	{
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++)
		{
			HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
			if (hKey != INVALID_HANDLE_VALUE)
			{
				char portName[256];
				DWORD size = sizeof(portName);
				DWORD type = 0;

				if (RegQueryValueExA(hKey, "PortName", NULL, &type, (LPBYTE)portName, &size) == ERROR_SUCCESS)
				{
					if (type == REG_SZ && strncmp(portName, "COM", 3) == 0)
					{
						PortInfo info;
						info.portType = PortType::PORT_TYPE_SERIAL;
						info.portName = portName;

						// 获取设备友好名称
						info.displayName = GetDeviceFriendlyName(portName);
						info.description = "串口设备：" + info.displayName;

						// 检测端口状态
						info.status = CheckSerialPortStatus(portName);

						// 设置状态文本
						switch (info.status)
						{
							case PortStatus::Available:
							case PortStatus::Connected:
								info.statusText = "已连接";
								break;
							case PortStatus::Offline:
								info.statusText = "未连接";
								break;
							case PortStatus::Busy:
								info.statusText = "忙碌";
								break;
							default:
								info.statusText = "未知";
								break;
						}

						portInfos.push_back(info);
					}
				}

				RegCloseKey(hKey);
			}
		}

		SetupDiDestroyDeviceInfoList(hDevInfo);
	}

	// 按端口号排序
	std::sort(portInfos.begin(), portInfos.end(), [](const PortInfo& a, const PortInfo& b) {
		int numA = atoi(a.portName.substr(3).c_str());
		int numB = atoi(b.portName.substr(3).c_str());
		return numA < numB;
		});

	return portInfos;
}

// 获取设备友好名称
std::string SerialTransport::GetDeviceFriendlyName(const std::string& portName)
{
	// 使用SetupAPI枚举串口设备，查找对应的友好名称
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);

	if (hDevInfo != INVALID_HANDLE_VALUE)
	{
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++)
		{
			HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
			if (hKey != INVALID_HANDLE_VALUE)
			{
				char currentPortName[256];
				DWORD size = sizeof(currentPortName);
				DWORD type = 0;

				if (RegQueryValueExA(hKey, "PortName", NULL, &type, (LPBYTE)currentPortName, &size) == ERROR_SUCCESS)
				{
					if (strcmp(currentPortName, portName.c_str()) == 0)
					{
						// 查找FriendlyName
						char friendlyName[256] = {0};
						size = sizeof(friendlyName);
						if (RegQueryValueExA(hKey, "FriendlyName", NULL, &type, (LPBYTE)friendlyName, &size) == ERROR_SUCCESS)
						{
							RegCloseKey(hKey);
							SetupDiDestroyDeviceInfoList(hDevInfo);

							// 如果友好名称包含端口名，则返回它，否则返回"设备名 (端口名)"
							if (strstr(friendlyName, "COM") == nullptr)
							{
								return std::string(friendlyName) + " (" + portName + ")";
							}
							return std::string(friendlyName);
						}
					}
				}

				RegCloseKey(hKey);
			}
		}

		SetupDiDestroyDeviceInfoList(hDevInfo);
	}

	// 如果找不到友好名称，返回默认格式
	return "串口设备 (" + portName + ")";
}

// 检测串口状态
PortStatus SerialTransport::CheckSerialPortStatus(const std::string& portName)
{
	std::string portPath = "\\\\.\\" + portName;

	// 尝试打开句柄检测端口是否可用
	HANDLE hSerial = CreateFileA(
		portPath.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (hSerial == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_ACCESS_DENIED)
		{
			return PortStatus::Offline;
		}
		return PortStatus::Error;
	}

	// 尝试获取端口状态
	DCB dcb = {0};
	dcb.DCBlength = sizeof(DCB);

	if (!::GetCommState(hSerial, &dcb))
	{
		CloseHandle(hSerial);
		return PortStatus::Error;
	}

	// 设置超时（快速测试）
	COMMTIMEOUTS timeouts = {0};
	timeouts.ReadIntervalTimeout = 1000;
	timeouts.ReadTotalTimeoutConstant = 1000;
	::SetCommTimeouts(hSerial, &timeouts);

	CloseHandle(hSerial);
	return PortStatus::Available;
}

bool SerialTransport::IsSerialPortAvailable(const std::string& portName)
{
	std::string portPath = "\\\\.\\" + portName;
	HANDLE hSerial = CreateFileA(
		portPath.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (hSerial != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hSerial);
		return true;
	}

	return false;
}