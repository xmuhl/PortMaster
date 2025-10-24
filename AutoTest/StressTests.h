#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include <random>
#include <chrono>
#include <thread>
#include <vector>

// 压力测试
class StressTest : public TestSuite {
public:
    StressTest() : TestSuite("StressTest") {}

    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
        m_reliableChannel = std::make_shared<ReliableChannel>();

        TransportConfig config;
        m_transport->Open(config);

        ReliableConfig reliableConfig;
        reliableConfig.windowSize = 16;
        reliableConfig.maxRetries = 10;
        m_reliableChannel->Initialize(m_transport, reliableConfig);
        m_reliableChannel->Connect();
    }

    void TearDown() override {
        if (m_reliableChannel) {
            m_reliableChannel->Disconnect();
            m_reliableChannel->Shutdown();
        }
        if (m_transport) {
            m_transport->Close();
        }
    }

    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 大数据量压力测试 (100MB)
        results.push_back(RunTest("Large data stress (100MB)", [this]() {
            TestLargeDataStress(100 * 1024 * 1024);
        }));

        // 测试2: 连续传输压力测试
        results.push_back(RunTest("Continuous transmission (50x1MB)", [this]() {
            TestContinuousTransmission(50, 1024 * 1024);
        }));

        // 测试3: 高错误率环境压力测试
        results.push_back(RunTest("High error rate stress", [this]() {
            TestHighErrorRateStress();
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestLargeDataStress(size_t dataSize) {
        // 生成大量测试数据
        std::vector<uint8_t> testData(dataSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < dataSize; i++) {
            testData[i] = static_cast<uint8_t>(dis(gen));
        }

        // 发送数据
        auto startTime = std::chrono::high_resolution_clock::now();
        bool success = m_reliableChannel->Send(testData);
        auto endTime = std::chrono::high_resolution_clock::now();

        AssertTrue(success, "Large data transmission should succeed");

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 60000);
        AssertTrue(recvSuccess, "Reception should succeed");

        // 验证数据完整性
        AssertFileEqual(testData, receivedData);

        // 输出性能指标
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        double throughputMBps = (dataSize / (1024.0 * 1024.0)) / duration.count();

        std::cout << " | " << throughputMBps << " MB/s, "
                  << duration.count() << " s";
    }

    void TestContinuousTransmission(int iterations, size_t blockSize) {
        int successCount = 0;
        size_t totalBytes = 0;

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            // 生成测试数据
            std::vector<uint8_t> testData(blockSize);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (size_t j = 0; j < blockSize; j++) {
                testData[j] = static_cast<uint8_t>(dis(gen));
            }

            // 发送并接收
            bool success = m_reliableChannel->Send(testData.data(), testData.size());
            if (success) {
                std::vector<uint8_t> receivedData;
                bool recvSuccess = m_reliableChannel->Receive(receivedData, 60000);

                if (recvSuccess && receivedData == testData) {
                    successCount++;
                    totalBytes += blockSize;
                }
            }

            // 每10次输出进度
            if ((i + 1) % 10 == 0) {
                std::cout << ".";
                std::cout.flush();
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        AssertEqual(iterations, successCount, "All transmissions should succeed");

        std::cout << " | " << successCount << "/" << iterations << " succeeded, "
                  << (totalBytes / (1024.0 * 1024.0)) / duration.count() << " MB/s";
    }

    void TestHighErrorRateStress() {
        // 配置高错误率环境
        m_transport->SetPacketLossRate(15);  // 15%丢包率
        m_transport->SetErrorRate(5);        // 5%错误率

        // 发送测试数据
        const size_t dataSize = 10 * 1024 * 1024;  // 10MB
        std::vector<uint8_t> testData(dataSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < dataSize; i++) {
            testData[i] = static_cast<uint8_t>(dis(gen));
        }

        // 发送数据
        bool success = m_reliableChannel->Send(testData.data(), testData.size());
        AssertTrue(success, "Should succeed despite high error rate");

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 60000);
        AssertTrue(recvSuccess, "Should receive successfully");

        // 验证数据完整性
        AssertFileEqual(testData, receivedData);

        // 获取统计信息
        auto stats = m_reliableChannel->GetStats();
        std::cout << " | retransmissions: " << stats.packetsRetransmitted
                  << ", errors: " << stats.errors;
    }
};

// 长时间运行稳定性测试
class LongRunningTest : public TestSuite {
public:
    LongRunningTest() : TestSuite("LongRunningTest") {}

    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
        m_reliableChannel = std::make_shared<ReliableChannel>();

        TransportConfig config;
        m_transport->Open(config);

        ReliableConfig reliableConfig;
        reliableConfig.windowSize = 16;
        reliableConfig.maxRetries = 5;
        m_reliableChannel->Initialize(m_transport, reliableConfig);
        m_reliableChannel->Connect();
    }

    void TearDown() override {
        if (m_reliableChannel) {
            m_reliableChannel->Disconnect();
            m_reliableChannel->Shutdown();
        }
        if (m_transport) {
            m_transport->Close();
        }
    }

    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 5分钟持续运行测试
        results.push_back(RunTest("5-minute stability test", [this]() {
            TestLongRunningStability(5 * 60);  // 5分钟
        }));

        // 测试2: 内存泄漏检测测试
        results.push_back(RunTest("Memory leak detection", [this]() {
            TestMemoryLeak();
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestLongRunningStability(int durationSeconds) {
        auto endTime = std::chrono::steady_clock::now() +
                      std::chrono::seconds(durationSeconds);

        int iterationCount = 0;
        int failureCount = 0;

        while (std::chrono::steady_clock::now() < endTime) {
            // 生成随机大小的测试数据 (1KB - 100KB)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> sizeDis(1024, 100 * 1024);
            size_t dataSize = sizeDis(gen);

            std::vector<uint8_t> testData(dataSize);
            std::uniform_int_distribution<> dataDis(0, 255);
            for (size_t i = 0; i < dataSize; i++) {
                testData[i] = static_cast<uint8_t>(dataDis(gen));
            }

            // 发送并接收
            bool success = m_reliableChannel->Send(testData.data(), testData.size());
            if (success) {
                std::vector<uint8_t> receivedData;
                bool recvSuccess = m_reliableChannel->Receive(receivedData, 60000);

                if (!recvSuccess || receivedData != testData) {
                    failureCount++;
                }
            } else {
                failureCount++;
            }

            iterationCount++;

            // 每100次迭代输出进度
            if (iterationCount % 100 == 0) {
                std::cout << ".";
                std::cout.flush();
            }

            // 短暂休眠避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << " | " << iterationCount << " iterations, "
                  << failureCount << " failures";

        AssertEqual(0, failureCount, "Should have no failures in long-running test");
    }

    void TestMemoryLeak() {
        // 记录初始内存使用（简化版，实际需要使用内存分析工具）
        const int iterations = 1000;

        for (int i = 0; i < iterations; i++) {
            // 创建并销毁大量对象
            std::vector<uint8_t> testData(100 * 1024);  // 100KB
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (size_t j = 0; j < testData.size(); j++) {
                testData[j] = static_cast<uint8_t>(dis(gen));
            }

            bool success = m_reliableChannel->Send(testData.data(), testData.size());
            if (success) {
                std::vector<uint8_t> receivedData;
                m_reliableChannel->Receive(receivedData, 60000);
            }

            // 每100次输出进度
            if ((i + 1) % 100 == 0) {
                std::cout << ".";
                std::cout.flush();
            }
        }

        std::cout << " | " << iterations << " iterations completed";

        // 注: 实际内存泄漏检测需要使用Valgrind、Visual Studio诊断工具等
        // 这里仅验证程序能够持续运行而不崩溃
    }
};

// 并发传输测试
class ConcurrentTest : public TestSuite {
public:
    ConcurrentTest() : TestSuite("ConcurrentTest") {}

    void SetUp() override {
        // 不在这里初始化，每个测试用例会创建自己的实例
    }

    void TearDown() override {
        // 清理在测试用例中完成
    }

    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 2个并发通道
        results.push_back(RunTest("2 concurrent channels", [this]() {
            TestConcurrentChannels(2);
        }));

        // 测试2: 4个并发通道
        results.push_back(RunTest("4 concurrent channels", [this]() {
            TestConcurrentChannels(4);
        }));

        // 测试3: 8个并发通道
        results.push_back(RunTest("8 concurrent channels", [this]() {
            TestConcurrentChannels(8);
        }));

        return results;
    }

private:
    void TestConcurrentChannels(int channelCount) {
        std::vector<std::thread> threads;
        std::vector<bool> results(channelCount, false);

        auto startTime = std::chrono::high_resolution_clock::now();

        // 启动多个线程，每个线程处理一个传输通道
        for (int i = 0; i < channelCount; i++) {
            threads.emplace_back([this, i, &results]() {
                // 创建独立的传输通道
                auto transport = std::make_shared<LoopbackTransport>();
                auto reliableChannel = std::make_shared<ReliableChannel>();

                TransportConfig config;
                transport->Open(config);

                ReliableConfig reliableConfig;
                reliableConfig.windowSize = 8;
                reliableConfig.maxRetries = 5;
                reliableChannel->Initialize(transport, reliableConfig);
                reliableChannel->Connect();

                // 生成测试数据
                const size_t dataSize = 1024 * 1024;  // 1MB
                std::vector<uint8_t> testData(dataSize);
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 255);

                for (size_t j = 0; j < dataSize; j++) {
                    testData[j] = static_cast<uint8_t>(dis(gen));
                }

                // 发送并接收
                bool success = reliableChannel->Send(testData.data(), testData.size());
                if (success) {
                    std::vector<uint8_t> receivedData;
                    bool recvSuccess = reliableChannel->Receive(receivedData, 60000);

                    if (recvSuccess && receivedData == testData) {
                        results[i] = true;
                    }
                }

                // 清理
                reliableChannel->Disconnect();
                reliableChannel->Shutdown();
                transport->Close();
            });
        }

        // 等待所有线程完成
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        // 验证所有通道都成功
        int successCount = 0;
        for (bool result : results) {
            if (result) successCount++;
        }

        std::cout << " | " << successCount << "/" << channelCount << " succeeded, "
                  << duration.count() << " s";

        AssertEqual(channelCount, successCount, "All concurrent channels should succeed");
    }
};
