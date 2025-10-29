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

// USB打印端口专用配置
struct UsbPrintConfig : public TransportConfig
{
	std::string deviceName = "USB001";      // 设备名称 (USB001, USB002, etc.)
	std::string deviceId;                   // 设备ID
	std::string printerName;                // 打印机名称
	DWORD accessMode = GENERIC_WRITE;       // 访问模式 (默认只写)
	DWORD shareMode = 0;                    // 共享模式
	DWORD creationDisposition = OPEN_EXISTING; // 创建方式
	DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL; // 文件属性
	bool checkStatus = true;                // 检查设备状态
	DWORD statusCheckInterval = 100;        // 状态检查间隔(ms)

	UsbPrintConfig()
	{
		portName = deviceName;
		readTimeout = 1000;                 // USB读取超时较短
		writeTimeout = 2000;                // USB写入超时
		bufferSize = 2048;                  // USB缓冲区中等
		asyncMode = false;                  // 默认同步模式
	}
};

// USB设备状态枚举
enum class UsbDeviceStatus
{
	Unknown = 0,
	Ready = 1,
	Busy = 2,
	Offline = 4,
	OutOfPaper = 8,
	Error = 16,
	NotConnected = 32
};

// USB打印传输实现类
class UsbPrintTransport : public ITransport
{
public:
	UsbPrintTransport();
	virtual ~UsbPrintTransport();

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

	// USB专用方法
	UsbDeviceStatus GetDeviceStatus() const;
	std::string GetDeviceInfo() const;
	bool IsDeviceConnected() const;

	// 设备控制方法
	TransportError ResetDevice();
	std::string GetDeviceDescriptor() const;

	// 静态辅助方法
	static std::vector<std::string> EnumerateUsbPorts();
	static std::vector<PortInfo> EnumerateUsbPortsWithInfo();
	static bool IsUsbPortAvailable(const std::string& portName);
	static std::string GetDeviceStatusString(UsbDeviceStatus status);

private:
	// 内部状态
	mutable std::mutex m_mutex;
	std::atomic<TransportState> m_state;
	HANDLE m_hDevice;
	UsbPrintConfig m_config;
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
	UsbDeviceStatus m_lastStatus;

	// 内部方法
	void SetState(TransportState newState);
	void NotifyError(TransportError error, const std::string& message);
	void UpdateStats(uint64_t bytesSent, uint64_t bytesReceived);

	// 设备操作
	TransportError OpenDeviceHandle();
	void CloseDeviceHandle();
	TransportError WriteToDevice(const void* data, size_t size, size_t* written);
	TransportError ReadFromDevice(void* buffer, size_t size, size_t* read, DWORD timeout);

	// 状态检查
	UsbDeviceStatus QueryDeviceStatus() const;
	void StatusMonitorThread();

	// 异步操作线程
	void AsyncReadThread();
	void AsyncWriteThread();

	// 错误处理
	TransportError GetLastError();
	std::string GetSystemErrorMessage(DWORD errorCode) const;

	// 配置验证
	bool ValidateConfig(const UsbPrintConfig& config) const;
	std::string NormalizePortName(const std::string& portName) const;

	// 设备控制
	TransportError SetDeviceTimeouts();

	// 禁用复制构造和赋值
	UsbPrintTransport(const UsbPrintTransport&) = delete;
	UsbPrintTransport& operator=(const UsbPrintTransport&) = delete;
};