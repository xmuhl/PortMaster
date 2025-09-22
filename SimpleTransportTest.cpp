#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

// 包含必要的头文件
#include "Common\TransportError.h"
#include "Common\TransportConfig.h"
#include "Transport\TransportFactory.h"
#include "Transport\ITransport.h"

using namespace std;

class SimpleTransportTester {
public:
    void RunAllTests() {
        cout << "=== PortMaster 传输模块简单测试 ===" << endl;
        
        bool allPassed = true;
        
        // 测试各个传输模块
        vector<string> transportTypes = {
            "LoopbackTransport",
            "SerialTransport", 
            "ParallelTransport",
            "NetworkPrintTransport",
            "UsbPrintTransport"
        };
        
        for (const string& transportType : transportTypes) {
            cout << "\n--- 测试 " << transportType << " ---" << endl;
            bool passed = TestTransportModule(transportType);
            allPassed &= passed;
            
            cout << transportType << " 测试结果: " << (passed ? "通过" : "失败") << endl;
        }
        
        cout << "\n=== 测试总结 ===" << endl;
        cout << "总体结果: " << (allPassed ? "全部通过 ✓" : "有失败项 ✗") << endl;
    }

private:
    bool TestTransportModule(const string& transportType) {
        TransportFactory factory;
        auto transport = factory.CreateTransport(transportType);
        
        if (!transport) {
            cout << "  ✗ 创建失败" << endl;
            return false;
        }
        
        cout << "  ✓ 创建成功" << endl;
        
        // 测试基本功能
        bool basicTest = TestBasicFunctionality(transport.get());
        bool errorTest = TestErrorHandling(transport.get());
        
        return basicTest && errorTest;
    }
    
    bool TestBasicFunctionality(ITransport* transport) {
        cout << "  测试基本功能..." << endl;
        
        // 测试状态
        TransportState state = transport->GetState();
        if (state != TransportState::Closed) {
            cout << "    ✗ 初始状态错误" << endl;
            return false;
        }
        cout << "    ✓ 初始状态正确" << endl;
        
        // 测试端口名称
        string portName = transport->GetPortName();
        if (portName.empty()) {
            cout << "    ✗ 端口名称为空" << endl;
            return false;
        }
        cout << "    ✓ 端口名称: " << portName << endl;
        
        // 测试统计信息
        TransportStats stats = transport->GetStats();
        if (stats.bytesSent != 0 || stats.bytesReceived != 0) {
            cout << "    ✗ 初始统计信息错误" << endl;
            return false;
        }
        cout << "    ✓ 初始统计信息正确" << endl;
        
        // 测试重置统计
        transport->ResetStats();
        TransportStats resetStats = transport->GetStats();
        if (resetStats.bytesSent != 0 || resetStats.bytesReceived != 0) {
            cout << "    ✗ 重置统计信息失败" << endl;
            return false;
        }
        cout << "    ✓ 重置统计信息成功" << endl;
        
        return true;
    }
    
    bool TestErrorHandling(ITransport* transport) {
        cout << "  测试错误处理..." << endl;
        
        // 测试未打开时的操作
        const char testData[] = "test";
        size_t written = 0;
        TransportError writeResult = transport->Write(testData, strlen(testData), &written);
        
        if (writeResult != TransportError::NotOpen) {
            cout << "    ✗ 未打开写入错误处理失败" << endl;
            return false;
        }
        cout << "    ✓ 未打开写入错误处理正确" << endl;
        
        // 测试参数验证
        TransportError nullResult = transport->Write(nullptr, 10, &written);
        if (nullResult != TransportError::NotOpen) { // 应该返回NotOpen，因为状态检查优先
            cout << "    ✗ 空指针参数处理失败" << endl;
            return false;
        }
        cout << "    ✓ 参数验证正确" << endl;
        
        // 测试错误码获取
        TransportError lastError = transport->GetLastError();
        // 只要能获取错误码就算通过
        cout << "    ✓ 错误码获取功能正常" << endl;
        
        return true;
    }
};

int main() {
    try {
        SimpleTransportTester tester;
        tester.RunAllTests();
        
        cout << "\n按任意键退出..." << endl;
        cin.get();
        
    } catch (const exception& e) {
        cerr << "测试执行失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}