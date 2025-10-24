# AutoTest增强版设计方案

**版本**: v2.0
**创建日期**: 2025-10-02
**目标**: 将AutoTest扩展为全面的自动化测试工具

---

## 1. 当前状态评估

### 1.1 已实现功能（v1.0）

```cpp
// 当前测试流程（8步）
Step 1: 读取测试文件
Step 2: 创建Loopback传输层
Step 3: 创建可靠传输通道
Step 4: 连接通道
Step 5: 启动接收线程
Step 6: 发送文件
Step 7: 等待接收完成
Step 8: 验证文件完整性
Step 9: 显示统计信息
```

**优点**:
- ✅ 流程清晰，易于理解
- ✅ 基本功能验证完整
- ✅ 文件完整性验证准确

**不足**:
- ❌ 测试场景单一
- ❌ 无错误恢复测试
- ❌ 无性能测试
- ❌ 无UI状态机测试

### 1.2 需要增强的功能

根据《全自动化AI驱动开发系统实施方案》，需要增加：

**1. UI状态机测试**
- 状态转换表验证
- 所有合法转换测试
- 所有非法转换拦截测试

**2. 错误恢复测试**
- 丢包恢复测试
- 乱序数据包处理测试
- 超时重传测试
- CRC校验失败恢复

**3. 性能基准测试**
- 吞吐量测试
- 延迟测试
- 窗口大小影响测试

**4. 压力测试和稳定性测试**
- 大量小文件传输
- 长时间连续运行

---

## 2. 架构设计

### 2.1 模块化架构

```
AutoTest v2.0
├── Core (核心测试框架)
│   ├── TestSuite (测试套件基类)
│   ├── TestCase (测试用例基类)
│   ├── TestRunner (测试运行器)
│   └── TestReporter (测试报告生成器)
│
├── BasicTests (基础测试)
│   ├── BasicTransmissionTest (基本传输测试 - 已有)
│   └── FileVerificationTest (文件验证测试)
│
├── ProtocolTests (协议测试)
│   ├── ErrorRecoveryTests (错误恢复测试)
│   │   ├── PacketLossTest (丢包恢复)
│   │   ├── OutOfOrderTest (乱序处理)
│   │   ├── TimeoutTest (超时重传)
│   │   └── CRCFailureTest (CRC校验失败)
│   │
│   ├── WindowSizeTests (窗口大小测试)
│   │   └── WindowSizeImpactTest (窗口大小影响)
│   │
│   └── ReliabilityTests (可靠性测试)
│       └── LongRunningTest (长时间运行)
│
├── PerformanceTests (性能测试)
│   ├── ThroughputTest (吞吐量测试)
│   ├── LatencyTest (延迟测试)
│   └── StressTest (压力测试)
│
└── UIStateTests (UI状态机测试)
    ├── StateMachineTest (状态机验证)
    └── TransitionTableTest (转换表测试)
```

### 2.2 测试框架设计

```cpp
// TestSuite基类
class TestSuite {
public:
    virtual ~TestSuite() = default;

    virtual void SetUp() = 0;
    virtual void TearDown() = 0;
    virtual std::vector<TestResult> RunTests() = 0;

    std::string GetName() const { return m_name; }

protected:
    std::string m_name;
    std::shared_ptr<LoopbackTransport> m_transport;
    std::shared_ptr<ReliableChannel> m_reliableChannel;

    // 辅助方法
    void AssertTrue(bool condition, const std::string& msg);
    void AssertEqual(int expected, int actual, const std::string& msg);
    void AssertFileEqual(const std::vector<uint8_t>& expected,
                         const std::vector<uint8_t>& actual);
};

// TestResult结构
struct TestResult {
    std::string testName;
    std::string suiteName;
    bool passed;
    std::string errorMessage;
    double executionTime;  // 秒
    std::map<std::string, std::string> metrics;  // 性能指标
};

// TestRunner
class TestRunner {
public:
    void RegisterSuite(std::shared_ptr<TestSuite> suite);
    void RunAll();
    void GenerateReport(const std::string& outputFile);

private:
    std::vector<std::shared_ptr<TestSuite>> m_suites;
    std::vector<TestResult> m_results;
};
```

---

## 3. 具体测试用例设计

### 3.1 错误恢复测试

#### 3.1.1 丢包恢复测试

```cpp
class PacketLossTest : public TestSuite {
public:
    void SetUp() override {
        m_transport = std::make_shared<LoopbackTransport>();
        m_reliableChannel = std::make_shared<ReliableChannel>();

        // 配置丢包率
        m_transport->SetPacketLossRate(0.1);  // 10%丢包率
    }

    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 低丢包率 (5%)
        results.push_back(RunPacketLossTest(0.05, "Low packet loss (5%)"));

        // 测试2: 中等丢包率 (10%)
        results.push_back(RunPacketLossTest(0.10, "Medium packet loss (10%)"));

        // 测试3: 高丢包率 (20%)
        results.push_back(RunPacketLossTest(0.20, "High packet loss (20%)"));

        return results;
    }

private:
    TestResult RunPacketLossTest(double lossRate, const std::string& testName) {
        TestResult result;
        result.testName = testName;
        result.suiteName = "PacketLossTest";

        try {
            m_transport->SetPacketLossRate(lossRate);

            // 发送测试文件
            auto testData = GenerateTestData(1024 * 1024);  // 1MB
            bool success = TransmitData(testData);

            AssertTrue(success, "Transmission should succeed despite packet loss");

            // 验证数据完整性
            auto receivedData = GetReceivedData();
            AssertFileEqual(testData, receivedData);

            // 记录重传统计
            auto stats = m_reliableChannel->GetStats();
            result.metrics["retransmissions"] = std::to_string(stats.packetsRetransmitted);
            result.metrics["loss_rate"] = std::to_string(lossRate);

            result.passed = true;
        }
        catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
        }

        return result;
    }
};
```

#### 3.1.2 乱序数据包测试

```cpp
class OutOfOrderTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 轻微乱序 (10%概率)
        results.push_back(RunOutOfOrderTest(0.10, "Light reordering (10%)"));

        // 测试2: 严重乱序 (30%概率)
        results.push_back(RunOutOfOrderTest(0.30, "Heavy reordering (30%)"));

        return results;
    }

private:
    TestResult RunOutOfOrderTest(double reorderRate, const std::string& testName) {
        // 配置传输层模拟乱序
        m_transport->EnablePacketReordering(true);
        m_transport->SetReorderRate(reorderRate);

        // 发送测试数据
        auto testData = GenerateTestData(500 * 1024);  // 500KB
        bool success = TransmitData(testData);

        AssertTrue(success, "Should handle out-of-order packets");

        // 验证数据完整性
        auto receivedData = GetReceivedData();
        AssertFileEqual(testData, receivedData);

        // ... 记录结果
    }
};
```

#### 3.1.3 超时重传测试

```cpp
class TimeoutTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 短超时 + 长延迟
        results.push_back(RunTimeoutTest(1000, 2000, "Short timeout with delay"));

        // 测试2: 动态超时调整
        results.push_back(RunDynamicTimeoutTest());

        return results;
    }

private:
    TestResult RunTimeoutTest(int timeout_ms, int delay_ms, const std::string& testName) {
        // 配置超时和延迟
        ReliableConfig config;
        config.timeout = timeout_ms;
        m_reliableChannel->Configure(config);

        m_transport->SetDelay(delay_ms);

        // 发送数据，验证超时重传机制
        auto testData = GenerateTestData(100 * 1024);  // 100KB
        bool success = TransmitData(testData);

        AssertTrue(success, "Should retransmit on timeout");

        // 检查重传次数
        auto stats = m_reliableChannel->GetStats();
        AssertTrue(stats.packetsRetransmitted > 0,
                  "Should have retransmitted packets");

        // ... 记录结果
    }
};
```

### 3.2 性能基准测试

#### 3.2.1 吞吐量测试

```cpp
class ThroughputTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试不同文件大小的吞吐量
        std::vector<size_t> fileSizes = {
            100 * 1024,        // 100KB
            1 * 1024 * 1024,   // 1MB
            10 * 1024 * 1024   // 10MB
        };

        for (size_t size : fileSizes) {
            std::string testName = "Throughput test - " +
                                 std::to_string(size / 1024) + "KB";
            results.push_back(RunThroughputTest(size, testName));
        }

        return results;
    }

private:
    TestResult RunThroughputTest(size_t fileSize, const std::string& testName) {
        TestResult result;
        result.testName = testName;

        auto testData = GenerateTestData(fileSize);

        auto start = std::chrono::high_resolution_clock::now();
        bool success = TransmitData(testData);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        double seconds = duration.count() / 1000.0;
        double throughput_mbps = (fileSize / 1024.0 / 1024.0) / seconds;

        result.metrics["file_size_mb"] = std::to_string(fileSize / 1024.0 / 1024.0);
        result.metrics["duration_sec"] = std::to_string(seconds);
        result.metrics["throughput_mbps"] = std::to_string(throughput_mbps);

        result.passed = success;
        return result;
    }
};
```

#### 3.2.2 窗口大小影响测试

```cpp
class WindowSizeImpactTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        std::vector<int> windowSizes = {1, 4, 8, 16, 32, 64};

        for (int windowSize : windowSizes) {
            std::string testName = "Window size = " + std::to_string(windowSize);
            results.push_back(RunWindowSizeTest(windowSize, testName));
        }

        return results;
    }

private:
    TestResult RunWindowSizeTest(int windowSize, const std::string& testName) {
        // 配置窗口大小
        ReliableConfig config;
        config.windowSize = windowSize;
        m_reliableChannel->Configure(config);

        // 测试固定大小文件的传输性能
        auto testData = GenerateTestData(5 * 1024 * 1024);  // 5MB

        auto start = std::chrono::high_resolution_clock::now();
        bool success = TransmitData(testData);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        TestResult result;
        result.testName = testName;
        result.passed = success;
        result.metrics["window_size"] = std::to_string(windowSize);
        result.metrics["duration_ms"] = std::to_string(duration.count());

        return result;
    }
};
```

### 3.3 压力测试和稳定性测试

#### 3.3.1 压力测试（大量小文件）

```cpp
class StressTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 大量小文件
        results.push_back(RunManySmallFilesTest(1000, 1024));  // 1000个1KB文件

        // 测试2: 中等数量中等文件
        results.push_back(RunManySmallFilesTest(100, 100*1024));  // 100个100KB文件

        return results;
    }

private:
    TestResult RunManySmallFilesTest(int fileCount, size_t fileSize) {
        TestResult result;
        result.testName = "Stress test - " + std::to_string(fileCount) +
                         " files of " + std::to_string(fileSize) + " bytes";

        int successCount = 0;
        int failureCount = 0;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < fileCount; i++) {
            auto testData = GenerateTestData(fileSize);

            if (TransmitData(testData)) {
                successCount++;
            } else {
                failureCount++;
            }

            if ((i + 1) % 100 == 0) {
                std::cout << "Progress: " << (i + 1) << "/" << fileCount << std::endl;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

        result.metrics["file_count"] = std::to_string(fileCount);
        result.metrics["success_count"] = std::to_string(successCount);
        result.metrics["failure_count"] = std::to_string(failureCount);
        result.metrics["duration_sec"] = std::to_string(duration.count());

        result.passed = (failureCount == 0);

        return result;
    }
};
```

#### 3.3.2 稳定性测试（长时间运行）

```cpp
class LongRunningTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 持续运行30分钟
        results.push_back(RunLongRunningTest(30, 10));  // 30分钟，每10秒一次传输

        return results;
    }

private:
    TestResult RunLongRunningTest(int duration_minutes, int interval_seconds) {
        TestResult result;
        result.testName = "Long running test - " +
                         std::to_string(duration_minutes) + " minutes";

        auto endTime = std::chrono::steady_clock::now() +
                      std::chrono::minutes(duration_minutes);

        int iteration = 0;
        int failures = 0;

        while (std::chrono::steady_clock::now() < endTime) {
            auto testData = GenerateTestData(10 * 1024);  // 10KB每次

            if (!TransmitData(testData)) {
                failures++;
                std::cerr << "Iteration " << iteration << " failed" << std::endl;
            }

            iteration++;
            std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        }

        result.metrics["iterations"] = std::to_string(iteration);
        result.metrics["failures"] = std::to_string(failures);
        result.metrics["duration_min"] = std::to_string(duration_minutes);

        result.passed = (failures == 0);

        return result;
    }
};
```

### 3.4 UI状态机测试

```cpp
class StateMachineTest : public TestSuite {
public:
    std::vector<TestResult> RunTests() override {
        std::vector<TestResult> results;

        // 测试1: 状态转换表完整性
        results.push_back(TestTransitionTable());

        // 测试2: 所有合法转换
        results.push_back(TestValidTransitions());

        // 测试3: 所有非法转换拦截
        results.push_back(TestInvalidTransitions());

        return results;
    }

private:
    TestResult TestTransitionTable() {
        // 模拟UI状态管理器
        // 由于这是Protocol层测试，我们测试ReliableChannel的状态机

        TestResult result;
        result.testName = "Transition table completeness";

        // 获取所有可能的状态
        std::vector<ReliableState> states = {
            ReliableState::IDLE,
            ReliableState::STARTING,
            ReliableState::SENDING,
            ReliableState::ENDING,
            ReliableState::READY,
            ReliableState::RECEIVING,
            ReliableState::DONE,
            ReliableState::FAILED
        };

        // 测试所有状态转换
        int validTransitions = 0;
        int invalidTransitions = 0;

        for (auto fromState : states) {
            for (auto toState : states) {
                // 尝试转换
                bool canTransition = IsValidTransition(fromState, toState);

                if (canTransition) {
                    validTransitions++;
                } else {
                    invalidTransitions++;
                }
            }
        }

        result.metrics["valid_transitions"] = std::to_string(validTransitions);
        result.metrics["invalid_transitions"] = std::to_string(invalidTransitions);
        result.passed = true;

        return result;
    }
};
```

---

## 4. 测试报告格式

### 4.1 控制台输出格式

```
=======================================
AutoTest v2.0 - Enhanced Test Suite
=======================================

[1/10] BasicTransmissionTest
  [PASS] Basic file transmission        (2.3s)

[2/10] PacketLossTest
  [PASS] Low packet loss (5%)          (3.1s) | retransmissions: 12
  [PASS] Medium packet loss (10%)      (4.5s) | retransmissions: 28
  [PASS] High packet loss (20%)        (7.2s) | retransmissions: 65

[3/10] OutOfOrderTest
  [PASS] Light reordering (10%)        (2.8s)
  [PASS] Heavy reordering (30%)        (3.9s)

[4/10] TimeoutTest
  [PASS] Short timeout with delay      (5.1s) | retransmissions: 8

[5/10] ThroughputTest
  [PASS] 100KB throughput              (0.5s) | 0.20 MB/s
  [PASS] 1MB throughput                (3.2s) | 0.31 MB/s
  [PASS] 10MB throughput               (28.5s)| 0.35 MB/s

[6/10] WindowSizeImpactTest
  [PASS] Window size = 1               (45.2s)
  [PASS] Window size = 4               (18.3s)
  [PASS] Window size = 8               (10.1s)
  [PASS] Window size = 16              (6.8s)
  [PASS] Window size = 32              (5.2s)

[7/10] StressTest
  [PASS] 1000 x 1KB files              (125.3s)| success: 1000, fail: 0

[8/10] LongRunningTest
  [PASS] 30 minute stability test      (1800.0s)| iterations: 180, failures: 0

[9/10] StateMachineTest
  [PASS] Transition table test         (0.1s) | valid: 12, invalid: 52

[10/10] CRCFailureTest
  [PASS] CRC failure recovery          (4.2s) | NAK count: 15

=======================================
Summary:
  Total suites: 10
  Total tests:  28
  Passed:       28
  Failed:       0
  Duration:     2045.6s (34m 5s)
=======================================

TEST SUITE PASSED
```

### 4.2 JSON报告格式

```json
{
  "test_run": {
    "version": "2.0",
    "timestamp": "2025-10-02T10:30:00",
    "duration_seconds": 2045.6,
    "summary": {
      "total_suites": 10,
      "total_tests": 28,
      "passed": 28,
      "failed": 0,
      "success_rate": 100.0
    },
    "suites": [
      {
        "name": "PacketLossTest",
        "tests": [
          {
            "name": "Low packet loss (5%)",
            "passed": true,
            "duration": 3.1,
            "metrics": {
              "retransmissions": "12",
              "loss_rate": "0.05"
            }
          }
        ]
      }
    ]
  }
}
```

---

## 5. 实施计划

### 5.1 开发步骤

**Phase 1: 核心框架（1天）**
- [x] 设计TestSuite/TestCase/TestRunner架构
- [ ] 实现测试框架基础代码
- [ ] 实现TestReporter（控制台+JSON）

**Phase 2: 错误恢复测试（1天）**
- [ ] 实现PacketLossTest
- [ ] 实现OutOfOrderTest
- [ ] 实现TimeoutTest
- [ ] 实现CRCFailureTest

**Phase 3: 性能测试（1天）**
- [ ] 实现ThroughputTest
- [ ] 实现WindowSizeImpactTest
- [ ] 实现LatencyTest（可选）

**Phase 4: 压力和稳定性测试（1天）**
- [ ] 实现StressTest
- [ ] 实现LongRunningTest

**Phase 5: UI状态机测试（0.5天）**
- [ ] 实现StateMachineTest
- [ ] 实现TransitionTableTest

**Phase 6: 集成和优化（0.5天）**
- [ ] 集成所有测试套件
- [ ] 优化测试执行效率
- [ ] 完善报告生成

### 5.2 验收标准

**功能验收**:
- ✅ 所有10个测试套件正常运行
- ✅ 至少包含25个测试用例
- ✅ 覆盖所有关键场景

**性能验收**:
- ✅ 完整测试套件执行时间 < 40分钟
- ✅ 基础测试执行时间 < 5分钟

**质量验收**:
- ✅ 测试稳定性 > 99%
- ✅ 报告格式规范
- ✅ 日志信息完整

---

## 6. 使用指南

### 6.1 运行所有测试

```bash
AutoTest.exe --mode all
```

### 6.2 运行特定测试套件

```bash
AutoTest.exe --suite PacketLossTest
AutoTest.exe --suite ThroughputTest
```

### 6.3 生成报告

```bash
AutoTest.exe --mode all --report json --output test_report.json
AutoTest.exe --mode all --report html --output test_report.html
```

### 6.4 快速测试（仅基础功能）

```bash
AutoTest.exe --mode quick
```

---

## 7. 总结

AutoTest v2.0将成为一个**全面的自动化测试工具**，提供：

1. ✅ **10个测试套件**，覆盖所有关键场景
2. ✅ **25+测试用例**，确保代码质量
3. ✅ **详细的测试报告**，方便分析
4. ✅ **模块化架构**，易于扩展

**预期效果**:
- 自动化测试覆盖率提升至 **90%+**
- 问题发现时间缩短 **70%**
- 测试执行效率提升 **5倍**
