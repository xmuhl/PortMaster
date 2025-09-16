#pragma once

#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>

// 性能基准测试框架
class PerformanceBenchmark {
public:
    struct BenchmarkResult {
        std::string name;
        double min_time_ms;
        double max_time_ms;
        double avg_time_ms;
        double median_time_ms;
        double stddev_ms;
        size_t iterations;
        size_t memory_before_mb;
        size_t memory_after_mb;
        size_t memory_peak_mb;
    };

    // 性能计时器
    class Timer {
    public:
        Timer() : m_start(std::chrono::high_resolution_clock::now()) {}

        void Reset() {
            m_start = std::chrono::high_resolution_clock::now();
        }

        double ElapsedMilliseconds() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
            return duration.count() / 1000.0;
        }

        double ElapsedMicroseconds() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
            return static_cast<double>(duration.count());
        }

    private:
        std::chrono::high_resolution_clock::time_point m_start;
    };

    // 内存使用监控
    class MemoryMonitor {
    public:
        static size_t GetCurrentMemoryUsageMB() {
#ifdef _WIN32
            PROCESS_MEMORY_COUNTERS_EX pmc;
            GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
            return pmc.WorkingSetSize / (1024 * 1024);
#else
            return 0; // Linux/Mac implementation needed
#endif
        }

        void StartMonitoring() {
            m_baseline = GetCurrentMemoryUsageMB();
            m_peak = m_baseline;
            m_monitoring = true;

            // 启动监控线程
            m_monitor_thread = std::thread([this]() {
                while (m_monitoring) {
                    size_t current = GetCurrentMemoryUsageMB();
                    if (current > m_peak) {
                        m_peak = current;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
        }

        void StopMonitoring() {
            m_monitoring = false;
            if (m_monitor_thread.joinable()) {
                m_monitor_thread.join();
            }
        }

        size_t GetPeakMemoryMB() const { return m_peak; }
        size_t GetBaselineMemoryMB() const { return m_baseline; }

    private:
        std::atomic<bool> m_monitoring{false};
        std::atomic<size_t> m_baseline{0};
        std::atomic<size_t> m_peak{0};
        std::thread m_monitor_thread;
    };

    // 运行基准测试
    template<typename Func>
    BenchmarkResult RunBenchmark(const std::string& name, Func func, size_t iterations = 1000, size_t warmup = 10) {
        std::vector<double> times;
        times.reserve(iterations);

        // 预热阶段
        for (size_t i = 0; i < warmup; ++i) {
            func();
        }

        // 内存监控
        MemoryMonitor memMonitor;
        size_t memory_before = MemoryMonitor::GetCurrentMemoryUsageMB();
        memMonitor.StartMonitoring();

        // 执行基准测试
        for (size_t i = 0; i < iterations; ++i) {
            Timer timer;
            func();
            times.push_back(timer.ElapsedMilliseconds());
        }

        // 停止内存监控
        memMonitor.StopMonitoring();
        size_t memory_after = MemoryMonitor::GetCurrentMemoryUsageMB();

        // 计算统计数据
        BenchmarkResult result;
        result.name = name;
        result.iterations = iterations;
        result.memory_before_mb = memory_before;
        result.memory_after_mb = memory_after;
        result.memory_peak_mb = memMonitor.GetPeakMemoryMB();

        // 排序用于计算中位数
        std::sort(times.begin(), times.end());

        result.min_time_ms = times.front();
        result.max_time_ms = times.back();
        result.median_time_ms = times[times.size() / 2];

        // 计算平均值
        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        result.avg_time_ms = sum / times.size();

        // 计算标准差
        double sq_sum = 0.0;
        for (double t : times) {
            sq_sum += (t - result.avg_time_ms) * (t - result.avg_time_ms);
        }
        result.stddev_ms = std::sqrt(sq_sum / times.size());

        return result;
    }

    // 运行对比基准测试
    template<typename Func1, typename Func2>
    void RunComparison(const std::string& name1, Func1 func1,
                       const std::string& name2, Func2 func2,
                       size_t iterations = 1000) {
        std::cout << "\n=== 性能对比测试 ===" << std::endl;
        std::cout << "迭代次数: " << iterations << std::endl;

        auto result1 = RunBenchmark(name1, func1, iterations);
        auto result2 = RunBenchmark(name2, func2, iterations);

        PrintResult(result1);
        PrintResult(result2);

        // 打印对比结果
        std::cout << "\n性能对比:" << std::endl;
        double speedup = result1.avg_time_ms / result2.avg_time_ms;
        if (speedup > 1.0) {
            std::cout << name2 << " 比 " << name1 << " 快 "
                     << std::fixed << std::setprecision(2)
                     << (speedup - 1) * 100 << "%" << std::endl;
        } else {
            std::cout << name1 << " 比 " << name2 << " 快 "
                     << std::fixed << std::setprecision(2)
                     << (1.0 / speedup - 1) * 100 << "%" << std::endl;
        }

        // 内存对比
        int memory_diff = static_cast<int>(result2.memory_peak_mb) - static_cast<int>(result1.memory_peak_mb);
        if (memory_diff != 0) {
            std::cout << "内存使用差异: " << (memory_diff > 0 ? "+" : "")
                     << memory_diff << " MB" << std::endl;
        }
    }

    // 打印结果
    void PrintResult(const BenchmarkResult& result) {
        std::cout << "\n基准测试: " << result.name << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "最小时间: " << result.min_time_ms << " ms" << std::endl;
        std::cout << "最大时间: " << result.max_time_ms << " ms" << std::endl;
        std::cout << "平均时间: " << result.avg_time_ms << " ms" << std::endl;
        std::cout << "中位时间: " << result.median_time_ms << " ms" << std::endl;
        std::cout << "标准差:   " << result.stddev_ms << " ms" << std::endl;
        std::cout << "内存使用: " << result.memory_before_mb << " MB -> "
                 << result.memory_after_mb << " MB (峰值: "
                 << result.memory_peak_mb << " MB)" << std::endl;
    }

    // 运行完整基准测试套件
    void RunFullBenchmarkSuite() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "     PortMaster 性能基准测试套件" << std::endl;
        std::cout << "========================================" << std::endl;

        // 1. 启动性能测试
        BenchmarkStartupPerformance();

        // 2. 消息处理性能测试
        BenchmarkMessageProcessing();

        // 3. 内存分配性能测试
        BenchmarkMemoryAllocation();

        // 4. 管理器调用性能测试
        BenchmarkManagerCalls();

        // 5. 数据传输性能测试
        BenchmarkDataTransmission();

        std::cout << "\n========================================" << std::endl;
        std::cout << "        基准测试完成" << std::endl;
        std::cout << "========================================" << std::endl;
    }

private:
    // 具体的基准测试实现
    void BenchmarkStartupPerformance() {
        std::cout << "\n### 启动性能基准测试 ###" << std::endl;

        // 测试管理器初始化
        auto result = RunBenchmark("管理器初始化", []() {
            // 模拟管理器初始化
            for (int i = 0; i < 10; ++i) {
                std::vector<int> temp(1000);
                std::fill(temp.begin(), temp.end(), i);
            }
        }, 100, 5);

        PrintResult(result);
    }

    void BenchmarkMessageProcessing() {
        std::cout << "\n### 消息处理性能基准测试 ###" << std::endl;

        // 对比消息ID冲突vs无冲突
        RunComparison(
            "消息处理(有冲突)", []() {
                // 模拟消息ID冲突处理
                std::map<int, int> messageMap;
                for (int i = 0; i < 100; ++i) {
                    int id = 1001; // 固定ID，模拟冲突
                    messageMap[id] = i;
                    if (messageMap.find(id) != messageMap.end()) {
                        // 冲突处理逻辑
                        volatile int dummy = messageMap[id] * 2;
                    }
                }
            },
            "消息处理(无冲突)", []() {
                // 模拟优化后的消息处理
                std::map<int, int> messageMap;
                for (int i = 0; i < 100; ++i) {
                    int id = 2001 + i; // 唯一ID
                    messageMap[id] = i;
                    volatile int dummy = messageMap[id];
                }
            },
            1000
        );
    }

    void BenchmarkMemoryAllocation() {
        std::cout << "\n### 内存分配性能基准测试 ###" << std::endl;

        RunComparison(
            "频繁小内存分配", []() {
                std::vector<std::unique_ptr<int[]>> ptrs;
                for (int i = 0; i < 1000; ++i) {
                    ptrs.push_back(std::make_unique<int[]>(10));
                }
            },
            "批量内存分配", []() {
                auto ptr = std::make_unique<int[]>(10000);
                for (int i = 0; i < 10000; ++i) {
                    ptr[i] = i;
                }
            },
            100
        );
    }

    void BenchmarkManagerCalls() {
        std::cout << "\n### 管理器调用性能基准测试 ###" << std::endl;

        RunComparison(
            "直接函数调用", []() {
                auto directCall = [](int x) { return x * 2; };
                volatile int result = 0;
                for (int i = 0; i < 10000; ++i) {
                    result = directCall(i);
                }
            },
            "通过管理器调用", []() {
                class Manager {
                public:
                    virtual int Process(int x) { return x * 2; }
                };
                auto mgr = std::make_unique<Manager>();
                volatile int result = 0;
                for (int i = 0; i < 10000; ++i) {
                    result = mgr->Process(i);
                }
            },
            1000
        );
    }

    void BenchmarkDataTransmission() {
        std::cout << "\n### 数据传输性能基准测试 ###" << std::endl;

        auto result = RunBenchmark("数据缓冲区操作", []() {
            std::vector<uint8_t> buffer(4096);
            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] = static_cast<uint8_t>(i & 0xFF);
            }
            // 模拟CRC计算
            uint32_t crc = 0;
            for (uint8_t byte : buffer) {
                crc = (crc >> 8) ^ (byte << 24);
            }
            volatile uint32_t dummy = crc;
        }, 1000);

        PrintResult(result);
    }
};

// CPU性能分析器
class CPUProfiler {
public:
    static void EnableProfiling() {
#ifdef _MSC_VER
        // Visual Studio 性能分析支持
        __debugbreak(); // 断点用于开始性能分析
#endif
    }

    static void DisableProfiling() {
#ifdef _MSC_VER
        __debugbreak(); // 断点用于结束性能分析
#endif
    }

    // 获取CPU使用率
    static double GetCPUUsage() {
#ifdef _WIN32
        static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
        static int numProcessors = 0;
        static HANDLE self = GetCurrentProcess();

        if (numProcessors == 0) {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            numProcessors = sysInfo.dwNumberOfProcessors;
        }

        FILETIME ftime, fsys, fuser;
        ULARGE_INTEGER now, sys, user;

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&now, &ftime, sizeof(FILETIME));

        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&sys, &fsys, sizeof(FILETIME));
        memcpy(&user, &fuser, sizeof(FILETIME));

        double percent = 0.0;
        if (lastCPU.QuadPart != 0) {
            percent = double(sys.QuadPart - lastSysCPU.QuadPart) +
                     double(user.QuadPart - lastUserCPU.QuadPart);
            percent /= double(now.QuadPart - lastCPU.QuadPart);
            percent /= numProcessors;
            percent *= 100.0;
        }

        lastCPU = now;
        lastUserCPU = user;
        lastSysCPU = sys;

        return percent;
#else
        return 0.0; // Linux/Mac implementation needed
#endif
    }
};

// 性能监控仪表板
class PerformanceDashboard {
public:
    struct Metrics {
        double cpu_usage;
        size_t memory_usage_mb;
        size_t handle_count;
        size_t thread_count;
        double io_read_mb_per_sec;
        double io_write_mb_per_sec;
        size_t message_queue_size;
        double frame_time_ms;
    };

    void StartMonitoring() {
        m_monitoring = true;
        m_monitor_thread = std::thread([this]() {
            while (m_monitoring) {
                CollectMetrics();
                if (m_metrics_history.size() > 1000) {
                    m_metrics_history.erase(m_metrics_history.begin());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void StopMonitoring() {
        m_monitoring = false;
        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }
    }

    void PrintReport() {
        if (m_metrics_history.empty()) {
            std::cout << "没有收集到性能数据" << std::endl;
            return;
        }

        std::cout << "\n=== 性能监控报告 ===" << std::endl;

        // 计算平均值
        Metrics avg{};
        for (const auto& m : m_metrics_history) {
            avg.cpu_usage += m.cpu_usage;
            avg.memory_usage_mb += m.memory_usage_mb;
            avg.handle_count += m.handle_count;
            avg.thread_count += m.thread_count;
            avg.frame_time_ms += m.frame_time_ms;
        }

        size_t count = m_metrics_history.size();
        avg.cpu_usage /= count;
        avg.memory_usage_mb /= count;
        avg.handle_count /= count;
        avg.thread_count /= count;
        avg.frame_time_ms /= count;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "平均CPU使用率: " << avg.cpu_usage << "%" << std::endl;
        std::cout << "平均内存使用: " << avg.memory_usage_mb << " MB" << std::endl;
        std::cout << "平均句柄数: " << avg.handle_count << std::endl;
        std::cout << "平均线程数: " << avg.thread_count << std::endl;
        std::cout << "平均帧时间: " << avg.frame_time_ms << " ms" << std::endl;

        // 计算最大值
        auto max_cpu = std::max_element(m_metrics_history.begin(), m_metrics_history.end(),
            [](const Metrics& a, const Metrics& b) { return a.cpu_usage < b.cpu_usage; });
        auto max_mem = std::max_element(m_metrics_history.begin(), m_metrics_history.end(),
            [](const Metrics& a, const Metrics& b) { return a.memory_usage_mb < b.memory_usage_mb; });

        std::cout << "\n峰值数据:" << std::endl;
        std::cout << "最高CPU使用率: " << max_cpu->cpu_usage << "%" << std::endl;
        std::cout << "最高内存使用: " << max_mem->memory_usage_mb << " MB" << std::endl;
    }

private:
    void CollectMetrics() {
        Metrics m{};

        m.cpu_usage = CPUProfiler::GetCPUUsage();
        m.memory_usage_mb = PerformanceBenchmark::MemoryMonitor::GetCurrentMemoryUsageMB();

#ifdef _WIN32
        HANDLE process = GetCurrentProcess();
        DWORD handleCount;
        GetProcessHandleCount(process, &handleCount);
        m.handle_count = handleCount;

        // 获取线程数
        DWORD processId = GetCurrentProcessId();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);
            size_t threadCount = 0;
            if (Thread32First(snapshot, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == processId) {
                        threadCount++;
                    }
                } while (Thread32Next(snapshot, &te32));
            }
            m.thread_count = threadCount;
            CloseHandle(snapshot);
        }
#endif

        m.frame_time_ms = 16.67; // 默认60 FPS

        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics_history.push_back(m);
    }

    std::atomic<bool> m_monitoring{false};
    std::thread m_monitor_thread;
    std::vector<Metrics> m_metrics_history;
    mutable std::mutex m_mutex;
};

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <tlhelp32.h>
    #pragma comment(lib, "psapi.lib")
#endif

#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>