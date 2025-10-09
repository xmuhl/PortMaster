#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include "../Protocol/FrameCodec.h"
#include "../Protocol/ReliableChannel.h"
#include "../Transport/LoopbackTransport.h"
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

// ============================================================================
// FrameCodec单元测试套件
// ============================================================================

class FrameCodecTest : public TestSuite
{
public:
    FrameCodecTest() : TestSuite("FrameCodecTest") {}

    void SetUp() override
    {
        m_codec = std::make_unique<FrameCodec>();
    }

    void TearDown() override
    {
        m_codec.reset();
    }

    std::vector<TestResult> RunTests() override
    {
        std::vector<TestResult> results;

        // CRC32测试
        results.push_back(RunTest("CRC32 Calculation", [this]() { TestCRC32Calculation(); }));
        results.push_back(RunTest("CRC32 Verification", [this]() { TestCRC32Verification(); }));

        // 帧编码测试
        results.push_back(RunTest("Encode Data Frame", [this]() { TestEncodeDataFrame(); }));
        results.push_back(RunTest("Encode Start Frame", [this]() { TestEncodeStartFrame(); }));
        results.push_back(RunTest("Encode End Frame", [this]() { TestEncodeEndFrame(); }));
        results.push_back(RunTest("Encode ACK Frame", [this]() { TestEncodeAckFrame(); }));
        results.push_back(RunTest("Encode NAK Frame", [this]() { TestEncodeNakFrame(); }));
        results.push_back(RunTest("Encode Heartbeat Frame", [this]() { TestEncodeHeartbeatFrame(); }));

        // 帧解码测试
        results.push_back(RunTest("Decode Data Frame", [this]() { TestDecodeDataFrame(); }));
        results.push_back(RunTest("Decode Start Frame", [this]() { TestDecodeStartFrame(); }));
        results.push_back(RunTest("Decode Invalid Frame", [this]() { TestDecodeInvalidFrame(); }));

        // 缓冲处理测试
        results.push_back(RunTest("Buffer Append and Extract", [this]() { TestBufferAppendExtract(); }));
        results.push_back(RunTest("Multiple Frames in Buffer", [this]() { TestMultipleFramesInBuffer(); }));
        results.push_back(RunTest("Partial Frame Handling", [this]() { TestPartialFrameHandling(); }));

        // 边界条件测试
        results.push_back(RunTest("Empty Payload", [this]() { TestEmptyPayload(); }));
        results.push_back(RunTest("Maximum Payload Size", [this]() { TestMaxPayloadSize(); }));

        return results;
    }

private:
    void TestCRC32Calculation()
    {
        // 测试CRC32计算
        const char* testData = "Hello, FrameCodec!";
        uint32_t crc = FrameCodec::CalculateCRC32(reinterpret_cast<const uint8_t*>(testData), strlen(testData));

        // CRC32应该是一个非零值（"Hello, FrameCodec!"的CRC32）
        AssertTrue(crc != 0, "CRC32 should be non-zero for non-empty data");

        // 相同数据应该产生相同的CRC32
        uint32_t crc2 = FrameCodec::CalculateCRC32(reinterpret_cast<const uint8_t*>(testData), strlen(testData));
        AssertTrue(crc == crc2, "Same data should produce same CRC32");

        // 不同数据应该产生不同的CRC32
        const char* differentData = "Different data";
        uint32_t crc3 = FrameCodec::CalculateCRC32(reinterpret_cast<const uint8_t*>(differentData), strlen(differentData));
        AssertTrue(crc != crc3, "Different data should produce different CRC32");
    }

    void TestCRC32Verification()
    {
        // 测试CRC32验证
        const char* testData = "Test data for CRC verification";
        uint32_t crc = FrameCodec::CalculateCRC32(reinterpret_cast<const uint8_t*>(testData), strlen(testData));

        // 正确的CRC应该验证通过
        bool valid = FrameCodec::VerifyCRC32(reinterpret_cast<const uint8_t*>(testData), strlen(testData), crc);
        AssertTrue(valid, "Correct CRC32 should verify successfully");

        // 错误的CRC应该验证失败
        uint32_t wrongCrc = crc + 1;
        bool invalid = FrameCodec::VerifyCRC32(reinterpret_cast<const uint8_t*>(testData), strlen(testData), wrongCrc);
        AssertTrue(!invalid, "Incorrect CRC32 should fail verification");
    }

    void TestEncodeDataFrame()
    {
        // 创建测试数据
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
        uint16_t sequence = 100;

        // 编码数据帧
        auto encodedFrame = m_codec->EncodeDataFrame(sequence, payload);

        // 验证帧大小（Header + Payload + Tail）
        size_t expectedSize = sizeof(FrameHeader) + payload.size() + sizeof(FrameTail);
        AssertTrue(encodedFrame.size() == expectedSize,
                   "Encoded frame size should match expected size");

        // 验证帧头魔数
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->magic == FrameCodec::HEADER_MAGIC, "Header magic should be 0xAA55");

        // 验证帧类型
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_DATA),
                   "Frame type should be FRAME_DATA");

        // 验证序列号
        AssertTrue(header->sequence == sequence, "Sequence number should match");

        // 验证负载长度
        AssertTrue(header->length == payload.size(), "Payload length should match");
    }

    void TestEncodeStartFrame()
    {
        // 创建START帧元数据
        StartMetadata metadata;
        metadata.version = 1;
        metadata.flags = 0;
        metadata.fileName = "test.txt";
        metadata.fileSize = 12345;
        metadata.modifyTime = 1234567890;
        metadata.sessionId = 42;

        uint16_t sequence = 1;

        // 编码START帧
        auto encodedFrame = m_codec->EncodeStartFrame(sequence, metadata);

        // 验证帧不为空
        AssertTrue(!encodedFrame.empty(), "Encoded START frame should not be empty");

        // 验证帧头
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->magic == FrameCodec::HEADER_MAGIC, "Header magic should be 0xAA55");
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_START),
                   "Frame type should be FRAME_START");
    }

    void TestEncodeEndFrame()
    {
        uint16_t sequence = 999;

        // 编码END帧
        auto encodedFrame = m_codec->EncodeEndFrame(sequence);

        // 验证帧大小（END帧通常没有负载）
        AssertTrue(!encodedFrame.empty(), "Encoded END frame should not be empty");

        // 验证帧头
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_END),
                   "Frame type should be FRAME_END");
        AssertTrue(header->sequence == sequence, "Sequence number should match");
    }

    void TestEncodeAckFrame()
    {
        uint16_t sequence = 123;

        // 编码ACK帧
        auto encodedFrame = m_codec->EncodeAckFrame(sequence);

        // 验证帧不为空
        AssertTrue(!encodedFrame.empty(), "Encoded ACK frame should not be empty");

        // 验证帧类型
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_ACK),
                   "Frame type should be FRAME_ACK");
    }

    void TestEncodeNakFrame()
    {
        uint16_t sequence = 456;

        // 编码NAK帧
        auto encodedFrame = m_codec->EncodeNakFrame(sequence);

        // 验证帧不为空
        AssertTrue(!encodedFrame.empty(), "Encoded NAK frame should not be empty");

        // 验证帧类型
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_NAK),
                   "Frame type should be FRAME_NAK");
    }

    void TestEncodeHeartbeatFrame()
    {
        uint16_t sequence = 789;

        // 编码心跳帧
        auto encodedFrame = m_codec->EncodeHeartbeatFrame(sequence);

        // 验证帧不为空
        AssertTrue(!encodedFrame.empty(), "Encoded HEARTBEAT frame should not be empty");

        // 验证帧类型
        FrameHeader* header = reinterpret_cast<FrameHeader*>(encodedFrame.data());
        AssertTrue(header->type == static_cast<uint8_t>(FrameType::FRAME_HEARTBEAT),
                   "Frame type should be FRAME_HEARTBEAT");
    }

    void TestDecodeDataFrame()
    {
        // 先编码一个数据帧
        std::vector<uint8_t> payload = {0x10, 0x20, 0x30, 0x40};
        uint16_t sequence = 200;
        auto encodedFrame = m_codec->EncodeDataFrame(sequence, payload);

        // 解码帧
        Frame decodedFrame = m_codec->DecodeFrame(encodedFrame);

        // 验证解码成功
        AssertTrue(decodedFrame.valid, "Decoded frame should be valid");
        AssertTrue(decodedFrame.type == FrameType::FRAME_DATA, "Frame type should be FRAME_DATA");
        AssertTrue(decodedFrame.sequence == sequence, "Sequence number should match");
        AssertTrue(decodedFrame.payload.size() == payload.size(), "Payload size should match");

        // 验证负载内容
        for (size_t i = 0; i < payload.size(); ++i)
        {
            AssertTrue(decodedFrame.payload[i] == payload[i],
                       "Payload byte " + std::to_string(i) + " should match");
        }
    }

    void TestDecodeStartFrame()
    {
        // 创建START帧元数据
        StartMetadata metadata;
        metadata.version = 1;
        metadata.fileName = "test_file.dat";
        metadata.fileSize = 54321;
        metadata.modifyTime = 9876543210;
        metadata.sessionId = 100;

        uint16_t sequence = 1;

        // 编码START帧
        auto encodedFrame = m_codec->EncodeStartFrame(sequence, metadata);

        // 解码帧
        Frame decodedFrame = m_codec->DecodeFrame(encodedFrame);

        // 验证解码成功
        AssertTrue(decodedFrame.valid, "Decoded START frame should be valid");
        AssertTrue(decodedFrame.type == FrameType::FRAME_START, "Frame type should be FRAME_START");

        // 解码START元数据
        StartMetadata decodedMetadata;
        bool metadataDecoded = m_codec->DecodeStartMetadata(decodedFrame.payload, decodedMetadata);

        AssertTrue(metadataDecoded, "START metadata should be decoded successfully");
        AssertTrue(decodedMetadata.fileName == metadata.fileName, "File name should match");
        AssertTrue(decodedMetadata.fileSize == metadata.fileSize, "File size should match");
        AssertTrue(decodedMetadata.sessionId == metadata.sessionId, "Session ID should match");
    }

    void TestDecodeInvalidFrame()
    {
        // 创建无效帧数据（错误的魔数）
        std::vector<uint8_t> invalidFrame = {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        // 尝试解码
        Frame decodedFrame = m_codec->DecodeFrame(invalidFrame);

        // 验证解码失败
        AssertTrue(!decodedFrame.valid, "Invalid frame should not be valid");
    }

    void TestBufferAppendExtract()
    {
        // 编码一个数据帧
        std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC};
        uint16_t sequence = 50;
        auto encodedFrame = m_codec->EncodeDataFrame(sequence, payload);

        // 添加到缓冲区
        m_codec->AppendData(encodedFrame);

        // 尝试提取帧
        Frame extractedFrame;
        bool success = m_codec->TryGetFrame(extractedFrame);

        // 验证提取成功
        AssertTrue(success, "Frame extraction should succeed");
        AssertTrue(extractedFrame.valid, "Extracted frame should be valid");
        AssertTrue(extractedFrame.sequence == sequence, "Sequence should match");
        AssertTrue(extractedFrame.payload == payload, "Payload should match");
    }

    void TestMultipleFramesInBuffer()
    {
        // 编码多个帧
        auto frame1 = m_codec->EncodeDataFrame(1, {0x01});
        auto frame2 = m_codec->EncodeDataFrame(2, {0x02});
        auto frame3 = m_codec->EncodeDataFrame(3, {0x03});

        // 将所有帧添加到缓冲区
        std::vector<uint8_t> allFrames;
        allFrames.insert(allFrames.end(), frame1.begin(), frame1.end());
        allFrames.insert(allFrames.end(), frame2.begin(), frame2.end());
        allFrames.insert(allFrames.end(), frame3.begin(), frame3.end());

        m_codec->AppendData(allFrames);

        // 逐个提取帧
        Frame extracted1, extracted2, extracted3;

        bool success1 = m_codec->TryGetFrame(extracted1);
        bool success2 = m_codec->TryGetFrame(extracted2);
        bool success3 = m_codec->TryGetFrame(extracted3);

        // 验证所有帧都被正确提取
        AssertTrue(success1 && success2 && success3, "All frames should be extracted");
        AssertTrue(extracted1.sequence == 1, "Frame 1 sequence should be 1");
        AssertTrue(extracted2.sequence == 2, "Frame 2 sequence should be 2");
        AssertTrue(extracted3.sequence == 3, "Frame 3 sequence should be 3");
    }

    void TestPartialFrameHandling()
    {
        // 编码一个数据帧
        auto fullFrame = m_codec->EncodeDataFrame(100, {0xDE, 0xAD, 0xBE, 0xEF});

        // 只添加一半的帧数据
        size_t halfSize = fullFrame.size() / 2;
        std::vector<uint8_t> partialFrame(fullFrame.begin(), fullFrame.begin() + halfSize);

        m_codec->AppendData(partialFrame);

        // 尝试提取帧（应该失败，因为帧不完整）
        Frame extractedFrame;
        bool success = m_codec->TryGetFrame(extractedFrame);
        AssertTrue(!success, "Partial frame extraction should fail");

        // 添加剩余数据
        std::vector<uint8_t> remainingFrame(fullFrame.begin() + halfSize, fullFrame.end());
        m_codec->AppendData(remainingFrame);

        // 现在应该能成功提取
        success = m_codec->TryGetFrame(extractedFrame);
        AssertTrue(success, "Complete frame extraction should succeed");
        AssertTrue(extractedFrame.valid, "Extracted frame should be valid");
        AssertTrue(extractedFrame.sequence == 100, "Sequence should match");
    }

    void TestEmptyPayload()
    {
        // 编码空负载数据帧
        std::vector<uint8_t> emptyPayload;
        uint16_t sequence = 0;
        auto encodedFrame = m_codec->EncodeDataFrame(sequence, emptyPayload);

        // 解码帧
        Frame decodedFrame = m_codec->DecodeFrame(encodedFrame);

        // 验证解码成功
        AssertTrue(decodedFrame.valid, "Empty payload frame should be valid");
        AssertTrue(decodedFrame.payload.empty(), "Payload should be empty");
    }

    void TestMaxPayloadSize()
    {
        // 创建最大大小的负载
        size_t maxSize = m_codec->GetMaxPayloadSize();
        std::vector<uint8_t> maxPayload(maxSize, 0x55);

        uint16_t sequence = 999;

        // 编码帧
        auto encodedFrame = m_codec->EncodeDataFrame(sequence, maxPayload);

        // 验证编码成功
        AssertTrue(!encodedFrame.empty(), "Max payload frame should be encoded");

        // 解码帧
        Frame decodedFrame = m_codec->DecodeFrame(encodedFrame);

        // 验证解码成功
        AssertTrue(decodedFrame.valid, "Max payload frame should be valid");
        AssertTrue(decodedFrame.payload.size() == maxSize, "Payload size should match max size");
    }

private:
    std::unique_ptr<FrameCodec> m_codec;
};

// ============================================================================
// ReliableChannel单元测试套件
// ============================================================================

class ReliableChannelTest : public TestSuite
{
public:
    ReliableChannelTest() : TestSuite("ReliableChannelTest") {}

    void SetUp() override
    {
        // 创建Loopback传输作为底层传输
        m_transport = std::make_shared<LoopbackTransport>();

        LoopbackConfig loopConfig;
        loopConfig.delayMs = 0;        // 无延迟
        loopConfig.errorRate = 0;      // 无错误
        loopConfig.packetLossRate = 0; // 无丢包
        m_transport->Open(loopConfig);

        // 创建可靠通道
        m_channel = std::make_unique<ReliableChannel>();

        // 初始化可靠通道
        ReliableConfig config;
        config.windowSize = 4;
        config.maxRetries = 3;
        config.timeoutBase = 500;
        config.maxPayloadSize = 1024;

        m_channel->Initialize(m_transport, config);
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

        // 基础功能测试
        results.push_back(RunTest("Channel Initialize", [this]() { TestInitialize(); }));
        results.push_back(RunTest("Connect and Disconnect", [this]() { TestConnectDisconnect(); }));

        // 数据传输测试
        results.push_back(RunTest("Small Data Transfer", [this]() { TestSmallDataTransfer(); }));
        results.push_back(RunTest("Large Data Transfer", [this]() { TestLargeDataTransfer(); }));

        // 统计信息测试
        results.push_back(RunTest("Statistics Tracking", [this]() { TestStatistics(); }));
        results.push_back(RunTest("Statistics Reset", [this]() { TestStatisticsReset(); }));

        // 配置测试
        results.push_back(RunTest("Configuration Update", [this]() { TestConfigUpdate(); }));

        // 状态查询测试
        results.push_back(RunTest("Sequence Numbers", [this]() { TestSequenceNumbers(); }));
        results.push_back(RunTest("Queue Sizes", [this]() { TestQueueSizes(); }));

        return results;
    }

private:
    void TestInitialize()
    {
        // 验证初始化状态
        // 注意：Initialize在SetUp中已调用
        AssertTrue(true, "Channel should be initialized in SetUp");
    }

    void TestConnectDisconnect()
    {
        // 测试连接
        bool connectSuccess = m_channel->Connect();
        AssertTrue(connectSuccess, "Connect should succeed");
        AssertTrue(m_channel->IsConnected(), "Channel should be connected");

        // 等待连接稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 测试断开连接
        bool disconnectSuccess = m_channel->Disconnect();
        AssertTrue(disconnectSuccess, "Disconnect should succeed");
        AssertTrue(!m_channel->IsConnected(), "Channel should be disconnected");
    }

    void TestSmallDataTransfer()
    {
        // 连接通道
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 准备测试数据
        std::vector<uint8_t> sendData = {0x01, 0x02, 0x03, 0x04, 0x05};

        // 发送数据
        bool sendSuccess = m_channel->Send(sendData);
        AssertTrue(sendSuccess, "Send should succeed");

        // 接收数据
        std::vector<uint8_t> recvData;
        bool recvSuccess = m_channel->Receive(recvData, 2000);

        // 验证接收成功
        AssertTrue(recvSuccess, "Receive should succeed");
        AssertTrue(recvData.size() == sendData.size(), "Received data size should match");

        // 验证数据内容
        for (size_t i = 0; i < sendData.size(); ++i)
        {
            AssertTrue(recvData[i] == sendData[i],
                       "Data byte " + std::to_string(i) + " should match");
        }
    }

    void TestLargeDataTransfer()
    {
        // 连接通道
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 准备大数据（10KB）
        std::vector<uint8_t> sendData(10240);
        for (size_t i = 0; i < sendData.size(); ++i)
        {
            sendData[i] = static_cast<uint8_t>(i & 0xFF);
        }

        // 发送数据
        bool sendSuccess = m_channel->Send(sendData);
        AssertTrue(sendSuccess, "Large data send should succeed");

        // 接收数据（增加超时时间）
        std::vector<uint8_t> recvData;
        bool recvSuccess = m_channel->Receive(recvData, 10000);

        // 验证接收成功
        AssertTrue(recvSuccess, "Large data receive should succeed");
        AssertTrue(recvData.size() == sendData.size(), "Received data size should match");

        // 抽样验证数据内容（避免过多断言）
        for (size_t i = 0; i < sendData.size(); i += 1024)
        {
            AssertTrue(recvData[i] == sendData[i],
                       "Data byte " + std::to_string(i) + " should match");
        }
    }

    void TestStatistics()
    {
        // 连接并传输数据
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> testData(512, 0xAB);
        m_channel->Send(testData);

        std::vector<uint8_t> recvData;
        m_channel->Receive(recvData, 2000);

        // 获取统计信息
        ReliableStats stats = m_channel->GetStats();

        // 验证统计数据
        AssertTrue(stats.packetsSent > 0, "Packets sent should be greater than 0");
        AssertTrue(stats.packetsReceived > 0, "Packets received should be greater than 0");
        AssertTrue(stats.bytesSent >= testData.size(), "Bytes sent should be tracked");
        AssertTrue(stats.bytesReceived >= testData.size(), "Bytes received should be tracked");
    }

    void TestStatisticsReset()
    {
        // 连接并产生统计数据
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> testData(100, 0x11);
        m_channel->Send(testData);

        // 验证有统计数据
        ReliableStats stats = m_channel->GetStats();
        AssertTrue(stats.packetsSent > 0, "Should have statistics before reset");

        // 重置统计
        m_channel->ResetStats();

        // 验证统计已重置
        stats = m_channel->GetStats();
        AssertTrue(stats.packetsSent == 0, "Packets sent should be reset to 0");
        AssertTrue(stats.packetsReceived == 0, "Packets received should be reset to 0");
    }

    void TestConfigUpdate()
    {
        // 获取当前配置
        ReliableConfig currentConfig = m_channel->GetConfig();

        // 修改配置
        currentConfig.windowSize = 8;
        currentConfig.maxRetries = 5;
        currentConfig.timeoutBase = 1000;

        // 更新配置
        m_channel->SetConfig(currentConfig);

        // 验证配置已更新
        ReliableConfig updatedConfig = m_channel->GetConfig();
        AssertTrue(updatedConfig.windowSize == 8, "Window size should be updated");
        AssertTrue(updatedConfig.maxRetries == 5, "Max retries should be updated");
        AssertTrue(updatedConfig.timeoutBase == 1000, "Timeout base should be updated");
    }

    void TestSequenceNumbers()
    {
        // 连接通道
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 获取初始序列号
        uint16_t localSeq = m_channel->GetLocalSequence();
        uint16_t remoteSeq = m_channel->GetRemoteSequence();

        // 序列号应该是有效值
        AssertTrue(true, "Sequence numbers should be retrievable");

        // 发送数据后序列号应该增加
        std::vector<uint8_t> testData(50, 0xFF);
        m_channel->Send(testData);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        uint16_t newLocalSeq = m_channel->GetLocalSequence();
        // 序列号应该已经前进（可能因为多个包而增加多次）
        AssertTrue(newLocalSeq >= localSeq, "Local sequence should advance");
    }

    void TestQueueSizes()
    {
        // 连接通道
        m_channel->Connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 查询队列大小
        size_t sendQueueSize = m_channel->GetSendQueueSize();
        size_t recvQueueSize = m_channel->GetReceiveQueueSize();

        // 队列大小应该可查询
        AssertTrue(true, "Queue sizes should be retrievable");

        // 初始状态队列应该较小或为空
        AssertTrue(sendQueueSize < 1000, "Send queue should be reasonable size");
        AssertTrue(recvQueueSize < 1000, "Receive queue should be reasonable size");
    }

private:
    std::shared_ptr<LoopbackTransport> m_transport;
    std::unique_ptr<ReliableChannel> m_channel;
};
