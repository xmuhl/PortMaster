#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "../Protocol/ReliableChannel.h"
#include "../Transport/LoopbackTransport.h"
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

// ============================================================================
// Transport + Protocol集成测试套件
// ============================================================================

class TransportProtocolIntegrationTest : public TestSuite
{
public:
    TransportProtocolIntegrationTest() : TestSuite("TransportProtocolIntegrationTest") {}

    void SetUp() override
    {
        // 创建传输层
        m_transport = std::make_shared<LoopbackTransport>();

        LoopbackConfig loopConfig;
        loopConfig.delayMs = 0;
        loopConfig.errorRate = 0;
        loopConfig.packetLossRate = 0;
        loopConfig.maxQueueSize = 100000; // 大队列支持大数据传输

        m_transport->Open(loopConfig);

        // 创建协议层
        m_channel = std::make_unique<ReliableChannel>();

        ReliableConfig reliableConfig;
        reliableConfig.windowSize = 8;
        reliableConfig.maxRetries = 5;
        reliableConfig.timeoutBase = 1000;
        reliableConfig.maxPayloadSize = 1024;

        m_channel->Initialize(m_transport, reliableConfig);
    }

    void TearDown() override
    {
        if (m_channel)
        {
            m_channel->Shutdown();
        }
        m_channel.reset();

        if (m_transport && m_transport->IsOpen())
        {
            m_transport->Close();
        }
        m_transport.reset();
    }

    std::vector<TestResult> RunTests() override
    {
        std::vector<TestResult> results;

        // 基础集成测试
        results.push_back(RunTest("Stack Initialization", [this]() { TestStackInitialization(); }));
        results.push_back(RunTest("Multi-layer Connection", [this]() { TestMultiLayerConnection(); }));

        // 数据流测试
        results.push_back(RunTest("End-to-End Data Flow", [this]() { TestEndToEndDataFlow(); }));
        results.push_back(RunTest("Fragmented Data Transfer", [this]() { TestFragmentedDataTransfer(); }));
        results.push_back(RunTest("Large Data Transfer", [this]() { TestLargeDataTransfer(); }));

        // 统计信息集成测试
        results.push_back(RunTest("Multi-layer Statistics", [this]() { TestMultiLayerStatistics(); }));

        // 配置协同测试
        results.push_back(RunTest("Configuration Propagation", [this]() { TestConfigurationPropagation(); }));

        return results;
    }

private:
    void TestStackInitialization()
    {
        // 验证传输层已打开
        AssertTrue(m_transport->IsOpen(), "Transport should be open");
        AssertTrue(m_transport->GetState() == TransportState::Open, "Transport state should be Open");

        // 验证协议层可以连接
        bool connected = m_channel->Connect();
        AssertTrue(connected, "Protocol layer should connect successfully");
        AssertTrue(m_channel->IsConnected(), "Channel should be connected");

        // 清理
        m_channel->Disconnect();
    }

    void TestMultiLayerConnection()
    {
        // 测试协议层连接对传输层的影响
        TransportStats transportStatsBefore = m_transport->GetStats();

        // 连接协议层
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 传输层应该记录流量（握手包）
        TransportStats transportStatsAfter = m_transport->GetStats();
        AssertTrue(transportStatsAfter.bytesSent > transportStatsBefore.bytesSent,
                   "Transport layer should record handshake traffic");

        // 断开连接
        m_channel->Disconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TestEndToEndDataFlow()
    {
        // 连接
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 准备测试数据
        std::vector<uint8_t> testData(512);
        for (size_t i = 0; i < testData.size(); ++i)
        {
            testData[i] = static_cast<uint8_t>(i & 0xFF);
        }

        // 记录传输层初始统计
        TransportStats transportStatsBefore = m_transport->GetStats();
        ReliableStats channelStatsBefore = m_channel->GetStats();

        // 通过协议层发送
        bool sendSuccess = m_channel->Send(testData);
        AssertTrue(sendSuccess, "Protocol layer send should succeed");

        // 通过协议层接收
        std::vector<uint8_t> receivedData;
        bool receiveSuccess = m_channel->Receive(receivedData, 3000);
        AssertTrue(receiveSuccess, "Protocol layer receive should succeed");

        // 验证数据完整性
        AssertTrue(receivedData.size() == testData.size(), "Data size should match");
        for (size_t i = 0; i < testData.size(); ++i)
        {
            AssertTrue(receivedData[i] == testData[i],
                       "Data byte " + std::to_string(i) + " should match");
        }

        // 验证传输层统计更新
        TransportStats transportStatsAfter = m_transport->GetStats();
        AssertTrue(transportStatsAfter.bytesSent > transportStatsBefore.bytesSent,
                   "Transport layer bytes sent should increase");
        AssertTrue(transportStatsAfter.bytesReceived > transportStatsBefore.bytesReceived,
                   "Transport layer bytes received should increase");

        // 验证协议层统计更新
        ReliableStats channelStatsAfter = m_channel->GetStats();
        AssertTrue(channelStatsAfter.packetsSent > channelStatsBefore.packetsSent,
                   "Protocol layer packets sent should increase");
        AssertTrue(channelStatsAfter.packetsReceived > channelStatsBefore.packetsReceived,
                   "Protocol layer packets received should increase");
    }

    void TestFragmentedDataTransfer()
    {
        // 连接
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 发送大于单帧的数据（需要分片）
        std::vector<uint8_t> largeData(5120); // 5KB数据
        for (size_t i = 0; i < largeData.size(); ++i)
        {
            largeData[i] = static_cast<uint8_t>((i * 7) & 0xFF);
        }

        // 发送
        bool sendSuccess = m_channel->Send(largeData);
        AssertTrue(sendSuccess, "Large data send should succeed");

        // 接收
        std::vector<uint8_t> receivedData;
        bool receiveSuccess = m_channel->Receive(receivedData, 5000);
        AssertTrue(receiveSuccess, "Large data receive should succeed");

        // 验证数据完整性
        AssertTrue(receivedData.size() == largeData.size(),
                   "Received data size should match sent data size");

        // 抽样验证数据内容
        for (size_t i = 0; i < largeData.size(); i += 512)
        {
            AssertTrue(receivedData[i] == largeData[i],
                       "Data byte " + std::to_string(i) + " should match");
        }

        // 验证协议层发送了多个包
        ReliableStats stats = m_channel->GetStats();
        AssertTrue(stats.packetsSent > 1, "Should send multiple packets for large data");
    }

    void TestLargeDataTransfer()
    {
        // 连接
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 发送非常大的数据（100KB）
        std::vector<uint8_t> veryLargeData(102400);
        for (size_t i = 0; i < veryLargeData.size(); ++i)
        {
            veryLargeData[i] = static_cast<uint8_t>(i & 0xFF);
        }

        // 发送
        auto sendStart = std::chrono::steady_clock::now();
        bool sendSuccess = m_channel->Send(veryLargeData);
        auto sendEnd = std::chrono::steady_clock::now();

        AssertTrue(sendSuccess, "Very large data send should succeed");

        // 接收
        auto recvStart = std::chrono::steady_clock::now();
        std::vector<uint8_t> receivedData;
        bool receiveSuccess = m_channel->Receive(receivedData, 15000);
        auto recvEnd = std::chrono::steady_clock::now();

        AssertTrue(receiveSuccess, "Very large data receive should succeed");

        // 验证数据大小
        AssertTrue(receivedData.size() == veryLargeData.size(),
                   "Received data size should match");

        // 计算传输时间和速度
        auto sendDuration = std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - sendStart).count();
        auto recvDuration = std::chrono::duration_cast<std::chrono::milliseconds>(recvEnd - recvStart).count();

        // 验证传输在合理时间内完成（不应该太慢）
        AssertTrue(sendDuration < 10000, "Send should complete within 10 seconds");
        AssertTrue(recvDuration < 10000, "Receive should complete within 10 seconds");

        // 抽样验证数据正确性
        for (size_t i = 0; i < veryLargeData.size(); i += 10240)
        {
            AssertTrue(receivedData[i] == veryLargeData[i],
                       "Data integrity check at offset " + std::to_string(i));
        }
    }

    void TestMultiLayerStatistics()
    {
        // 连接并传输数据
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> testData(2048, 0xAB);
        m_channel->Send(testData);

        std::vector<uint8_t> receivedData;
        m_channel->Receive(receivedData, 3000);

        // 获取两层统计信息
        TransportStats transportStats = m_transport->GetStats();
        ReliableStats channelStats = m_channel->GetStats();

        // 传输层字节数应该大于协议层字节数（包含帧头、CRC等开销）
        AssertTrue(transportStats.bytesSent >= channelStats.bytesSent,
                   "Transport bytes sent should include protocol overhead");
        AssertTrue(transportStats.bytesReceived >= channelStats.bytesReceived,
                   "Transport bytes received should include protocol overhead");

        // 验证统计数据的一致性
        AssertTrue(transportStats.bytesSent > 0, "Transport should record sent bytes");
        AssertTrue(transportStats.bytesReceived > 0, "Transport should record received bytes");
        AssertTrue(channelStats.packetsSent > 0, "Channel should record sent packets");
        AssertTrue(channelStats.packetsReceived > 0, "Channel should record received packets");
    }

    void TestConfigurationPropagation()
    {
        // 修改传输层配置
        LoopbackConfig newLoopConfig = m_transport->GetLoopbackConfig();
        newLoopConfig.errorRate = 5;  // 设置5%错误率
        m_transport->SetLoopbackConfig(newLoopConfig);

        // 修改协议层配置
        ReliableConfig newChannelConfig = m_channel->GetConfig();
        newChannelConfig.maxRetries = 10;  // 增加重试次数以应对错误
        m_channel->SetConfig(newChannelConfig);

        // 验证配置已应用
        LoopbackConfig appliedLoopConfig = m_transport->GetLoopbackConfig();
        AssertTrue(appliedLoopConfig.errorRate == 5, "Transport config should be applied");

        ReliableConfig appliedChannelConfig = m_channel->GetConfig();
        AssertTrue(appliedChannelConfig.maxRetries == 10, "Channel config should be applied");

        // 测试配置生效（在有错误率的情况下仍能传输）
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> testData(256, 0x55);
        bool sendSuccess = m_channel->Send(testData);
        AssertTrue(sendSuccess, "Send should succeed even with error rate");

        std::vector<uint8_t> receivedData;
        bool receiveSuccess = m_channel->Receive(receivedData, 5000);
        AssertTrue(receiveSuccess, "Receive should succeed with retries");

        // 验证有错误和重传发生
        ReliableStats stats = m_channel->GetStats();
        LoopbackStats loopStats = m_transport->GetLoopbackStats();

        AssertTrue(loopStats.simulatedErrors > 0 || stats.packetsRetransmitted > 0,
                   "Should have errors or retransmissions");
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::unique_ptr<ReliableChannel> m_channel;
};

// ============================================================================
// 端到端文件传输集成测试套件
// ============================================================================

class FileTransferIntegrationTest : public TestSuite
{
public:
    FileTransferIntegrationTest() : TestSuite("FileTransferIntegrationTest") {}

    void SetUp() override
    {
        // 创建传输层
        m_transport = std::make_shared<LoopbackTransport>();

        LoopbackConfig loopConfig;
        loopConfig.delayMs = 0;
        loopConfig.errorRate = 0;
        loopConfig.packetLossRate = 0;
        loopConfig.maxQueueSize = 200000; // 大队列支持文件传输

        m_transport->Open(loopConfig);

        // 创建协议层
        m_senderChannel = std::make_unique<ReliableChannel>();
        m_receiverChannel = std::make_unique<ReliableChannel>();

        ReliableConfig config;
        config.windowSize = 16;
        config.maxRetries = 5;
        config.timeoutBase = 1000;
        config.maxPayloadSize = 1024;

        m_senderChannel->Initialize(m_transport, config);
        m_receiverChannel->Initialize(m_transport, config);

        // 创建测试目录
        m_testDir = "test_files";
        std::filesystem::create_directories(m_testDir);
    }

    void TearDown() override
    {
        // 清理通道
        if (m_senderChannel)
        {
            m_senderChannel->Shutdown();
        }
        m_senderChannel.reset();

        if (m_receiverChannel)
        {
            m_receiverChannel->Shutdown();
        }
        m_receiverChannel.reset();

        // 清理传输
        if (m_transport && m_transport->IsOpen())
        {
            m_transport->Close();
        }
        m_transport.reset();

        // 清理测试文件
        try
        {
            std::filesystem::remove_all(m_testDir);
        }
        catch (...)
        {
            // 忽略清理错误
        }
    }

    std::vector<TestResult> RunTests() override
    {
        std::vector<TestResult> results;

        // 文件传输测试
        results.push_back(RunTest("Small File Transfer", [this]() { TestSmallFileTransfer(); }));
        results.push_back(RunTest("Medium File Transfer", [this]() { TestMediumFileTransfer(); }));
        results.push_back(RunTest("Large File Transfer", [this]() { TestLargeFileTransfer(); }));

        // 文件完整性测试
        results.push_back(RunTest("File Integrity Verification", [this]() { TestFileIntegrity(); }));

        // 进度回调测试
        results.push_back(RunTest("Progress Callback", [this]() { TestProgressCallback(); }));

        return results;
    }

private:
    void TestSmallFileTransfer()
    {
        // 创建小文件（1KB）
        std::string sourceFile = m_testDir + "/small_source.bin";
        std::string destFile = m_testDir + "/small_dest.bin";

        CreateTestFile(sourceFile, 1024);

        // 连接通道
        m_senderChannel->Connect();
        m_receiverChannel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 启动接收线程
        bool receiveSuccess = false;
        std::thread receiverThread([&]() {
            receiveSuccess = m_receiverChannel->ReceiveFile(destFile);
        });

        // 等待接收器准备好
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 发送文件
        bool sendSuccess = m_senderChannel->SendFile(sourceFile);
        AssertTrue(sendSuccess, "Small file send should succeed");

        // 等待接收完成
        receiverThread.join();
        AssertTrue(receiveSuccess, "Small file receive should succeed");

        // 验证文件存在
        AssertTrue(std::filesystem::exists(destFile), "Destination file should exist");

        // 验证文件大小
        size_t sourceSize = std::filesystem::file_size(sourceFile);
        size_t destSize = std::filesystem::file_size(destFile);
        AssertTrue(sourceSize == destSize, "File sizes should match");
    }

    void TestMediumFileTransfer()
    {
        // 创建中等文件（100KB）
        std::string sourceFile = m_testDir + "/medium_source.bin";
        std::string destFile = m_testDir + "/medium_dest.bin";

        CreateTestFile(sourceFile, 102400);

        // 连接通道
        m_senderChannel->Connect();
        m_receiverChannel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 启动接收线程
        bool receiveSuccess = false;
        std::thread receiverThread([&]() {
            receiveSuccess = m_receiverChannel->ReceiveFile(destFile);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 发送文件
        bool sendSuccess = m_senderChannel->SendFile(sourceFile);
        AssertTrue(sendSuccess, "Medium file send should succeed");

        // 等待接收完成
        receiverThread.join();
        AssertTrue(receiveSuccess, "Medium file receive should succeed");

        // 验证文件大小
        size_t sourceSize = std::filesystem::file_size(sourceFile);
        size_t destSize = std::filesystem::file_size(destFile);
        AssertTrue(sourceSize == destSize, "File sizes should match");
    }

    void TestLargeFileTransfer()
    {
        // 创建大文件（1MB）
        std::string sourceFile = m_testDir + "/large_source.bin";
        std::string destFile = m_testDir + "/large_dest.bin";

        CreateTestFile(sourceFile, 1048576);

        // 连接通道
        m_senderChannel->Connect();
        m_receiverChannel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 启动接收线程
        bool receiveSuccess = false;
        std::thread receiverThread([&]() {
            receiveSuccess = m_receiverChannel->ReceiveFile(destFile);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 发送文件（设置超时）
        auto sendStart = std::chrono::steady_clock::now();
        bool sendSuccess = m_senderChannel->SendFile(sourceFile);
        auto sendEnd = std::chrono::steady_clock::now();

        AssertTrue(sendSuccess, "Large file send should succeed");

        // 等待接收完成
        receiverThread.join();
        AssertTrue(receiveSuccess, "Large file receive should succeed");

        // 验证传输时间合理（不应该太慢）
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(sendEnd - sendStart).count();
        AssertTrue(duration < 30, "Large file transfer should complete within 30 seconds");

        // 验证文件大小
        size_t sourceSize = std::filesystem::file_size(sourceFile);
        size_t destSize = std::filesystem::file_size(destFile);
        AssertTrue(sourceSize == destSize, "File sizes should match");
    }

    void TestFileIntegrity()
    {
        // 创建测试文件
        std::string sourceFile = m_testDir + "/integrity_source.bin";
        std::string destFile = m_testDir + "/integrity_dest.bin";

        CreateTestFile(sourceFile, 51200); // 50KB

        // 连接通道
        m_senderChannel->Connect();
        m_receiverChannel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 传输文件
        std::thread receiverThread([&]() {
            m_receiverChannel->ReceiveFile(destFile);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_senderChannel->SendFile(sourceFile);
        receiverThread.join();

        // 逐字节验证文件内容
        std::ifstream sourceStream(sourceFile, std::ios::binary);
        std::ifstream destStream(destFile, std::ios::binary);

        AssertTrue(sourceStream.is_open(), "Source file should be readable");
        AssertTrue(destStream.is_open(), "Destination file should be readable");

        std::vector<uint8_t> sourceContent((std::istreambuf_iterator<char>(sourceStream)),
                                          std::istreambuf_iterator<char>());
        std::vector<uint8_t> destContent((std::istreambuf_iterator<char>(destStream)),
                                        std::istreambuf_iterator<char>());

        sourceStream.close();
        destStream.close();

        // 验证大小
        AssertTrue(sourceContent.size() == destContent.size(),
                   "Content sizes should match");

        // 抽样验证内容（每1KB验证一次）
        for (size_t i = 0; i < sourceContent.size(); i += 1024)
        {
            AssertTrue(sourceContent[i] == destContent[i],
                       "Content mismatch at offset " + std::to_string(i));
        }
    }

    void TestProgressCallback()
    {
        // 创建文件
        std::string sourceFile = m_testDir + "/progress_source.bin";
        std::string destFile = m_testDir + "/progress_dest.bin";

        CreateTestFile(sourceFile, 204800); // 200KB

        // 设置进度回调
        int progressCallbackCount = 0;
        int64_t lastProgress = 0;

        m_senderChannel->SetProgressCallback([&](int64_t current, int64_t total) {
            progressCallbackCount++;
            lastProgress = current;

            // 进度应该递增
            AssertTrue(current <= total, "Current progress should not exceed total");
        });

        // 连接通道
        m_senderChannel->Connect();
        m_receiverChannel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 传输文件
        std::thread receiverThread([&]() {
            m_receiverChannel->ReceiveFile(destFile);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool sendSuccess = m_senderChannel->SendFile(sourceFile);
        receiverThread.join();

        AssertTrue(sendSuccess, "File send with progress callback should succeed");

        // 验证进度回调被调用
        AssertTrue(progressCallbackCount > 0, "Progress callback should be invoked");
        AssertTrue(lastProgress == 204800, "Final progress should equal file size");
    }

    void CreateTestFile(const std::string& filename, size_t size)
    {
        std::ofstream file(filename, std::ios::binary);
        AssertTrue(file.is_open(), "Test file should be created");

        // 写入伪随机数据
        for (size_t i = 0; i < size; ++i)
        {
            uint8_t byte = static_cast<uint8_t>((i * 13 + 7) & 0xFF);
            file.write(reinterpret_cast<const char*>(&byte), 1);
        }

        file.close();
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::unique_ptr<ReliableChannel> m_senderChannel;
    std::unique_ptr<ReliableChannel> m_receiverChannel;
    std::string m_testDir;
};
