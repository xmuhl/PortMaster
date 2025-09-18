#pragma once

#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "CommonTypes.h"

namespace PortMaster {

    /// <summary>
    /// 日志中心管理器
    /// 提供分级日志记录、实时显示和导出功能
    /// </summary>
    class LogCenter {
    public:
        LogCenter();
        virtual ~LogCenter();

        /// <summary>
        /// 初始化日志系统
        /// </summary>
        /// <param name="logDir">日志目录</param>
        /// <param name="minLevel">最小日志级别</param>
        /// <param name="enableFile">是否启用文件输出</param>
        /// <returns>成功返回true</returns>
        bool Initialize(const CString& logDir, LogLevel minLevel, bool enableFile);

        /// <summary>
        /// 写入调试日志
        /// </summary>
        /// <param name="module">模块名称</param>
        /// <param name="message">日志消息</param>
        /// <param name="port">端口标识（可选）</param>
        /// <param name="taskId">任务ID（可选）</param>
        void Debug(const CString& module, const CString& message,
                  const CString& port = _T(""), DWORD taskId = 0);

        /// <summary>
        /// 写入信息日志
        /// </summary>
        /// <param name="module">模块名称</param>
        /// <param name="message">日志消息</param>
        /// <param name="port">端口标识（可选）</param>
        /// <param name="taskId">任务ID（可选）</param>
        void Info(const CString& module, const CString& message,
                 const CString& port = _T(""), DWORD taskId = 0);

        /// <summary>
        /// 写入警告日志
        /// </summary>
        /// <param name="module">模块名称</param>
        /// <param name="message">日志消息</param>
        /// <param name="port">端口标识（可选）</param>
        /// <param name="taskId">任务ID（可选）</param>
        void Warning(const CString& module, const CString& message,
                    const CString& port = _T(""), DWORD taskId = 0);

        /// <summary>
        /// 写入错误日志
        /// </summary>
        /// <param name="module">模块名称</param>
        /// <param name="message">日志消息</param>
        /// <param name="port">端口标识（可选）</param>
        /// <param name="taskId">任务ID（可选）</param>
        void Error(const CString& module, const CString& message,
                  const CString& port = _T(""), DWORD taskId = 0);

        /// <summary>
        /// 获取最近的日志条目
        /// </summary>
        /// <param name="maxCount">最大条目数</param>
        /// <param name="minLevel">最小级别过滤</param>
        /// <returns>日志条目列表</returns>
        std::vector<LogEntry> GetRecentEntries(size_t maxCount = 100,
                                              LogLevel minLevel = LogLevel::LOG_DEBUG) const;

        /// <summary>
        /// 导出日志到文件
        /// </summary>
        /// <param name="filePath">导出文件路径</param>
        /// <param name="startTime">开始时间（可选）</param>
        /// <param name="endTime">结束时间（可选）</param>
        /// <returns>成功返回true</returns>
        bool ExportToFile(const CString& filePath,
                         const SYSTEMTIME* startTime = nullptr,
                         const SYSTEMTIME* endTime = nullptr) const;

        /// <summary>
        /// 清空内存中的日志
        /// </summary>
        void Clear();

        /// <summary>
        /// 设置最小日志级别
        /// </summary>
        /// <param name="level">新的最小级别</param>
        void SetMinLevel(LogLevel level);

        /// <summary>
        /// 获取当前最小日志级别
        /// </summary>
        /// <returns>当前最小级别</returns>
        LogLevel GetMinLevel() const;

        /// <summary>
        /// 关闭日志系统
        /// </summary>
        void Shutdown();

        // 静态便利方法
        static LogCenter& Instance();
        static CString LevelToString(LogLevel level);
        static CString FormatTimestamp(const SYSTEMTIME& time);

    private:
        // 内部日志写入
        void WriteEntry(const LogEntry& entry);
        bool ShouldLog(LogLevel level) const;
        CString GenerateLogFileName() const;
        void RotateLogFiles();

        // 成员变量
        mutable std::mutex logMutex_;               // 日志访问锁
        std::vector<LogEntry> entries_;             // 内存中的日志条目
        LogLevel minLevel_;                         // 最小日志级别
        bool enableFileOutput_;                     // 启用文件输出
        CString logDirectory_;                      // 日志目录
        CString currentLogFile_;                    // 当前日志文件
        FILE* logFileHandle_;                       // 日志文件句柄

        static const size_t MAX_MEMORY_ENTRIES = 1000;  // 内存中最大条目数
        static const size_t MAX_LOG_FILES = 10;         // 最大日志文件数
    };

    // 便利宏定义
    #define LOG_DEBUG(module, message) LogCenter::Instance().Debug(module, message)
    #define LOG_INFO(module, message) LogCenter::Instance().Info(module, message)
    #define LOG_WARNING(module, message) LogCenter::Instance().Warning(module, message)
    #define LOG_ERROR(module, message) LogCenter::Instance().Error(module, message)

} // namespace PortMaster