#pragma execution_character_set("utf-8")
// 独立可靠传输回路测试工具
// 用于测试ReliableChannel + LoopbackTransport完整传输流程

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>

// 引入项目头文件
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include "Protocol/FrameCodec.h"
#include "Common/ConfigManager.h"

// 全局变量
std::atomic<bool> g_transmissionComplete{false};
std::atomic<bool> g_transmissionFailed{false};
std::vector<uint8_t> g_receivedData;
std::string g_errorMessage;
std::string g_receivedFilename;

// 错误回调
void OnError(const std::string& errorMsg)
{
    std::cout << "❌ 错误: " << errorMsg << std::endl;
    g_errorMessage = errorMsg;
    g_transmissionFailed = true;
}

// 接收进度回调
void OnReceiveProgress(uint64_t current, uint64_t total, const std::string& filename)
{
    static uint64_t lastPrint = 0;
    if (current - lastPrint > 10240 || current == total)  // 每10KB打印一次
    {
        double percent = total > 0 ? (current * 100.0 / total) : 0;
        std::cout << "📥 接收进度: " << current << "/" << total
                  << " (" << percent << "%)" << std::endl;
        lastPrint = current;
    }

    if (current == total)
    {
        g_receivedFilename = filename;
        std::cout << "✅ 接收完成: " << filename << ", 大小: " << total << " 字节" << std::endl;
    }
}

// 发送进度回调
void OnSendProgress(uint64_t current, uint64_t total, const std::string& filename)
{
    static uint64_t lastPrint = 0;
    if (current - lastPrint > 10240 || current == total)
    {
        double percent = total > 0 ? (current * 100.0 / total) : 0;
        std::cout << "📤 发送进度: " << current << "/" << total
                  << " (" << percent << "%)" << std::endl;
        lastPrint = current;
    }
}

// 状态变更回调
void OnStateChanged(ReliableState newState)
{
    std::cout << "🔄 状态变更: " << static_cast<int>(newState) << std::endl;

    if (newState == ReliableState::RELIABLE_DONE)
    {
        g_transmissionComplete = true;
        std::cout << "✅ 传输完成" << std::endl;
    }
    else if (newState == ReliableState::RELIABLE_FAILED)
    {
        g_transmissionFailed = true;
        std::cout << "❌ 传输失败" << std::endl;
    }
}

// 数据接收回调
void OnDataReceived(const uint8_t* data, size_t size)
{
    g_receivedData.insert(g_receivedData.end(), data, data + size);
}

// 读取文件
bool ReadFile(const std::string& filepath, std::vector<uint8_t>& data)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "❌ 无法打开文件: " << filepath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    std::cout << "📄 读取文件成功: " << filepath << ", 大小: " << fileSize << " 字节" << std::endl;
    return true;
}

// 保存文件
bool SaveFile(const std::string& filepath, const std::vector<uint8_t>& data)
{
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "❌ 无法创建文件: " << filepath << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    std::cout << "💾 保存文件成功: " << filepath << ", 大小: " << data.size() << " 字节" << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "可靠传输回路测试工具" << std::endl;
    std::cout << "========================================" << std::endl;

    // 解析命令行参数
    std::string inputFile = "招商证券股份有限公司融资融券业务合同.pdf";
    std::string outputFile = "test_output.pdf";

    if (argc >= 2)
    {
        inputFile = argv[1];
    }
    if (argc >= 3)
    {
        outputFile = argv[2];
    }

    std::cout << "📂 输入文件: " << inputFile << std::endl;
    std::cout << "📂 输出文件: " << outputFile << std::endl;
    std::cout << std::endl;

    // 1. 读取测试文件
    std::cout << "步骤1: 读取测试文件..." << std::endl;
    std::vector<uint8_t> fileData;
    if (!ReadFile(inputFile, fileData))
    {
        return 1;
    }
    std::cout << std::endl;

    // 2. 创建Loopback传输层
    std::cout << "步骤2: 创建Loopback传输层..." << std::endl;
    auto transport = std::make_shared<LoopbackTransport>();

    TransportConfig config;
    config.maxQueueSize = 100;  // 队列大小
    config.processInterval = 1;  // 处理间隔1ms

    TransportError openError = transport->Open(config);
    if (openError != TransportError::Success)
    {
        std::cerr << "❌ 打开传输层失败: " << static_cast<int>(openError) << std::endl;
        return 1;
    }
    std::cout << "✅ Loopback传输层创建成功，队列大小: " << config.maxQueueSize << std::endl;
    std::cout << std::endl;

    // 3. 创建可靠传输通道
    std::cout << "步骤3: 创建可靠传输通道..." << std::endl;
    auto frameCodec = std::make_shared<FrameCodec>();
    auto reliableChannel = std::make_shared<ReliableChannel>(transport, frameCodec);

    ReliableConfig reliableConfig;
    reliableConfig.windowSize = 1;          // 滑动窗口大小
    reliableConfig.maxRetries = 10;         // 最大重试次数
    reliableConfig.ackTimeout = 1000;       // ACK超时1秒
    reliableConfig.handshakeTimeout = 5000; // 握手超时5秒

    reliableChannel->SetConfig(reliableConfig);
    std::cout << "✅ 可靠传输通道配置成功" << std::endl;
    std::cout << "   - 窗口大小: " << reliableConfig.windowSize << std::endl;
    std::cout << "   - 最大重试: " << reliableConfig.maxRetries << std::endl;
    std::cout << "   - ACK超时: " << reliableConfig.ackTimeout << "ms" << std::endl;
    std::cout << "   - 握手超时: " << reliableConfig.handshakeTimeout << "ms" << std::endl;
    std::cout << std::endl;

    // 4. 注册回调
    std::cout << "步骤4: 注册回调函数..." << std::endl;
    reliableChannel->SetErrorCallback(OnError);
    reliableChannel->SetReceiveProgressCallback(OnReceiveProgress);
    reliableChannel->SetSendProgressCallback(OnSendProgress);
    reliableChannel->SetStateCallback(OnStateChanged);
    std::cout << "✅ 回调函数注册完成" << std::endl;
    std::cout << std::endl;

    // 5. 启动接收线程
    std::cout << "步骤5: 启动接收端..." << std::endl;
    if (!reliableChannel->StartReceive())
    {
        std::cerr << "❌ 启动接收失败" << std::endl;
        return 1;
    }
    std::cout << "✅ 接收端启动成功" << std::endl;
    std::cout << std::endl;

    // 6. 发送文件
    std::cout << "步骤6: 开始发送文件..." << std::endl;
    std::cout << "文件大小: " << fileData.size() << " 字节" << std::endl;

    // 将数据写入内存流
    std::string tempFilePath = "temp_test_input.bin";
    if (!SaveFile(tempFilePath, fileData))
    {
        std::cerr << "❌ 创建临时文件失败" << std::endl;
        return 1;
    }

    bool sendSuccess = reliableChannel->SendFile(tempFilePath, inputFile);

    if (!sendSuccess)
    {
        std::cerr << "❌ 发送文件失败" << std::endl;
        std::cerr << "错误信息: " << g_errorMessage << std::endl;
        return 1;
    }

    std::cout << "✅ 发送请求提交成功，等待传输完成..." << std::endl;
    std::cout << std::endl;

    // 7. 等待传输完成
    std::cout << "步骤7: 等待传输完成..." << std::endl;
    int waitSeconds = 0;
    const int maxWaitSeconds = 30;

    while (!g_transmissionComplete && !g_transmissionFailed && waitSeconds < maxWaitSeconds)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        waitSeconds++;

        if (waitSeconds % 5 == 0)
        {
            std::cout << "⏳ 等待中... (" << waitSeconds << "秒)" << std::endl;
        }
    }

    if (g_transmissionFailed)
    {
        std::cerr << "❌ 传输失败: " << g_errorMessage << std::endl;
        return 1;
    }

    if (waitSeconds >= maxWaitSeconds)
    {
        std::cerr << "❌ 传输超时（>" << maxWaitSeconds << "秒）" << std::endl;
        return 1;
    }

    std::cout << "✅ 传输完成" << std::endl;
    std::cout << std::endl;

    // 8. 获取接收的数据
    std::cout << "步骤8: 验证接收的数据..." << std::endl;

    // 从ReliableChannel获取接收的数据
    std::vector<uint8_t> receivedData = reliableChannel->GetReceivedData();

    std::cout << "原始文件大小: " << fileData.size() << " 字节" << std::endl;
    std::cout << "接收文件大小: " << receivedData.size() << " 字节" << std::endl;

    if (receivedData.size() != fileData.size())
    {
        std::cerr << "❌ 文件大小不匹配！" << std::endl;
        std::cerr << "   期望: " << fileData.size() << " 字节" << std::endl;
        std::cerr << "   实际: " << receivedData.size() << " 字节" << std::endl;
        return 1;
    }

    // 验证数据内容
    bool dataMatch = (receivedData == fileData);

    if (!dataMatch)
    {
        std::cerr << "❌ 文件内容不匹配！" << std::endl;

        // 找出第一个不匹配的字节
        for (size_t i = 0; i < std::min(receivedData.size(), fileData.size()); i++)
        {
            if (receivedData[i] != fileData[i])
            {
                std::cerr << "   第一个不匹配位置: " << i << std::endl;
                std::cerr << "   期望值: 0x" << std::hex << static_cast<int>(fileData[i]) << std::endl;
                std::cerr << "   实际值: 0x" << std::hex << static_cast<int>(receivedData[i]) << std::endl;
                break;
            }
        }
        return 1;
    }

    std::cout << "✅ 文件大小和内容完全匹配" << std::endl;
    std::cout << std::endl;

    // 9. 保存接收的文件
    std::cout << "步骤9: 保存接收的文件..." << std::endl;
    if (!SaveFile(outputFile, receivedData))
    {
        std::cerr << "❌ 保存文件失败" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // 10. 清理
    std::cout << "步骤10: 清理资源..." << std::endl;
    reliableChannel->StopReceive();
    transport->Close();

    // 删除临时文件
    std::remove(tempFilePath.c_str());

    std::cout << "✅ 资源清理完成" << std::endl;
    std::cout << std::endl;

    // 11. 显示统计信息
    std::cout << "========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;

    ReliableStats stats = reliableChannel->GetStats();
    std::cout << "传输统计:" << std::endl;
    std::cout << "  - 发送数据包: " << stats.packetsSent << std::endl;
    std::cout << "  - 接收数据包: " << stats.packetsReceived << std::endl;
    std::cout << "  - 重传数据包: " << stats.packetsRetransmitted << std::endl;
    std::cout << "  - 发送字节数: " << stats.bytesSent << std::endl;
    std::cout << "  - 接收字节数: " << stats.bytesReceived << std::endl;
    std::cout << "  - 错误次数: " << stats.errors << std::endl;
    std::cout << std::endl;

    std::cout << "✅ 测试成功！" << std::endl;
    std::cout << "📂 输出文件: " << outputFile << std::endl;

    return 0;
}