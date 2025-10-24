#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include <random>
#include <chrono>

// 吞吐量测试
class ThroughputTest : public TestSuite {
public:
    ThroughputTest() : TestSuite("ThroughputTest") {}

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

        // 测试1: 小文件吞吐量 (100KB)
        results.push_back(RunTest("Small file throughput (100KB)", [this]() {
            TestThroughput(100 * 1024);
        }));

        // 测试2: 中等文件吞吐量 (1MB)
        results.push_back(RunTest("Medium file throughput (1MB)", [this]() {
            TestThroughput(1024 * 1024);
        }));

        // 测试3: 大文件吞吐量 (10MB)
        results.push_back(RunTest("Large file throughput (10MB)", [this]() {
            TestThroughput(10 * 1024 * 1024);
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestThroughput(size_t dataSize) {
        // 生成测试数据
        std::vector<uint8_t> testData(dataSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < dataSize; i++) {
            testData[i] = static_cast<uint8_t>(dis(gen));
        }

        // 记录开始时间
        auto startTime = std::chrono::high_resolution_clock::now();

        // 发送数据
        bool success = m_reliableChannel->Send(testData);
        AssertTrue(success, "Data transmission should succeed");

        // 【P0修复】Send()是异步的，需要给SendThread时间处理队列
        // 延迟时间基于数据大小：每MB数据等待200ms，最少200ms（更保守的策略）
        uint32_t delayMs = static_cast<uint32_t>((dataSize / (1024 * 1024)) * 200);
        delayMs = (delayMs < 200) ? 200 : delayMs;  // 避免Windows max宏冲突
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 60000);  // 60秒超时
        AssertTrue(recvSuccess, "Data reception should succeed");

        // 记录结束时间
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // 验证数据完整性
        AssertFileEqual(testData, receivedData);

        // 计算吞吐量
        double seconds = duration.count() / 1000.0;
        double throughputMBps = (dataSize / (1024.0 * 1024.0)) / seconds;

        std::cout << " | " << throughputMBps << " MB/s, "
                  << duration.count() << " ms";
    }
};

// 窗口大小影响测试
class WindowSizeImpactTest : public TestSuite {
public:
    WindowSizeImpactTest() : TestSuite("WindowSizeImpactTest") {}

    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
    }

    void TearDown() override {
        if (m_transport) {
            m_transport->Close();
        }
    }

    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试不同窗口大小
        std::vector<int> windowSizes = {1, 4, 8, 16, 32};

        for (int windowSize : windowSizes) {
            std::string testName = "Window size " + std::to_string(windowSize);
            results.push_back(RunTest(testName, [this, windowSize]() {
                TestWindowSize(windowSize);
            }));
        }

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;

    void TestWindowSize(int windowSize) {
        // 创建ReliableChannel
        auto reliableChannel = std::make_shared<ReliableChannel>();

        // 配置窗口大小
        TransportConfig config;
        m_transport->Open(config);

        ReliableConfig reliableConfig;
        reliableConfig.windowSize = windowSize;
        reliableConfig.maxRetries = 5;
        reliableChannel->Initialize(m_transport, reliableConfig);
        reliableChannel->Connect();

        // 生成测试数据 (1MB)
        const size_t dataSize = 1024 * 1024;
        std::vector<uint8_t> testData(dataSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < dataSize; i++) {
            testData[i] = static_cast<uint8_t>(dis(gen));
        }

        // 记录开始时间
        auto startTime = std::chrono::high_resolution_clock::now();

        // 发送数据
        bool success = reliableChannel->Send(testData);
        AssertTrue(success, "Should send successfully");

        // 【P0修复】Send()是异步的，需要给SendThread时间处理队列
        // 延迟时间基于数据大小和窗口大小：每MB等待200ms，大窗口加倍
        uint32_t delayMs = 200;  // 1MB数据基础延迟200ms（更保守）
        if (windowSize > 8) {
            delayMs *= 2;  // 大窗口需要更多时间等待ACK
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = reliableChannel->Receive(receivedData, 60000);  // 60秒超时
        AssertTrue(recvSuccess, "Should receive successfully");

        // 记录结束时间
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // 验证数据
        AssertFileEqual(testData, receivedData);

        // 输出性能指标
        double seconds = duration.count() / 1000.0;
        double throughputMBps = (dataSize / (1024.0 * 1024.0)) / seconds;

        std::cout << " | " << throughputMBps << " MB/s, "
                  << duration.count() << " ms";

        // 清理
        reliableChannel->Disconnect();
        reliableChannel->Shutdown();
    }
};

// 延迟测试
class LatencyTest : public TestSuite {
public:
    LatencyTest() : TestSuite("LatencyTest") {}

    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
        m_reliableChannel = std::make_shared<ReliableChannel>();

        TransportConfig config;
        m_transport->Open(config);

        ReliableConfig reliableConfig;
        reliableConfig.windowSize = 1;  // 使用小窗口测试延迟
        reliableConfig.maxRetries = 3;
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

        // 测试1: 零延迟环境
        results.push_back(RunTest("Zero latency", [this]() {
            TestLatency(0);
        }));

        // 测试2: 10ms延迟
        results.push_back(RunTest("10ms latency", [this]() {
            TestLatency(10);
        }));

        // 测试3: 50ms延迟
        results.push_back(RunTest("50ms latency", [this]() {
            TestLatency(50);
        }));

        // 测试4: 100ms延迟
        results.push_back(RunTest("100ms latency", [this]() {
            TestLatency(100);
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestLatency(int latency_ms) {
        // 配置延迟（通过LoopbackConfig）
        LoopbackConfig loopConfig = m_transport->GetLoopbackConfig();
        loopConfig.delayMs = latency_ms;
        m_transport->SetLoopbackConfig(loopConfig);

        // 发送小数据包以测试延迟
        const size_t dataSize = 1024;  // 1KB
        std::vector<uint8_t> testData(dataSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < dataSize; i++) {
            testData[i] = static_cast<uint8_t>(dis(gen));
        }

        // 记录开始时间
        auto startTime = std::chrono::high_resolution_clock::now();

        // 发送数据
        bool success = m_reliableChannel->Send(testData);
        AssertTrue(success, "Should send successfully");

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 30000);  // 30秒超时
        AssertTrue(recvSuccess, "Should receive successfully");

        // 记录结束时间
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // 验证数据
        AssertFileEqual(testData, receivedData);

        // 输出往返时间
        std::cout << " | RTT: " << duration.count() << " ms";

        // 验证延迟在预期范围内（允许一定误差）
        if (latency_ms > 0) {
            int expectedMinRTT = latency_ms * 2;  // 往返延迟至少是单向延迟的2倍
            AssertGreater(static_cast<int>(duration.count()), expectedMinRTT - 10,
                         "RTT should be at least 2x latency");
        }
    }
};
