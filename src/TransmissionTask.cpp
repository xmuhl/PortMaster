#include "pch.h"
#include "TransmissionTask.h"
#include <algorithm>
#include <fstream>
#include <windows.h>

// 【P1修复】传输任务基类实现

TransmissionTask::TransmissionTask()
    : m_state(TransmissionTaskState::Ready)
    , m_totalBytes(0)
    , m_bytesTransmitted(0)
    , m_chunkSize(1024)            // 默认1KB块大小
    , m_maxRetries(3)              // 默认最大重试3次
    , m_retryDelayMs(50)           // 默认重试延迟50ms
    , m_progressUpdateIntervalMs(100) // 默认进度更新间隔100ms
{
}

TransmissionTask::~TransmissionTask()
{
    Stop();
}

bool TransmissionTask::Start(const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state != TransmissionTaskState::Ready)
    {
        WriteLog("TransmissionTask::Start - 任务状态错误，当前状态: " + std::to_string(static_cast<int>(m_state.load())));
        return false;
    }

    if (data.empty())
    {
        WriteLog("TransmissionTask::Start - 数据为空，无法开始传输");
        return false;
    }

    if (!IsTransportReady())
    {
        WriteLog("TransmissionTask::Start - 传输通道未就绪: " + GetTransportDescription());
        return false;
    }

    // 保存数据和初始化状态
    m_data = data;
    m_totalBytes = data.size();
    m_bytesTransmitted = 0;
    m_state = TransmissionTaskState::Running;
    m_startTime = std::chrono::steady_clock::now();
    m_lastProgressUpdate = m_startTime;

    WriteLog("TransmissionTask::Start - 开始传输任务，数据大小: " + std::to_string(m_totalBytes) +
             " 字节，传输通道: " + GetTransportDescription());

    // 启动后台工作线程
    m_workerThread = std::make_unique<std::thread>(&TransmissionTask::ExecuteTransmission, this);

    // 立即报告初始进度
    UpdateProgress(0, m_totalBytes, "传输开始");

    return true;
}

void TransmissionTask::Pause()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state == TransmissionTaskState::Running)
    {
        m_state = TransmissionTaskState::Paused;
        WriteLog("TransmissionTask::Pause - 传输任务已暂停");

        size_t transmitted = m_bytesTransmitted.load();
        UpdateProgress(transmitted, m_totalBytes, "传输已暂停");
    }
}

void TransmissionTask::Resume()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state == TransmissionTaskState::Paused)
    {
        m_state = TransmissionTaskState::Running;
        WriteLog("TransmissionTask::Resume - 传输任务已恢复");

        size_t transmitted = m_bytesTransmitted.load();
        UpdateProgress(transmitted, m_totalBytes, "传输已恢复");
    }
}

void TransmissionTask::Cancel()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state == TransmissionTaskState::Running || m_state == TransmissionTaskState::Paused)
    {
        m_state = TransmissionTaskState::Cancelled;
        WriteLog("TransmissionTask::Cancel - 传输任务已取消");

        size_t transmitted = m_bytesTransmitted.load();
        UpdateProgress(transmitted, m_totalBytes, "传输已取消");
    }
}

void TransmissionTask::Stop()
{
    // 取消任务
    Cancel();

    // 【回归修复】增强线程安全防护：防止线程自阻塞
    if (m_workerThread && m_workerThread->joinable())
    {
        // 检查当前线程是否是工作线程本身
        // 如果是，则跳过join()操作，避免线程试图等待自己的死锁问题
        if (m_workerThread->get_id() == std::this_thread::get_id())
        {
            WriteLog("TransmissionTask::Stop - 检测到线程自阻塞风险，跳过join()操作");
            // 线程自己调用Stop()时，不执行join()，让线程自然结束
            // 外部析构函数会在适当的时候清理线程对象
        }
        else
        {
            WriteLog("TransmissionTask::Stop - 等待工作线程安全结束");
            m_workerThread->join();
        }
    }
    m_workerThread.reset();

    WriteLog("TransmissionTask::Stop - 传输任务已停止");
}

TransmissionTaskState TransmissionTask::GetState() const
{
    return m_state.load();
}

TransmissionProgress TransmissionTask::GetProgress() const
{
    size_t transmitted = m_bytesTransmitted.load();
    TransmissionTaskState currentState = m_state.load();

    std::string status;
    switch (currentState)
    {
    case TransmissionTaskState::Ready:    status = "准备就绪"; break;
    case TransmissionTaskState::Running:  status = "正在传输"; break;
    case TransmissionTaskState::Paused:   status = "传输暂停"; break;
    case TransmissionTaskState::Cancelled: status = "传输取消"; break;
    case TransmissionTaskState::Completed: status = "传输完成"; break;
    case TransmissionTaskState::Failed:   status = "传输失败"; break;
    default: status = "未知状态"; break;
    }

    return TransmissionProgress(transmitted, m_totalBytes, status);
}

bool TransmissionTask::IsRunning() const
{
    return m_state.load() == TransmissionTaskState::Running;
}

bool TransmissionTask::IsPaused() const
{
    return m_state.load() == TransmissionTaskState::Paused;
}

bool TransmissionTask::IsCompleted() const
{
    TransmissionTaskState state = m_state.load();
    return state == TransmissionTaskState::Completed ||
           state == TransmissionTaskState::Failed ||
           state == TransmissionTaskState::Cancelled;
}

bool TransmissionTask::IsCancelled() const
{
    return m_state.load() == TransmissionTaskState::Cancelled;
}

void TransmissionTask::SetProgressCallback(ProgressCallback callback)
{
    m_progressCallback = callback;
}

void TransmissionTask::SetCompletionCallback(CompletionCallback callback)
{
    m_completionCallback = callback;
}

void TransmissionTask::SetLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

void TransmissionTask::SetChunkSize(size_t chunkSize)
{
    if (chunkSize > 0 && chunkSize <= 64 * 1024) // 限制在64KB以内
    {
        m_chunkSize = chunkSize;
    }
}

void TransmissionTask::SetRetrySettings(int maxRetries, int retryDelayMs)
{
    m_maxRetries = (maxRetries > 0) ? maxRetries : 0;
    m_retryDelayMs = (retryDelayMs > 1) ? retryDelayMs : 1;
}

void TransmissionTask::SetProgressUpdateInterval(int intervalMs)
{
    m_progressUpdateIntervalMs = (intervalMs > 10) ? intervalMs : 10;
}

void TransmissionTask::ExecuteTransmission()
{
    WriteLog("TransmissionTask::ExecuteTransmission - 后台传输线程开始");

    try
    {
        size_t totalSent = 0;
        const size_t dataSize = m_data.size();

        while (totalSent < dataSize)
        {
            // 检查暂停和取消状态
            if (!CheckPauseAndCancel())
            {
                WriteLog("TransmissionTask::ExecuteTransmission - 传输被取消或出错");
                break;
            }

            // 计算当前块大小
            size_t currentChunkSize = (m_chunkSize < (dataSize - totalSent)) ? m_chunkSize : (dataSize - totalSent);

            WriteLog("TransmissionTask::ExecuteTransmission - 发送块 " +
                     std::to_string(totalSent / m_chunkSize + 1) +
                     "/" + std::to_string((dataSize + m_chunkSize - 1) / m_chunkSize) +
                     "，大小: " + std::to_string(currentChunkSize));

            // 重试发送当前块
            TransportError chunkError = TransportError::Success;
            int retryCount = 0;

            do
            {
                chunkError = DoSendChunk(m_data.data() + totalSent, currentChunkSize);

                if (chunkError == TransportError::Success)
                {
                    break; // 发送成功，退出重试循环
                }

                if (chunkError == TransportError::Busy && retryCount < m_maxRetries)
                {
                    WriteLog("TransmissionTask::ExecuteTransmission - 传输忙碌，重试 " +
                             std::to_string(retryCount + 1) + "/" + std::to_string(m_maxRetries));

                    std::this_thread::sleep_for(std::chrono::milliseconds(m_retryDelayMs));
                    retryCount++;
                }
                else
                {
                    WriteLog("TransmissionTask::ExecuteTransmission - 块发送失败，错误码: " +
                             std::to_string(static_cast<int>(chunkError)));
                    break;
                }

            } while (retryCount <= m_maxRetries);

            // 检查块发送结果
            if (chunkError != TransportError::Success)
            {
                WriteLog("TransmissionTask::ExecuteTransmission - 块发送最终失败，停止传输");
                ReportCompletion(TransmissionTaskState::Failed, chunkError,
                               "数据块发送失败，位置: " + std::to_string(totalSent));
                return;
            }

            // 更新进度
            totalSent += currentChunkSize;
            m_bytesTransmitted = totalSent;

            // 定期更新进度（避免过于频繁的UI更新）
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_lastProgressUpdate).count();

            if (timeSinceLastUpdate >= m_progressUpdateIntervalMs || totalSent == dataSize)
            {
                int progress = static_cast<int>((totalSent * 100) / dataSize);
                UpdateProgress(totalSent, dataSize,
                              "正在传输: " + std::to_string(totalSent) + "/" + std::to_string(dataSize) +
                              " 字节 (" + std::to_string(progress) + "%)");
                m_lastProgressUpdate = now;
            }

            // 添加小延迟，避免过快发送
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 检查最终状态
        if (m_state.load() == TransmissionTaskState::Cancelled)
        {
            WriteLog("TransmissionTask::ExecuteTransmission - 传输被用户取消");
            ReportCompletion(TransmissionTaskState::Cancelled, TransportError::WriteFailed, "用户取消传输");
        }
        else if (totalSent == dataSize)
        {
            WriteLog("TransmissionTask::ExecuteTransmission - 传输成功完成");
            ReportCompletion(TransmissionTaskState::Completed, TransportError::Success);
        }
        else
        {
            WriteLog("TransmissionTask::ExecuteTransmission - 传输未完成，数据不完整");
            ReportCompletion(TransmissionTaskState::Failed, TransportError::WriteFailed, "数据传输不完整");
        }
    }
    catch (const std::exception& e)
    {
        WriteLog("TransmissionTask::ExecuteTransmission - 传输过程中发生异常: " + std::string(e.what()));
        ReportCompletion(TransmissionTaskState::Failed, TransportError::WriteFailed,
                        "传输异常: " + std::string(e.what()));
    }

    WriteLog("TransmissionTask::ExecuteTransmission - 后台传输线程结束");
}

void TransmissionTask::UpdateProgress(size_t transmitted, size_t total, const std::string& status)
{
    if (m_progressCallback)
    {
        TransmissionProgress progress(transmitted, total, status);
        m_progressCallback(progress);
    }
}

void TransmissionTask::ReportCompletion(TransmissionTaskState finalState, TransportError errorCode, const std::string& errorMsg)
{
    m_state = finalState;

    if (m_completionCallback)
    {
        TransmissionResult result;
        result.finalState = finalState;
        result.errorCode = errorCode;
        result.bytesTransmitted = m_bytesTransmitted.load();
        result.errorMessage = errorMsg;

        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_startTime);

        m_completionCallback(result);
    }
}

void TransmissionTask::WriteLog(const std::string& message)
{
    if (m_logCallback)
    {
        m_logCallback(message);
    }
}

bool TransmissionTask::CheckPauseAndCancel()
{
    while (true)
    {
        TransmissionTaskState currentState = m_state.load();

        if (currentState == TransmissionTaskState::Cancelled)
        {
            return false; // 任务被取消
        }

        if (currentState == TransmissionTaskState::Running)
        {
            return true; // 任务正常运行
        }

        if (currentState == TransmissionTaskState::Paused)
        {
            // 暂停状态，等待恢复或取消
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 其他状态认为是错误
        return false;
    }
}

// 【P1修复】可靠传输任务实现类

ReliableTransmissionTask::ReliableTransmissionTask(std::shared_ptr<ReliableChannel> reliableChannel)
    : m_reliableChannel(reliableChannel), m_useFileProtocol(true)
{
    // 【重构】可靠传输使用完整文件协议，不再分块发送
    // 设置较大的块大小，因为实际传输由SendFile内部管理
    SetChunkSize(65536); // 64KB，但实际不会用到分块逻辑
}

ReliableTransmissionTask::~ReliableTransmissionTask()
{
    CleanupTempFile();
    
    // 【关键修复】确保协议层正确清理，防止线程泄漏
    if (m_reliableChannel)
    {
        WriteLog("ReliableTransmissionTask::~ReliableTransmissionTask - 开始清理协议层");
        
        // 如果传输仍在活跃状态，先停止传输
        if (m_reliableChannel->IsFileTransferActive())
        {
            WriteLog("ReliableTransmissionTask::~ReliableTransmissionTask - 检测到传输仍活跃，强制停止");
        }
        
        // 关闭协议层，确保所有线程正确退出
        m_reliableChannel->Shutdown();
        WriteLog("ReliableTransmissionTask::~ReliableTransmissionTask - 协议层清理完成");
    }
}

TransportError ReliableTransmissionTask::DoSendChunk(const uint8_t* data, size_t size)
{
    // 【重构】不再使用分块发送，而是使用完整文件协议
    if (!m_reliableChannel || !m_reliableChannel->IsConnected())
    {
        return TransportError::NotOpen;
    }

    // 如果启用文件协议，则使用SendFile
    if (m_useFileProtocol)
    {
        return ExecuteFileTransmission() ? TransportError::Success : TransportError::WriteFailed;
    }
    else
    {
        // 兼容模式：保持原有的分块发送逻辑
        std::vector<uint8_t> chunk(data, data + size);
        bool success = m_reliableChannel->Send(chunk);
        return success ? TransportError::Success : TransportError::WriteFailed;
    }
}

bool ReliableTransmissionTask::IsTransportReady() const
{
    return m_reliableChannel && m_reliableChannel->IsConnected();
}

std::string ReliableTransmissionTask::GetTransportDescription() const
{
    return "可靠传输通道";
}

// 【重构】使用SendFile完整协议执行文件传输
bool ReliableTransmissionTask::ExecuteFileTransmission()
{
    WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 开始使用完整文件协议传输");
    
    // 创建临时文件
    if (!CreateTempFileFromData(GetTransmissionData(), m_tempFilePath))
    {
        WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 创建临时文件失败");
        return false;
    }
    
    WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 临时文件创建成功: " + m_tempFilePath);
    
    // 设置进度回调
    auto progressCallback = [this](int64_t current, int64_t total) {
        if (total > 0)
        {
            int progress = static_cast<int>((current * 100) / total);
            UpdateProgress(static_cast<size_t>(current), static_cast<size_t>(total),
                          "正在传输: " + std::to_string(current) + "/" + std::to_string(total) +
                          " 字节 (" + std::to_string(progress) + "%)");
        }
    };
    
    // 使用SendFile执行完整的START/DATA/END协议
    WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 调用SendFile执行完整协议");
    bool success = m_reliableChannel->SendFile(m_tempFilePath, progressCallback);
    
    if (success)
    {
        WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 文件传输成功完成");
    }
    else
    {
        WriteLog("ReliableTransmissionTask::ExecuteFileTransmission - 文件传输失败");
    }
    
    // 清理临时文件
    CleanupTempFile();
    
    return success;
}

// 【重构】从内存数据创建临时文件
bool ReliableTransmissionTask::CreateTempFileFromData(const std::vector<uint8_t>& data, std::string& tempFilePath)
{
    if (data.empty())
    {
        WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 数据为空");
        return false;
    }
    
    // 生成临时文件路径
    char tempPath[MAX_PATH];
    char tempFileName[MAX_PATH];
    
    if (GetTempPathA(MAX_PATH, tempPath) == 0)
    {
        WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 获取临时目录失败");
        return false;
    }
    
    if (GetTempFileNameA(tempPath, "PMT", 0, tempFileName) == 0)
    {
        WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 生成临时文件名失败");
        return false;
    }
    
    tempFilePath = tempFileName;
    
    // 写入数据到临时文件
    try
    {
        std::ofstream file(tempFilePath, std::ios::binary);
        if (!file.is_open())
        {
            WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 打开临时文件失败: " + tempFilePath);
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 临时文件创建成功，大小: " + 
                 std::to_string(data.size()) + " 字节");
        return true;
    }
    catch (const std::exception& e)
    {
        WriteLog("ReliableTransmissionTask::CreateTempFileFromData - 异常: " + std::string(e.what()));
        return false;
    }
}

// 【重构】清理临时文件
void ReliableTransmissionTask::CleanupTempFile()
{
    if (!m_tempFilePath.empty())
    {
        if (DeleteFileA(m_tempFilePath.c_str()))
        {
            WriteLog("ReliableTransmissionTask::CleanupTempFile - 临时文件清理成功: " + m_tempFilePath);
        }
        else
        {
            WriteLog("ReliableTransmissionTask::CleanupTempFile - 临时文件清理失败: " + m_tempFilePath);
        }
        m_tempFilePath.clear();
    }
}

// 【P1修复】原始传输任务实现类

RawTransmissionTask::RawTransmissionTask(std::shared_ptr<ITransport> transport)
    : m_transport(transport)
{
    // 为原始传输设置较大的块大小以提高效率
    SetChunkSize(4096);
}

TransportError RawTransmissionTask::DoSendChunk(const uint8_t* data, size_t size)
{
    if (!m_transport || !m_transport->IsOpen())
    {
        return TransportError::NotOpen;
    }

    return m_transport->Write(data, size);
}

bool RawTransmissionTask::IsTransportReady() const
{
    return m_transport && m_transport->IsOpen();
}

std::string RawTransmissionTask::GetTransportDescription() const
{
    return "原始传输通道";
}