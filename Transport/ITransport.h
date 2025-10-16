#pragma once
#pragma execution_character_set("utf-8")

#include <Windows.h>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include "../Common/CommonTypes.h"

// 传输层状态枚举
enum class TransportState
{
	Closed,    // 已关闭
	Opening,   // 正在打开
	Open,      // 已打开
	Closing,   // 正在关闭
	Error      // 错误状态
};

// 传输层错误码
enum class TransportError
{
	Success = 0,           // 成功
	OpenFailed,            // 打开失败
	CloseFailed,           // 关闭失败
	ReadFailed,            // 读取失败
	WriteFailed,           // 写入失败
	Timeout,               // 超时
	Busy,                  // 设备忙
	NotOpen,               // 未打开
	InvalidParameter,      // 无效参数
	InvalidConfig,         // 无效配置
	AlreadyOpen,           // 已经打开
	ConnectionClosed,      // 连接关闭
	FlushFailed,           // 刷新失败
	ConfigFailed,          // 配置失败
	AuthenticationFailed,  // 认证失败
	AccessDenied          // 访问被拒绝
};

// 前向声明
enum class PortType;

// 传输层配置基类
struct TransportConfig
{
	std::string portName;        // 端口名称
	DWORD readTimeout = 2000;     // 读超时(ms)
	DWORD writeTimeout = 2000;    // 写超时(ms)
	DWORD bufferSize = 4096;      // 缓冲区大小
	bool asyncMode = false;       // 异步模式
	PortType portType = PortType::PORT_TYPE_SERIAL;  // 端口类型

	// 串口特定配置（为了PortSessionController的统一访问）
	DWORD baudRate = 9600;        // 波特率
	BYTE dataBits = 8;            // 数据位
	BYTE parity = NOPARITY;       // 校验位
	BYTE stopBits = ONESTOPBIT;   // 停止位
	DWORD flowControl = 0;        // 流控制

	virtual ~TransportConfig() = default;
};

// 传输层统计信息
struct TransportStats
{
	uint64_t bytesSent = 0;       // 发送字节数
	uint64_t bytesReceived = 0;   // 接收字节数
	uint64_t packetsTotal = 0;    // 总包数
	uint64_t packetsError = 0;    // 错误包数
	double throughputBps = 0.0;   // 吞吐率(Bps)
	DWORD lastErrorCode = 0;      // 最后错误码
};

// 数据接收回调
using DataReceivedCallback = std::function<void(const std::vector<uint8_t>&)>;

// 状态变化回调
using StateChangedCallback = std::function<void(TransportState)>;

// 错误发生回调
using ErrorOccurredCallback = std::function<void(TransportError, const std::string&)>;

// 传输层接口
class ITransport
{
public:
	virtual ~ITransport() = default;

	// 打开传输通道
	virtual TransportError Open(const TransportConfig& config) = 0;

	// 关闭传输通道
	virtual TransportError Close() = 0;

	// 同步写入数据
	virtual TransportError Write(const void* data, size_t size, size_t* written = nullptr) = 0;

	// 同步读取数据
	virtual TransportError Read(void* buffer, size_t size, size_t* read, DWORD timeout = INFINITE) = 0;

	// 异步写入数据
	virtual TransportError WriteAsync(const void* data, size_t size) = 0;

	// 异步读取启动
	virtual TransportError StartAsyncRead() = 0;

	// 停止异步读取
	virtual TransportError StopAsyncRead() = 0;

	// 查询传输状态
	virtual TransportState GetState() const = 0;

	// 检查是否已打开
	virtual bool IsOpen() const = 0;

	// 获取统计信息
	virtual TransportStats GetStats() const = 0;

	// 重置统计信息
	virtual void ResetStats() = 0;

	// 获取端口名称
	virtual std::string GetPortName() const = 0;

	// 设置数据接收回调
	virtual void SetDataReceivedCallback(DataReceivedCallback callback) = 0;

	// 设置状态变化回调
	virtual void SetStateChangedCallback(StateChangedCallback callback) = 0;

	// 设置错误发生回调
	virtual void SetErrorOccurredCallback(ErrorOccurredCallback callback) = 0;

	// 清空缓冲区
	virtual TransportError FlushBuffers() = 0;

	// 获取可用字节数
	virtual size_t GetAvailableBytes() const = 0;

	// 获取错误描述
	static std::string GetErrorString(TransportError error);
};

// 传输层工厂类
class TransportFactory
{
public:
	// 创建传输实例
	static std::unique_ptr<ITransport> CreateTransport(const std::string& type);

	// 枚举可用端口
	static std::vector<std::string> EnumeratePorts(const std::string& type);

	// 检测端口占用
	static bool IsPortAvailable(const std::string& portName);
};