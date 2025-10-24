// 可靠传输独立测试工具
// 完全脱离MFC，独立测试ReliableChannel功能

#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <Windows.h>

// 项目头文件
#include "Transport/ITransport.h"
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include "Protocol/FrameCodec.h"

using namespace std;

// 全局状态
atomic<bool> g_sendComplete(false);
atomic<bool> g_recvComplete(false);
atomic<bool> g_hasError(false);
string g_errorMsg;
vector<uint8_t> g_receivedData;
uint64_t g_totalBytes = 0;
uint64_t g_receivedBytes = 0;

// 回调函数
void OnError(const string& msg)
{
    cout << "[错误] " << msg << endl;
    g_errorMsg = msg;
    g_hasError = true;
}

void OnSendProgress(uint64_t current, uint64_t total, const string& filename)
{
    static uint64_t lastPrint = 0;
    if (current - lastPrint >= 10240 || current == total)
    {
        printf("[发送] %llu / %llu (%.1f%%)\n", current, total, total > 0 ? current * 100.0 / total : 0);
        lastPrint = current;
    }
}

void OnRecvProgress(uint64_t current, uint64_t total, const string& filename)
{
    g_receivedBytes = current;
    g_totalBytes = total;

    static uint64_t lastPrint = 0;
    if (current - lastPrint >= 10240 || current == total)
    {
        printf("[接收] %llu / %llu (%.1f%%)\n", current, total, total > 0 ? current * 100.0 / total : 0);
        lastPrint = current;
    }
}

void OnStateChanged(ReliableState state)
{
    const char* stateNames[] = {
        "IDLE", "STARTING", "SENDING", "ENDING",
        "READY", "RECEIVING", "DONE", "FAILED"
    };

    int idx = static_cast<int>(state);
    if (idx >= 0 && idx < 8)
    {
        cout << "[状态] " << stateNames[idx] << endl;
    }

    if (state == ReliableState::RELIABLE_DONE)
    {
        g_recvComplete = true;
    }
    else if (state == ReliableState::RELIABLE_FAILED)
    {
        g_hasError = true;
    }
}

// 读取文件
bool ReadFile(const string& path, vector<uint8_t>& data)
{
    ifstream file(path, ios::binary);
    if (!file)
    {
        cout << "[错误] 无法打开文件: " << path << endl;
        return false;
    }

    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);

    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    cout << "[信息] 读取文件: " << path << ", 大小: " << size << " 字节" << endl;
    return true;
}

// 写入文件
bool WriteFile(const string& path, const vector<uint8_t>& data)
{
    ofstream file(path, ios::binary);
    if (!file)
    {
        cout << "[错误] 无法创建文件: " << path << endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    cout << "[信息] 保存文件: " << path << ", 大小: " << data.size() << " 字节" << endl;
    return true;
}

int main(int argc, char* argv[])
{
    // 设置控制台UTF-8输出
    SetConsoleOutputCP(CP_UTF8);

    cout << "========================================" << endl;
    cout << "可靠传输独立测试工具" << endl;
    cout << "========================================" << endl;

    // 解析参数
    string inputFile = "招商证券股份有限公司融资融券业务合同.pdf";
    string outputFile = "test_output.pdf";

    if (argc >= 2)
        inputFile = argv[1];
    if (argc >= 3)
        outputFile = argv[2];

    cout << "输入文件: " << inputFile << endl;
    cout << "输出文件: " << outputFile << endl;
    cout << endl;

    // 1. 读取测试文件
    cout << "步骤1: 读取测试文件..." << endl;
    vector<uint8_t> originalData;
    if (!ReadFile(inputFile, originalData))
        return 1;
    cout << endl;

    // 2. 创建Loopback传输层
    cout << "步骤2: 创建Loopback传输层..." << endl;
    auto transport = make_shared<LoopbackTransport>();

    TransportConfig config;
    config.maxQueueSize = 100;
    config.processInterval = 1;

    if (transport->Open(config) != TransportError::Success)
    {
        cout << "[错误] 打开传输层失败" << endl;
        return 1;
    }
    cout << "[成功] Loopback传输层已创建，队列大小: " << config.maxQueueSize << endl;
    cout << endl;

    // 3. 创建可靠传输通道
    cout << "步骤3: 创建可靠传输通道..." << endl;
    auto frameCodec = make_shared<FrameCodec>();
    auto reliableChannel = make_shared<ReliableChannel>(transport, frameCodec);

    ReliableConfig reliableConfig;
    reliableConfig.windowSize = 1;
    reliableConfig.maxRetries = 10;
    reliableConfig.ackTimeout = 1000;
    reliableConfig.handshakeTimeout = 5000;

    reliableChannel->SetConfig(reliableConfig);
    cout << "[成功] 可靠传输通道已配置" << endl;
    cout << "  - 窗口大小: " << reliableConfig.windowSize << endl;
    cout << "  - 最大重试: " << reliableConfig.maxRetries << endl;
    cout << "  - ACK超时: " << reliableConfig.ackTimeout << "ms" << endl;
    cout << endl;

    // 4. 注册回调
    cout << "步骤4: 注册回调..." << endl;
    reliableChannel->SetErrorCallback(OnError);
    reliableChannel->SetSendProgressCallback(OnSendProgress);
    reliableChannel->SetReceiveProgressCallback(OnRecvProgress);
    reliableChannel->SetStateCallback(OnStateChanged);
    cout << "[成功] 回调已注册" << endl;
    cout << endl;

    // 5. 启动接收
    cout << "步骤5: 启动接收端..." << endl;
    if (!reliableChannel->StartReceive())
    {
        cout << "[错误] 启动接收失败" << endl;
        return 1;
    }
    cout << "[成功] 接收端已启动" << endl;
    cout << endl;

    // 6. 保存临时文件并发送
    cout << "步骤6: 发送文件..." << endl;
    string tempFile = "test_temp.bin";
    if (!WriteFile(tempFile, originalData))
        return 1;

    bool sendResult = reliableChannel->SendFile(tempFile, inputFile);
    DeleteFileA(tempFile.c_str());

    if (!sendResult)
    {
        cout << "[错误] 发送失败: " << g_errorMsg << endl;
        return 1;
    }
    cout << "[信息] 发送请求已提交" << endl;
    cout << endl;

    // 7. 等待完成
    cout << "步骤7: 等待传输完成..." << endl;
    int waitSeconds = 0;
    const int maxWait = 60;

    while (!g_recvComplete && !g_hasError && waitSeconds < maxWait)
    {
        Sleep(1000);
        waitSeconds++;

        if (waitSeconds % 5 == 0)
        {
            cout << "[等待] " << waitSeconds << " 秒..." << endl;
        }
    }

    if (g_hasError)
    {
        cout << "[失败] 传输出错: " << g_errorMsg << endl;
        return 1;
    }

    if (waitSeconds >= maxWait)
    {
        cout << "[失败] 传输超时" << endl;
        return 1;
    }

    cout << "[成功] 传输完成" << endl;
    cout << endl;

    // 8. 获取接收数据
    cout << "步骤8: 验证数据..." << endl;
    vector<uint8_t> receivedData = reliableChannel->GetReceivedData();

    cout << "原始大小: " << originalData.size() << " 字节" << endl;
    cout << "接收大小: " << receivedData.size() << " 字节" << endl;

    if (receivedData.size() != originalData.size())
    {
        cout << "[失败] 文件大小不匹配！" << endl;
        return 1;
    }

    if (receivedData != originalData)
    {
        cout << "[失败] 文件内容不匹配！" << endl;
        for (size_t i = 0; i < min(receivedData.size(), originalData.size()); i++)
        {
            if (receivedData[i] != originalData[i])
            {
                printf("[错误] 第一个不匹配位置: %zu, 期望: 0x%02X, 实际: 0x%02X\n",
                       i, originalData[i], receivedData[i]);
                break;
            }
        }
        return 1;
    }

    cout << "[成功] 文件完全一致" << endl;
    cout << endl;

    // 9. 保存结果
    cout << "步骤9: 保存结果..." << endl;
    if (!WriteFile(outputFile, receivedData))
        return 1;
    cout << endl;

    // 10. 统计信息
    ReliableStats stats = reliableChannel->GetStats();
    cout << "========================================" << endl;
    cout << "传输统计" << endl;
    cout << "========================================" << endl;
    cout << "发送包数: " << stats.packetsSent << endl;
    cout << "接收包数: " << stats.packetsReceived << endl;
    cout << "重传包数: " << stats.packetsRetransmitted << endl;
    cout << "发送字节: " << stats.bytesSent << endl;
    cout << "接收字节: " << stats.bytesReceived << endl;
    cout << "错误次数: " << stats.errors << endl;
    cout << endl;

    // 11. 清理
    reliableChannel->StopReceive();
    transport->Close();

    cout << "========================================" << endl;
    cout << "✅ 测试成功！" << endl;
    cout << "========================================" << endl;

    return 0;
}