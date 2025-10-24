#pragma execution_character_set("utf-8")

#include "pch.h"
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include "../Transport/LoopbackTransport.h"
#include "../Protocol/ReliableChannel.h"

// 简单的回路测试 - 最小化复现1024字节停滞问题
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "简单回路传输测试" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    // 创建传输层
    auto transport = std::make_shared<LoopbackTransport>();

    // 创建可靠通道
    auto channel = std::make_shared<ReliableChannel>();

    // 配置
    TransportConfig transportConfig;
    transport->Open(transportConfig);

    ReliableConfig reliableConfig;
    reliableConfig.windowSize = 1;  // 使用窗口大小1 - AutoTest中失败的配置
    reliableConfig.maxPayloadSize = 1024;
    reliableConfig.maxRetries = 5;

    channel->Initialize(transport, reliableConfig);
    channel->Connect();

    std::cout << "传输通道初始化完成" << std::endl;
    std::cout << "窗口大小: " << reliableConfig.windowSize << std::endl;
    std::cout << "最大负载: " << reliableConfig.maxPayloadSize << std::endl << std::endl;

    // 生成测试数据 (1MB - 与AutoTest相同)
    const size_t testSize = 1024 * 1024;
    std::vector<uint8_t> testData(testSize);
    for (size_t i = 0; i < testSize; i++) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }

    std::cout << "生成测试数据: " << testSize << " 字节" << std::endl << std::endl;

    // 发送数据
    std::cout << "开始发送数据..." << std::endl;
    auto startTime = std::chrono::steady_clock::now();

    bool sendSuccess = channel->Send(testData);

    auto sendEndTime = std::chrono::steady_clock::now();
    auto sendDuration = std::chrono::duration_cast<std::chrono::milliseconds>(sendEndTime - startTime).count();

    std::cout << "Send()返回: " << (sendSuccess ? "成功" : "失败") << std::endl;
    std::cout << "发送耗时: " << sendDuration << "ms" << std::endl << std::endl;

    // 等待数据传输
    std::cout << "等待数据接收..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 接收数据
    std::vector<uint8_t> receivedData;
    bool recvSuccess = channel->Receive(receivedData, 5000);  // 5秒超时

    auto endTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "Receive()返回: " << (recvSuccess ? "成功" : "失败") << std::endl;
    std::cout << "总耗时: " << totalDuration << "ms" << std::endl << std::endl;

    // 验证结果
    std::cout << "========================================" << std::endl;
    std::cout << "传输结果验证" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "原始数据大小: " << testData.size() << " 字节" << std::endl;
    std::cout << "接收数据大小: " << receivedData.size() << " 字节" << std::endl;

    if (testData.size() == receivedData.size()) {
        bool dataMatch = (testData == receivedData);
        if (dataMatch) {
            std::cout << "✅ 测试通过：数据完全匹配" << std::endl;
        } else {
            std::cout << "❌ 测试失败：数据不匹配" << std::endl;
        }
    } else {
        std::cout << "❌ 测试失败：大小不匹配" << std::endl;
        if (receivedData.size() == 1024) {
            std::cout << "⚠️  传输停在1024字节！这是已知的Bug。" << std::endl;
        }
    }

    // 清理
    channel->Disconnect();
    transport->Close();

    std::cout << std::endl << "测试完成，按回车键退出..." << std::endl;
    std::cin.get();

    return (testData.size() == receivedData.size()) ? 0 : 1;
}
