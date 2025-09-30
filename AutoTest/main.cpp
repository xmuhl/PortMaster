// 自动化测试工具 - 严格按照指定流程执行
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <Windows.h>

#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"
#include "Protocol/FrameCodec.h"

using namespace std;

// 全局状态
atomic<bool> g_completed(false);
atomic<bool> g_failed(false);
string g_errorMsg;

// 回调函数
void OnError(const string& msg)
{
    cerr << "[ERROR] " << msg << endl;
    g_errorMsg = msg;
    g_failed = true;
}

void OnSendProgress(int64_t current, int64_t total)
{
    static int64_t last = 0;
    if (current - last >= 50000 || current == total)
    {
        printf("[SEND] %lld / %lld (%.1f%%)\r", current, total, current * 100.0 / total);
        last = current;
        if (current == total) printf("\n");
    }
}

void OnRecvProgress(int64_t current, int64_t total)
{
    static int64_t last = 0;
    if (current - last >= 50000 || current == total)
    {
        printf("[RECV] %lld / %lld (%.1f%%)\r", current, total, current * 100.0 / total);
        last = current;
        if (current == total) printf("\n");
    }
}

void OnStateChanged(bool connected)
{
    if (!connected && !g_failed)
    {
        cout << "[INFO] Transmission completed" << endl;
        g_completed = true;
    }
}

// 使用Windows API读取文件（完全绕过C++标准库的编码问题）
bool ReadFileW(const wstring& widePath, vector<uint8_t>& data)
{
    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cerr << "[ERROR] Cannot open file (Error: " << GetLastError() << ")" << endl;
        return false;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE)
    {
        cerr << "[ERROR] Cannot get file size" << endl;
        CloseHandle(hFile);
        return false;
    }

    data.resize(fileSize);
    DWORD bytesRead = 0;
    if (!::ReadFile(hFile, data.data(), fileSize, &bytesRead, NULL))
    {
        cerr << "[ERROR] Cannot read file" << endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    cout << "[OK] Read file: " << fileSize << " bytes" << endl;
    return true;
}

bool ReadFile(const string& path, vector<uint8_t>& data)
{
    // 转换为宽字符串
    int wideSize = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, NULL, 0);
    wstring widePath(wideSize, 0);
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &widePath[0], wideSize);
    widePath.resize(wideSize - 1);

    return ReadFileW(widePath, data);
}

// 使用Windows API写入文件
bool WriteFileW(const wstring& widePath, const vector<uint8_t>& data)
{
    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cerr << "[ERROR] Cannot create file (Error: " << GetLastError() << ")" << endl;
        return false;
    }

    DWORD bytesWritten = 0;
    if (!::WriteFile(hFile, data.data(), (DWORD)data.size(), &bytesWritten, NULL))
    {
        cerr << "[ERROR] Cannot write file" << endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    cout << "[OK] Saved file: " << data.size() << " bytes" << endl;
    return true;
}

bool WriteFile(const string& path, const vector<uint8_t>& data)
{
    // 转换为宽字符串
    int wideSize = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, NULL, 0);
    wstring widePath(wideSize, 0);
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &widePath[0], wideSize);
    widePath.resize(wideSize - 1);

    return WriteFileW(widePath, data);
}

int main()
{
    cout << "======================================" << endl;
    cout << "Automated Reliable Transmission Test" << endl;
    cout << "======================================" << endl;
    cout << endl;

    // 测试配置 - 使用ASCII文件名避免编码问题
    wstring inputFileWide = L"test_input.pdf";
    string outputFile = "test_received.pdf";
    string tempFile = "test_temp.bin";
    string inputFileWin_display = "test_input.pdf";

    // 步骤1: 读取测试文件
    cout << "[STEP 1/8] Reading test file..." << endl;
    cout << "Input: " << inputFileWin_display << endl;

    vector<uint8_t> originalData;
    if (!ReadFileW(inputFileWide, originalData))
    {
        return 1;
    }
    cout << endl;

    // 步骤2: 创建本地回路传输层
    cout << "[STEP 2/8] Creating Loopback transport..." << endl;

    auto transport = make_shared<LoopbackTransport>();

    TransportConfig transportConfig;
    if (transport->Open(transportConfig) != TransportError::Success)
    {
        cerr << "[ERROR] Failed to open transport" << endl;
        return 1;
    }

    cout << "[OK] Loopback transport created" << endl;
    cout << endl;

    // 步骤3: 创建可靠传输通道（单通道架构）
    cout << "[STEP 3/8] Creating reliable channel..." << endl;

    auto reliableChannel = make_shared<ReliableChannel>();

    ReliableConfig reliableConfig;
    reliableConfig.windowSize = 16;  // 增加窗口大小以支持更多并行传输
    reliableConfig.maxRetries = 10;

    if (!reliableChannel->Initialize(transport, reliableConfig))
    {
        cerr << "[ERROR] Failed to initialize channel" << endl;
        return 1;
    }

    reliableChannel->SetErrorCallback(OnError);
    reliableChannel->SetProgressCallback(OnSendProgress);

    cout << "[OK] Reliable channel configured" << endl;
    cout << endl;

    // 步骤4: 连接通道
    cout << "[STEP 4/8] Connecting channel..." << endl;

    if (!reliableChannel->Connect())
    {
        cerr << "[ERROR] Failed to connect channel" << endl;
        return 1;
    }

    cout << "[OK] Channel connected" << endl;
    cout << endl;

    // 步骤5: 启动接收线程（必须在发送前启动）
    cout << "[STEP 5/8] Starting receive thread..." << endl;

    atomic<bool> recvStarted(false);
    atomic<bool> recvCompleted(false);
    atomic<bool> recvFailed(false);

    thread recvThread([&]() {
        recvStarted = true;
        cout << "[RECV] Receive thread started" << endl;

        bool result = reliableChannel->ReceiveFile(outputFile, OnRecvProgress);

        if (result)
        {
            cout << "[RECV] Receive completed successfully" << endl;
            recvCompleted = true;
        }
        else
        {
            cerr << "[RECV] Receive failed" << endl;
            recvFailed = true;
        }
    });

    // 等待接收线程启动
    while (!recvStarted)
    {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    cout << "[OK] Receive thread ready" << endl;
    cout << endl;

    // 步骤6: 发送文件
    cout << "[STEP 6/8] Sending file..." << endl;

    if (!WriteFile(tempFile, originalData))
    {
        return 1;
    }

    bool sendResult = reliableChannel->SendFile(tempFile, OnSendProgress);
    DeleteFileA(tempFile.c_str());

    if (!sendResult)
    {
        cerr << "[ERROR] Send failed: " << g_errorMsg << endl;
        recvThread.join();
        return 1;
    }

    cout << "[OK] Send completed" << endl;
    cout << endl;

    // 步骤7: 等待接收完成
    cout << "[STEP 7/8] Waiting for receive to complete..." << endl;

    recvThread.join();

    if (recvFailed)
    {
        cerr << "[ERROR] Receive failed" << endl;
        return 1;
    }

    if (!recvCompleted)
    {
        cerr << "[ERROR] Receive not completed" << endl;
        return 1;
    }

    cout << "[OK] Receive completed" << endl;
    cout << endl;

    // 步骤8: 验证文件
    cout << "[STEP 8/9] Verifying file..." << endl;

    vector<uint8_t> receivedData;
    if (!ReadFile(outputFile, receivedData))
    {
        return 1;
    }

    cout << "Original size: " << originalData.size() << " bytes" << endl;
    cout << "Received size: " << receivedData.size() << " bytes" << endl;

    if (receivedData.size() != originalData.size())
    {
        cerr << "[FAIL] Size mismatch!" << endl;
        return 1;
    }

    if (receivedData != originalData)
    {
        cerr << "[FAIL] Content mismatch!" << endl;
        return 1;
    }

    cout << "[OK] File verified - perfect match" << endl;
    cout << endl;

    // 步骤9: 显示统计
    cout << "[STEP 9/9] Statistics..." << endl;

    ReliableStats stats = reliableChannel->GetStats();

    cout << "Packets sent:         " << stats.packetsSent << endl;
    cout << "Packets retransmitted:" << stats.packetsRetransmitted << endl;
    cout << "Packets received:     " << stats.packetsReceived << endl;
    cout << "Total errors:         " << stats.errors << endl;
    cout << endl;

    // 清理
    reliableChannel->Disconnect();
    reliableChannel->Shutdown();
    transport->Close();

    cout << "======================================" << endl;
    cout << "TEST PASSED" << endl;
    cout << "======================================" << endl;

    return 0;
}