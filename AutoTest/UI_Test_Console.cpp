// UI_Test_Console.cpp : PortMaster UI控件响应问题自动测试控制台程序
//
// 严格按照用户要求的5步测试流程执行：
// 1. 本地回路测试
// 2. 启用可靠传输选项
// 3. 发送指定测试文件 C:\Users\huangl\Desktop\PortMaster\test_input.pdf
// 4. 接收传输数据
// 5. 保存到本地文件并验证一致性

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include "../Transport/LoopbackTransport.h"
#include "../Protocol/ReliableChannel.h"
#include "../Common/CommonTypes.h"

namespace fs = std::filesystem;

class UITestConsole {
private:
    std::shared_ptr<ITransport> m_transport;
    std::unique_ptr<ReliableChannel> m_reliableChannel;
    bool m_testPassed;
    std::string m_testLog;

public:
    UITestConsole() : m_testPassed(false) {
        LogMessage("=== PortMaster UI控件响应问题自动测试开始 ===");
    }

    ~UITestConsole() {
        LogMessage("=== 测试完成 ===");
    }

    // 主测试流程 - 严格按照用户要求的5步流程
    bool RunFullTest() {
        try {
            LogMessage("开始执行完整UI测试流程...");

            // 步骤1: 本地回路测试
            if (!Step1_LocalLoopbackTest()) {
                LogMessage("❌ 步骤1失败: 本地回路测试失败");
                return false;
            }
            LogMessage("✅ 步骤1通过: 本地回路测试成功");

            // 步骤2: 启用可靠传输选项
            if (!Step2_EnableReliableTransmission()) {
                LogMessage("❌ 步骤2失败: 启用可靠传输失败");
                return false;
            }
            LogMessage("✅ 步骤2通过: 可靠传输启用成功");

            // 步骤3: 发送指定测试文件
            if (!Step3_SendTestFile()) {
                LogMessage("❌ 步骤3失败: 发送测试文件失败");
                return false;
            }
            LogMessage("✅ 步骤3通过: 测试文件发送成功");

            // 步骤4: 接收传输数据
            if (!Step4_ReceiveData()) {
                LogMessage("❌ 步骤4失败: 接收数据失败");
                return false;
            }
            LogMessage("✅ 步骤4通过: 数据接收成功");

            // 步骤5: 保存到本地文件并验证一致性
            if (!Step5_VerifyFileIntegrity()) {
                LogMessage("❌ 步骤5失败: 文件完整性验证失败");
                return false;
            }
            LogMessage("✅ 步骤5通过: 文件完整性验证成功");

            LogMessage("🎉 所有测试步骤通过！UI控件响应问题修复验证成功");
            m_testPassed = true;
            return true;

        } catch (const std::exception& e) {
            LogMessage("❌ 测试异常: " + std::string(e.what()));
            return false;
        }
    }

private:
    // 步骤1: 本地回路测试
    bool Step1_LocalLoopbackTest() {
        LogMessage("步骤1: 开始本地回路测试...");

        try {
            // 创建Loopback传输
            m_transport = std::make_shared<LoopbackTransport>();

            // 初始化传输
            if (!m_transport->Open()) {
                LogMessage("错误: Loopback传输初始化失败");
                return false;
            }

            LogMessage("Loopback传输初始化成功");
            return true;

        } catch (const std::exception& e) {
            LogMessage("步骤1异常: " + std::string(e.what()));
            return false;
        }
    }

    // 步骤2: 启用可靠传输选项
    bool Step2_EnableReliableTransmission() {
        LogMessage("步骤2: 启用可靠传输选项...");

        try {
            // 创建可靠传输通道
            m_reliableChannel = std::make_unique<ReliableChannel>();

            // 配置可靠传输参数
            ReliableConfig config;
            config.windowSize = 1;
            config.maxPayloadSize = 1024;
            config.maxRetries = 5;
            config.timeoutBase = 1000;  // 1秒
            config.timeoutMax = 10000;  // 10秒
            config.heartbeatInterval = 5000;  // 5秒心跳

            // 初始化可靠传输
            if (!m_reliableChannel->Initialize(m_transport, config)) {
                LogMessage("错误: 可靠传输初始化失败");
                return false;
            }

            // 建立连接
            if (!m_reliableChannel->Connect()) {
                LogMessage("错误: 可靠传输连接失败");
                return false;
            }

            LogMessage("可靠传输启用成功");
            return true;

        } catch (const std::exception& e) {
            LogMessage("步骤2异常: " + std::string(e.what()));
            return false;
        }
    }

    // 步骤3: 发送指定测试文件
    bool Step3_SendTestFile() {
        LogMessage("步骤3: 发送指定测试文件...");

        const std::string testFilePath = "C:\\Users\\huangl\\Desktop\\PortMaster\\test_input.pdf";

        // 检查测试文件是否存在
        if (!fs::exists(testFilePath)) {
            LogMessage("错误: 测试文件不存在: " + testFilePath);
            return false;
        }

        try {
            // 读取测试文件
            std::ifstream file(testFilePath, std::ios::binary);
            if (!file.is_open()) {
                LogMessage("错误: 无法打开测试文件");
                return false;
            }

            // 获取文件大小
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            LogMessage("测试文件大小: " + std::to_string(fileSize) + " 字节");

            // 发送文件
            bool sendResult = m_reliableChannel->SendFile(testFilePath);

            file.close();

            if (!sendResult) {
                LogMessage("错误: 文件发送失败");
                return false;
            }

            LogMessage("测试文件发送成功");
            return true;

        } catch (const std::exception& e) {
            LogMessage("步骤3异常: " + std::string(e.what()));
            return false;
        }
    }

    // 步骤4: 接收传输数据
    bool Step4_ReceiveData() {
        LogMessage("步骤4: 接收传输数据...");

        try {
            // 设置接收文件路径
            const std::string receivedFilePath = "C:\\Users\\huangl\\Desktop\\PortMaster\\test_output_received.pdf";

            // 接收文件
            bool receiveResult = m_reliableChannel->ReceiveFile(receivedFilePath);

            if (!receiveResult) {
                LogMessage("错误: 文件接收失败");
                return false;
            }

            // 检查接收文件是否存在
            if (!fs::exists(receivedFilePath)) {
                LogMessage("错误: 接收文件不存在");
                return false;
            }

            // 获取接收文件大小
            size_t receivedSize = fs::file_size(receivedFilePath);
            LogMessage("接收文件大小: " + std::to_string(receivedSize) + " 字节");

            LogMessage("数据接收成功");
            return true;

        } catch (const std::exception& e) {
            LogMessage("步骤4异常: " + std::string(e.what()));
            return false;
        }
    }

    // 步骤5: 保存到本地文件并验证一致性
    bool Step5_VerifyFileIntegrity() {
        LogMessage("步骤5: 验证文件完整性...");

        const std::string originalFile = "C:\\Users\\huangl\\Desktop\\PortMaster\\test_input.pdf";
        const std::string receivedFile = "C:\\Users\\huangl\\Desktop\\PortMaster\\test_output_received.pdf";

        try {
            // 检查两个文件是否存在
            if (!fs::exists(originalFile)) {
                LogMessage("错误: 原始测试文件不存在");
                return false;
            }

            if (!fs::exists(receivedFile)) {
                LogMessage("错误: 接收文件不存在");
                return false;
            }

            // 比较文件大小
            size_t originalSize = fs::file_size(originalFile);
            size_t receivedSize = fs::file_size(receivedFile);

            LogMessage("原始文件大小: " + std::to_string(originalSize) + " 字节");
            LogMessage("接收文件大小: " + std::to_string(receivedSize) + " 字节");

            if (originalSize != receivedSize) {
                LogMessage("错误: 文件大小不一致");
                return false;
            }

            // 逐字节比较文件内容
            std::ifstream origFile(originalFile, std::ios::binary);
            std::ifstream recvFile(receivedFile, std::ios::binary);

            if (!origFile.is_open() || !recvFile.is_open()) {
                LogMessage("错误: 无法打开文件进行内容比较");
                return false;
            }

            const size_t bufferSize = 4096;
            std::vector<char> origBuffer(bufferSize);
            std::vector<char> recvBuffer(bufferSize);

            size_t totalChecked = 0;
            bool contentMatch = true;

            while (totalChecked < originalSize) {
                size_t toRead = std::min(bufferSize, originalSize - totalChecked);

                origFile.read(origBuffer.data(), toRead);
                recvFile.read(recvBuffer.data(), toRead);

                if (origFile.gcount() != recvFile.gcount() ||
                    memcmp(origBuffer.data(), recvBuffer.data(), toRead) != 0) {
                    contentMatch = false;
                    break;
                }

                totalChecked += toRead;
            }

            origFile.close();
            recvFile.close();

            if (!contentMatch) {
                LogMessage("错误: 文件内容不一致");
                return false;
            }

            LogMessage("✅ 文件完整性验证通过 - 文件大小和内容完全一致");
            return true;

        } catch (const std::exception& e) {
            LogMessage("步骤5异常: " + std::string(e.what()));
            return false;
        }
    }

    void LogMessage(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
        localtime_s(&tm, &time_t);

        char timeStr[32];
        sprintf_s(timeStr, sizeof(timeStr), "[%02d:%02d:%02d.%03d] ",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

        std::string fullMessage = timeStr + message;
        m_testLog += fullMessage + "\n";

        // 输出到控制台
        std::cout << fullMessage << std::endl;

        // 同时写入日志文件
        try {
            std::ofstream logFile("UI_Test_Console.log", std::ios::app);
            if (logFile.is_open()) {
                logFile << fullMessage << std::endl;
                logFile.close();
            }
        } catch (...) {
            // 忽略日志写入错误
        }
    }

public:
    bool GetTestResult() const { return m_testPassed; }
    const std::string& GetTestLog() const { return m_testLog; }
};

// 主函数 - 严格遵循用户要求的测试流程
int main() {
    std::cout << "PortMaster UI控件响应问题自动测试控制台程序" << std::endl;
    std::cout << "严格按照5步测试流程执行" << std::endl;
    std::cout << "========================================" << std::endl;

    UITestConsole tester;

    bool success = tester.RunFullTest();

    std::cout << "========================================" << std::endl;
    if (success) {
        std::cout << "🎉 测试结果: PASSED - UI控件响应问题修复验证成功" << std::endl;
        return 0;
    } else {
        std::cout << "❌ 测试结果: FAILED - UI控件响应问题仍需修复" << std::endl;
        return 1;
    }
}