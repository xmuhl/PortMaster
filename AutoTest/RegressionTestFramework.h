#pragma once
#pragma execution_character_set("utf-8")

#include "TestFramework.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <chrono>
#include <algorithm>

// ============================================================================
// 回归测试基线数据结构
// ============================================================================

struct TestBaseline
{
    std::string testName;
    std::string suiteName;
    bool expectedPass;
    double maxExecutionTime;      // 最大允许执行时间(ms)
    std::map<std::string, double> performanceMetrics;  // 性能指标基线

    TestBaseline() : expectedPass(true), maxExecutionTime(0.0) {}
};

struct RegressionBaseline
{
    std::string version;
    std::string timestamp;
    std::vector<TestBaseline> testBaselines;
    std::map<std::string, double> globalMetrics;  // 全局性能指标

    RegressionBaseline() : version("1.0.0") {}
};

// ============================================================================
// 回归测试结果对比
// ============================================================================

struct RegressionDifference
{
    std::string testName;
    std::string suiteName;

    enum class DifferenceType
    {
        NewTest,              // 新增测试
        RemovedTest,          // 移除的测试
        StatusChanged,        // 状态变化（通过↔失败）
        PerformanceRegression,// 性能回归
        PerformanceImproved,  // 性能改进
        ExecutionTimeExceeded // 执行时间超标
    };

    DifferenceType type;
    std::string description;
    double value;             // 数值差异

    RegressionDifference() : type(DifferenceType::NewTest), value(0.0) {}
};

struct RegressionReport
{
    std::string baselineVersion;
    std::string currentVersion;
    std::string timestamp;

    int totalTests;
    int passedTests;
    int failedTests;
    int newTests;
    int removedTests;
    int regressionTests;      // 性能回归测试数

    std::vector<RegressionDifference> differences;
    std::map<std::string, double> performanceComparison;

    bool hasRegression;       // 是否存在回归

    RegressionReport() : totalTests(0), passedTests(0), failedTests(0),
                         newTests(0), removedTests(0), regressionTests(0),
                         hasRegression(false) {}
};

// ============================================================================
// 回归测试管理器
// ============================================================================

class RegressionTestManager
{
public:
    RegressionTestManager(const std::string& baselineDir = "test_baselines")
        : m_baselineDir(baselineDir)
    {
        std::filesystem::create_directories(m_baselineDir);
    }

    // 创建基线
    bool CreateBaseline(const std::vector<TestResult>& results,
                       const std::string& version,
                       const std::map<std::string, double>& globalMetrics = {})
    {
        RegressionBaseline baseline;
        baseline.version = version;
        baseline.timestamp = GetCurrentTimestamp();
        baseline.globalMetrics = globalMetrics;

        // 转换测试结果为基线
        for (const auto& result : results)
        {
            TestBaseline testBaseline;
            testBaseline.testName = result.testName;
            testBaseline.suiteName = result.suiteName;
            testBaseline.expectedPass = result.passed;
            testBaseline.maxExecutionTime = result.executionTime * 1.5; // 允许50%浮动

            // 【修复】将string类型的metrics转换为double类型
            for (const auto& [key, value] : result.metrics)
            {
                try {
                    testBaseline.performanceMetrics[key] = std::stod(value);
                }
                catch (...) {
                    // 如果转换失败，使用默认值0.0
                    testBaseline.performanceMetrics[key] = 0.0;
                }
            }

            baseline.testBaselines.push_back(testBaseline);
        }

        // 保存基线
        return SaveBaseline(baseline, version);
    }

    // 加载基线
    bool LoadBaseline(const std::string& version, RegressionBaseline& baseline)
    {
        std::string filename = GetBaselineFilename(version);
        std::ifstream file(filename);

        if (!file.is_open())
        {
            return false;
        }

        try
        {
            // 读取JSON格式基线
            std::string line;
            std::string jsonContent;
            while (std::getline(file, line))
            {
                jsonContent += line;
            }

            // 简化的JSON解析（实际项目应使用JSON库）
            baseline.version = version;
            baseline.timestamp = ExtractJsonString(jsonContent, "timestamp");

            file.close();
            return true;
        }
        catch (...)
        {
            file.close();
            return false;
        }
    }

    // 执行回归测试对比
    RegressionReport CompareWithBaseline(const std::vector<TestResult>& currentResults,
                                         const std::string& baselineVersion,
                                         const std::string& currentVersion)
    {
        RegressionReport report;
        report.baselineVersion = baselineVersion;
        report.currentVersion = currentVersion;
        report.timestamp = GetCurrentTimestamp();

        // 加载基线
        RegressionBaseline baseline;
        if (!LoadBaseline(baselineVersion, baseline))
        {
            report.hasRegression = true;
            RegressionDifference diff;
            diff.type = RegressionDifference::DifferenceType::NewTest;
            diff.description = "无法加载基线版本: " + baselineVersion;
            report.differences.push_back(diff);
            return report;
        }

        // 构建基线测试映射
        std::map<std::string, TestBaseline> baselineMap;
        for (const auto& baseline_test : baseline.testBaselines)
        {
            std::string key = baseline_test.suiteName + "::" + baseline_test.testName;
            baselineMap[key] = baseline_test;
        }

        // 构建当前测试映射
        std::map<std::string, TestResult> currentMap;
        for (const auto& current_test : currentResults)
        {
            std::string key = current_test.suiteName + "::" + current_test.testName;
            currentMap[key] = current_test;
        }

        // 对比测试
        report.totalTests = static_cast<int>(currentResults.size());

        for (const auto& [key, currentTest] : currentMap)
        {
            if (currentTest.passed)
            {
                report.passedTests++;
            }
            else
            {
                report.failedTests++;
            }

            auto baselineIt = baselineMap.find(key);

            if (baselineIt == baselineMap.end())
            {
                // 新增测试
                report.newTests++;
                RegressionDifference diff;
                diff.testName = currentTest.testName;
                diff.suiteName = currentTest.suiteName;
                diff.type = RegressionDifference::DifferenceType::NewTest;
                diff.description = "新增测试";
                report.differences.push_back(diff);
            }
            else
            {
                const auto& baselineTest = baselineIt->second;

                // 检查状态变化
                if (currentTest.passed != baselineTest.expectedPass)
                {
                    report.hasRegression = true;
                    RegressionDifference diff;
                    diff.testName = currentTest.testName;
                    diff.suiteName = currentTest.suiteName;
                    diff.type = RegressionDifference::DifferenceType::StatusChanged;
                    diff.description = currentTest.passed ? "测试由失败变为通过" : "测试由通过变为失败";
                    report.differences.push_back(diff);
                }

                // 检查执行时间
                if (currentTest.executionTime > baselineTest.maxExecutionTime)
                {
                    report.hasRegression = true;
                    report.regressionTests++;
                    RegressionDifference diff;
                    diff.testName = currentTest.testName;
                    diff.suiteName = currentTest.suiteName;
                    diff.type = RegressionDifference::DifferenceType::ExecutionTimeExceeded;
                    diff.value = currentTest.executionTime - baselineTest.maxExecutionTime;
                    diff.description = "执行时间超标 " + std::to_string(diff.value) + "ms";
                    report.differences.push_back(diff);
                }

                // 检查性能指标
                for (const auto& [metricName, baselineValue] : baselineTest.performanceMetrics)
                {
                    auto currentMetricIt = currentTest.metrics.find(metricName);
                    if (currentMetricIt != currentTest.metrics.end())
                    {
                        // 【修复】将string类型的metric值转换为double
                        double currentValue = 0.0;
                        try {
                            currentValue = std::stod(currentMetricIt->second);
                        }
                        catch (...) {
                            // 转换失败，跳过该指标
                            continue;
                        }

                        double threshold = baselineValue * 1.1; // 允许10%性能下降

                        if (currentValue > threshold)
                        {
                            report.hasRegression = true;
                            report.regressionTests++;
                            RegressionDifference diff;
                            diff.testName = currentTest.testName;
                            diff.suiteName = currentTest.suiteName;
                            diff.type = RegressionDifference::DifferenceType::PerformanceRegression;
                            diff.value = ((currentValue - baselineValue) / baselineValue) * 100.0;
                            diff.description = "性能指标 " + metricName + " 下降 " +
                                             std::to_string(diff.value) + "%";
                            report.differences.push_back(diff);
                        }
                        else if (currentValue < baselineValue * 0.9)
                        {
                            // 性能改进
                            RegressionDifference diff;
                            diff.testName = currentTest.testName;
                            diff.suiteName = currentTest.suiteName;
                            diff.type = RegressionDifference::DifferenceType::PerformanceImproved;
                            diff.value = ((baselineValue - currentValue) / baselineValue) * 100.0;
                            diff.description = "性能指标 " + metricName + " 改进 " +
                                             std::to_string(diff.value) + "%";
                            report.differences.push_back(diff);
                        }
                    }
                }
            }
        }

        // 检查移除的测试
        for (const auto& [key, baselineTest] : baselineMap)
        {
            if (currentMap.find(key) == currentMap.end())
            {
                report.removedTests++;
                RegressionDifference diff;
                diff.testName = baselineTest.testName;
                diff.suiteName = baselineTest.suiteName;
                diff.type = RegressionDifference::DifferenceType::RemovedTest;
                diff.description = "测试被移除";
                report.differences.push_back(diff);
            }
        }

        return report;
    }

    // 生成回归报告
    void GenerateRegressionReport(const RegressionReport& report, const std::string& filename)
    {
        std::ofstream file(filename);

        if (!file.is_open())
        {
            return;
        }

        // 生成Markdown格式报告
        file << "# 回归测试报告\n\n";
        file << "**基线版本**: " << report.baselineVersion << "\n";
        file << "**当前版本**: " << report.currentVersion << "\n";
        file << "**测试时间**: " << report.timestamp << "\n\n";

        file << "## 测试概览\n\n";
        file << "| 指标 | 数量 |\n";
        file << "|------|------|\n";
        file << "| 总测试数 | " << report.totalTests << " |\n";
        file << "| 通过测试 | " << report.passedTests << " |\n";
        file << "| 失败测试 | " << report.failedTests << " |\n";
        file << "| 新增测试 | " << report.newTests << " |\n";
        file << "| 移除测试 | " << report.removedTests << " |\n";
        file << "| 性能回归 | " << report.regressionTests << " |\n\n";

        if (report.hasRegression)
        {
            file << "## ⚠️ 发现回归问题\n\n";
        }
        else
        {
            file << "## ✅ 未发现回归问题\n\n";
        }

        if (!report.differences.empty())
        {
            file << "## 详细差异\n\n";

            // 按类型分组
            std::map<RegressionDifference::DifferenceType, std::vector<RegressionDifference>> groupedDiffs;
            for (const auto& diff : report.differences)
            {
                groupedDiffs[diff.type].push_back(diff);
            }

            // 输出各类型差异
            for (const auto& [type, diffs] : groupedDiffs)
            {
                file << "### " << GetDifferenceTypeName(type) << "\n\n";

                for (const auto& diff : diffs)
                {
                    file << "- **" << diff.suiteName << "::" << diff.testName << "**: ";
                    file << diff.description << "\n";
                }

                file << "\n";
            }
        }

        file.close();
    }

    // 列出所有基线版本
    std::vector<std::string> ListBaselineVersions()
    {
        std::vector<std::string> versions;

        for (const auto& entry : std::filesystem::directory_iterator(m_baselineDir))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                if (filename.find("baseline_") == 0 && filename.find(".json") != std::string::npos)
                {
                    // 提取版本号
                    size_t start = strlen("baseline_");
                    size_t end = filename.find(".json");
                    std::string version = filename.substr(start, end - start);
                    versions.push_back(version);
                }
            }
        }

        // 排序版本
        std::sort(versions.begin(), versions.end());

        return versions;
    }

private:
    std::string m_baselineDir;

    bool SaveBaseline(const RegressionBaseline& baseline, const std::string& version)
    {
        std::string filename = GetBaselineFilename(version);
        std::ofstream file(filename);

        if (!file.is_open())
        {
            return false;
        }

        // 保存为JSON格式（简化版）
        file << "{\n";
        file << "  \"version\": \"" << baseline.version << "\",\n";
        file << "  \"timestamp\": \"" << baseline.timestamp << "\",\n";
        file << "  \"tests\": [\n";

        for (size_t i = 0; i < baseline.testBaselines.size(); ++i)
        {
            const auto& test = baseline.testBaselines[i];
            file << "    {\n";
            file << "      \"suite\": \"" << test.suiteName << "\",\n";
            file << "      \"name\": \"" << test.testName << "\",\n";
            file << "      \"expectedPass\": " << (test.expectedPass ? "true" : "false") << ",\n";
            file << "      \"maxExecutionTime\": " << test.maxExecutionTime << "\n";
            file << "    }";

            if (i < baseline.testBaselines.size() - 1)
            {
                file << ",";
            }
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        file.close();
        return true;
    }

    std::string GetBaselineFilename(const std::string& version)
    {
        return m_baselineDir + "/baseline_" + version + ".json";
    }

    std::string GetCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return std::string(buffer);
    }

    std::string ExtractJsonString(const std::string& json, const std::string& key)
    {
        std::string searchKey = "\"" + key + "\": \"";
        size_t pos = json.find(searchKey);
        if (pos != std::string::npos)
        {
            pos += searchKey.length();
            size_t endPos = json.find("\"", pos);
            if (endPos != std::string::npos)
            {
                return json.substr(pos, endPos - pos);
            }
        }
        return "";
    }

    std::string GetDifferenceTypeName(RegressionDifference::DifferenceType type)
    {
        switch (type)
        {
        case RegressionDifference::DifferenceType::NewTest:
            return "新增测试";
        case RegressionDifference::DifferenceType::RemovedTest:
            return "移除的测试";
        case RegressionDifference::DifferenceType::StatusChanged:
            return "状态变化";
        case RegressionDifference::DifferenceType::PerformanceRegression:
            return "性能回归";
        case RegressionDifference::DifferenceType::PerformanceImproved:
            return "性能改进";
        case RegressionDifference::DifferenceType::ExecutionTimeExceeded:
            return "执行时间超标";
        default:
            return "未知";
        }
    }
};

// ============================================================================
// 自动化回归测试运行器
// ============================================================================

class AutomatedRegressionRunner
{
public:
    AutomatedRegressionRunner(TestRunner& runner, RegressionTestManager& manager)
        : m_runner(runner), m_manager(manager)
    {
    }

    // 运行并创建新基线
    bool RunAndCreateBaseline(const std::string& version)
    {
        m_runner.RunAll();
        const auto& results = m_runner.GetResults();

        // 收集全局性能指标
        std::map<std::string, double> globalMetrics;
        double totalTime = 0.0;
        for (const auto& result : results)
        {
            totalTime += result.executionTime;
        }
        globalMetrics["total_execution_time"] = totalTime;
        globalMetrics["test_count"] = static_cast<double>(results.size());

        return m_manager.CreateBaseline(results, version, globalMetrics);
    }

    // 运行回归测试
    RegressionReport RunRegressionTest(const std::string& baselineVersion,
                                       const std::string& currentVersion)
    {
        m_runner.RunAll();
        const auto& results = m_runner.GetResults();

        return m_manager.CompareWithBaseline(results, baselineVersion, currentVersion);
    }

    // 自动回归测试（与最新基线对比）
    RegressionReport AutoRegression(const std::string& currentVersion)
    {
        auto versions = m_manager.ListBaselineVersions();

        if (versions.empty())
        {
            RegressionReport emptyReport;
            emptyReport.currentVersion = currentVersion;
            emptyReport.hasRegression = true;
            RegressionDifference diff;
            diff.description = "没有可用的基线版本";
            emptyReport.differences.push_back(diff);
            return emptyReport;
        }

        // 使用最新版本作为基线
        std::string latestBaseline = versions.back();
        return RunRegressionTest(latestBaseline, currentVersion);
    }

private:
    TestRunner& m_runner;
    RegressionTestManager& m_manager;
};
