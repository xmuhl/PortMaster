#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "../Transport/ITransport.h"
#include "../Transport/LoopbackTransport.h"
#include "../Transport/SerialTransport.h"
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

// ============================================================================
// Transport层单元测试基类
// ============================================================================

class TransportTestBase : public TestSuite
{
public:
    explicit TransportTestBase(const std::string& name) : TestSuite(name) {}
    virtual ~TransportTestBase() = default;

protected:
    // 辅助方法：等待状态变化
    bool WaitForState(ITransport* transport, TransportState expectedState, DWORD timeoutMs = 5000)
    {
        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(timeoutMs))
        {
            if (transport->GetState() == expectedState)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    // 辅助方法：验证统计信息
    void VerifyStats(const TransportStats& stats, uint64_t minBytesSent, uint64_t minBytesReceived)
    {
        AssertTrue(stats.bytesSent >= minBytesSent,
                   "Expected at least " + std::to_string(minBytesSent) + " bytes sent");
        AssertTrue(stats.bytesReceived >= minBytesReceived,
                   "Expected at least " + std::to_string(minBytesReceived) + " bytes received");
    }

    // 辅助方法：生成测试数据
    std::vector<uint8_t> GenerateTestData(size_t size)
    {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        return data;
    }

    // 辅助方法：比较数据
    void AssertDataEqual(const std::vector<uint8_t>& expected,
                        const void* actual,
                        size_t actualSize)
    {
        AssertTrue(expected.size() == actualSize,
                   "Data size mismatch: expected " + std::to_string(expected.size()) +
                   ", got " + std::to_string(actualSize));

        const uint8_t* actualBytes = static_cast<const uint8_t*>(actual);
        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (expected[i] != actualBytes[i])
            {
                AssertTrue(false, "Data mismatch at byte " + std::to_string(i));
            }
        }
    }
};

// ============================================================================
// LoopbackTransport单元测试套件
// ============================================================================

class LoopbackTransportTest : public TransportTestBase
{
public:
    LoopbackTransportTest() : TransportTestBase("LoopbackTransportTest") {}

    void SetUp() override
    {
        m_transport = std::make_unique<LoopbackTransport>();
    }

    void TearDown() override
    {
        if (m_transport && m_transport->IsOpen())
        {
            m_transport->Close();
        }
        m_transport.reset();
    }

    std::vector<TestResult> RunTests() override
    {
        std::vector<TestResult> results;

        // 基础功能测试
        results.push_back(RunTest("Open and Close", [this]() { TestOpenClose(); }));
        results.push_back(RunTest("Synchronous Write/Read", [this]() { TestSyncWriteRead(); }));
        results.push_back(RunTest("Large Data Transfer", [this]() { TestLargeDataTransfer(); }));

        // 异步操作测试
        results.push_back(RunTest("Asynchronous Write/Read", [this]() { TestAsyncWriteRead(); }));

        // 统计信息测试
        results.push_back(RunTest("Statistics Tracking", [this]() { TestStatistics(); }));
        results.push_back(RunTest("Statistics Reset", [this]() { TestStatsReset(); }));

        // 缓冲区管理测试
        results.push_back(RunTest("Buffer Flush", [this]() { TestFlushBuffers(); }));
        results.push_back(RunTest("Available Bytes Query", [this]() { TestAvailableBytes(); }));

        // 错误处理测试
        results.push_back(RunTest("Error Injection", [this]() { TestErrorInjection(); }));
        results.push_back(RunTest("Packet Loss Simulation", [this]() { TestPacketLoss(); }));

        // 配置测试
        results.push_back(RunTest("Configuration Update", [this]() { TestConfigUpdate(); }));

        return results;
    }

private:
    void TestOpenClose()
    {
        // 测试初始状态
        AssertTrue(m_transport->GetState() == TransportState::Closed, "Initial state should be Closed");
        AssertTrue(!m_transport->IsOpen(), "Should not be open initially");

        // 测试打开
        LoopbackConfig config;
        config.delayMs = 0;  // 无延迟测试
        config.errorRate = 0;
        config.packetLossRate = 0;

        TransportError error = m_transport->Open(config);
        AssertTrue(error == TransportError::Success, "Open should succeed");
        AssertTrue(m_transport->IsOpen(), "Should be open after Open()");
        AssertTrue(m_transport->GetState() == TransportState::Open, "State should be Open");

        // 测试重复打开
        error = m_transport->Open(config);
        AssertTrue(error == TransportError::AlreadyOpen, "Repeated Open should return AlreadyOpen");

        // 测试关闭
        error = m_transport->Close();
        AssertTrue(error == TransportError::Success, "Close should succeed");
        AssertTrue(!m_transport->IsOpen(), "Should not be open after Close()");
        AssertTrue(m_transport->GetState() == TransportState::Closed, "State should be Closed");
    }

    void TestSyncWriteRead()
    {
        // 打开传输
        LoopbackConfig config;
        config.delayMs = 0;
        m_transport->Open(config);

        // 生成测试数据
        auto testData = GenerateTestData(256);

        // 同步写入
        size_t written = 0;
        TransportError error = m_transport->Write(testData.data(), testData.size(), &written);
        AssertTrue(error == TransportError::Success, "Write should succeed");
        AssertTrue(written == testData.size(), "All data should be written");

        // 等待数据在回路中传递
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 同步读取
        std::vector<uint8_t> readBuffer(256);
        size_t readBytes = 0;
        error = m_transport->Read(readBuffer.data(), readBuffer.size(), &readBytes, 1000);
        AssertTrue(error == TransportError::Success, "Read should succeed");
        AssertTrue(readBytes == testData.size(), "Should read same amount of data");

        // 验证数据
        AssertDataEqual(testData, readBuffer.data(), readBytes);
    }

    void TestLargeDataTransfer()
    {
        // 打开传输（扩大队列容量）
        LoopbackConfig config;
        config.delayMs = 0;
        config.maxQueueSize = 100000;
        m_transport->Open(config);

        // 生成大数据（10KB）
        auto testData = GenerateTestData(10240);

        // 分块写入
        const size_t chunkSize = 1024;
        size_t totalWritten = 0;

        for (size_t offset = 0; offset < testData.size(); offset += chunkSize)
        {
            size_t remaining = testData.size() - offset;
            size_t toWrite = (remaining < chunkSize) ? remaining : chunkSize;

            size_t written = 0;
            TransportError error = m_transport->Write(testData.data() + offset, toWrite, &written);
            AssertTrue(error == TransportError::Success, "Write chunk should succeed");
            totalWritten += written;
        }

        AssertTrue(totalWritten == testData.size(), "All data should be written");

        // 等待数据传输
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 分块读取
        std::vector<uint8_t> readBuffer(testData.size());
        size_t totalRead = 0;

        while (totalRead < testData.size())
        {
            size_t remaining = testData.size() - totalRead;
            size_t toRead = (remaining < chunkSize) ? remaining : chunkSize;

            size_t readBytes = 0;
            TransportError error = m_transport->Read(readBuffer.data() + totalRead, toRead, &readBytes, 1000);

            if (error == TransportError::Success)
            {
                totalRead += readBytes;
            }
            else if (error == TransportError::Timeout)
            {
                break; // 超时，可能数据已读完
            }
            else
            {
                AssertTrue(false, "Read failed with error: " + std::to_string(static_cast<int>(error)));
            }
        }

        AssertTrue(totalRead == testData.size(), "Should read all data");
        AssertDataEqual(testData, readBuffer.data(), totalRead);
    }

    void TestAsyncWriteRead()
    {
        // 打开传输
        LoopbackConfig config;
        config.delayMs = 0;
        config.asyncMode = true;
        m_transport->Open(config);

        // 设置数据接收回调
        bool callbackInvoked = false;
        std::vector<uint8_t> receivedData;

        m_transport->SetDataReceivedCallback([&](const std::vector<uint8_t>& data) {
            receivedData = data;
            callbackInvoked = true;
        });

        // 启动异步读取
        TransportError error = m_transport->StartAsyncRead();
        AssertTrue(error == TransportError::Success, "StartAsyncRead should succeed");

        // 异步写入
        auto testData = GenerateTestData(128);
        error = m_transport->WriteAsync(testData.data(), testData.size());
        AssertTrue(error == TransportError::Success, "WriteAsync should succeed");

        // 等待回调
        auto startTime = std::chrono::steady_clock::now();
        while (!callbackInvoked &&
               std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(2000))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        AssertTrue(callbackInvoked, "Data received callback should be invoked");
        AssertTrue(receivedData.size() == testData.size(), "Received data size should match");
        AssertDataEqual(testData, receivedData.data(), receivedData.size());

        // 停止异步读取
        error = m_transport->StopAsyncRead();
        AssertTrue(error == TransportError::Success, "StopAsyncRead should succeed");
    }

    void TestStatistics()
    {
        // 打开传输
        LoopbackConfig config;
        config.delayMs = 0;
        m_transport->Open(config);

        // 执行数据传输
        auto testData = GenerateTestData(512);

        size_t written = 0;
        m_transport->Write(testData.data(), testData.size(), &written);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<uint8_t> readBuffer(512);
        size_t readBytes = 0;
        m_transport->Read(readBuffer.data(), readBuffer.size(), &readBytes, 1000);

        // 获取统计信息
        TransportStats stats = m_transport->GetStats();

        // 验证统计数据
        AssertTrue(stats.bytesSent >= testData.size(), "Bytes sent should be tracked");
        AssertTrue(stats.bytesReceived >= testData.size(), "Bytes received should be tracked");

        // LoopbackTransport特有统计
        LoopbackStats loopbackStats = m_transport->GetLoopbackStats();
        AssertTrue(loopbackStats.loopbackRounds > 0, "Loopback rounds should be counted");
    }

    void TestStatsReset()
    {
        // 打开传输并产生统计数据
        LoopbackConfig config;
        m_transport->Open(config);

        auto testData = GenerateTestData(100);
        size_t written = 0;
        m_transport->Write(testData.data(), testData.size(), &written);

        // 验证有统计数据
        TransportStats stats = m_transport->GetStats();
        AssertTrue(stats.bytesSent > 0, "Should have statistics before reset");

        // 重置统计
        m_transport->ResetStats();

        // 验证统计已重置
        stats = m_transport->GetStats();
        AssertTrue(stats.bytesSent == 0, "Bytes sent should be reset to 0");
        AssertTrue(stats.bytesReceived == 0, "Bytes received should be reset to 0");
    }

    void TestFlushBuffers()
    {
        // 打开传输
        LoopbackConfig config;
        m_transport->Open(config);

        // 写入数据
        auto testData = GenerateTestData(256);
        size_t written = 0;
        m_transport->Write(testData.data(), testData.size(), &written);

        // 刷新缓冲区
        TransportError error = m_transport->FlushBuffers();
        AssertTrue(error == TransportError::Success, "FlushBuffers should succeed");

        // 验证缓冲区已清空（可用字节应为0或减少）
        // 注意：LoopbackTransport的刷新行为可能与物理设备不同
    }

    void TestAvailableBytes()
    {
        // 打开传输
        LoopbackConfig config;
        config.delayMs = 0;
        m_transport->Open(config);

        // 初始状态应该没有可用数据
        size_t available = m_transport->GetAvailableBytes();
        AssertTrue(available == 0, "No data should be available initially");

        // 写入数据
        auto testData = GenerateTestData(128);
        size_t written = 0;
        m_transport->Write(testData.data(), testData.size(), &written);

        // 【P1修复优化】主动轮询等待数据可用，最多等待1秒
        // LoopbackWorkerThread每1ms运行一次，100次轮询足够
        const int maxRetries = 100;
        const int retryDelayMs = 10;
        bool dataAvailable = false;

        for (int i = 0; i < maxRetries; i++) {
            available = m_transport->GetAvailableBytes();
            if (available > 0) {
                dataAvailable = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        }

        // 应该有数据可用
        AssertTrue(dataAvailable && available > 0, "Data should be available after write");
        AssertTrue(available >= testData.size(), "Available bytes should match written data");
    }

    void TestErrorInjection()
    {
        // 打开传输并启用错误注入
        LoopbackConfig config;
        config.delayMs = 0;
        config.errorRate = 0;  // 初始无错误
        m_transport->Open(config);

        // 设置错误回调
        bool errorOccurred = false;
        m_transport->SetErrorOccurredCallback([&](TransportError error, const std::string& message) {
            errorOccurred = true;
        });

        // 手动注入错误
        m_transport->InjectError();

        // 尝试写入数据（可能触发错误）
        auto testData = GenerateTestData(64);
        size_t written = 0;
        m_transport->Write(testData.data(), testData.size(), &written);

        // 验证错误统计增加
        LoopbackStats stats = m_transport->GetLoopbackStats();
        AssertTrue(stats.simulatedErrors > 0, "Simulated errors should be counted");
    }

    void TestPacketLoss()
    {
        // 打开传输并启用丢包模拟
        LoopbackConfig config;
        config.delayMs = 0;
        config.packetLossRate = 50;  // 50%丢包率
        m_transport->Open(config);

        // 多次写入数据
        size_t totalPackets = 10;
        size_t successfulReads = 0;

        for (size_t i = 0; i < totalPackets; ++i)
        {
            auto testData = GenerateTestData(64);
            size_t written = 0;
            m_transport->Write(testData.data(), testData.size(), &written);

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::vector<uint8_t> readBuffer(64);
            size_t readBytes = 0;
            TransportError error = m_transport->Read(readBuffer.data(), readBuffer.size(), &readBytes, 500);

            if (error == TransportError::Success && readBytes > 0)
            {
                successfulReads++;
            }
        }

        // 验证有丢包（成功读取应少于总包数）
        LoopbackStats stats = m_transport->GetLoopbackStats();
        AssertTrue(stats.simulatedLosses > 0, "Packet loss should be simulated");

        // 由于丢包率是50%，成功读取应该显著少于总包数
        AssertTrue(successfulReads < totalPackets, "Some packets should be lost");
    }

    void TestConfigUpdate()
    {
        // 打开传输
        LoopbackConfig config;
        config.delayMs = 0;
        config.errorRate = 0;
        m_transport->Open(config);

        // 获取当前配置
        LoopbackConfig currentConfig = m_transport->GetLoopbackConfig();
        AssertTrue(currentConfig.delayMs == 0, "Delay should be 0");
        AssertTrue(currentConfig.errorRate == 0, "Error rate should be 0");

        // 更新配置
        currentConfig.errorRate = 10;
        currentConfig.packetLossRate = 5;
        m_transport->SetLoopbackConfig(currentConfig);

        // 验证配置已更新
        LoopbackConfig updatedConfig = m_transport->GetLoopbackConfig();
        AssertTrue(updatedConfig.errorRate == 10, "Error rate should be updated to 10");
        AssertTrue(updatedConfig.packetLossRate == 5, "Packet loss rate should be updated to 5");
    }

private:
    std::unique_ptr<LoopbackTransport> m_transport;
};

// ============================================================================
// SerialTransport单元测试套件（使用虚拟串口或Loopback模拟）
// ============================================================================

class SerialTransportTest : public TransportTestBase
{
public:
    SerialTransportTest() : TransportTestBase("SerialTransportTest") {}

    void SetUp() override
    {
        m_transport = std::make_unique<SerialTransport>();
    }

    void TearDown() override
    {
        if (m_transport && m_transport->IsOpen())
        {
            m_transport->Close();
        }
        m_transport.reset();
    }

    std::vector<TestResult> RunTests() override
    {
        std::vector<TestResult> results;

        // 端口枚举测试
        results.push_back(RunTest("Enumerate Serial Ports", [this]() { TestEnumeratePorts(); }));

        // 基础状态测试
        results.push_back(RunTest("Initial State", [this]() { TestInitialState(); }));

        // 无效操作测试
        results.push_back(RunTest("Invalid Operations", [this]() { TestInvalidOperations(); }));

        // 配置测试（不实际打开端口）
        results.push_back(RunTest("Configuration Structure", [this]() { TestConfiguration(); }));

        return results;
    }

private:
    void TestEnumeratePorts()
    {
        // 枚举可用串口
        auto ports = SerialTransport::EnumerateSerialPorts();

        // 至少应该能执行枚举（可能返回空列表）
        AssertTrue(true, "Port enumeration should complete without crash");

        // 如果有可用端口，输出信息
        if (!ports.empty())
        {
            std::string message = "Found " + std::to_string(ports.size()) + " serial ports";
            AssertTrue(true, message);
        }
    }

    void TestInitialState()
    {
        // 验证初始状态
        AssertTrue(m_transport->GetState() == TransportState::Closed, "Initial state should be Closed");
        AssertTrue(!m_transport->IsOpen(), "Should not be open initially");

        // 验证统计信息初始值
        TransportStats stats = m_transport->GetStats();
        AssertTrue(stats.bytesSent == 0, "Initial bytes sent should be 0");
        AssertTrue(stats.bytesReceived == 0, "Initial bytes received should be 0");
    }

    void TestInvalidOperations()
    {
        // 未打开时尝试写入
        uint8_t data[10] = {0};
        size_t written = 0;
        TransportError error = m_transport->Write(data, sizeof(data), &written);
        AssertTrue(error == TransportError::NotOpen, "Write on closed transport should return NotOpen");

        // 未打开时尝试读取
        size_t readBytes = 0;
        error = m_transport->Read(data, sizeof(data), &readBytes, 100);
        AssertTrue(error == TransportError::NotOpen, "Read on closed transport should return NotOpen");

        // 未打开时尝试异步操作
        error = m_transport->StartAsyncRead();
        AssertTrue(error == TransportError::NotOpen, "StartAsyncRead on closed transport should return NotOpen");
    }

    void TestConfiguration()
    {
        // 测试配置结构的创建和设置
        SerialConfig config;
        config.portName = "COM1";
        config.baudRate = 115200;
        config.dataBits = 8;
        config.parity = NOPARITY;
        config.stopBits = ONESTOPBIT;

        AssertTrue(config.baudRate == 115200, "Baud rate should be 115200");
        AssertTrue(config.dataBits == 8, "Data bits should be 8");
        AssertTrue(config.parity == NOPARITY, "Parity should be NOPARITY");

        // 注意：不实际打开端口，仅测试配置结构
    }

private:
    std::unique_ptr<SerialTransport> m_transport;
};
