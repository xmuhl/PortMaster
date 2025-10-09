// 可靠传输自动化测试工具
// 引入现有项目源码，自动执行完整传输流程

#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <Windows.h>

// 引入现有项目头文件
#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include "Protocol/FrameCodec.h"

using namespace std;

// 全局状态
atomic<bool> g_completed(false);
atomic<bool> g_failed(false);
string g_errorMsg;

// 错误回调
void OnError(const string& msg)
{
    cerr << "[ERROR] " << msg << endl;
    g_errorMsg = msg;
    g_failed = true;
}

// 进度回调
void OnSendProgress(uint64_t current, uint64_t total, const string& filename)
{
    static uint64_t last = 0;
    if (current - last >= 50000 || current == total)
    {
        printf("[SEND] %llu / %llu (%.1f%%)\r", current, total, current * 100.0 / total);
        last = current;
        if (current == total) printf("\n");
    }
}

void OnRecvProgress(uint64_t current, uint64_t total, const string& filename)
{
    static uint64_t last = 0;
    if (current - last >= 50000 || current == total)
    {
        printf("[RECV] %llu / %llu (%.1f%%)\r", current, total, current * 100.0 / total);
        last = current;
        if (current == total) printf("\n");
    }
}

// 状态回调
void OnStateChanged(ReliableState state)
{
    if (state == ReliableState::RELIABLE_DONE)
    {
        cout << "[INFO] Transmission completed" << endl;
        g_completed = true;
    }
    else if (state == ReliableState::RELIABLE_FAILED)
    {
        cout << "[ERROR] Transmission failed" << endl;
        g_failed = true;
    }
}

// 读取文件
bool ReadFile(const string& path, vector<uint8_t>& data)
{
    ifstream file(path, ios::binary);
    if (!file)
    {
        cerr << "[ERROR] Cannot open file: " << path << endl;
        return false;
    }

    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);

    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    cout << "[INFO] Read file: " << path << ", size: " << size << " bytes" << endl;
    return true;
}

// 写入文件
bool WriteFile(const string& path, const vector<uint8_t>& data)
{
    ofstream file(path, ios::binary);
    if (!file)
    {
        cerr << "[ERROR] Cannot create file: " << path << endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    cout << "[INFO] Saved file: " << path << ", size: " << data.size() << " bytes" << endl;
    return true;
}

int main(int argc, char* argv[])
{
    cout << "========================================" << endl;
    cout << "Reliable Transmission Auto Test" << endl;
    cout << "========================================" << endl;

    // 测试文件路径
    string inputFile = "招商证券股份有限公司融资融券业务合同.pdf";
    string outputFile = "test_received.pdf";
    string tempFile = "test_temp.bin";

    if (argc >= 2) inputFile = argv[1];
    if (argc >= 3) outputFile = argv[2];

    cout << "[CONF] Input:  " << inputFile << endl;
    cout << "[CONF] Output: " << outputFile << endl;
    cout << endl;

    // 步骤1: 读取测试文件
    cout << "[STEP 1] Reading test file..." << endl;
    vector<uint8_t> originalData;
    if (!ReadFile(inputFile, originalData))
    {
        return 1;
    }
    cout << endl;

    // 步骤2: 创建Loopback传输层（本地回路测试）
    cout << "[STEP 2] Creating Loopback transport..." << endl;
    auto transport = make_shared<LoopbackTransport>();

    TransportConfig transportConfig;
    transportConfig.maxQueueSize = 100;
    transportConfig.processInterval = 1;

    if (transport->Open(transportConfig) != TransportError::Success)
    {
        cerr << "[ERROR] Failed to open transport" << endl;
        return 1;
    }
    cout << "[OK] Loopback transport created, queue size: " << transportConfig.maxQueueSize << endl;
    cout << endl;

    // 步骤3: 创建可靠传输通道
    cout << "[STEP 3] Creating reliable channel..." << endl;
    auto frameCodec = make_shared<FrameCodec>();
    auto reliableChannel = make_shared<ReliableChannel>(transport, frameCodec);

    ReliableConfig reliableConfig;
    reliableConfig.windowSize = 1;
    reliableConfig.maxRetries = 10;
    reliableConfig.ackTimeout = 1000;
    reliableConfig.handshakeTimeout = 5000;

    reliableChannel->SetConfig(reliableConfig);
    reliableChannel->SetErrorCallback(OnError);
    reliableChannel->SetSendProgressCallback(OnSendProgress);
    reliableChannel->SetReceiveProgressCallback(OnRecvProgress);
    reliableChannel->SetStateCallback(OnStateChanged);

    cout << "[OK] Reliable channel configured" << endl;
    cout << "     - Window size: " << reliableConfig.windowSize << endl;
    cout << "     - Max retries: " << reliableConfig.maxRetries << endl;
    cout << "     - ACK timeout: " << reliableConfig.ackTimeout << "ms" << endl;
    cout << endl;

    // 步骤4: 启动接收端
    cout << "[STEP 4] Starting receiver..." << endl;
    if (!reliableChannel->StartReceive())
    {
        cerr << "[ERROR] Failed to start receiver" << endl;
        return 1;
    }
    cout << "[OK] Receiver started" << endl;
    cout << endl;

    // 步骤5: 发送文件
    cout << "[STEP 5] Sending file..." << endl;
    if (!WriteFile(tempFile, originalData))
    {
        return 1;
    }

    bool sendResult = reliableChannel->SendFile(tempFile, inputFile);
    DeleteFileA(tempFile.c_str());

    if (!sendResult)
    {
        cerr << "[ERROR] Send failed: " << g_errorMsg << endl;
        return 1;
    }
    cout << "[OK] Send request submitted" << endl;
    cout << endl;

    // 步骤6: 等待传输完成
    cout << "[STEP 6] Waiting for completion..." << endl;
    int waitSeconds = 0;
    const int maxWait = 60;

    while (!g_completed && !g_failed && waitSeconds < maxWait)
    {
        Sleep(1000);
        waitSeconds++;
    }

    if (g_failed)
    {
        cerr << "[FAIL] Transmission failed: " << g_errorMsg << endl;
        return 1;
    }

    if (waitSeconds >= maxWait)
    {
        cerr << "[FAIL] Transmission timeout" << endl;
        return 1;
    }

    cout << "[OK] Transmission completed in " << waitSeconds << " seconds" << endl;
    cout << endl;

    // 步骤7: 接收数据并验证
    cout << "[STEP 7] Verifying received data..." << endl;
    vector<uint8_t> receivedData = reliableChannel->GetReceivedData();

    cout << "[INFO] Original size: " << originalData.size() << " bytes" << endl;
    cout << "[INFO] Received size: " << receivedData.size() << " bytes" << endl;

    if (receivedData.size() != originalData.size())
    {
        cerr << "[FAIL] Size mismatch!" << endl;
        return 1;
    }

    if (receivedData != originalData)
    {
        cerr << "[FAIL] Content mismatch!" << endl;
        for (size_t i = 0; i < min(receivedData.size(), originalData.size()); i++)
        {
            if (receivedData[i] != originalData[i])
            {
                printf("[ERROR] First mismatch at offset %zu: expected 0x%02X, got 0x%02X\n",
                       i, originalData[i], receivedData[i]);
                break;
            }
        }
        return 1;
    }

    cout << "[OK] Data verified - size and content match perfectly" << endl;
    cout << endl;

    // 步骤8: 保存接收的文件
    cout << "[STEP 8] Saving received file..." << endl;
    if (!WriteFile(outputFile, receivedData))
    {
        return 1;
    }
    cout << endl;

    // 步骤9: 显示统计信息
    ReliableStats stats = reliableChannel->GetStats();
    cout << "========================================" << endl;
    cout << "Statistics" << endl;
    cout << "========================================" << endl;
    cout << "Packets sent:         " << stats.packetsSent << endl;
    cout << "Packets received:     " << stats.packetsReceived << endl;
    cout << "Packets retransmitted:" << stats.packetsRetransmitted << endl;
    cout << "Bytes sent:           " << stats.bytesSent << endl;
    cout << "Bytes received:       " << stats.bytesReceived << endl;
    cout << "Errors:               " << stats.errors << endl;
    cout << endl;

    // 清理
    reliableChannel->StopReceive();
    transport->Close();

    cout << "========================================" << endl;
    cout << "TEST PASSED" << endl;
    cout << "========================================" << endl;

    return 0;
}