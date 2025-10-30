#pragma execution_character_set("utf-8")

#include "pch.h"
#include "Logger.h"
#include <Windows.h>
#include <ctime>
#include <iomanip>

// 静态成员初始化
std::string Logger::s_logFilePath = "";
std::mutex Logger::s_mutex;
bool Logger::s_initialized = false;

void Logger::Initialize(const std::string& logFilePath)
{
	std::lock_guard<std::mutex> lock(s_mutex);

	if (s_initialized) {
		return; // 已经初始化
	}

	s_logFilePath = logFilePath;
	s_initialized = true;

	// 清空或创建日志文件
	std::ofstream logFile(s_logFilePath, std::ios::trunc);
	if (logFile.is_open()) {
		logFile << "=== PortMaster Debug Log ===" << std::endl;
		logFile << "启动时间: " << GetTimeStamp() << std::endl;
		logFile << "============================" << std::endl;
		logFile.close();
	}
}

void Logger::Shutdown()
{
	std::lock_guard<std::mutex> lock(s_mutex);

	if (s_initialized) {
		std::ofstream logFile(s_logFilePath, std::ios::app);
		if (logFile.is_open()) {
			logFile << "=== 日志系统关闭 ===" << std::endl;
			logFile.close();
		}
		s_initialized = false;
	}
}

void Logger::Log(const std::string& message)
{
	WriteInternal("INFO", message);
}

void Logger::LogError(const std::string& message)
{
	WriteInternal("ERROR", message);
}

void Logger::LogWarning(const std::string& message)
{
	WriteInternal("WARNING", message);
}

void Logger::LogDebug(const std::string& message)
{
	WriteInternal("DEBUG", message);
}

void Logger::WriteInternal(const std::string& level, const std::string& message)
{
	std::lock_guard<std::mutex> lock(s_mutex);

	// 构造日志行
	std::string logLine = GetTimeStamp() + " [" + level + "] " + message;

	// 写入文件
	if (!s_logFilePath.empty()) {
		std::ofstream logFile(s_logFilePath, std::ios::app);
		if (logFile.is_open()) {
			logFile << logLine << std::endl;
			logFile.close();
		}
	}

	// 同时输出到调试器（可选）
	OutputDebugStringA((logLine + "\n").c_str());
}

std::string Logger::GetTimeStamp()
{
	auto now = std::chrono::system_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	auto timer = std::chrono::system_clock::to_time_t(now);

	std::tm bt;
	localtime_s(&bt, &timer);

	std::ostringstream oss;
	oss << std::put_time(&bt, "%H:%M:%S");
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

	return "[" + oss.str() + "]";
}
