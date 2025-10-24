#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include <random>

// 辅助函数：生成测试数据
inline std::vector<uint8_t> GenerateTestData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<uint8_t>(dis(gen));
    }

    return data;
}

// 丢包恢复测试
class PacketLossTest : public TestSuite {
public:
    PacketLossTest() : TestSuite("PacketLossTest") {}

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

        // 测试1: 低丢包率 (5%)
        results.push_back(RunTest("Low packet loss (5%)", [this]() {
            TestPacketLoss(0.05);
        }));

        // 测试2: 中等丢包率 (10%)
        results.push_back(RunTest("Medium packet loss (10%)", [this]() {
            TestPacketLoss(0.10);
        }));

        // 测试3: 高丢包率 (20%)
        results.push_back(RunTest("High packet loss (20%)", [this]() {
            TestPacketLoss(0.20);
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestPacketLoss(double lossRate) {
        // 配置丢包率（0-100%）
        m_transport->SetPacketLossRate(static_cast<uint32_t>(lossRate * 100));

        // 生成测试数据
        auto testData = GenerateTestData(512 * 1024);  // 512KB

        // 发送数据
        bool sendSuccess = m_reliableChannel->Send(testData);

        AssertTrue(sendSuccess, "Send should succeed despite packet loss");

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 30000);  // 30秒超时

        AssertTrue(recvSuccess, "Receive should succeed");

        // 验证数据完整性
        AssertFileEqual(testData, receivedData);

        // 获取统计信息
        auto stats = m_reliableChannel->GetStats();

        std::cout << " | retransmissions: " << stats.packetsRetransmitted;
    }
};

// 超时重传测试
class TimeoutTest : public TestSuite {
public:
    TimeoutTest() : TestSuite("TimeoutTest") {}

    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
        m_reliableChannel = std::make_shared<ReliableChannel>();

        TransportConfig config;
        m_transport->Open(config);
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

        // 测试1: 短超时配置
        results.push_back(RunTest("Short timeout with delay", [this]() {
            TestTimeout(1000, 2000);  // 1秒超时，2秒延迟
        }));

        // 测试2: 动态超时调整
        results.push_back(RunTest("Dynamic timeout adjustment", [this]() {
            TestDynamicTimeout();
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestTimeout(int timeout_ms, int delay_ms) {
        // 配置超时
        ReliableConfig config;
        config.timeoutBase = timeout_ms;
        config.windowSize = 8;
        config.maxRetries = 5;

        m_reliableChannel->Initialize(m_transport, config);
        m_reliableChannel->Connect();

        // 配置延迟（通过LoopbackConfig）
        LoopbackConfig loopConfig = m_transport->GetLoopbackConfig();
        loopConfig.delayMs = delay_ms;
        m_transport->SetLoopbackConfig(loopConfig);

        // 发送测试数据
        auto testData = GenerateTestData(100 * 1024);  // 100KB
        bool success = m_reliableChannel->Send(testData);

        AssertTrue(success, "Should retransmit on timeout");

        // 验证有重传发生
        auto stats = m_reliableChannel->GetStats();
        AssertGreater(static_cast<int>(stats.packetsRetransmitted), 0,
                     "Should have retransmitted packets");

        std::cout << " | retransmissions: " << stats.packetsRetransmitted;
    }

    void TestDynamicTimeout() {
        // 使用动态超时配置
        ReliableConfig config;
        config.timeoutBase = 500;  // 初始500ms
        config.windowSize = 16;

        m_reliableChannel->Initialize(m_transport, config);
        m_reliableChannel->Connect();

        // 逐渐增加延迟，测试超时调整
        std::vector<int> delays = {100, 500, 1000, 1500};

        for (int delay : delays) {
            LoopbackConfig loopConfig = m_transport->GetLoopbackConfig();
            loopConfig.delayMs = delay;
            m_transport->SetLoopbackConfig(loopConfig);

            auto testData = GenerateTestData(50 * 1024);  // 50KB

            bool success = m_reliableChannel->Send(testData);
            AssertTrue(success, "Should adapt to changing delays");
        }

        std::cout << " | adaptive timeout worked";
    }
};

// CRC校验失败恢复测试
class CRCFailureTest : public TestSuite {
public:
    CRCFailureTest() : TestSuite("CRCFailureTest") {}

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

        // 测试1: 数据损坏恢复
        results.push_back(RunTest("CRC failure recovery", [this]() {
            TestCRCFailure();
        }));

        return results;
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    void TestCRCFailure() {
        // 配置错误率（模拟数据损坏）
        m_transport->SetErrorRate(5);  // 5%错误率

        // 发送测试数据
        auto testData = GenerateTestData(256 * 1024);  // 256KB
        bool success = m_reliableChannel->Send(testData);

        AssertTrue(success, "Should recover from CRC failures");

        // 接收数据
        std::vector<uint8_t> receivedData;
        bool recvSuccess = m_reliableChannel->Receive(receivedData, 30000);  // 30秒超时

        AssertTrue(recvSuccess, "Should receive successfully");
        AssertFileEqual(testData, receivedData);

        // 获取统计信息
        auto stats = m_reliableChannel->GetStats();
        std::cout << " | errors: " << stats.errors
                  << ", retransmissions: " << stats.packetsRetransmitted;
    }
};
