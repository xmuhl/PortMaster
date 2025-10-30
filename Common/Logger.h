#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief 全局日志工具类
 *
 * 职责：提供线程安全的日志输出功能，支持文件和调试输出
 *
 * 功能说明：
 * - 自动添加时间戳
 * - 线程安全
 * - 同时输出到文件和OutputDebugString
 * - 支持日志级别
 *
 * 使用示例：
 * @code
 * Logger::Log("[USB] 端口枚举开始");
 * Logger::LogError("[USB] 端口打开失败");
 * @endcode
 */
class Logger
{
public:
	/**
	 * @brief 初始化日志系统
	 * @param logFilePath 日志文件路径，默认为"PortMaster_debug.log"
	 */
	static void Initialize(const std::string& logFilePath = "PortMaster_debug.log");

	/**
	 * @brief 关闭日志系统
	 */
	static void Shutdown();

	/**
	 * @brief 写入日志（普通级别）
	 * @param message 日志消息
	 */
	static void Log(const std::string& message);

	/**
	 * @brief 写入错误日志
	 * @param message 错误消息
	 */
	static void LogError(const std::string& message);

	/**
	 * @brief 写入警告日志
	 * @param message 警告消息
	 */
	static void LogWarning(const std::string& message);

	/**
	 * @brief 写入调试日志
	 * @param message 调试消息
	 */
	static void LogDebug(const std::string& message);

private:
	static std::string s_logFilePath;      // 日志文件路径
	static std::mutex s_mutex;             // 线程安全互斥锁
	static bool s_initialized;             // 是否已初始化

	/**
	 * @brief 内部写入方法
	 * @param level 日志级别
	 * @param message 消息内容
	 */
	static void WriteInternal(const std::string& level, const std::string& message);

	/**
	 * @brief 获取当前时间戳字符串
	 * @return 格式化的时间戳字符串 [HH:MM:SS.mmm]
	 */
	static std::string GetTimeStamp();
};
