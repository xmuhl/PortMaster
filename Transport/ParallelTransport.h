#pragma once
#pragma execution_character_set("utf-8")

#include "ITransport.h"
#include <Windows.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>

// 并口专用配置
struct ParallelPortConfig : public TransportConfig
{
	std::string deviceName = "LPT1";     // 设备名称 (LPT1, LPT2, etc.)
	DWORD accessMode = GENERIC_WRITE;    // 访问模式 (默认只写)
	DWORD shareMode = 0;                 // 共享模式
	DWORD creationDisposition = OPEN_EXISTING; // 创建方式
	DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL; // 文件属性
	bool enableBidirectional = false;   // 启用双向通信
	bool checkStatus = true;             // 检查设备状态
	DWORD statusCheckInterval = 100;     // 状态检查间隔(ms)

	ParallelPortConfig()
	{
		portName = deviceName;
		readTimeout = 1000;              // 并口读取超时较短
		writeTimeout = 2000;             // 并口写入超时
		bufferSize = 1024;               // 并口缓冲区较小
		asyncMode = false;               // 默认同步模式
	}
};

// 并口状态枚举
enum class ParallelPortStatus
{
	Unknown = 0,
	Ready = 1,
	Busy = 2,
	OutOfPaper = 4,
	Offline = 8,
	IOError = 16,
	Selected = 32,
	TimeOut = 64,
	NotError = 128
};

// 并口传输实现类
class ParallelTransport : public ITransport
{
public:
	ParallelTransport();
	virtual ~ParallelTransport();

	// ITransport接口实现
	virtual TransportError Open(const TransportConfig& config) override;
	virtual TransportError Close() override;
	virtual TransportError Write(const void* data, size_t size, size_t* written = nullptr) override;
	virtual TransportError Read(void* buffer, size_t size, size_t* read, DWORD timeout = INFINITE) override;
	virtual TransportError WriteAsync(const void* data, size_t size) override;
	virtual TransportError StartAsyncRead() override;
	virtual TransportError StopAsyncRead() override;
	virtual TransportState GetState() const override;
	virtual bool IsOpen() const override;
	virtual TransportStats GetStats() const override;
	virtual void ResetStats() override;
	virtual std::string GetPortName() const override;
	virtual void SetDataReceivedCallback(DataReceivedCallback callback) override;
	virtual void SetStateChangedCallback(StateChangedCallback callback) override;
	virtual void SetErrorOccurredCallback(ErrorOccurredCallback callback) override;
	virtual TransportError FlushBuffers() override;
	virtual size_t GetAvailableBytes() const override;

	// 并口专用方法
	ParallelPortStatus GetPortStatus() const;
	bool IsPortBusy() const;
	bool IsPortReady() const;
	bool IsPortOnline() const;
	TransportError ResetPort();
	TransportError ConfigurePort(const ParallelPortConfig& config);

	// 静态辅助方法
	static std::vector<std::string> EnumerateParallelPorts();
	static bool IsParallelPortAvailable(const std::string& portName);
	static std::string GetPortStatusString(ParallelPortStatus status);

private:
	// 内部状态
	mutable std::mutex m_mutex;
	std::atomic<TransportState> m_state;
	HANDLE m_hPort;
	ParallelPortConfig m_config;
	TransportStats m_stats;

	// 回调函数
	DataReceivedCallback m_dataReceivedCallback;
	StateChangedCallback m_stateChangedCallback;
	ErrorOccurredCallback m_errorOccurredCallback;

	// 异步操作支持
	std::atomic<bool> m_asyncReadRunning;
	std::thread m_asyncReadThread;
	std::queue<std::vector<uint8_t>> m_writeQueue;
	std::mutex m_writeQueueMutex;
	std::thread m_asyncWriteThread;
	std::atomic<bool> m_asyncWriteRunning;

	// 状态监控
	std::thread m_statusThread;
	std::atomic<bool> m_statusThreadRunning;
	ParallelPortStatus m_lastStatus;

	// 内部方法
	void SetState(TransportState newState);
	void NotifyError(TransportError error, const std::string& message);
	void UpdateStats(uint64_t bytesSent, uint64_t bytesReceived);

	// 端口操作
	TransportError OpenPortHandle();
	void ClosePortHandle();
	TransportError WriteToPort(const void* data, size_t size, size_t* written);
	TransportError ReadFromPort(void* buffer, size_t size, size_t* read, DWORD timeout);

	// 状态检查
	ParallelPortStatus QueryPortStatus() const;
	void StatusMonitorThread();

	// 异步操作线程
	void AsyncReadThread();
	void AsyncWriteThread();

	// 错误处理
	TransportError GetLastError();
	std::string GetSystemErrorMessage(DWORD errorCode) const;

	// 配置验证
	bool ValidateConfig(const ParallelPortConfig& config) const;
	std::string NormalizePortName(const std::string& portName) const;

	// 设备控制
	TransportError SetPortTimeouts();
	TransportError QueryPortInfo();

	// 禁用复制构造和赋值
	ParallelTransport(const ParallelTransport&) = delete;
	ParallelTransport& operator=(const ParallelTransport&) = delete;
};

// 并口专用错误码
enum class ParallelPortError
{
	Success = 0,
	PortNotFound,
	PortBusy,
	PortOffline,
	OutOfPaper,
	IOError,
	PermissionDenied,
	DeviceNotReady,
	DriverNotInstalled,
	ConfigurationError
};

// 并口错误码转换
class ParallelErrorConverter
{
public:
	static TransportError ConvertToTransportError(ParallelPortError error);
	static std::string GetParallelErrorString(ParallelPortError error);
};