// test_protocol_diagnosis.cpp - 协议层数据传输问题诊断程序
#include "include/framework.h"
#include "include/pch.h"
#include "Protocol/ReliableChannel.h"
#include "Transport/LoopbackTransport.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

// 诊断可靠传输协议的数据传输完整性问题
class ProtocolDiagnosis {
private:
    unique_ptr<ReliableChannel> m_sender;
    unique_ptr<ReliableChannel> m_receiver;
    unique_ptr<LoopbackTransport> m_transport;

public:
    ProtocolDiagnosis() {
        // 创建环回传输用于测试
        m_transport = make_unique<LoopbackTransport>();

        // 创建发送方和接收方
        m_sender = make_unique<ReliableChannel>();
        m_receiver = make_unique<ReliableChannel>();

        cout << "[INFO] 协议诊断测试初始化完成" << endl;
    }

    // 测试1：验证基础连接和握手
    bool TestBasicConnection() {
        cout << "\n=== 测试1：基础连接和握手验证 ===" << endl;

        // 启动传输层
        if (!m_transport->Open()) {
            cout << "[ERROR] 传输层打开失败" << endl;
            return false;
        }

        // 连接发送方
        if (!m_sender->Connect(m_transport.get())) {
            cout << "[ERROR] 发送方连接失败" << endl;
            return false;
        }

        // 连接接收方
        if (!m_receiver->Connect(m_transport.get())) {
            cout << "[ERROR] 接收方连接失败" << endl;
            return false;
        }

        // 等待握手完成
        this_thread::sleep_for(chrono::milliseconds(500));

        bool senderConnected = m_sender->IsConnected();
        bool receiverConnected = m_receiver->IsConnected();

        cout << "[INFO] 发送方连接状态: " << (senderConnected ? "已连接" : "未连接") << endl;
        cout << "[INFO] 接收方连接状态: " << (receiverConnected ? "已连接" : "未连接") << endl;

        if (senderConnected && receiverConnected) {
            cout << "[SUCCESS] 基础连接测试通过" << endl;
            return true;
        } else {
            cout << "[FAIL] 基础连接测试失败" << endl;
            return false;
        }
    }

    // 测试2：小文件完整传输验证
    bool TestSmallFileTransfer() {
        cout << "\n=== 测试2：小文件完整传输验证 ===" << endl;

        // 创建测试文件内容 (1KB)
        const int testSize = 1024;
        vector<uint8_t> testData(testSize);
        for (int i = 0; i < testSize; i++) {
            testData[i] = static_cast<uint8_t>(i % 256);
        }

        string testFile = "test_small_file.dat";

        // 写入测试文件
        ofstream outFile(testFile, ios::binary);
        outFile.write(reinterpret_cast<char*>(testData.data()), testData.size());
        outFile.close();

        cout << "[INFO] 创建测试文件: " << testFile << " (" << testData.size() << " 字节)" << endl;

        // 接收方设置文件保存路径
        string receivedFile = "received_small_file.dat";
        m_receiver->SetReceiveFilePath(receivedFile);

        // 监控传输状态
        bool transferComplete = false;
        int64_t bytesTransmitted = 0;
        int64_t totalBytes = testData.size();

        // 发送方进度回调
        auto senderProgress = [&](int64_t current, int64_t total) {
            bytesTransmitted = current;
            cout << "[SEND] 进度: " << current << "/" << total << " ("
                 << (total > 0 ? (current * 100 / total) : 0) << "%)" << endl;
        };

        // 接收方进度回调
        auto receiverProgress = [&](int64_t current, int64_t total) {
            cout << "[RECV] 进度: " << current << "/" << total << " ("
                 << (total > 0 ? (current * 100 / total) : 0) << "%)" << endl;
        };

        // 开始传输
        cout << "[INFO] 开始小文件传输..." << endl;
        auto startTime = chrono::steady_clock::now();

        // 启动发送和接收（在实际应用中这些是异步的）
        thread senderThread([&]() {
            if (m_sender->SendFile(testFile, senderProgress)) {
                cout << "[SEND] 文件发送完成" << endl;
                transferComplete = true;
            } else {
                cout << "[SEND] 文件发送失败" << endl;
            }
        });

        thread receiverThread([&]() {
            if (m_receiver->ReceiveFile(receivedFile, receiverProgress)) {
                cout << "[RECV] 文件接收完成" << endl;
            } else {
                cout << "[RECV] 文件接收失败" << endl;
            }
        });

        // 等待传输完成或超时
        const int maxWaitSeconds = 30;
        for (int i = 0; i < maxWaitSeconds; i++) {
            this_thread::sleep_for(chrono::seconds(1));

            if (transferComplete) {
                break;
            }

            // 检查传输状态
            if (m_sender->GetTransferStats().state == ReliableChannel::ReliableState::RELIABLE_DONE &&
                m_receiver->GetTransferStats().state == ReliableChannel::ReliableState::RELIABLE_DONE) {
                cout << "[INFO] 双方都显示传输完成" << endl;
                break;
            }
        }

        senderThread.join();
        receiverThread.join();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

        cout << "[INFO] 传输耗时: " << duration.count() << " 毫秒" << endl;

        // 验证文件完整性
        ifstream receivedFileStream(receivedFile, ios::binary | ios::ate);
        if (receivedFileStream) {
            int64_t receivedSize = receivedFileStream.tellg();
            receivedFileStream.close();

            cout << "[INFO] 原始文件大小: " << testData.size() << " 字节" << endl;
            cout << "[INFO] 接收文件大小: " << receivedSize << " 字节" << endl;

            if (receivedSize == testData.size()) {
                cout << "[SUCCESS] 小文件传输测试通过" << endl;

                // 清理测试文件
                remove(testFile.c_str());
                remove(receivedFile.c_str());

                return true;
            } else {
                cout << "[FAIL] 文件大小不匹配: 丢失 " << (testData.size() - receivedSize) << " 字节" << endl;
            }
        } else {
            cout << "[FAIL] 无法打开接收文件" << endl;
        }

        // 清理测试文件
        remove(testFile.c_str());
        remove(receivedFile.c_str());

        return false;
    }

    // 测试3：模拟AutoTest的大文件传输问题
    bool TestLargeFileIssue() {
        cout << "\n=== 测试3：模拟大文件传输问题诊断 ===" << endl;

        // 创建较大的测试文件 (模拟AutoTest的1MB文件)
        const int testSize = 1024 * 1024; // 1MB
        vector<uint8_t> testData(testSize);

        // 生成测试数据模式
        for (int i = 0; i < testSize; i++) {
            testData[i] = static_cast<uint8_t>((i * 7 + 13) % 256); // 伪随机模式
        }

        string testFile = "test_large_file.dat";

        // 写入测试文件
        ofstream outFile(testFile, ios::binary);
        outFile.write(reinterpret_cast<char*>(testData.data()), testData.size());
        outFile.close();

        cout << "[INFO] 创建大测试文件: " << testFile << " (" << testData.size() << " 字节)" << endl;

        // 接收方设置文件保存路径
        string receivedFile = "received_large_file.dat";
        m_receiver->SetReceiveFilePath(receivedFile);

        // 详细监控传输过程
        bool sendCompleted = false;
        bool receiveCompleted = false;
        int64_t lastSenderProgress = 0;
        int64_t lastReceiverProgress = 0;
        int progressStallCount = 0;

        auto senderProgress = [&](int64_t current, int64_t total) {
            if (current != lastSenderProgress) {
                cout << "[SEND] 进度: " << current << "/" << total << " ("
                     << (total > 0 ? (current * 100 / total) : 0) << "%)" << endl;
                lastSenderProgress = current;
                progressStallCount = 0; // 重置停滞计数
            } else {
                progressStallCount++;
                if (progressStallCount > 10) {
                    cout << "[WARNING] 发送进度停滞超过10次更新" << endl;
                }
            }
        };

        auto receiverProgress = [&](int64_t current, int64_t total) {
            if (current != lastReceiverProgress) {
                cout << "[RECV] 进度: " << current << "/" << total << " ("
                     << (total > 0 ? (current * 100 / total) : 0) << "%)" << endl;
                lastReceiverProgress = current;
            }
        };

        // 开始传输
        cout << "[INFO] 开始大文件传输诊断..." << endl;
        auto startTime = chrono::steady_clock::now();

        thread senderThread([&]() {
            cout << "[SEND] 开始发送文件..." << endl;
            bool result = m_sender->SendFile(testFile, senderProgress);
            cout << "[SEND] 发送结果: " << (result ? "成功" : "失败") << endl;
            if (result) {
                sendCompleted = true;
            }
        });

        thread receiverThread([&]() {
            cout << "[RECV] 开始接收文件..." << endl;
            bool result = m_receiver->ReceiveFile(receivedFile, receiverProgress);
            cout << "[RECV] 接收结果: " << (result ? "成功" : "失败") << endl;
            if (result) {
                receiveCompleted = true;
            }
        });

        // 监控传输状态，详细记录问题
        const int maxWaitSeconds = 120; // 2分钟超时
        bool transferTimedOut = false;

        for (int i = 0; i < maxWaitSeconds; i++) {
            this_thread::sleep_for(chrono::seconds(1));

            // 每隔10秒报告状态
            if (i % 10 == 0) {
                cout << "[INFO] 传输进行中... " << i << "秒" << endl;
                cout << "[INFO] 发送方状态: " << static_cast<int>(m_sender->GetTransferStats().state) << endl;
                cout << "[INFO] 接收方状态: " << static_cast<int>(m_receiver->GetTransferStats().state) << endl;
                cout << "[INFO] 发送进度: " << lastSenderProgress << "/" << testData.size() << endl;
                cout << "[INFO] 接收进度: " << lastReceiverProgress << "/" << testData.size() << endl;
            }

            // 检查是否双方都完成
            if (sendCompleted && receiveCompleted) {
                cout << "[INFO] 双方都报告传输完成" << endl;
                break;
            }

            // 检查是否有异常状态
            auto senderStats = m_sender->GetTransferStats();
            auto receiverStats = m_receiver->GetTransferStats();

            if (senderStats.state == ReliableChannel::ReliableState::RELIABLE_FAILED ||
                receiverStats.state == ReliableChannel::ReliableState::RELIABLE_FAILED) {
                cout << "[ERROR] 检测到传输失败状态" << endl;
                break;
            }
        }

        if (i >= maxWaitSeconds - 1) {
            transferTimedOut = true;
            cout << "[WARNING] 传输超时！" << endl;
        }

        senderThread.join();
        receiverThread.join();

        auto endTime = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(endTime - startTime);

        cout << "[INFO] 传输耗时: " << duration.count() << " 秒" << endl;
        cout << "[INFO] 最终发送进度: " << lastSenderProgress << "/" << testData.size() << endl;
        cout << "[INFO] 最终接收进度: " << lastReceiverProgress << "/" << testData.size() << endl;
        cout << "[INFO] 发送完成状态: " << (sendCompleted ? "是" : "否") << endl;
        cout << "[INFO] 接收完成状态: " << (receiveCompleted ? "是" : "否") << endl;
        cout << "[INFO] 传输超时: " << (transferTimedOut ? "是" : "否") << endl;

        // 验证最终结果
        ifstream receivedFileStream(receivedFile, ios::binary | ios::ate);
        bool testPassed = false;

        if (receivedFileStream) {
            int64_t receivedSize = receivedFileStream.tellg();
            receivedFileStream.close();

            cout << "[INFO] 最终文件完整性验证:" << endl;
            cout << "[INFO]   原始文件大小: " << testData.size() << " 字节" << endl;
            cout << "[INFO]   接收文件大小: " << receivedSize << " 字节" << endl;
            cout << "[INFO]   数据丢失: " << (testData.size() - receivedSize) << " 字节 ("
                 << ((testData.size() - receivedSize) * 100.0 / testData.size()) << "%)" << endl;

            if (receivedSize == testData.size()) {
                cout << "[SUCCESS] 大文件传输测试通过" << endl;
                testPassed = true;
            } else {
                cout << "[FAIL] 大文件传输存在数据丢失问题" << endl;

                // 模拟AutoTest的问题场景
                if (receivedSize < testData.size() * 0.1) {
                    cout << "[CRITICAL] 检测到类似AutoTest的严重数据丢失问题 (<10%)" << endl;
                }
            }
        } else {
            cout << "[FAIL] 无法打开接收文件进行验证" << endl;
        }

        // 清理测试文件
        remove(testFile.c_str());
        remove(receivedFile.c_str());

        return testPassed;
    }

    // 运行完整诊断流程
    void RunDiagnosis() {
        cout << "=== PortMaster 协议层数据传输问题诊断程序 ===" << endl;
        cout << "目标：识别AutoTest数据传输不完整的根本原因" << endl;
        cout << "现象：发送1,113,432字节，只接收50,176字节 (4.5%)" << endl;

        bool allTestsPassed = true;

        // 测试1：基础连接
        if (!TestBasicConnection()) {
            cout << "\n[CRITICAL] 基础连接测试失败，停止后续测试" << endl;
            return;
        }

        // 测试2：小文件传输
        if (!TestSmallFileTransfer()) {
            cout << "\n[WARNING] 小文件传输测试失败，继续进行大文件测试" << endl;
            allTestsPassed = false;
        }

        // 测试3：大文件问题诊断
        if (!TestLargeFileIssue()) {
            cout << "\n[CRITICAL] 大文件传输测试失败，确认存在数据完整性问题" << endl;
            allTestsPassed = false;
        }

        // 总结诊断结果
        cout << "\n=== 诊断结果总结 ===" << endl;
        if (allTestsPassed) {
            cout << "[INFO] 所有测试通过，协议层工作正常" << endl;
            cout << "[INFO] AutoTest问题可能源于其他因素" << endl;
        } else {
            cout << "[CRITICAL] 确认协议层存在数据完整性问题" << endl;
            cout << "[INFO] 需要进一步分析ProcessEndFrame和数据传输逻辑" << endl;

            cout << "\n=== 推荐的修复方向 ===" << endl;
            cout << "1. 检查ProcessEndFrame的不完整传输处理逻辑" << endl;
            cout << "2. 分析发送方提前发送END帧的原因" << endl;
            cout << "3. 验证重传机制和超时处理的有效性" << endl;
            cout << "4. 考虑在ProcessEndFrame中添加强制完成机制" << endl;
        }

        cout << "\n诊断程序执行完成" << endl;
    }
};

int main() {
    try {
        ProtocolDiagnosis diagnosis;
        diagnosis.RunDiagnosis();
    } catch (const exception& e) {
        cout << "[ERROR] 诊断程序异常: " << e.what() << endl;
        return 1;
    }

    return 0;
}