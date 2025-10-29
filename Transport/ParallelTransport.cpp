#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ParallelTransport.h"
#include <algorithm>
#include <sstream>
#include <chrono>

// 【修复宏重复定义风险】将系统宏定义移到文件头部并添加条件编译保护
#ifndef FILE_DEVICE_PARALLEL_PORT
#define FILE_DEVICE_PARALLEL_PORT   0x00000016
#endif

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

// 构造函数
ParallelTransport::ParallelTransport()
	: m_state(TransportState::Closed)
	, m_hPort(INVALID_HANDLE_VALUE)
	, m_asyncReadRunning(false)
	, m_asyncWriteRunning(false)
	, m_statusThreadRunning(false)
	, m_lastStatus(ParallelPortStatus::Unknown)
{
	// 初始化统计信息
	memset(&m_stats, 0, sizeof(m_stats));
}

// 析构函数
ParallelTransport::~ParallelTransport()
{
	Close();
}

// 打开传输通道
TransportError ParallelTransport::Open(const TransportConfig& baseConfig)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state != TransportState::Closed)
	{
		return TransportError::AlreadyOpen;
	}

	// 转换配置类型
	const ParallelPortConfig* config = dynamic_cast<const ParallelPortConfig*>(&baseConfig);
	if (!config)
	{
		// 如果不是并口配置，创建默认配置
		ParallelPortConfig defaultConfig;
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

	// 验证配置
	if (!ValidateConfig(m_config))
	{
		return TransportError::InvalidConfig;
	}

	// 标准化端口名
	m_config.deviceName = NormalizePortName(m_config.portName);
	m_config.portName = m_config.deviceName;

	SetState(TransportState::Opening);

	// 打开端口句柄
	TransportError result = OpenPortHandle();
	if (result != TransportError::Success)
	{
		SetState(TransportState::Error);
		return result;
	}

	// 设置端口超时
	result = SetPortTimeouts();
	if (result != TransportError::Success)
	{
		ClosePortHandle();
		SetState(TransportState::Error);
		return result;
	}

	// 查询端口信息
	result = QueryPortInfo();
	if (result != TransportError::Success)
	{
		ClosePortHandle();
		SetState(TransportState::Error);
		return result;
	}

	// 启动状态监控线程
	if (m_config.checkStatus)
	{
		m_statusThreadRunning = true;
		m_statusThread = std::thread(&ParallelTransport::StatusMonitorThread, this);
	}

	// 启动异步读取
	if (m_config.asyncMode)
	{
		result = StartAsyncRead();
		if (result != TransportError::Success)
		{
			ClosePortHandle();
			SetState(TransportState::Error);
			return result;
		}
	}

	SetState(TransportState::Open);
	return TransportError::Success;
}

// 关闭传输通道
TransportError ParallelTransport::Close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state == TransportState::Closed)
	{
		return TransportError::Success;
	}

	SetState(TransportState::Closing);

	// 停止异步操作
	StopAsyncRead();

	// 停止状态监控线程
	if (m_statusThreadRunning)
	{
		m_statusThreadRunning = false;
		if (m_statusThread.joinable())
		{
			m_statusThread.join();
		}
	}

	// 停止异步写入
	if (m_asyncWriteRunning)
	{
		m_asyncWriteRunning = false;
		if (m_asyncWriteThread.joinable())
		{
			m_asyncWriteThread.join();
		}
	}

	// 关闭端口句柄
	ClosePortHandle();

	SetState(TransportState::Closed);
	return TransportError::Success;
}

// 同步写入数据
TransportError ParallelTransport::Write(const void* data, size_t size, size_t* written)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!data || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	// 检查端口状态
	if (!IsPortReady())
	{
		return TransportError::Busy;
	}

	return WriteToPort(data, size, written);
}

// 同步读取数据
TransportError ParallelTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!buffer || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	// 并口通常不支持读取，除非启用了双向模式
	if (!m_config.enableBidirectional)
	{
		if (read) *read = 0;
		return TransportError::ReadFailed;
	}

	return ReadFromPort(buffer, size, read, timeout);
}

// 异步写入数据
TransportError ParallelTransport::WriteAsync(const void* data, size_t size)
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!data || size == 0)
	{
		return TransportError::InvalidParameter;
	}

	// 将数据加入写入队列
	std::vector<uint8_t> buffer(static_cast<const uint8_t*>(data),
		static_cast<const uint8_t*>(data) + size);

	{
		std::lock_guard<std::mutex> lock(m_writeQueueMutex);
		m_writeQueue.push(std::move(buffer));
	}

	// 启动异步写入线程
	if (!m_asyncWriteRunning)
	{
		m_asyncWriteRunning = true;
		m_asyncWriteThread = std::thread(&ParallelTransport::AsyncWriteThread, this);
	}

	return TransportError::Success;
}

// 异步读取启动
TransportError ParallelTransport::StartAsyncRead()
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!m_config.enableBidirectional)
	{
		return TransportError::ReadFailed;
	}

	if (m_asyncReadRunning)
	{
		return TransportError::Success;
	}

	m_asyncReadRunning = true;
	m_asyncReadThread = std::thread(&ParallelTransport::AsyncReadThread, this);

	return TransportError::Success;
}

// 停止异步读取
TransportError ParallelTransport::StopAsyncRead()
{
	if (!m_asyncReadRunning)
	{
		return TransportError::Success;
	}

	m_asyncReadRunning = false;

	if (m_asyncReadThread.joinable())
	{
		m_asyncReadThread.join();
	}

	return TransportError::Success;
}

// 查询传输状态
TransportState ParallelTransport::GetState() const
{
	return m_state;
}

// 检查是否已打开
bool ParallelTransport::IsOpen() const
{
	return m_state == TransportState::Open && m_hPort != INVALID_HANDLE_VALUE;
}

// 获取统计信息
TransportStats ParallelTransport::GetStats() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_stats;
}

// 重置统计信息
void ParallelTransport::ResetStats()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	memset(&m_stats, 0, sizeof(m_stats));
}

// 获取端口名称
std::string ParallelTransport::GetPortName() const
{
	return m_config.portName;
}

// 设置数据接收回调
void ParallelTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
	m_dataReceivedCallback = callback;
}

// 设置状态变化回调
void ParallelTransport::SetStateChangedCallback(StateChangedCallback callback)
{
	m_stateChangedCallback = callback;
}

// 设置错误发生回调
void ParallelTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback)
{
	m_errorOccurredCallback = callback;
}

// 清空缓冲区
TransportError ParallelTransport::FlushBuffers()
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	if (!FlushFileBuffers(m_hPort))
	{
		return this->GetLastError();
	}

	return TransportError::Success;
}

// 获取可用字节数
size_t ParallelTransport::GetAvailableBytes() const
{
	// 并口通常不支持查询可用字节数
	return 0;
}

// 获取端口状态
ParallelPortStatus ParallelTransport::GetPortStatus() const
{
	return QueryPortStatus();
}

// 检查端口是否忙碌
bool ParallelTransport::IsPortBusy() const
{
	ParallelPortStatus status = GetPortStatus();
	return (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Busy)) != 0;
}

// 检查端口是否就绪
bool ParallelTransport::IsPortReady() const
{
	ParallelPortStatus status = GetPortStatus();
	return (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Ready)) != 0;
}

// 检查端口是否在线
bool ParallelTransport::IsPortOnline() const
{
	ParallelPortStatus status = GetPortStatus();
	return (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Offline)) == 0;
}

// 重置端口
TransportError ParallelTransport::ResetPort()
{
	if (!IsOpen())
	{
		return TransportError::NotOpen;
	}

	// 并口重置通常通过重新打开端口实现
	TransportError result = Close();
	if (result == TransportError::Success)
	{
		result = Open(m_config);
	}

	return result;
}

// 配置端口
TransportError ParallelTransport::ConfigurePort(const ParallelPortConfig& config)
{
	if (IsOpen())
	{
		return TransportError::AlreadyOpen;
	}

	if (!ValidateConfig(config))
	{
		return TransportError::InvalidConfig;
	}

	m_config = config;
	return TransportError::Success;
}

// 枚举并口端口
std::vector<std::string> ParallelTransport::EnumerateParallelPorts()
{
	std::vector<std::string> ports;

	// 常见的并口名称
	std::vector<std::string> commonPorts = { "LPT1", "LPT2", "LPT3", "LPT4" };

	for (const auto& port : commonPorts)
	{
		if (IsParallelPortAvailable(port))
		{
			ports.push_back(port);
		}
	}

	return ports;
}

// 增强版并口枚举（获取设备描述）
std::vector<PortInfo> ParallelTransport::EnumerateParallelPortsWithInfo()
{
	std::vector<PortInfo> portInfos;

	// 检查常见的并口名称
	std::vector<std::string> commonPorts = { "LPT1", "LPT2", "LPT3", "LPT4" };

	for (const auto& port : commonPorts)
	{
		PortInfo info;
		info.portType = PortType::PORT_TYPE_PARALLEL;
		info.portName = port;

		// 获取设备信息
		info.displayName = GetParallelDeviceInfo(port);
		info.description = "并口设备：" + info.displayName;

		// 检测端口状态
		info.status = CheckParallelPortStatus(port);

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

	return portInfos;
}

// 获取并口设备信息
std::string ParallelTransport::GetParallelDeviceInfo(const std::string& portName)
{
	// 并口设备信息获取比较简单，主要是查询系统信息
	// 这里可以扩展为使用WMI查询详细设备信息

	// 检查端口是否可用，如果可用则返回设备信息
	if (IsParallelPortAvailable(portName))
	{
		// 这里可以进一步查询设备描述
		return "并口设备 (" + portName + ")";
	}

	return "未知设备 (" + portName + ")";
}

// 检测并口状态
PortStatus ParallelTransport::CheckParallelPortStatus(const std::string& portName)
{
	std::string devicePath = "\\\\.\\" + portName;

	// 尝试打开句柄检测端口是否可用
	HANDLE hPort = CreateFileA(
		devicePath.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hPort != INVALID_HANDLE_VALUE)
	{
		// 端口可以打开，标记为可用
		CloseHandle(hPort);
		return PortStatus::Available;
	}

	// 无法打开端口
	DWORD error = ::GetLastError();
	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_ACCESS_DENIED)
	{
		return PortStatus::Offline;
	}
	return PortStatus::Error;
}

// 检查并口是否可用
bool ParallelTransport::IsParallelPortAvailable(const std::string& portName)
{
	std::string devicePath = "\\\\.\\" + portName;

	HANDLE hPort = CreateFileA(
		devicePath.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hPort);
		return true;
	}

	return false;
}

// 获取端口状态字符串
std::string ParallelTransport::GetPortStatusString(ParallelPortStatus status)
{
	std::vector<std::string> statusStrings;

	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Ready))
		statusStrings.push_back("就绪");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Busy))
		statusStrings.push_back("忙碌");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::OutOfPaper))
		statusStrings.push_back("缺纸");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Offline))
		statusStrings.push_back("离线");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::IOError))
		statusStrings.push_back("IO错误");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Selected))
		statusStrings.push_back("已选择");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::TimeOut))
		statusStrings.push_back("超时");
	if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::NotError))
		statusStrings.push_back("无错误");

	if (statusStrings.empty())
	{
		return "未知";
	}

	std::string result;
	for (size_t i = 0; i < statusStrings.size(); ++i)
	{
		if (i > 0) result += ", ";
		result += statusStrings[i];
	}

	return result;
}

// 设置状态
void ParallelTransport::SetState(TransportState newState)
{
	TransportState oldState = m_state;
	m_state = newState;

	if (oldState != newState && m_stateChangedCallback)
	{
		m_stateChangedCallback(newState);
	}
}

// 通知错误
void ParallelTransport::NotifyError(TransportError error, const std::string& message)
{
	if (m_errorOccurredCallback)
	{
		m_errorOccurredCallback(error, message);
	}
}

// 更新统计信息
void ParallelTransport::UpdateStats(uint64_t bytesSent, uint64_t bytesReceived)
{
	m_stats.bytesSent += bytesSent;
	m_stats.bytesReceived += bytesReceived;
	m_stats.packetsTotal++;

	// 计算吞吐率
	static auto lastTime = std::chrono::steady_clock::now();
	auto currentTime = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);

	if (elapsed.count() > 1000) // 每秒更新一次
	{
		m_stats.throughputBps = (bytesSent + bytesReceived) * 1000.0 / elapsed.count();
		lastTime = currentTime;
	}
}

// 打开端口句柄
TransportError ParallelTransport::OpenPortHandle()
{
	std::string devicePath = "\\\\.\\" + m_config.deviceName;

	// 【调试信息】记录尝试打开的设备路径
	std::string msg1 = "【并口】尝试打开设备路径: " + devicePath + "\n";
	std::string msg2 = "【并口】设备名称: " + m_config.deviceName + "\n";
	std::string msg3 = "【并口】端口名称: " + m_config.portName + "\n";

	OutputDebugStringA(msg1.c_str());
	OutputDebugStringA(msg2.c_str());
	OutputDebugStringA(msg3.c_str());

	m_hPort = CreateFileA(
		devicePath.c_str(),
		m_config.accessMode,
		m_config.shareMode,
		nullptr,
		m_config.creationDisposition,
		m_config.flagsAndAttributes,
		nullptr
	);

	if (m_hPort == INVALID_HANDLE_VALUE)
	{
		DWORD error = ::GetLastError();
		std::string errorMsg = GetSystemErrorMessage(error);

		// 【调试信息】记录打开失败
		std::string msg1 = "【并口】打开设备失败！错误码: " + std::to_string(error) + "\n";
		std::string msg2 = "【并口】错误信息: " + errorMsg + "\n";
		std::string msg3 = "【并口】设备路径: " + devicePath + "\n";
		std::string msg4 = "【并口】访问模式: 0x" + std::to_string(m_config.accessMode) + ", 共享模式: 0x" + std::to_string(m_config.shareMode) + "\n";

		OutputDebugStringA(msg1.c_str());
		OutputDebugStringA(msg2.c_str());
		OutputDebugStringA(msg3.c_str());
		OutputDebugStringA(msg4.c_str());

		// 【错误诊断】根据错误码提供具体诊断信息
		if (error == ERROR_FILE_NOT_FOUND)
		{
			std::string msg = "【并口】诊断: 设备不存在，请检查：1)端口名称是否正确 2)并口设备是否正确连接 3)驱动程序是否安装\n";
			OutputDebugStringA(msg.c_str());
			return TransportError::OpenFailed;
		}
		else if (error == ERROR_ACCESS_DENIED)
		{
			std::string msg = "【并口】诊断: 访问被拒绝，可能原因：1)设备正被其他程序使用 2)权限不足 3)设备已被锁定\n";
			OutputDebugStringA(msg.c_str());
			return TransportError::Busy;
		}
		else if (error == ERROR_SHARING_VIOLATION)
		{
			std::string msg = "【并口】诊断: 共享冲突，设备正被其他进程使用\n";
			OutputDebugStringA(msg.c_str());
			return TransportError::Busy;
		}
		else
		{
			std::string msg = "【并口】诊断: 其他错误，请检查设备连接和配置\n";
			OutputDebugStringA(msg.c_str());
			return this->GetLastError();
		}
	}

	// 【调试信息】记录打开成功
	std::string msg = "【并口】设备打开成功！句柄值: 0x" + std::to_string(reinterpret_cast<uintptr_t>(m_hPort)) + "\n";
	OutputDebugStringA(msg.c_str());
	return TransportError::Success;
}

// 关闭端口句柄
void ParallelTransport::ClosePortHandle()
{
	if (m_hPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hPort);
		m_hPort = INVALID_HANDLE_VALUE;
	}
}

// 向端口写入数据
TransportError ParallelTransport::WriteToPort(const void* data, size_t size, size_t* written)
{
	DWORD bytesWritten = 0;
	BOOL success = WriteFile(m_hPort, data, static_cast<DWORD>(size), &bytesWritten, nullptr);

	if (written)
	{
		*written = bytesWritten;
	}

	if (!success)
	{
		return this->GetLastError();
	}

	UpdateStats(bytesWritten, 0);
	return TransportError::Success;
}

// 从端口读取数据
TransportError ParallelTransport::ReadFromPort(void* buffer, size_t size, size_t* read, DWORD timeout)
{
	if (!m_config.enableBidirectional)
	{
		return TransportError::ReadFailed;
	}

	DWORD bytesRead = 0;
	BOOL success = ReadFile(m_hPort, buffer, static_cast<DWORD>(size), &bytesRead, nullptr);

	if (read)
	{
		*read = bytesRead;
	}

	if (!success)
	{
		return this->GetLastError();
	}

	UpdateStats(0, bytesRead);
	return TransportError::Success;
}

// 查询端口状态
ParallelPortStatus ParallelTransport::QueryPortStatus() const
{
	if (!IsOpen())
	{
		return ParallelPortStatus::Unknown;
	}

	// 【关键修复】使用DeviceIoControl查询真实硬件状态
	DWORD bytesReturned = 0;
	UCHAR statusByte = 0;

	// 尝试使用并口特定的状态查询IOCTL
	// 不同的并口驱动可能支持不同的IOCTL代码
	const DWORD IOCTL_PAR_QUERY_INFORMATION = CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 1, METHOD_BUFFERED, FILE_ANY_ACCESS);

	BOOL success = DeviceIoControl(
		m_hPort,                    // 设备句柄
		IOCTL_PAR_QUERY_INFORMATION, // 控制代码
		nullptr,                    // 输入缓冲区
		0,                          // 输入缓冲区大小
		&statusByte,                // 输出缓冲区
		sizeof(statusByte),         // 输出缓冲区大小
		&bytesReturned,             // 返回字节数
		nullptr                     // 重叠结构
	);

	if (success && bytesReturned >= sizeof(statusByte))
	{
		// 解析并口状态字节（标准LPT状态位定义）
		ParallelPortStatus status = ParallelPortStatus::Unknown;

		// 状态位解析（基于标准并口状态寄存器）
		if (statusByte & 0x08) status = static_cast<ParallelPortStatus>(static_cast<int>(status) | static_cast<int>(ParallelPortStatus::NotError));  // !Error
		if (statusByte & 0x10) status = static_cast<ParallelPortStatus>(static_cast<int>(status) | static_cast<int>(ParallelPortStatus::Selected)); // Select
		if (statusByte & 0x20) status = static_cast<ParallelPortStatus>(static_cast<int>(status) | static_cast<int>(ParallelPortStatus::OutOfPaper)); // PError (纸尽)
		if (statusByte & 0x40) status = static_cast<ParallelPortStatus>(static_cast<int>(status) | static_cast<int>(ParallelPortStatus::Ready));     // Ack (就绪)
		if (statusByte & 0x80) status = static_cast<ParallelPortStatus>(static_cast<int>(status) | static_cast<int>(ParallelPortStatus::Busy));      // Busy

		// 推导设备整体状态
		if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Busy))
		{
			return ParallelPortStatus::Busy;
		}
		else if (static_cast<int>(status) & static_cast<int>(ParallelPortStatus::OutOfPaper))
		{
			return ParallelPortStatus::OutOfPaper;
		}
		else if ((static_cast<int>(status) & static_cast<int>(ParallelPortStatus::Selected)) &&
			(static_cast<int>(status) & static_cast<int>(ParallelPortStatus::NotError)))
		{
			return ParallelPortStatus::Ready;
		}
		else
		{
			return ParallelPortStatus::Offline;
		}
	}
	else
	{
		// DeviceIoControl失败，尝试通过写入测试检测状态
		DWORD error = ::GetLastError();

		// 某些并口驱动不支持状态查询IOCTL，使用写入测试
		if (error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED)
		{
			// 尝试零字节写入测试端口可用性
			DWORD bytesWritten = 0;
			OVERLAPPED overlapped = { 0 };
			overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

			if (overlapped.hEvent)
			{
				BOOL writeResult = WriteFile(m_hPort, "", 0, &bytesWritten, &overlapped);
				if (!writeResult)
				{
					DWORD writeError = ::GetLastError();
					if (writeError == ERROR_IO_PENDING)
					{
						// 等待操作完成（短超时）
						DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
						if (waitResult == WAIT_OBJECT_0)
						{
							GetOverlappedResult(m_hPort, &overlapped, &bytesWritten, FALSE);
							CloseHandle(overlapped.hEvent);
							return ParallelPortStatus::Ready;
						}
						else
						{
							CancelIo(m_hPort);
							CloseHandle(overlapped.hEvent);
							return ParallelPortStatus::Busy;
						}
					}
					else if (writeError == ERROR_GEN_FAILURE)
					{
						CloseHandle(overlapped.hEvent);
						return ParallelPortStatus::Offline;
					}
				}
				else
				{
					CloseHandle(overlapped.hEvent);
					return ParallelPortStatus::Ready;
				}

				CloseHandle(overlapped.hEvent);
			}
		}

		// 所有方法都失败，返回IOError
		return ParallelPortStatus::IOError;
	}
}

// 状态监控线程
void ParallelTransport::StatusMonitorThread()
{
	while (m_statusThreadRunning)
	{
		ParallelPortStatus currentStatus = QueryPortStatus();

		if (currentStatus != m_lastStatus)
		{
			m_lastStatus = currentStatus;
			// 可以在这里触发状态变化回调
		}

		Sleep(m_config.statusCheckInterval);
	}
}

// 异步读取线程
void ParallelTransport::AsyncReadThread()
{
	const size_t bufferSize = 1024;
	std::vector<uint8_t> buffer(bufferSize);

	while (m_asyncReadRunning && IsOpen())
	{
		size_t bytesRead = 0;
		TransportError result = ReadFromPort(buffer.data(), bufferSize, &bytesRead, m_config.readTimeout);

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

// 异步写入线程
void ParallelTransport::AsyncWriteThread()
{
	while (m_asyncWriteRunning)
	{
		std::vector<uint8_t> data;

		// 从队列中获取数据
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

		// 写入数据
		size_t written = 0;
		TransportError result = WriteToPort(data.data(), data.size(), &written);

		if (result != TransportError::Success)
		{
			NotifyError(result, "异步写入失败");
		}
	}
}

// 获取最后错误
TransportError ParallelTransport::GetLastError()
{
	DWORD error = ::GetLastError();
	m_stats.lastErrorCode = error;

	switch (error)
	{
	case ERROR_SUCCESS:
		return TransportError::Success;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return TransportError::OpenFailed;
	case ERROR_ACCESS_DENIED:
		return TransportError::Busy;
	case ERROR_INVALID_HANDLE:
		return TransportError::NotOpen;
	case ERROR_TIMEOUT:
	case WAIT_TIMEOUT:
		return TransportError::Timeout;
	case ERROR_INVALID_PARAMETER:
		return TransportError::InvalidParameter;
	default:
		return TransportError::WriteFailed;
	}
}

// 获取系统错误消息
std::string ParallelTransport::GetSystemErrorMessage(DWORD errorCode) const
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

// 配置验证
bool ParallelTransport::ValidateConfig(const ParallelPortConfig& config) const
{
	if (config.deviceName.empty() || config.portName.empty())
	{
		return false;
	}

	if (config.readTimeout == 0 || config.writeTimeout == 0)
	{
		return false;
	}

	if (config.bufferSize == 0)
	{
		return false;
	}

	return true;
}

// 标准化端口名
std::string ParallelTransport::NormalizePortName(const std::string& portName) const
{
	std::string normalized = portName;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);

	// 确保是标准的LPT格式
	if (normalized.find("LPT") != 0)
	{
		if (normalized == "1") normalized = "LPT1";
		else if (normalized == "2") normalized = "LPT2";
		else if (normalized == "3") normalized = "LPT3";
		else if (normalized == "4") normalized = "LPT4";
		else normalized = "LPT1"; // 默认值
	}

	return normalized;
}

// 设置端口超时
TransportError ParallelTransport::SetPortTimeouts()
{
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = m_config.readTimeout;
	timeouts.ReadTotalTimeoutConstant = m_config.readTimeout;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = m_config.writeTimeout;
	timeouts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(m_hPort, &timeouts))
	{
		return this->GetLastError();
	}

	return TransportError::Success;
}

// 查询端口信息
TransportError ParallelTransport::QueryPortInfo()
{
	// 查询设备信息，验证是否为有效的并口设备
	// 这里可以添加更多的验证逻辑
	return TransportError::Success;
}

// 并口错误码转换
TransportError ParallelErrorConverter::ConvertToTransportError(ParallelPortError error)
{
	switch (error)
	{
	case ParallelPortError::Success:
		return TransportError::Success;
	case ParallelPortError::PortNotFound:
		return TransportError::OpenFailed;
	case ParallelPortError::PortBusy:
		return TransportError::Busy;
	case ParallelPortError::PortOffline:
		return TransportError::ConnectionClosed;
	case ParallelPortError::OutOfPaper:
		return TransportError::WriteFailed;
	case ParallelPortError::IOError:
		return TransportError::WriteFailed;
	case ParallelPortError::PermissionDenied:
		return TransportError::OpenFailed;
	case ParallelPortError::DeviceNotReady:
		return TransportError::NotOpen;
	case ParallelPortError::DriverNotInstalled:
		return TransportError::OpenFailed;
	case ParallelPortError::ConfigurationError:
		return TransportError::InvalidConfig;
	default:
		return TransportError::WriteFailed;
	}
}

// 获取并口错误字符串
std::string ParallelErrorConverter::GetParallelErrorString(ParallelPortError error)
{
	switch (error)
	{
	case ParallelPortError::Success:
		return "成功";
	case ParallelPortError::PortNotFound:
		return "端口未找到";
	case ParallelPortError::PortBusy:
		return "端口忙碌";
	case ParallelPortError::PortOffline:
		return "端口离线";
	case ParallelPortError::OutOfPaper:
		return "缺纸";
	case ParallelPortError::IOError:
		return "IO错误";
	case ParallelPortError::PermissionDenied:
		return "权限不足";
	case ParallelPortError::DeviceNotReady:
		return "设备未就绪";
	case ParallelPortError::DriverNotInstalled:
		return "驱动未安装";
	case ParallelPortError::ConfigurationError:
		return "配置错误";
	default:
		return "未知错误";
	}
}