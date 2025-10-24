#pragma execution_character_set("utf-8")

// AutoTest v2.0 - 增强版自动化测试工具
// 集成测试框架，支持错误恢复、性能基准、压力和稳定性测试

#include "TestFramework.h"
#include "ErrorRecoveryTests.h"
#include "PerformanceTests.h"
#include "StressTests.h"
#include "TransportUnitTests.h"
#include "ProtocolUnitTests.h"
#include "IntegrationTests.h"
#include "RegressionTestFramework.h"

#include <iostream>
#include <string>
#include <Windows.h>

void PrintUsage() {
    std::cout << "AutoTest v2.0 - Enhanced Automated Testing Tool" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  AutoTest.exe [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --all              Run all test suites (default)" << std::endl;
    std::cout << "  --unit-tests       Run unit tests (Transport + Protocol)" << std::endl;
    std::cout << "  --integration      Run integration tests" << std::endl;
    std::cout << "  --error-recovery   Run error recovery tests only" << std::endl;
    std::cout << "  --performance      Run performance tests only" << std::endl;
    std::cout << "  --stress           Run stress tests only" << std::endl;
    std::cout << "  --report <file>    Generate JSON report (default: test_report.json)" << std::endl;
    std::cout << std::endl;
    std::cout << "Regression Testing:" << std::endl;
    std::cout << "  --create-baseline <version>    Create regression baseline" << std::endl;
    std::cout << "  --regression <version>         Run regression test against baseline" << std::endl;
    std::cout << "  --auto-regression <version>    Auto regression against latest baseline" << std::endl;
    std::cout << "  --list-baselines               List all baseline versions" << std::endl;
    std::cout << std::endl;
    std::cout << "  --help             Show this help message" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // 解析命令行参数
    std::string mode = "all";
    std::string reportFile = "test_report.json";
    std::string regressionMode = "";
    std::string baselineVersion = "";
    std::string currentVersion = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        }
        else if (arg == "--all") {
            mode = "all";
        }
        else if (arg == "--unit-tests") {
            mode = "unit-tests";
        }
        else if (arg == "--integration") {
            mode = "integration";
        }
        else if (arg == "--error-recovery") {
            mode = "error-recovery";
        }
        else if (arg == "--performance") {
            mode = "performance";
        }
        else if (arg == "--stress") {
            mode = "stress";
        }
        else if (arg == "--report" && i + 1 < argc) {
            reportFile = argv[++i];
        }
        else if (arg == "--create-baseline" && i + 1 < argc) {
            regressionMode = "create-baseline";
            baselineVersion = argv[++i];
        }
        else if (arg == "--regression" && i + 1 < argc) {
            regressionMode = "regression";
            baselineVersion = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                currentVersion = argv[++i];
            }
        }
        else if (arg == "--auto-regression" && i + 1 < argc) {
            regressionMode = "auto-regression";
            currentVersion = argv[++i];
        }
        else if (arg == "--list-baselines") {
            regressionMode = "list-baselines";
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage();
            return 1;
        }
    }

    // 创建测试运行器
    TestRunner runner;

    // 根据模式注册测试套件
    if (mode == "all" || mode == "unit-tests") {
        // Transport层单元测试
        runner.RegisterSuite(std::make_shared<LoopbackTransportTest>());
        runner.RegisterSuite(std::make_shared<SerialTransportTest>());

        // Protocol层单元测试
        runner.RegisterSuite(std::make_shared<FrameCodecTest>());
        runner.RegisterSuite(std::make_shared<ReliableChannelTest>());
    }

    if (mode == "all" || mode == "integration") {
        // 集成测试
        runner.RegisterSuite(std::make_shared<TransportProtocolIntegrationTest>());
        runner.RegisterSuite(std::make_shared<FileTransferIntegrationTest>());
    }

    if (mode == "all" || mode == "error-recovery") {
        runner.RegisterSuite(std::make_shared<PacketLossTest>());
        runner.RegisterSuite(std::make_shared<TimeoutTest>());
        runner.RegisterSuite(std::make_shared<CRCFailureTest>());
    }

    if (mode == "all" || mode == "performance") {
        runner.RegisterSuite(std::make_shared<ThroughputTest>());
        runner.RegisterSuite(std::make_shared<WindowSizeImpactTest>());
        runner.RegisterSuite(std::make_shared<LatencyTest>());
    }

    if (mode == "all" || mode == "stress") {
        runner.RegisterSuite(std::make_shared<StressTest>());
        runner.RegisterSuite(std::make_shared<LongRunningTest>());
        runner.RegisterSuite(std::make_shared<ConcurrentTest>());
    }

    // 处理回归测试模式
    if (!regressionMode.empty()) {
        RegressionTestManager regressionManager;

        if (regressionMode == "list-baselines") {
            auto versions = regressionManager.ListBaselineVersions();
            std::cout << "Available baseline versions:" << std::endl;
            for (const auto& version : versions) {
                std::cout << "  - " << version << std::endl;
            }
            return 0;
        }
        else if (regressionMode == "create-baseline") {
            AutomatedRegressionRunner autoRunner(runner, regressionManager);
            bool success = autoRunner.RunAndCreateBaseline(baselineVersion);

            if (success) {
                std::cout << "Baseline created successfully: " << baselineVersion << std::endl;
                runner.GenerateJsonReport(reportFile);
                return 0;
            }
            else {
                std::cerr << "Failed to create baseline" << std::endl;
                return 1;
            }
        }
        else if (regressionMode == "regression") {
            AutomatedRegressionRunner autoRunner(runner, regressionManager);
            auto report = autoRunner.RunRegressionTest(baselineVersion,
                                                      currentVersion.empty() ? "current" : currentVersion);

            regressionManager.GenerateRegressionReport(report, "regression_report.md");
            runner.GenerateJsonReport(reportFile);

            std::cout << std::endl;
            std::cout << "Regression report saved to: regression_report.md" << std::endl;
            std::cout << "Test report saved to: " << reportFile << std::endl;
            std::cout << std::endl;

            if (report.hasRegression) {
                std::cout << "WARNING: Regression detected!" << std::endl;
                return 1;
            }
            else {
                std::cout << "No regression detected." << std::endl;
                return 0;
            }
        }
        else if (regressionMode == "auto-regression") {
            AutomatedRegressionRunner autoRunner(runner, regressionManager);
            auto report = autoRunner.AutoRegression(currentVersion);

            regressionManager.GenerateRegressionReport(report, "regression_report.md");
            runner.GenerateJsonReport(reportFile);

            std::cout << std::endl;
            std::cout << "Regression report saved to: regression_report.md" << std::endl;
            std::cout << "Test report saved to: " << reportFile << std::endl;
            std::cout << std::endl;

            if (report.hasRegression) {
                std::cout << "WARNING: Regression detected!" << std::endl;
                return 1;
            }
            else {
                std::cout << "No regression detected." << std::endl;
                return 0;
            }
        }
    }

    // 运行所有测试
    runner.RunAll();

    // 生成JSON报告
    runner.GenerateJsonReport(reportFile);

    // 检查测试结果
    const auto& results = runner.GetResults();
    bool allPassed = true;
    for (const auto& result : results) {
        if (!result.passed) {
            allPassed = false;
            break;
        }
    }

    std::cout << std::endl;
    std::cout << "Test report saved to: " << reportFile << std::endl;
    std::cout << std::endl;

    return allPassed ? 0 : 1;
}
