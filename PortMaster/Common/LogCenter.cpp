#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "LogCenter.h"

namespace PortMaster {

    LogCenter::LogCenter()
        : minLevel_(LogLevel::LOG_INFO)
        , enableFileOutput_(false)
        , logFileHandle_(nullptr) {
    }

    LogCenter::~LogCenter() {
        Shutdown();
    }

    bool LogCenter::Initialize(const CString& logDir, LogLevel minLevel, bool enableFile) {
        std::lock_guard<std::mutex> lock(logMutex_);

        minLevel_ = minLevel;
        enableFileOutput_ = enableFile;
        logDirectory_ = logDir;

        if (enableFileOutput_) {
            // 确保日志目录存在
            DWORD attrs = GetFileAttributes(logDirectory_);
            if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                if (!CreateDirectory(logDirectory_, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }

            // 生成日志文件名
            currentLogFile_ = GenerateLogFileName();

            // 打开日志文件
            errno_t err = _tfopen_s(&logFileHandle_, currentLogFile_, _T("a+, ccs=UTF-8"));
            if (err != 0 || !logFileHandle_) {
                enableFileOutput_ = false;
                return false;
            }
        }

        // 写入初始化日志
        LogEntry initEntry;
        GetSystemTime(&initEntry.timestamp);
        initEntry.level = LogLevel::LOG_INFO;
        initEntry.module = _T("LogCenter");
        initEntry.message = _T("日志系统初始化成功");

        WriteEntry(initEntry);

        return true;
    }

    void LogCenter::Debug(const CString& module, const CString& message,
                         const CString& port, DWORD taskId) {
        if (!ShouldLog(LogLevel::LOG_DEBUG)) return;

        LogEntry entry;
        GetSystemTime(&entry.timestamp);
        entry.level = LogLevel::LOG_DEBUG;
        entry.module = module;
        entry.port = port;
        entry.message = message;
        entry.taskId = taskId;

        WriteEntry(entry);
    }

    void LogCenter::Info(const CString& module, const CString& message,
                        const CString& port, DWORD taskId) {
        if (!ShouldLog(LogLevel::LOG_INFO)) return;

        LogEntry entry;
        GetSystemTime(&entry.timestamp);
        entry.level = LogLevel::LOG_INFO;
        entry.module = module;
        entry.port = port;
        entry.message = message;
        entry.taskId = taskId;

        WriteEntry(entry);
    }

    void LogCenter::Warning(const CString& module, const CString& message,
                           const CString& port, DWORD taskId) {
        if (!ShouldLog(LogLevel::LOG_WARNING)) return;

        LogEntry entry;
        GetSystemTime(&entry.timestamp);
        entry.level = LogLevel::LOG_WARNING;
        entry.module = module;
        entry.port = port;
        entry.message = message;
        entry.taskId = taskId;

        WriteEntry(entry);
    }

    void LogCenter::Error(const CString& module, const CString& message,
                         const CString& port, DWORD taskId) {
        if (!ShouldLog(LogLevel::LOG_ERROR)) return;

        LogEntry entry;
        GetSystemTime(&entry.timestamp);
        entry.level = LogLevel::LOG_ERROR;
        entry.module = module;
        entry.port = port;
        entry.message = message;
        entry.taskId = taskId;

        WriteEntry(entry);
    }

    std::vector<LogEntry> LogCenter::GetRecentEntries(size_t maxCount, LogLevel minLevel) const {
        std::lock_guard<std::mutex> lock(logMutex_);

        std::vector<LogEntry> result;
        result.reserve(min(maxCount, entries_.size()));

        size_t count = 0;
        for (auto it = entries_.rbegin(); it != entries_.rend() && count < maxCount; ++it) {
            if (static_cast<int>(it->level) >= static_cast<int>(minLevel)) {
                result.push_back(*it);
                count++;
            }
        }

        return result;
    }

    bool LogCenter::ExportToFile(const CString& filePath,
                                const SYSTEMTIME* startTime,
                                const SYSTEMTIME* endTime) const {
        std::lock_guard<std::mutex> lock(logMutex_);

        FILE* exportFile = nullptr;
        errno_t err = _tfopen_s(&exportFile, filePath, _T("w, ccs=UTF-8"));
        if (err != 0 || !exportFile) {
            return false;
        }

        // 写入文件头
        _ftprintf_s(exportFile, _T("PortMaster 日志导出\n"));
        _ftprintf_s(exportFile, _T("导出时间: %s\n"), FormatTimestamp(SYSTEMTIME{}).GetString());
        _ftprintf_s(exportFile, _T("----------------------------------------\n\n"));

        // 写入日志条目
        for (const auto& entry : entries_) {
            // TODO: 实现时间范围过滤
            if (startTime || endTime) {
                // 时间比较逻辑
                continue;
            }

            CString taskInfo;
            if (entry.taskId > 0) {
                taskInfo.Format(_T(" (Task:%u)"), entry.taskId);
            }
            _ftprintf_s(exportFile, _T("[%s] [%s] [%s] %s%s%s\n"),
                       FormatTimestamp(entry.timestamp).GetString(),
                       LevelToString(entry.level).GetString(),
                       entry.module.GetString(),
                       entry.port.IsEmpty() ? _T("") : (entry.port + _T(" - ")).GetString(),
                       entry.message.GetString(),
                       taskInfo.GetString());
        }

        fclose(exportFile);
        return true;
    }

    void LogCenter::Clear() {
        std::lock_guard<std::mutex> lock(logMutex_);
        entries_.clear();
    }

    void LogCenter::SetMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(logMutex_);
        minLevel_ = level;
    }

    LogLevel LogCenter::GetMinLevel() const {
        std::lock_guard<std::mutex> lock(logMutex_);
        return minLevel_;
    }

    void LogCenter::Shutdown() {
        std::lock_guard<std::mutex> lock(logMutex_);

        if (logFileHandle_) {
            LogEntry shutdownEntry;
            GetSystemTime(&shutdownEntry.timestamp);
            shutdownEntry.level = LogLevel::LOG_INFO;
            shutdownEntry.module = _T("LogCenter");
            shutdownEntry.message = _T("日志系统关闭");

            WriteEntry(shutdownEntry);

            fclose(logFileHandle_);
            logFileHandle_ = nullptr;
        }

        entries_.clear();
    }

    LogCenter& LogCenter::Instance() {
        static LogCenter instance;
        return instance;
    }

    CString LogCenter::LevelToString(LogLevel level) {
        switch (level) {
        case LogLevel::LOG_DEBUG:   return _T("DEBUG");
        case LogLevel::LOG_INFO:    return _T("INFO");
        case LogLevel::LOG_WARNING: return _T("WARN");
        case LogLevel::LOG_ERROR:   return _T("ERROR");
        default:                    return _T("UNKNOWN");
        }
    }

    CString LogCenter::FormatTimestamp(const SYSTEMTIME& time) {
        CString result;
        result.Format(_T("%04d-%02d-%02d %02d:%02d:%02d.%03d"),
                     time.wYear, time.wMonth, time.wDay,
                     time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
        return result;
    }

    void LogCenter::WriteEntry(const LogEntry& entry) {
        // 添加到内存缓存
        entries_.push_back(entry);

        // 限制内存中的条目数量
        if (entries_.size() > MAX_MEMORY_ENTRIES) {
            entries_.erase(entries_.begin(), entries_.begin() + (MAX_MEMORY_ENTRIES / 4));
        }

        // 写入文件
        if (enableFileOutput_ && logFileHandle_) {
            CString taskInfo;
            if (entry.taskId > 0) {
                taskInfo.Format(_T(" (Task:%u)"), entry.taskId);
            }
            _ftprintf_s(logFileHandle_, _T("[%s] [%s] [%s] %s%s%s\n"),
                       FormatTimestamp(entry.timestamp).GetString(),
                       LevelToString(entry.level).GetString(),
                       entry.module.GetString(),
                       entry.port.IsEmpty() ? _T("") : (entry.port + _T(" - ")).GetString(),
                       entry.message.GetString(),
                       taskInfo.GetString());

            fflush(logFileHandle_);
        }
    }

    bool LogCenter::ShouldLog(LogLevel level) const {
        return static_cast<int>(level) >= static_cast<int>(minLevel_);
    }

    CString LogCenter::GenerateLogFileName() const {
        SYSTEMTIME time;
        GetLocalTime(&time);

        CString fileName;
        fileName.Format(_T("%s\\PortMaster_%04d%02d%02d_%02d%02d%02d.log"),
                       logDirectory_.GetString(),
                       time.wYear, time.wMonth, time.wDay,
                       time.wHour, time.wMinute, time.wSecond);

        return fileName;
    }

    void LogCenter::RotateLogFiles() {
        // TODO: 实现日志文件轮转逻辑
        // 删除过期的日志文件，保持文件数量在限制范围内
    }

} // namespace PortMaster

