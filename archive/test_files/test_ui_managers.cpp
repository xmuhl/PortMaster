// test_ui_managers.cpp - UI管理器功能测试程序
#pragma execution_character_set("utf-8")

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>

// 模拟包含我们创建的UI管理器头文件
#include "src/UIStateManager.h"
#include "src/TransmissionStateManager.h"
#include "src/ButtonStateManager.h"
#include "src/ThreadSafeUIUpdater.h"
#include "src/ThreadSafeProgressManager.h"

// 测试统计
struct TestStats {
    std::atomic<int> totalTests{0};
    std::atomic<int> passedTests{0};
    std::atomic<int> failedTests{0};

    void recordTest(bool passed) {
        totalTests++;
        if (passed) passedTests++;
        else failedTests++;
    }

    void printSummary() {
        std::cout << "\n=== 测试统计 ===" << std::endl;
        std::cout << "总测试数: " << totalTests.load() << std::endl;
        std::cout << "通过测试: " << passedTests.load() << std::endl;
        std::cout << "失败测试: " << failedTests.load() << std::endl;
        std::cout << "成功率: " << (totalTests.load() > 0 ? (passedTests.load() * 100 / totalTests.load()) : 0) << "%" << std::endl;
        std::cout << "================" << std::endl;
    }
};

// 全局测试统计
TestStats g_testStats;

// 测试辅助宏
#define TEST_ASSERT(condition, message) \
    do { \
        bool result = (condition); \
        std::cout << "[TEST] " << message << ": " << (result ? "PASS" : "FAIL") << std::endl; \
        g_testStats.recordTest(result); \
        if (!result) { \
            std::cout << "  错误位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
        } \
    } while(0)

// 模拟UI控件更新回调
class MockUIControl {
private:
    std::string m_currentText;
    bool m_enabled;
public:
    MockUIControl() : m_enabled(true) {}

    void SetText(const std::string& text) {
        m_currentText = text;
        std::cout << "  [UI更新] 控件文本: " << text << std::endl;
    }

    void EnableWindow(bool enable) {
        m_enabled = enable;
        std::cout << "  [UI更新] 控件状态: " << (enable ? "启用" : "禁用") << std::endl;
    }

    std::string GetText() const { return m_currentText; }
    bool IsEnabled() const { return m_enabled; }
};

// UIStateManager完整测试
void TestUIStateManager()
{
    std::cout << "\n=== 测试UIStateManager ===" << std::endl;

    auto manager = std::make_unique<UIStateManager>();
    MockUIControl mockControl;

    // 测试1: 基本状态更新
    std::cout << "测试1: 基本状态更新" << std::endl;
    manager->UpdateConnectionStatus("未连接", UIStateManager::Priority::Normal);
    TEST_ASSERT(manager->GetCurrentStatusText() == "未连接", "连接状态设置");

    // 测试2: 优先级测试
    std::cout << "测试2: 优先级测试" << std::endl;
    manager->UpdateTransmissionStatus("传输中...", UIStateManager::Priority::Normal);
    TEST_ASSERT(manager->GetCurrentStatusText() == "传输中...", "传输状态优先于连接状态");

    manager->UpdateErrorStatus("严重错误", UIStateManager::Priority::High);
    TEST_ASSERT(manager->GetCurrentStatusText() == "严重错误", "错误状态优先级最高");

    // 测试3: 状态清除
    std::cout << "测试3: 状态清除" << std::endl;
    manager->ClearErrorStatus();
    TEST_ASSERT(manager->GetCurrentStatusText() == "传输中...", "错误状态清除后恢复传输状态");

    manager->ClearTransmissionStatus();
    TEST_ASSERT(manager->GetCurrentStatusText() == "未连接", "传输状态清除后恢复连接状态");

    // 测试4: 多次更新相同优先级
    std::cout << "测试4: 相同优先级更新" << std::endl;
    manager->UpdateProgressStatus("50%", UIStateManager::Priority::Normal);
    manager->UpdateConnectionStatus("已连接", UIStateManager::Priority::Normal);
    TEST_ASSERT(manager->GetCurrentStatusText() == "已连接", "后更新的连接状态");

    std::cout << "UIStateManager测试完成" << std::endl;
}

// TransmissionStateManager完整测试
void TestTransmissionStateManager()
{
    std::cout << "\n=== 测试TransmissionStateManager ===" << std::endl;

    auto manager = std::make_unique<TransmissionStateManager>();

    // 设置状态变化回调
    bool callbackTriggered = false;
    TransmissionUIState oldState, newState;
    manager->SetStateChangeCallback([&](TransmissionUIState oldS, TransmissionUIState newS) {
        callbackTriggered = true;
        oldState = oldS;
        newState = newS;
        std::cout << "  [回调] " << manager->GetStateDescription(oldS)
                  << " -> " << manager->GetStateDescription(newS) << std::endl;
    });

    // 测试1: 初始状态
    std::cout << "测试1: 初始状态" << std::endl;
    TEST_ASSERT(manager->GetCurrentState() == TransmissionUIState::Idle, "初始状态为空闲");
    TEST_ASSERT(manager->CanStartNewTransmission(), "空闲状态可以开始新传输");

    // 测试2: 有效状态转换
    std::cout << "测试2: 有效状态转换" << std::endl;
    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Connecting, "开始连接"),
                "空闲状态 -> 连接中");
    TEST_ASSERT(callbackTriggered, "状态转换回调被触发");
    TEST_ASSERT(oldState == TransmissionUIState::Idle, "回调中的旧状态正确");
    TEST_ASSERT(newState == TransmissionUIState::Connecting, "回调中的新状态正确");

    callbackTriggered = false;
    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Connected, "连接成功"),
                "连接中 -> 已连接");
    TEST_ASSERT(callbackTriggered, "第二次状态转换回调被触发");

    // 测试3: 无效状态转换
    std::cout << "测试3: 无效状态转换" << std::endl;
    callbackTriggered = false;
    TEST_ASSERT(!manager->RequestStateTransition(TransmissionUIState::Idle, "尝试空闲转换"),
                "已连接状态不能直接回到空闲");
    TEST_ASSERT(!callbackTriggered, "无效状态转换不触发回调");

    // 测试4: 传输流程
    std::cout << "测试4: 完整传输流程" << std::endl;
    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Transmitting, "开始传输"),
                "连接中 -> 传输中");
    TEST_ASSERT(manager->IsTransmitting(), "传输中状态判断正确");

    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Completed, "传输完成"),
                "传输中 -> 完成");
    TEST_ASSERT(!manager->IsTransmitting(), "完成状态判断正确");
    TEST_ASSERT(manager->CanStartNewTransmission(), "完成状态可以开始新传输");

    // 测试5: 错误状态处理
    std::cout << "测试5: 错误状态处理" << std::endl;
    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Transmitting, "开始传输"),
                "完成状态 -> 传输中");
    TEST_ASSERT(manager->RequestStateTransition(TransmissionUIState::Failed, "传输失败"),
                "传输中 -> 失败");
    TEST_ASSERT(manager->IsErrorState(), "失败状态判断正确");

    std::cout << "当前状态: " << manager->GetStateDescription(manager->GetCurrentState()) << std::endl;
    std::cout << "TransmissionStateManager测试完成" << std::endl;
}

// ButtonStateManager完整测试
void TestButtonStateManager()
{
    std::cout << "\n=== 测试ButtonStateManager ===" << std::endl;

    auto manager = std::make_unique<ButtonStateManager>();

    // 测试1: 空闲状态
    std::cout << "测试1: 空闲状态" << std::endl;
    manager->ApplyIdleState();
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Connect), "空闲状态连接按钮启用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::Disconnect), "空闲状态断开按钮禁用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::Send), "空闲状态发送按钮禁用");
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::File), "空闲状态文件按钮启用");

    // 测试2: 传输状态
    std::cout << "测试2: 传输状态" << std::endl;
    manager->ApplyTransmittingState();
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::Connect), "传输状态连接按钮禁用");
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Stop), "传输状态停止按钮启用");
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Send), "传输状态发送按钮启用（变为中断）");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::File), "传输状态文件按钮禁用");

    // 测试3: 暂停状态
    std::cout << "测试3: 暂停状态" << std::endl;
    manager->ApplyPausedState();
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Send), "暂停状态发送按钮启用（变为继续）");
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Stop), "暂停状态停止按钮启用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::File), "暂停状态文件按钮禁用");

    // 测试4: 可靠传输模式
    std::cout << "测试4: 可靠传输模式" << std::endl;
    manager->ApplyReliableModeTransmittingState();
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::ClearReceive), "可靠模式清空接收按钮禁用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::CopyAll), "可靠模式复制全部按钮禁用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::SaveAll), "可靠模式保存全部按钮禁用");

    // 测试5: 错误状态恢复
    std::cout << "测试5: 错误状态恢复" << std::endl;
    manager->ApplyErrorState();
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::Connect), "错误状态连接按钮启用");
    TEST_ASSERT(!manager->IsButtonEnabled(ButtonID::Disconnect), "错误状态断开按钮禁用");
    TEST_ASSERT(manager->IsButtonEnabled(ButtonID::File), "错误状态文件按钮启用");

    std::cout << "ButtonStateManager测试完成" << std::endl;
}

// ThreadSafeUIUpdater完整测试
void TestThreadSafeUIUpdater()
{
    std::cout << "\n=== 测试ThreadSafeUIUpdater ===" << std::endl;

    auto updater = std::make_unique<ThreadSafeUIUpdater>();

    // 测试1: 启动停止
    std::cout << "测试1: 启动停止" << std::endl;
    TEST_ASSERT(updater->Start(), "UI更新器启动成功");
    TEST_ASSERT(updater->IsRunning(), "运行状态检查");
    TEST_ASSERT(updater->GetQueueSize() == 0, "初始队列为空");

    // 测试2: 基本队列操作
    std::cout << "测试2: 基本队列操作" << std::endl;
    bool updateExecuted = false;
    updater->QueueUpdate([&updateExecuted]() {
        updateExecuted = true;
        std::cout << "  [队列执行] 自定义更新执行" << std::endl;
    }, "测试更新");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    TEST_ASSERT(updateExecuted, "队列更新被执行");

    // 测试3: 批量更新
    std::cout << "测试3: 批量更新" << std::endl;
    std::atomic<int> updateCount{0};
    std::vector<UIUpdateOperation> operations;
    for (int i = 0; i < 5; ++i) {
        operations.emplace_back([&updateCount, i]() {
            updateCount++;
            std::cout << "  [批量更新] 执行操作 " << i << std::endl;
        });
    }

    updater->QueueBatchUpdates(operations);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    TEST_ASSERT(updateCount.load() == 5, "所有批量更新被执行");

    // 测试4: 优先级更新
    std::cout << "测试4: 优先级更新" << std::endl;
    bool priorityExecuted = false;
    updater->QueueUpdate([]() {
        std::cout << "  [普通更新] 普通更新" << std::endl;
    }, "普通更新");

    updater->QueuePriorityUpdate(UIUpdateType::UpdateStatusText, 0, "优先级更新");
    updater->QueueUpdate([&priorityExecuted]() {
        priorityExecuted = true;
        std::cout << "  [优先级更新] 优先级更新执行" << std::endl;
    }, "优先级更新后置");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    TEST_ASSERT(priorityExecuted, "优先级更新被执行");

    // 测试5: 队列清理
    std::cout << "测试5: 队列清理" << std::endl;
    updater->QueueUpdate([]() {
        std::cout << "  [清理前] 这个更新应该被执行" << std::endl;
    }, "清理前更新");

    updater->ClearQueue();
    TEST_ASSERT(updater->GetQueueSize() == 0, "队列清空后为空");

    updater->Stop();
    TEST_ASSERT(!updater->IsRunning(), "停止后运行状态");

    std::cout << "ThreadSafeUIUpdater测试完成" << std::endl;
}

// ThreadSafeProgressManager完整测试
void TestThreadSafeProgressManager()
{
    std::cout << "\n=== 测试ThreadSafeProgressManager ===" << std::endl;

    auto manager = std::make_unique<ThreadSafeProgressManager>();

    // 测试1: 基本进度设置
    std::cout << "测试1: 基本进度设置" << std::endl;
    TEST_ASSERT(manager->GetCurrentProgress() == 0, "初始进度为0");
    TEST_ASSERT(manager->GetTotalProgress() == 0, "初始总进度为0");
    TEST_ASSERT(manager->GetPercentageProgress() == 0, "初始百分比为0");

    manager->SetProgress(50, 100, "50%完成");
    TEST_ASSERT(manager->GetCurrentProgress() == 50, "当前进度设置正确");
    TEST_ASSERT(manager->GetTotalProgress() == 100, "总进度设置正确");
    TEST_ASSERT(manager->GetPercentageProgress() == 50, "百分比计算正确");
    TEST_ASSERT(manager->GetStatusText() == "50%完成", "状态文本设置正确");

    // 测试2: 进度回调
    std::cout << "测试2: 进度回调" << std::endl;
    bool callbackTriggered = false;
    ProgressInfo lastProgress;
    manager->SetProgressCallback([&callbackTriggered, &lastProgress](const ProgressInfo& progress) {
        callbackTriggered = true;
        lastProgress = progress;
        std::cout << "  [进度回调] " << progress.percentage << "% - " << progress.statusText << std::endl;
    });

    manager->SetProgress(75, 100, "75%完成");
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 等待回调
    TEST_ASSERT(callbackTriggered, "进度回调被触发");
    TEST_ASSERT(lastProgress.percentage == 75, "回调中的百分比正确");

    // 测试3: 增量更新
    std::cout << "测试3: 增量更新" << std::endl;
    callbackTriggered = false;
    manager->IncrementProgress(10, "增量更新");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    TEST_ASSERT(callbackTriggered, "增量更新回调被触发");
    TEST_ASSERT(manager->GetCurrentProgress() == 85, "增量更新后进度正确");

    // 测试4: 完成状态
    std::cout << "测试4: 完成状态" << std::endl;
    manager->SetComplete("完成！");
    TEST_ASSERT(manager->IsComplete(), "完成状态判断正确");
    TEST_ASSERT(!manager->IsInProgress(), "完成状态不是进行中");

    // 测试5: 重置功能
    std::cout << "测试5: 重置功能" << std::endl;
    manager->ResetProgress("重置完成");
    TEST_ASSERT(manager->GetCurrentProgress() == 0, "重置后当前进度为0");
    TEST_ASSERT(manager->GetTotalProgress() == 0, "重置后总进度为0");
    TEST_ASSERT(manager->GetStatusText() == "重置完成", "重置后状态文本正确");

    std::cout << "当前进度: " << manager->GetPercentageProgress() << "%" << std::endl;
    std::cout << "更新次数: " << manager->GetUpdateCount() << std::endl;
    std::cout << "ThreadSafeProgressManager测试完成" << std::endl;
}

// 线程安全测试
void TestThreadSafety()
{
    std::cout << "\n=== 测试线程安全性 ===" << std::endl;

    auto uiManager = std::make_unique<UIStateManager>();
    auto progressManager = std::make_unique<ThreadSafeProgressManager>();
    auto buttonManager = std::make_unique<ButtonStateManager>();

    // 多线程并发测试
    std::atomic<int> uiUpdateCount{0};
    std::atomic<int> progressUpdateCount{0};
    std::atomic<int> buttonUpdateCount{0};

    const int threadCount = 5;
    const int updatesPerThread = 20;
    std::vector<std::thread> threads;

    // UI状态更新线程
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&uiManager, &uiUpdateCount, updatesPerThread, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 4);

            for (int j = 0; j < updatesPerThread; ++j) {
                int type = dis(gen);
                switch (type) {
                case 1:
                    uiManager->UpdateConnectionStatus("连接状态" + std::to_string(i), UIStateManager::Priority::Normal);
                    break;
                case 2:
                    uiManager->UpdateTransmissionStatus("传输状态" + std::to_string(i), UIStateManager::Priority::Normal);
                    break;
                case 3:
                    uiManager->UpdateProgressStatus("进度" + std::to_string(i), UIStateManager::Priority::Normal);
                    break;
                case 4:
                    uiManager->UpdateErrorStatus("错误" + std::to_string(i), UIStateManager::Priority::High);
                    break;
                }
                uiUpdateCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // 进度更新线程
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&progressManager, &progressUpdateCount, updatesPerThread, i]() {
            for (int j = 0; j < updatesPerThread; ++j) {
                int progress = (j * 100) / updatesPerThread;
                progressManager->SetProgress(progress, 100, "线程" + std::to_string(i) + " 进度" + std::to_string(j));
                progressUpdateCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // 按钮状态更新线程
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&buttonManager, &buttonUpdateCount, updatesPerThread, i]() {
            for (int j = 0; j < updatesPerThread; ++j) {
                if (j % 2 == 0) {
                    buttonManager->ApplyTransmittingState();
                } else {
                    buttonManager->ApplyPausedState();
                }
                buttonUpdateCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "线程安全测试结果:" << std::endl;
    std::cout << "  UI更新总数: " << uiUpdateCount.load() << std::endl;
    std::cout << "  进度更新总数: " << progressUpdateCount.load() << std::endl;
    std::cout << "  按钮更新总数: " << buttonUpdateCount.load() << std::endl;

    TEST_ASSERT(uiUpdateCount.load() == threadCount * updatesPerThread, "所有UI更新完成");
    TEST_ASSERT(progressUpdateCount.load() == threadCount * updatesPerThread, "所有进度更新完成");
    TEST_ASSERT(buttonUpdateCount.load() == threadCount * updatesPerThread, "所有按钮更新完成");

    std::cout << "线程安全测试完成" << std::endl;
}

// 集成测试
void TestIntegration()
{
    std::cout << "\n=== 测试集成功能 ===" << std::endl;

    // 模拟PortMasterDlg中的完整集成
    auto uiManager = std::make_unique<UIStateManager>();
    auto transmissionManager = std::make_unique<TransmissionStateManager>();
    auto buttonManager = std::make_unique<ButtonStateManager>();
    auto progressManager = std::make_unique<ThreadSafeProgressManager>();
    auto uiUpdater = std::make_unique<ThreadSafeUIUpdater>();

    // 初始化全局指针
    g_uiStateManager = uiManager.get();
    g_transmissionStateManager = transmissionManager.get();
    g_buttonStateManager = buttonManager.get();
    g_threadSafeProgressManager = progressManager.get();
    g_threadSafeUIUpdater = uiUpdater.get();

    // 设置回调链
    transmissionManager->SetStateChangeCallback(
        [uiManager, buttonManager](TransmissionUIState oldState, TransmissionUIState newState) {
            std::cout << "  [集成] 传输状态变化: "
                      << transmissionManager->GetStateDescription(oldState)
                      << " -> " << transmissionManager->GetStateDescription(newState) << std::endl;

            // 根据传输状态更新UI
            switch (newState) {
            case TransmissionUIState::Idle:
                uiManager->UpdateConnectionStatus("准备就绪");
                buttonManager->ApplyIdleState();
                break;
            case TransmissionUIState::Transmitting:
                uiManager->UpdateTransmissionStatus("数据传输中...");
                buttonManager->ApplyTransmittingState();
                break;
            case TransmissionUIState::Paused:
                uiManager->UpdateTransmissionStatus("传输已暂停");
                buttonManager->ApplyPausedState();
                break;
            case TransmissionUIState::Completed:
                uiManager->UpdateTransmissionStatus("传输完成");
                buttonManager->ApplyCompletedState();
                break;
            case TransmissionUIState::Failed:
            case TransmissionUIState::Error:
                uiManager->UpdateErrorStatus("传输失败", UIStateManager::Priority::High);
                buttonManager->ApplyErrorState();
                break;
            default:
                break;
            }
        });

    progressManager->SetProgressCallback(
        [uiManager](const ProgressInfo& progress) {
            uiManager->UpdateProgressStatus(std::to_string(progress.percentage) + "%");
        });

    uiUpdater->Start();

    // 模拟完整传输流程
    std::cout << "模拟完整传输流程:" << std::endl;

    // 1. 开始传输
    TEST_ASSERT(transmissionManager->RequestStateTransition(TransmissionUIState::Transmitting, "开始传输"),
                "开始传输状态转换");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. 更新进度
    for (int i = 10; i <= 100; i += 10) {
        progressManager->SetProgress(i, 100, "传输中...");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 3. 模拟暂停
    TEST_ASSERT(transmissionManager->RequestStateTransition(TransmissionUIState::Paused, "暂停传输"),
                "暂停状态转换");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. 恢复传输
    TEST_ASSERT(transmissionManager->RequestStateTransition(TransmissionUIState::Transmitting, "恢复传输"),
                "恢复状态转换");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 5. 完成传输
    progressManager->SetComplete("传输完成");
    TEST_ASSERT(transmissionManager->RequestStateTransition(TransmissionUIState::Completed, "传输完成"),
                "完成状态转换");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证最终状态
    TEST_ASSERT(uiManager->GetCurrentStatusText() == "传输完成", "最终UI状态正确");
    TEST_ASSERT(!buttonManager->IsButtonEnabled(ButtonID::Disconnect), "完成状态断开按钮启用");
    TEST_ASSERT(buttonManager->IsButtonEnabled(ButtonID::SaveAll), "完成状态保存按钮启用");

    // 验证进度管理器状态
    TEST_ASSERT(progressManager->IsComplete(), "进度管理器完成状态");

    uiUpdater->Stop();

    std::cout << "集成测试完成" << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "    UI管理器功能测试程序 v2.0" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        TestUIStateManager();
        TestTransmissionStateManager();
        TestButtonStateManager();
        TestThreadSafeUIUpdater();
        TestThreadSafeProgressManager();
        TestThreadSafety();
        TestIntegration();

        g_testStats.printSummary();

        std::cout << "\n========================================" << std::endl;
        if (g_testStats.failedTests.load() == 0) {
            std::cout << "🎉 所有测试通过！UI管理器功能完全正常。" << std::endl;
        } else {
            std::cout << "⚠️  有 " << g_testStats.failedTests.load() << " 个测试失败，需要检查。" << std::endl;
        }
        std::cout << "========================================" << std::endl;

        return g_testStats.failedTests.load() == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知测试异常" << std::endl;
        return 1;
    }
}