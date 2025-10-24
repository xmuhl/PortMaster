#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <iostream>
#include <fstream>
#include <ctime>

// 测试结果结构
struct TestResult {
    std::string testName;
    std::string suiteName;
    bool passed;
    std::string errorMessage;
    double executionTime;  // 秒
    std::map<std::string, std::string> metrics;  // 性能指标

    TestResult() : passed(false), executionTime(0.0) {}
};

// 测试套件基类
class TestSuite {
public:
    explicit TestSuite(const std::string& name) : m_name(name) {}
    virtual ~TestSuite() = default;

    // 纯虚函数，子类必须实现
    virtual void SetUp() = 0;
    virtual void TearDown() = 0;
    virtual std::vector<TestResult> RunTests() = 0;

    std::string GetName() const { return m_name; }

protected:
    std::string m_name;

    // 辅助断言方法
    void AssertTrue(bool condition, const std::string& msg) {
        if (!condition) {
            throw std::runtime_error(msg);
        }
    }

    void AssertFalse(bool condition, const std::string& msg) {
        AssertTrue(!condition, msg);
    }

    void AssertEqual(int expected, int actual, const std::string& msg) {
        if (expected != actual) {
            throw std::runtime_error(
                msg + ": expected " + std::to_string(expected) +
                ", got " + std::to_string(actual)
            );
        }
    }

    void AssertNotEqual(int expected, int actual, const std::string& msg) {
        if (expected == actual) {
            throw std::runtime_error(
                msg + ": expected not equal to " + std::to_string(expected)
            );
        }
    }

    void AssertGreater(int value, int threshold, const std::string& msg) {
        if (value <= threshold) {
            throw std::runtime_error(
                msg + ": " + std::to_string(value) +
                " is not greater than " + std::to_string(threshold)
            );
        }
    }

    void AssertFileEqual(const std::vector<uint8_t>& expected,
                        const std::vector<uint8_t>& actual) {
        if (expected.size() != actual.size()) {
            throw std::runtime_error(
                "File size mismatch: expected " +
                std::to_string(expected.size()) + " bytes, got " +
                std::to_string(actual.size()) + " bytes"
            );
        }

        if (expected != actual) {
            throw std::runtime_error("File content mismatch");
        }
    }

    // 运行单个测试
    TestResult RunTest(const std::string& testName,
                      std::function<void()> testFunc) {
        TestResult result;
        result.testName = testName;
        result.suiteName = m_name;

        auto start = std::chrono::high_resolution_clock::now();

        try {
            testFunc();
            result.passed = true;
            std::cout << "  [PASS] " << testName;
        }
        catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
            std::cout << "  [FAIL] " << testName << ": " << e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        result.executionTime = duration.count() / 1000000.0;

        std::cout << " (" << result.executionTime << "s)" << std::endl;

        return result;
    }
};

// 测试运行器
class TestRunner {
public:
    // 注册测试套件
    void RegisterSuite(std::shared_ptr<TestSuite> suite) {
        m_suites.push_back(suite);
    }

    // 运行所有测试
    void RunAll() {
        std::cout << "=======================================" << std::endl;
        std::cout << "AutoTest v2.0 - Enhanced Test Suite" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::cout << std::endl;

        m_results.clear();

        auto totalStart = std::chrono::high_resolution_clock::now();

        int suiteIndex = 1;
        for (auto& suite : m_suites) {
            std::cout << "[" << suiteIndex << "/" << m_suites.size() << "] "
                     << suite->GetName() << std::endl;

            try {
                suite->SetUp();
                auto suiteResults = suite->RunTests();
                m_results.insert(m_results.end(), suiteResults.begin(), suiteResults.end());
                suite->TearDown();
            }
            catch (const std::exception& e) {
                std::cerr << "  [ERROR] Suite setup/teardown failed: " << e.what() << std::endl;
            }

            std::cout << std::endl;
            suiteIndex++;
        }

        auto totalEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart);

        // 打印总结
        PrintSummary(totalDuration.count() / 1000.0);
    }

    // 运行特定测试套件
    void RunSuite(const std::string& suiteName) {
        for (auto& suite : m_suites) {
            if (suite->GetName() == suiteName) {
                std::cout << "Running suite: " << suiteName << std::endl;
                suite->SetUp();
                auto results = suite->RunTests();
                m_results.insert(m_results.end(), results.begin(), results.end());
                suite->TearDown();
                return;
            }
        }

        std::cerr << "Suite not found: " << suiteName << std::endl;
    }

    // 生成JSON报告
    void GenerateJsonReport(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to create report file: " << filename << std::endl;
            return;
        }

        file << "{\n";
        file << "  \"test_run\": {\n";
        file << "    \"version\": \"2.0\",\n";
        file << "    \"timestamp\": \"" << GetCurrentTimestamp() << "\",\n";

        int totalTests = m_results.size();
        int passedTests = 0;
        double totalDuration = 0.0;

        for (const auto& result : m_results) {
            if (result.passed) passedTests++;
            totalDuration += result.executionTime;
        }

        file << "    \"duration_seconds\": " << totalDuration << ",\n";
        file << "    \"summary\": {\n";
        file << "      \"total_suites\": " << m_suites.size() << ",\n";
        file << "      \"total_tests\": " << totalTests << ",\n";
        file << "      \"passed\": " << passedTests << ",\n";
        file << "      \"failed\": " << (totalTests - passedTests) << ",\n";
        file << "      \"success_rate\": " << (totalTests > 0 ? (passedTests * 100.0 / totalTests) : 0.0) << "\n";
        file << "    },\n";

        file << "    \"results\": [\n";
        for (size_t i = 0; i < m_results.size(); i++) {
            const auto& result = m_results[i];
            file << "      {\n";
            file << "        \"suite\": \"" << result.suiteName << "\",\n";
            file << "        \"test\": \"" << result.testName << "\",\n";
            file << "        \"passed\": " << (result.passed ? "true" : "false") << ",\n";
            file << "        \"duration\": " << result.executionTime << ",\n";
            file << "        \"error\": \"" << (result.passed ? "" : result.errorMessage) << "\",\n";
            file << "        \"metrics\": {\n";

            size_t metricIndex = 0;
            for (const auto& metric : result.metrics) {
                file << "          \"" << metric.first << "\": \"" << metric.second << "\"";
                if (++metricIndex < result.metrics.size()) {
                    file << ",";
                }
                file << "\n";
            }

            file << "        }\n";
            file << "      }";
            if (i < m_results.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        file << "    ]\n";
        file << "  }\n";
        file << "}\n";

        file.close();
        std::cout << "JSON report generated: " << filename << std::endl;
    }

    // 获取结果
    const std::vector<TestResult>& GetResults() const {
        return m_results;
    }

private:
    std::vector<std::shared_ptr<TestSuite>> m_suites;
    std::vector<TestResult> m_results;

    void PrintSummary(double totalDuration) {
        int totalTests = m_results.size();
        int passedTests = 0;
        int failedTests = 0;

        for (const auto& result : m_results) {
            if (result.passed) {
                passedTests++;
            } else {
                failedTests++;
            }
        }

        std::cout << "=======================================" << std::endl;
        std::cout << "Summary:" << std::endl;
        std::cout << "  Total suites: " << m_suites.size() << std::endl;
        std::cout << "  Total tests:  " << totalTests << std::endl;
        std::cout << "  Passed:       " << passedTests << std::endl;
        std::cout << "  Failed:       " << failedTests << std::endl;
        std::cout << "  Duration:     " << totalDuration << "s";

        if (totalDuration >= 60) {
            int minutes = static_cast<int>(totalDuration / 60);
            int seconds = static_cast<int>(totalDuration) % 60;
            std::cout << " (" << minutes << "m " << seconds << "s)";
        }
        std::cout << std::endl;

        std::cout << "=======================================" << std::endl;
        std::cout << std::endl;

        if (failedTests == 0) {
            std::cout << "TEST SUITE PASSED" << std::endl;
        } else {
            std::cout << "TEST SUITE FAILED" << std::endl;
        }
    }

    std::string GetCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", std::localtime(&time_t_now));
        return std::string(buffer);
    }
};
