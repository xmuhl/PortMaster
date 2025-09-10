#pragma once

#include "AsyncMessageManager.h"
#include "../Transport/ITransport.h"
#include "../Protocol/ReliableChannel.h"
#include <memory>

// 异步消息系统集成工具类 - 用于将现有回调系统迁移到新的消息架构
class AsyncMessageIntegration
{
public:
    // 为传输层设置增强的异步回调
    static void SetupEnhancedTransportCallbacks(
        std::shared_ptr<ITransport> transport, 
        HWND mainWindow)
    {
        if (!transport)
            return;
            
        // 数据接收回调 - 使用高优先级异步处理
        transport->SetDataReceivedCallback([mainWindow](const std::vector<uint8_t>& data) {
            auto message = std::make_shared<DataReceivedMessage>(
                MessagePriority::HIGH,
                data,
                [mainWindow, data](const std::vector<uint8_t>& receivedData) {
                    // 通过Windows消息传递到主线程
                    if (::IsWindow(mainWindow))
                    {
                        // 使用PostMessage确保线程安全
                        // 数据通过全局缓存或其他机制传递
                        ::PostMessage(mainWindow, WM_USER + 200, 0, 0);
                    }
                }
            );
            
            AsyncMessageManager::GetInstance().PostMessage(message);
        });
        
        // 状态变化回调 - 使用关键优先级
        transport->SetStateChangedCallback([](TransportState state, const std::string& message) {
            auto asyncMessage = std::make_shared<StateChangedMessage>(
                MessagePriority::CRITICAL,
                static_cast<int>(state),
                message,
                [](int state, const std::string& desc) {
                    // 处理状态变化
                    WriteDebugLog(("[INFO] Transport state changed: " + desc).c_str());
                    // 可以在这里添加更多状态处理逻辑
                }
            );
            
            AsyncMessageManager::GetInstance().PostMessage(asyncMessage);
        });
    }
    
    // 为可靠通道设置增强的异步回调
    static void SetupEnhancedReliableChannelCallbacks(
        std::shared_ptr<ReliableChannel> channel,
        HWND mainWindow)
    {
        if (!channel)
            return;
            
        // 进度更新回调 - 使用普通优先级并启用限流
        channel->SetProgressCallback([](const TransferStats& stats) {
            AsyncMessageManager::GetInstance().PostMessage<UIUpdateMessage>(
                MessagePriority::NORMAL,
                [stats]() {
                    // 更新UI进度显示
                    WriteDebugLog(("[DEBUG] Progress: " + 
                                  std::to_string(stats.GetProgress() * 100) + "%").c_str());
                }
            );
        });
        
        // 完成回调 - 使用高优先级
        channel->SetCompletionCallback([](bool success, const std::string& message) {
            AsyncMessageManager::GetInstance().PostMessage<UIUpdateMessage>(
                MessagePriority::HIGH,
                [success, message]() {
                    WriteDebugLog(("[INFO] Transfer " + 
                                  std::string(success ? "completed" : "failed") + 
                                  ": " + message).c_str());
                }
            );
        });
        
        // 文件接收回调 - 使用高优先级
        channel->SetFileReceivedCallback([](const std::string& filename, 
                                           const std::vector<uint8_t>& data) {
            AsyncMessageManager::GetInstance().PostMessage<UIUpdateMessage>(
                MessagePriority::HIGH,
                [filename, data]() {
                    WriteDebugLog(("[INFO] File received: " + filename + 
                                  " (" + std::to_string(data.size()) + " bytes)").c_str());
                }
            );
        });
        
        // 数据块接收回调 - 使用普通优先级，启用限流
        channel->SetChunkReceivedCallback([](const std::vector<uint8_t>& chunk, 
                                            size_t current, size_t total) {
            // 只处理关键的进度更新，避免消息洪水
            if (current % 1024 == 0 || current == total) // 每1KB或最后一块
            {
                AsyncMessageManager::GetInstance().PostMessage<UIUpdateMessage>(
                    MessagePriority::NORMAL,
                    [current, total]() {
                        double progress = total > 0 ? (double)current / total * 100 : 0;
                        WriteDebugLog(("[DEBUG] Chunk progress: " + 
                                      std::to_string(progress) + "%").c_str());
                    }
                );
            }
        });
    }
    
    // 设置消息系统的最佳实践配置
    static void ConfigureBestPractices()
    {
        auto& msgManager = AsyncMessageManager::GetInstance();
        
        // 设置合理的限流策略
        msgManager.SetRateLimit(MessageType::UI_PROGRESS_UPDATE, 10);  // 10次/秒
        msgManager.SetRateLimit(MessageType::UI_STATUS_UPDATE, 5);     // 5次/秒
        msgManager.SetRateLimit(MessageType::PROTOCOL_PROGRESS_UPDATE, 20); // 20次/秒
        
        // 在调试模式下可能需要更多日志消息
        #ifdef _DEBUG
        msgManager.SetRateLimit(MessageType::UI_LOG_APPEND, 50);       // 调试模式允许更多日志
        #else
        msgManager.SetRateLimit(MessageType::UI_LOG_APPEND, 20);       // 发布模式限制日志频率
        #endif
        
        // 设置错误处理策略
        msgManager.SetErrorHandler([](const std::string& error, MessageType type) {
            WriteDebugLog(("[ERROR] AsyncMessage [Type=" + 
                          std::to_string(static_cast<int>(type)) + "]: " + error).c_str());
            
            // 对关键消息类型的错误进行特殊处理
            if (type == MessageType::SYSTEM_ERROR || type == MessageType::TRANSPORT_ERROR)
            {
                // 可以在这里添加关键错误的特殊处理逻辑
                // 比如显示错误对话框、记录到错误日志文件等
            }
        });
    }
    
    // 获取消息系统统计信息的便捷方法
    static std::string GetMessageSystemStatus()
    {
        auto stats = AsyncMessageManager::GetInstance().GetStatistics();
        
        std::string status = "AsyncMessageManager Status:\n";
        status += "  Total Processed: " + std::to_string(stats.totalProcessed) + "\n";
        status += "  Queue Size: " + std::to_string(stats.currentQueueSize) + "\n";
        status += "  Failed Messages: " + std::to_string(stats.failedMessages) + "\n";
        status += "  Retried Messages: " + std::to_string(stats.retriedMessages) + "\n";
        
        if (!stats.messageTypeCounts.empty())
        {
            status += "  Message Type Counts:\n";
            for (const auto& pair : stats.messageTypeCounts)
            {
                status += "    Type " + std::to_string(static_cast<int>(pair.first)) + 
                         ": " + std::to_string(pair.second) + "\n";
            }
        }
        
        return status;
    }
};

// 使用示例宏定义，简化常见的异步消息发送
#define POST_UI_UPDATE_MESSAGE(priority, code) \
    AsyncMessageManager::GetInstance().PostMessage<UIUpdateMessage>(priority, [this]() { code; })

#define POST_DATA_MESSAGE(data, handler) \
    AsyncMessageManager::GetInstance().PostMessage<DataReceivedMessage>( \
        MessagePriority::HIGH, data, handler)

#define POST_STATUS_MESSAGE(state, desc, handler) \
    AsyncMessageManager::GetInstance().PostMessage<StateChangedMessage>( \
        MessagePriority::CRITICAL, state, desc, handler)

extern void WriteDebugLog(const char* message);