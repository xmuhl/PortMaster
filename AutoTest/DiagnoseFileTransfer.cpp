#pragma execution_character_set("utf-8")

#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include "../Transport/LoopbackTransport.h"
#include "../Protocol/ReliableChannel.h"
#include "../Protocol/FrameCodec.h"

// 诊断文件传输问题的专用工具
class FileTransferDiagnostic {
private:
    std::shared_ptr<LoopbackTransport> m_senderTransport;
    std::shared_ptr<LoopbackTransport> m_receiverTransport;
    std::shared_ptr<ReliableChannel> m_senderChannel;
    std::shared_ptr<ReliableChannel> m_receiverChannel;

    std::string m_testFile;
    std::string m_outputFile;
    std::vector<uint8_t> m_originalData;
    std::vector<uint8_t> m_receivedData;

    struct TransferMetrics {
        int64_t totalBytesSent = 0;
        int64_t totalBytesReceived = 0;
        int64_t lastReportedSent = 0;
        int64_t lastReportedReceived = 0;
        int packetsCreated = 0;
        int packetsSent = 0;
        int packetsAcknowledged = 0;
        std::chrono::steady_clock::time_point startTime;
    };

    TransferMetrics m_metrics;

    void Log(const std::string& message) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_metrics.startTime).count();
        std::cout << "[" << elapsed << "ms] " << message << std::endl;
    }

public:
    FileTransferDiagnostic() {
        m_testFile = "test_input_diagnostic.bin";
        m_outputFile = "test_output_diagnostic.bin";
    }

    // 生成测试文件
    bool GenerateTestFile(size_t sizeBytes) {
        Log("生成测试文件: " + std::to_string(sizeBytes) + " 字节");

        m_originalData.clear();
        m_originalData.reserve(sizeBytes);

        // 生成可识别的数据模式
        for (size_t i = 0; i < sizeBytes; i++) {
            m_originalData.push_back(static_cast<uint8_t>(i % 256));
        }

        std::ofstream file(m_testFile, std::ios::binary);
        if (!file.is_open()) {
            Log("错误：无法创建测试文件");
            return false;
        }

        file.write(reinterpret_cast<const char*>(m_originalData.data()), m_originalData.size());
        file.close();

        Log("测试文件生成成功: " + m_testFile);
        return true;
    }

    // 初始化传输通道
    bool Initialize(uint16_t windowSize = 4, size_t maxPayloadSize = 1024) {
        Log("初始化传输通道 - 窗口大小=" + std::to_string(windowSize) +
            ", 最大负载=" + std::to_string(maxPayloadSize));

        m_metrics = TransferMetrics();
        m_metrics.startTime = std::chrono::steady_clock::now();

        // 创建本地回路传输对
        m_senderTransport = std::make_shared<LoopbackTransport>();
        m_receiverTransport = std::make_shared<LoopbackTransport>();

        // 连接两个传输
        m_senderTransport->ConnectTo(m_receiverTransport);

        // 配置可靠传输参数
        ReliableConfig config;
        config.windowSize = windowSize;
        config.maxPayloadSize = maxPayloadSize;
        config.timeoutBase = 500;
        config.timeoutMax = 2000;
        config.maxRetries = 3;

        // 创建可靠通道
        m_senderChannel = std::make_shared<ReliableChannel>();
        m_receiverChannel = std::make_shared<ReliableChannel>();

        // 初始化发送端
        if (!m_senderChannel->Initialize(m_senderTransport, config)) {
            Log("错误：发送端初始化失败");
            return false;
        }

        // 初始化接收端
        if (!m_receiverChannel->Initialize(m_receiverTransport, config)) {
            Log("错误：接收端初始化失败");
            return false;
        }

        // 连接
        if (!m_senderChannel->Connect()) {
            Log("错误：发送端连接失败");
            return false;
        }

        if (!m_receiverChannel->Connect()) {
            Log("错误：接收端连接失败");
            return false;
        }

        Log("传输通道初始化成功");
        return true;
    }

    // 执行传输测试
    bool RunTransferTest() {
        Log("========== 开始传输测试 ==========");

        // 启动接收文件
        std::thread receiverThread([this]() {
            Log("[接收线程] 开始接收文件");
            bool result = m_receiverChannel->ReceiveFile(m_outputFile, nullptr);
            if (result) {
                Log("[接收线程] 文件接收成功");
            } else {
                Log("[接收线程] 文件接收失败");
            }
        });

        // 短暂延迟，确保接收端准备好
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 发送文件
        Log("[发送线程] 开始发送文件: " + m_testFile);

        bool sendResult = m_senderChannel->SendFile(
            m_testFile,
            [this](int64_t sent, int64_t total) {
                // 每传输1024字节报告一次
                if (sent - m_metrics.lastReportedSent >= 1024 || sent == total) {
                    Log("[进度] 已发送: " + std::to_string(sent) + "/" + std::to_string(total) +
                        " (" + std::to_string(sent * 100 / total) + "%)");
                    m_metrics.lastReportedSent = sent;
                }
            }
        );

        Log("[发送线程] 发送完成，结果=" + std::string(sendResult ? "成功" : "失败"));

        // 等待接收完成
        receiverThread.join();

        Log("========== 传输测试完成 ==========");

        return sendResult;
    }

    // 验证传输结果
    bool VerifyTransfer() {
        Log("========== 验证传输结果 ==========");

        // 读取接收到的文件
        std::ifstream file(m_outputFile, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            Log("错误：无法打开输出文件");
            return false;
        }

        std::streamsize receivedSize = file.tellg();
        file.seekg(0, std::ios::beg);

        m_receivedData.resize(static_cast<size_t>(receivedSize));
        file.read(reinterpret_cast<char*>(m_receivedData.data()), receivedSize);
        file.close();

        Log("原始文件大小: " + std::to_string(m_originalData.size()) + " 字节");
        Log("接收文件大小: " + std::to_string(m_receivedData.size()) + " 字节");

        // 检查大小
        if (m_originalData.size() != m_receivedData.size()) {
            Log("❌ 文件大小不匹配");
            Log("   期望: " + std::to_string(m_originalData.size()) + " 字节");
            Log("   实际: " + std::to_string(m_receivedData.size()) + " 字节");
            Log("   差异: " + std::to_string(static_cast<int64_t>(m_originalData.size()) - static_cast<int64_t>(m_receivedData.size())) + " 字节");

            // 如果接收的数据小于原始数据，检查是否在1024字节处停止
            if (m_receivedData.size() < m_originalData.size()) {
                if (m_receivedData.size() == 1024) {
                    Log("⚠️  传输在1024字节处停止！");
                    Log("   这是已知的Bug，需要检查SendFile函数的循环逻辑");
                }
            }
            return false;
        }

        // 检查内容
        bool contentMatch = (m_originalData == m_receivedData);
        if (!contentMatch) {
            Log("❌ 文件内容不匹配");

            // 找出第一个不匹配的字节
            for (size_t i = 0; i < std::min(m_originalData.size(), m_receivedData.size()); i++) {
                if (m_originalData[i] != m_receivedData[i]) {
                    Log("   第一个不匹配位置: " + std::to_string(i));
                    Log("   期望值: 0x" + std::to_string(m_originalData[i]));
                    Log("   实际值: 0x" + std::to_string(m_receivedData[i]));
                    break;
                }
            }
            return false;
        }

        Log("✅ 传输验证成功：文件大小和内容完全匹配");
        return true;
    }

    // 清理
    void Cleanup() {
        if (m_senderChannel) {
            m_senderChannel->Disconnect();
        }
        if (m_receiverChannel) {
            m_receiverChannel->Disconnect();
        }

        // 删除测试文件
        std::remove(m_testFile.c_str());
        std::remove(m_outputFile.c_str());
    }
};

// 主函数
int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "文件传输诊断工具 v1.0" << std::endl;
    std::cout << "专门用于诊断1024字节传输停滞问题" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    FileTransferDiagnostic diagnostic;

    // 测试不同大小的文件
    struct TestCase {
        std::string name;
        size_t fileSize;
        uint16_t windowSize;
    };

    std::vector<TestCase> testCases = {
        {"小文件(512字节)", 512, 4},
        {"1KB文件(1024字节)", 1024, 4},
        {"2KB文件(2048字节)", 2048, 4},
        {"4KB文件(4096字节)", 4096, 4},
        {"10KB文件(10240字节)", 10240, 4},
        {"100KB文件(窗口=1)", 102400, 1},
        {"100KB文件(窗口=4)", 102400, 4},
        {"100KB文件(窗口=8)", 102400, 8}
    };

    int passedTests = 0;
    int totalTests = static_cast<int>(testCases.size());

    for (const auto& testCase : testCases) {
        std::cout << std::endl;
        std::cout << "========================================"  << std::endl;
        std::cout << "测试: " << testCase.name << std::endl;
        std::cout << "========================================"  << std::endl;

        // 生成测试文件
        if (!diagnostic.GenerateTestFile(testCase.fileSize)) {
            std::cout << "❌ 测试文件生成失败" << std::endl;
            continue;
        }

        // 初始化传输通道
        if (!diagnostic.Initialize(testCase.windowSize, 1024)) {
            std::cout << "❌ 传输通道初始化失败" << std::endl;
            diagnostic.Cleanup();
            continue;
        }

        // 执行传输测试
        bool transferSuccess = diagnostic.RunTransferTest();

        // 验证结果
        bool verifySuccess = diagnostic.VerifyTransfer();

        // 清理
        diagnostic.Cleanup();

        // 结果
        if (transferSuccess && verifySuccess) {
            std::cout << "✅ 测试通过" << std::endl;
            passedTests++;
        } else {
            std::cout << "❌ 测试失败" << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "测试总结" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总计: " << totalTests << " 个测试" << std::endl;
    std::cout << "通过: " << passedTests << " 个" << std::endl;
    std::cout << "失败: " << (totalTests - passedTests) << " 个" << std::endl;
    std::cout << "成功率: " << (passedTests * 100 / totalTests) << "%" << std::endl;
    std::cout << "========================================" << std::endl;

    return (passedTests == totalTests) ? 0 : 1;
}
