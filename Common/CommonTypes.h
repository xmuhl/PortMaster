#pragma once
#pragma execution_character_set("utf-8")

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>

// 应用程序版本信息
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_BUILD 0
#define APP_VERSION_REVISION 0

// 应用程序名称
#define APP_NAME _T("PortMaster 端口大师")
#define APP_COMPANY _T("PortMaster Development Team")
#define APP_COPYRIGHT _T("Copyright © 2024 PortMaster. All rights reserved.")

// 默认配置常量
#define DEFAULT_CONFIG_FILE _T("PortMaster.json")
#define DEFAULT_LOG_FILE _T("PortMaster.log")
#define DEFAULT_RECEIVE_DIR _T("ReceivedFiles")

// 超时常量
#define DEFAULT_READ_TIMEOUT 2000     // 默认读取超时(ms)
#define DEFAULT_WRITE_TIMEOUT 2000    // 默认写入超时(ms)
#define DEFAULT_CONNECT_TIMEOUT 5000  // 默认连接超时(ms)
#define DEFAULT_HEARTBEAT_INTERVAL 1000 // 默认心跳间隔(ms)

// 缓冲区大小常量
#define DEFAULT_BUFFER_SIZE 4096      // 默认缓冲区大小
#define MAX_BUFFER_SIZE 1048576       // 最大缓冲区大小(1MB)
#define MIN_BUFFER_SIZE 256           // 最小缓冲区大小

// 串口默认参数
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_DATA_BITS 8
#define DEFAULT_PARITY NOPARITY
#define DEFAULT_STOP_BITS ONESTOPBIT
#define DEFAULT_FLOW_CONTROL 0

// 可靠传输协议常量
#define RELIABLE_PROTOCOL_VERSION 1   // 协议版本
#define RELIABLE_MAX_PAYLOAD_SIZE 1024 // 最大负载大小
#define RELIABLE_WINDOW_SIZE 4       // 滑动窗口大小
#define RELIABLE_MAX_RETRIES 3       // 最大重试次数
#define RELIABLE_TIMEOUT_BASE 500    // 基础超时时间(ms)
#define RELIABLE_TIMEOUT_MAX 2000    // 最大超时时间(ms)

// 网络打印默认参数
#define DEFAULT_NETWORK_PORT 9100     // 默认网络打印端口
#define DEFAULT_LPR_PORT 515        // LPR协议端口
#define DEFAULT_IPP_PORT 631        // IPP协议端口
#define DEFAULT_KEEPALIVE_INTERVAL 30000 // KeepAlive间隔(ms)

// 文件类型定义
enum class FileType
{
	FILE_TYPE_TEXT,     // 文本文件
	FILE_TYPE_BINARY,   // 二进制文件
	FILE_TYPE_HEX,      // 十六进制文件
	FILE_TYPE_AUTO      // 自动检测
};

// 数据源类型
enum class DataSource
{
	DATA_SOURCE_MANUAL,  // 手动输入
	DATA_SOURCE_FILE,    // 文件加载
	DATA_SOURCE_CLIPBOARD, // 剪贴板
	DATA_SOURCE_LOOPBACK // 回路测试
};

// 传输模式
enum class TransferMode
{
	TRANSFER_MODE_DIRECT,   // 直通模式
	TRANSFER_MODE_RELIABLE  // 可靠模式
};

// 端口类型
enum class PortType
{
	PORT_TYPE_SERIAL,      // 串口
	PORT_TYPE_PARALLEL,      // 并口
	PORT_TYPE_USB_PRINT,     // USB打印
	PORT_TYPE_NETWORK_PRINT, // 网络打印
	PORT_TYPE_LOOPBACK       // 回路测试
};

// 网络协议类型
enum class NetworkProtocol
{
	PROTOCOL_RAW,     // RAW协议
	PROTOCOL_LPR,     // LPR/LPD协议
	PROTOCOL_IPP,     // IPP协议
	PROTOCOL_UNKNOWN  // 未知协议
};

// 日志级别
enum class LogLevel
{
	LOG_LEVEL_DEBUG = 0,    // 调试
	LOG_LEVEL_INFO = 1,     // 信息
	LOG_LEVEL_WARNING = 2,  // 警告
	LOG_LEVEL_ERROR = 3,    // 错误
	LOG_LEVEL_FATAL = 4     // 致命
};

// 操作结果
enum class OperationResult
{
	RESULT_SUCCESS = 0,        // 成功
	RESULT_FAILED = -1,        // 失败
	RESULT_CANCELLED = -2,     // 取消
	RESULT_TIMEOUT = -3,       // 超时
	RESULT_INVALID_PARAM = -4, // 无效参数
	RESULT_NOT_SUPPORTED = -5, // 不支持
	RESULT_ACCESS_DENIED = -6, // 访问被拒绝
	RESULT_OUT_OF_MEMORY = -7  // 内存不足
};

// 时间戳类型定义
using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;

// 字节向量类型定义
using ByteVector = std::vector<uint8_t>;
using ByteArray = std::vector<uint8_t>;

// 字符串类型定义
using String = std::string;
using WString = std::wstring;

// 回调函数类型定义
using ProgressCallback = std::function<void(int64_t current, int64_t total)>;
using StatusCallback = std::function<void(const std::string& status)>;
using ErrorCallback = std::function<void(int errorCode, const std::string& errorMessage)>;

// 常用工具函数
namespace CommonUtils
{
	// 获取当前时间戳
	inline Timestamp GetCurrentTimestamp()
	{
		return std::chrono::system_clock::now();
	}

	// 时间戳转字符串
	inline std::string TimestampToString(const Timestamp& timestamp)
	{
		auto time_t = std::chrono::system_clock::to_time_t(timestamp);
		struct tm tm;
		localtime_s(&tm, &time_t);
		std::stringstream ss;
		ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		return ss.str();
	}

	// 获取时间间隔(ms)
	inline int64_t GetDurationMs(const Timestamp& start, const Timestamp& end)
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	}

	// 字符串转大写
	inline std::string ToUpper(const std::string& str)
	{
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(), ::toupper);
		return result;
	}

	// 字符串转小写
	inline std::string ToLower(const std::string& str)
	{
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
		return result;
	}

	// 修剪字符串两端空白
	inline std::string Trim(const std::string& str)
	{
		auto start = str.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) return "";
		auto end = str.find_last_not_of(" \t\r\n");
		return str.substr(start, end - start + 1);
	}

	// 检查字符串是否以指定前缀开始
	inline bool StartsWith(const std::string& str, const std::string& prefix)
	{
		return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
	}

	// 检查字符串是否以指定后缀结束
	inline bool EndsWith(const std::string& str, const std::string& suffix)
	{
		return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
	}

	// 分割字符串
	inline std::vector<std::string> Split(const std::string& str, char delimiter)
	{
		std::vector<std::string> result;
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, delimiter))
		{
			if (!item.empty())
			{
				result.push_back(item);
			}
		}

		return result;
	}

	// 格式化字符串
	template<typename... Args>
	inline std::string Format(const std::string& format, Args... args)
	{
		int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
		std::vector<char> buffer(size);
		snprintf(buffer.data(), size, format.c_str(), args...);
		return std::string(buffer.data(), size - 1);
	}

	// 获取错误码描述
	inline std::string GetLastErrorString()
	{
		DWORD errorCode = GetLastError();
		if (errorCode == 0) return "";

		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&messageBuffer, 0, NULL);

		std::string message(messageBuffer, size);
		LocalFree(messageBuffer);

		// 移除末尾的换行符
		while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
		{
			message.pop_back();
		}

		return Format("[错误码: %lu] %s", errorCode, message.c_str());
	}

	// 检查文件是否存在
	inline bool FileExists(const std::string& path)
	{
		return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	// 创建目录
	inline bool CreateDirectory(const std::string& path)
	{
		return CreateDirectoryA(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
	}

	// 获取文件大小
	inline int64_t GetFileSize(const std::string& path)
	{
		WIN32_FILE_ATTRIBUTE_DATA fileData;
		if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData))
		{
			LARGE_INTEGER size;
			size.HighPart = fileData.nFileSizeHigh;
			size.LowPart = fileData.nFileSizeLow;
			return size.QuadPart;
		}
		return -1;
	}
}

// 作用域保护类
template<typename T>
class ScopeGuard
{
public:
	explicit ScopeGuard(T&& func) : m_func(std::move(func)), m_dismissed(false) {}

	~ScopeGuard()
	{
		if (!m_dismissed)
		{
			m_func();
		}
	}

	void Dismiss() { m_dismissed = true; }

	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;

	ScopeGuard(ScopeGuard&& other) noexcept
		: m_func(std::move(other.m_func)), m_dismissed(other.m_dismissed)
	{
		other.m_dismissed = true;
	}

private:
	T m_func;
	bool m_dismissed;
};

// 创建作用域保护
#define SCOPE_GUARD(func) ScopeGuard<decltype(func)> _scope_guard_##__LINE__(func)
#define ON_SCOPE_EXIT(func) auto _scope_guard_##__LINE__ = ScopeGuard<decltype(func)>(func)