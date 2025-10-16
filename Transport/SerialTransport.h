#pragma once
#pragma execution_character_set("utf-8")

#include "ITransport.h"
#include <thread>
#include <atomic>
#include <mutex>

// 串口配置
struct SerialConfig : public TransportConfig
{
	DWORD baudRate = 9600;           // 波特率
	BYTE dataBits = 8;               // 数据位
	BYTE parity = NOPARITY;          // 校验位
	BYTE stopBits = ONESTOPBIT;      // 停止位
	DWORD flowControl = 0;           // 流控制
	bool rts = false;                // RTS信号
	bool dtr = false;                // DTR信号
	bool xonXoff = false;            // 软件流控
	bool dsrSensitivity = false;    // DSR敏感
	bool continuousRead = true;      // 连续读取
};

// 串口传输实现
class SerialTransport : public ITransport
{
public:
	SerialTransport();
	virtual ~SerialTransport();

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

	// 串口特有方法
	bool SetCommState(const SerialConfig& config);
	bool GetCommState(DCB& dcb);
	bool SetCommTimeouts(DWORD readTimeout, DWORD writeTimeout);
	bool SendBreak();
	bool SetRTS(bool state);
	bool SetDTR(bool state);
	bool GetCTS() const;
	bool GetDSR() const;
	bool GetRING() const;
	bool GetRLSD() const;

	// 静态辅助方法
	static std::vector<std::string> EnumerateSerialPorts();
	static bool IsSerialPortAvailable(const std::string& portName);

private:
	// 内部方法
	void AsyncReadThread();
	void UpdateState(TransportState newState);
	void ReportError(TransportError error, const std::string& message);
	void UpdateStats(size_t bytesSent, size_t bytesReceived);

private:
	HANDLE m_hSerial;                        // 串口句柄
	SerialConfig m_config;                   // 串口配置
	std::atomic<TransportState> m_state;     // 当前状态
	TransportStats m_stats;                  // 统计信息
	mutable std::mutex m_statsMutex;         // 统计锁

	// 异步读取相关
	std::thread m_readThread;                // 读取线程
	std::atomic<bool> m_stopReading;         // 停止标志
	OVERLAPPED m_readOverlapped;             // 读取重叠结构
	OVERLAPPED m_writeOverlapped;            // 写入重叠结构
	std::vector<uint8_t> m_readBuffer;       // 读取缓冲区

	// 回调函数
	DataReceivedCallback m_dataReceivedCallback;
	StateChangedCallback m_stateChangedCallback;
	ErrorOccurredCallback m_errorOccurredCallback;

	// 同步对象
	mutable std::mutex m_mutex;              // 通用互斥锁
	HANDLE m_readEvent;                      // 读取事件
	HANDLE m_writeEvent;                     // 写入事件
};