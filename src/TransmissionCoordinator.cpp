#pragma execution_character_set("utf-8")

#include "pch.h"
#include "TransmissionCoordinator.h"

// ==================== 构造与析构 ====================

TransmissionCoordinator::TransmissionCoordinator()
	: m_chunkSize(1024)
	, m_maxRetries(3)
	, m_retryDelayMs(100)
	, m_progressUpdateIntervalMs(100)
{
}

TransmissionCoordinator::~TransmissionCoordinator()
{
	// 取消当前任务（如果有）
	Cancel();
}

// ==================== 传输控制接口 ====================

bool TransmissionCoordinator::Start(
	const std::vector<uint8_t>& data,
	std::shared_ptr<ReliableChannel> reliableChannel,
	std::shared_ptr<ITransport> transport)
{
	// 检查是否已有任务在运行
	if (m_currentTask && !m_currentTask->IsCompleted())
	{
		return false;
	}

	// 检查数据有效性
	if (data.empty())
	{
		return false;
	}

	// 创建任务
	m_currentTask = CreateTask(reliableChannel, transport);
	if (!m_currentTask)
	{
		return false;
	}

	// 设置任务参数
	m_currentTask->SetChunkSize(m_chunkSize);
	m_currentTask->SetRetrySettings(m_maxRetries, m_retryDelayMs);
	m_currentTask->SetProgressUpdateInterval(m_progressUpdateIntervalMs);

	// 设置任务回调
	m_currentTask->SetProgressCallback([this](const TransmissionProgress& progress) {
		OnProgress(progress);
		});
	m_currentTask->SetCompletionCallback([this](const TransmissionResult& result) {
		OnCompletion(result);
		});
	m_currentTask->SetLogCallback([this](const std::string& message) {
		OnLog(message);
		});

	// 启动任务
	return m_currentTask->Start(data);
}

void TransmissionCoordinator::Pause()
{
	if (m_currentTask && m_currentTask->IsRunning())
	{
		m_currentTask->Pause();
	}
}

void TransmissionCoordinator::Resume()
{
	if (m_currentTask && m_currentTask->IsPaused())
	{
		m_currentTask->Resume();
	}
}

void TransmissionCoordinator::Cancel()
{
	// 【阶段四修复】改为异步取消：不立即reset
	if (m_currentTask)
	{
		m_currentTask->Cancel();
		// 【关键】不在此处执行m_currentTask.reset()
		// 这样做可以避免UI线程等待工作线程退出
		// 工作线程会自行检查Cancelled状态并安全退出
		// 真实的reset()延迟至完成消息处理时执行（通过OnTransmissionComplete）
		// 或在关闭程序时由ShutdownActiveTransmission进行清理
	}
}

// 【阶段四修复】延迟清理方法
void TransmissionCoordinator::CleanupTransmissionTask()
{
	if (m_currentTask)
	{
		m_currentTask.reset();
	}
}

// ==================== 状态查询接口 ====================

TransmissionTaskState TransmissionCoordinator::GetState() const
{
	if (m_currentTask)
	{
		return m_currentTask->GetState();
	}
	return TransmissionTaskState::Ready;
}

bool TransmissionCoordinator::IsRunning() const
{
	return m_currentTask && m_currentTask->IsRunning();
}

bool TransmissionCoordinator::IsPaused() const
{
	return m_currentTask && m_currentTask->IsPaused();
}

bool TransmissionCoordinator::IsCompleted() const
{
	return m_currentTask && m_currentTask->IsCompleted();
}

// ==================== 回调接口 ====================

void TransmissionCoordinator::SetProgressCallback(ProgressCallback callback)
{
	m_progressCallback = callback;
}

void TransmissionCoordinator::SetCompletionCallback(CompletionCallback callback)
{
	m_completionCallback = callback;
}

void TransmissionCoordinator::SetLogCallback(LogCallback callback)
{
	m_logCallback = callback;
}

// ==================== 配置接口 ====================

void TransmissionCoordinator::SetChunkSize(size_t chunkSize)
{
	m_chunkSize = chunkSize;
}

void TransmissionCoordinator::SetRetrySettings(int maxRetries, int retryDelayMs)
{
	m_maxRetries = maxRetries;
	m_retryDelayMs = retryDelayMs;
}

void TransmissionCoordinator::SetProgressUpdateInterval(int intervalMs)
{
	m_progressUpdateIntervalMs = intervalMs;
}

// ==================== 内部方法 ====================

std::unique_ptr<TransmissionTask> TransmissionCoordinator::CreateTask(
	std::shared_ptr<ReliableChannel> reliableChannel,
	std::shared_ptr<ITransport> transport)
{
	if (reliableChannel && reliableChannel->IsConnected())
	{
		// 使用可靠传输
		return std::make_unique<ReliableTransmissionTask>(reliableChannel);
	}
	else if (transport && transport->IsOpen())
	{
		// 使用原始传输
		return std::make_unique<RawTransmissionTask>(transport);
	}

	return nullptr;
}

void TransmissionCoordinator::OnProgress(const TransmissionProgress& progress)
{
	// 转发到外部回调
	if (m_progressCallback)
	{
		m_progressCallback(progress);
	}
}

void TransmissionCoordinator::OnCompletion(const TransmissionResult& result)
{
	// 转发到外部回调
	if (m_completionCallback)
	{
		m_completionCallback(result);
	}
}

void TransmissionCoordinator::OnLog(const std::string& message)
{
	// 转发到外部回调
	if (m_logCallback)
	{
		m_logCallback(message);
	}
}