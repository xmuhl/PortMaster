#pragma once
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

// 专用于调试锁问题的日志文件
#define DEBUG_LOCK_LOG_FILE "PortMaster_lock_debug.log"

class DebugMutex {
public:
    DebugMutex() {
        // 构造时可以记录下锁的内存地址，方便追踪
        std::ostringstream oss;
        oss << this;
        m_mutexAddress = oss.str();
    }

    void lock(const char* file, int line) {
        log("Attempting to lock...", file, line);
        m_mutex.lock();
        log("Locked.", file, line);
    }

    void unlock(const char* file, int line) {
        // 解锁前不需要记录日志，因为unlock本身不会阻塞或抛异常
        m_mutex.unlock();
        log("Unlocked.", file, line);
    }

    // 提供对底层互斥锁的访问，以便用于条件变量
    std::mutex& get_native_mutex() {
        return m_mutex;
    }

private:
    void log(const std::string& action, const char* file, int line) {
        try {
            std::ofstream logFile(DEBUG_LOCK_LOG_FILE, std::ios::app);
            if (logFile.is_open()) {
                // 获取当前时间
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
                std::tm tm;
                localtime_s(&tm, &time_t);

                // 获取线程ID
                std::ostringstream threadIdStream;
                threadIdStream << std::this_thread::get_id();

                // 格式化日志
                logFile << "[" << std::put_time(&tm, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "]" 
                        << " [Thread: " << threadIdStream.str() << "]" 
                        << " [Mutex: " << m_mutexAddress << "]" 
                        << " " << action 
                        << " at " << file << ":" << line << std::endl;
            }
        } catch (...) {
            // 忽略日志写入错误
        }
    }

    std::mutex m_mutex;
    std::string m_mutexAddress;
};

// 辅助宏，自动传入文件和行号
#define DEBUG_LOCK(m) (m).lock(__FILE__, __LINE__)
#define DEBUG_UNLOCK(m) (m).unlock(__FILE__, __LINE__)
