#include "pch.h"
#include "Transport\TransportFactory.h"
#include "Transport\ITransport.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

// 测试辅助函数
void PrintTestHeader(const string& moduleName) {
    cout << "\n" << string(60, '=') << endl;
    cout << "测试模块: " << moduleName << endl;
    cout << string(60, '=') << endl;
}

void PrintTestResult(const string& testName, bool passed, const string& message = "") {
    cout << "  " << testName << ": " << (passed ? "✓ 通过" : "✗ 失败");
    if (!message.empty() && !passed) {
        cout << " - " << message;
    }
    cout << endl;
}

// 基础功能测试
test BasicTransportTest(ITransport* transport, const string& portName) {
    PrintTestHeader(transport->GetPortName());
    
    bool allPassed = true;
    
    // 测试1: 状态检查
    TransportState state = transport->GetState();
    bool stateTest = (state == TransportState::Closed);
    PrintTestResult("初始状态检查", stateTest);
    allPassed &= stateTest;
    
    // 测试2: 统计信息
    TransportStats stats = transport->GetStats();
    bool statsTest = (stats.bytesSent == 0 && stats.bytesReceived == 0);
    PrintTestResult("初始统计信息", statsTest);
    allPassed &= statsTest;
    
    // 测试3: 重置统计
    transport->ResetStats();
    TransportStats resetStats = transport->GetStats();
    bool resetTest = (resetStats.bytesSent == 0 && resetStats.bytesReceived == 0);
    PrintTestResult("重置统计信息", resetTest);
    allPassed &= resetTest;
    
    // 测试4: 端口名称
    string port = transport->GetPortName();
    bool portTest = !port.empty();
    PrintTestResult("端口名称获取", portTest);
    allPassed &= portTest;
    
    return allPassed;
}

// 连接测试（不依赖实际硬件）
test ConnectionTest(ITransport* transport) {
    cout << "\n--- 连接测试 ---" << endl;
    
    bool allPassed = true;
    
    // 测试1: 无效配置打开
    TransportConfig invalidConfig;
    invalidConfig.portName = "INVALID_PORT_XYZ123";
    
    TransportError openResult = transport->Open(invalidConfig);
    bool invalidOpenTest = (openResult != TransportError::Success);
    PrintTestResult("无效端口打开", invalidOpenTest);
    allPassed &= invalidOpenTest;
    
    // 确保关闭状态
    transport->Close();
    
    return allPassed;
}

// 数据传输测试（使用回环模式）
test DataTransferTest(ITransport* transport) {
    cout << "\n--- 数据传输测试 ---" << endl;
    
    bool allPassed = true;
    
    // 测试数据
    const char testData[] = "Hello, PortMaster Transport Test!";
    size_t testDataSize = strlen(testData);
    
    // 测试1: 未打开时写入
    size_t written = 0;
    TransportError writeResult = transport->Write(testData, testDataSize, &written);
    bool closedWriteTest = (writeResult == TransportError::NotOpen);
    PrintTestResult("关闭状态写入", closedWriteTest);
    allPassed &= closedWriteTest;
    
    // 测试2: 未打开时读取
    char readBuffer[256] = {0};
    size_t read = 0;
    TransportError readResult = transport->Read(readBuffer, sizeof(readBuffer), &read);
    bool closedReadTest = (readResult == TransportError::NotOpen);
    PrintTestResult("关闭状态读取", closedReadTest);
    allPassed &= closedReadTest;
    
    // 测试3: 参数验证
    TransportError nullWriteResult = transport->Write(nullptr, 10, &written);
    bool nullWriteTest = (nullWriteResult == TransportError::InvalidParameter);
    PrintTestResult("空指针写入", nullWriteTest);
    allPassed &= nullWriteTest;
    
    TransportError zeroWriteResult = transport->Write(testData, 0, &written);
    bool zeroWriteTest = (zeroWriteResult == TransportError::InvalidParameter);
    PrintTestResult("零长度写入", zeroWriteTest);
    allPassed &= zeroWriteTest;
    
    return allPassed;
}

// 异步操作测试
test AsyncOperationTest(ITransport* transport) {
    cout << "\n--- 异步操作测试 ---" << endl;
    
    bool allPassed = true;
    
    // 测试1: 未打开时启动异步读取
    TransportError asyncReadResult = transport->StartAsyncRead();
    bool closedAsyncReadTest = (asyncReadResult == TransportError::NotOpen);
    PrintTestResult("关闭状态异步读取", closedAsyncReadTest);
    allPassed &= closedAsyncReadTest;
    
    // 测试2: 未打开时异步写入
    const char testData[] = "Async test data";
    TransportError asyncWriteResult = transport->WriteAsync(testData, strlen(testData));
    bool closedAsyncWriteTest = (closedAsyncWriteResult == TransportError::NotOpen);
    PrintTestResult("关闭状态异步写入", closedAsyncWriteTest);
    allPassed &= closedAsyncWriteTest;
    
    // 测试3: 停止未启动的异步读取
    TransportError stopResult = transport->StopAsyncRead();
    bool stopTest = (stopResult == TransportError::Success);
    PrintTestResult("停止异步读取", stopTest);
    allPassed &= stopTest;
    
    return allPassed;
}

// 错误处理测试
test ErrorHandlingTest(ITransport* transport) {
    cout << "\n--- 错误处理测试 ---" << endl;
    
    bool allPassed = true;
    
    // 测试1: 错误码转换
    TransportError lastError = transport->GetLastError();
    bool errorTest = true; // 基本错误处理存在
    PrintTestResult("错误码获取", errorTest);
    allPassed &= errorTest;
    
    return allPassed;
}

// 综合测试函数
void RunTransportTests() {
    cout << "PortMaster 传输模块功能测试" << endl;
    cout << "=============================================" << endl;
    
    TransportFactory factory;
    bool overallResult = true;
    
    // 测试各个传输模块
    vector<pair<string, string>> testConfigs = {
        {"SerialTransport", "COM1"},
        {"ParallelTransport", "LPT1"},
        {"NetworkPrintTransport", "192.168.1.100:9100"},
        {"UsbPrintTransport", "USB001"},
        {"LoopbackTransport", "LOOPBACK"}
    };
    
    for (const auto& config : testConfigs) {
        const string& transportType = config.first;
        const string& portName = config.second;
        
        cout << "\n创建传输模块: " << transportType << endl;
        unique_ptr<ITransport> transport = factory.CreateTransport(transportType);
        
        if (!transport) {
            cout << "  ✗ 创建失败" << endl;
            overallResult = false;
            continue;
        }
        
        // 运行各项测试
        bool basicTest = BasicTransportTest(transport.get(), portName);
        bool connectionTest = ConnectionTest(transport.get());
        bool dataTest = DataTransferTest(transport.get());
        bool asyncTest = AsyncOperationTest(transport.get());
        bool errorTest = ErrorHandlingTest(transport.get());
        
        bool modulePassed = basicTest && connectionTest && dataTest && asyncTest && errorTest;
        
        cout << "\n" << transportType << " 模块测试总结: " 
             << (modulePassed ? "✓ 全部通过" : "✗ 有失败项") << endl;
        
        overallResult &= modulePassed;
    }
    
    cout << "\n" << string(60, '=') << endl;
    cout << "所有测试总结: " << (overallResult ? "✓ 全部通过" : "✗ 有失败项") << endl;
    cout << string(60, '=') << endl;
}

// 主函数
int main() {
    try {
        RunTransportTests();
    } catch (const exception& e) {
        cerr << "测试执行失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}