#pragma execution_character_set("utf-8")

#include "pch.h"
#include "TransmissionCoordinator.h"
#include "../Common/ProgressReportingStrategy.h"

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
	std::shared_ptr<ITransport> transport,
	PortType portType,
	const std::string& portName)
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

	// 【新增】智能进度报告策略初始化
	SmartProgressManager& progressManager = SmartProgressManagerSingleton::GetInstance();

	// 检测工作模式并配置进度报告策略
	bool useReliableMode = (reliableChannel != nullptr);
	WorkModeDetection detection = progressManager.DetectWorkMode(portType, portName, useReliableMode);

	// 记录检测结果
	if (m_logCallback)
	{
		m_logCallback("=== 智能进度报告策略检测结果 ===");
		m_logCallback("端口类型: " + std::to_string(static_cast<int>(portType)));
		m_logCallback("端口名称: " + (portName.empty() ? "未指定" : portName));
		m_logCallback("可靠模式: " + std::string(useReliableMode ? "是" : "否"));
		m_logCallback("检测原因: " + detection.detectionReason);
		m_logCallback("选用策略: " + std::string(detection.strategy == ProgressReportingStrategy::SenderDriven ? "发送方驱动" : "接收方驱动"));
		m_logCallback("================================");
	}

	// 重置进度管理器状态
	progressManager.Reset();

	// 配置进度管理器回调
	progressManager.SetProgressDataCallback([this](int progressPercent) {
		// 转发到外部进度回调
		if (m_progressCallback)
		{
			TransmissionProgress progress;
			progress.progressPercent = progressPercent;
			progress.bytesTransmitted = 0; // 此处由具体策略填充
			progress.totalBytes = 0;
			progress.statusText = "智能进度报告";
			m_progressCallback(progress);
		}
	});

	progressManager.SetStatusTextCallback([this](const std::string& statusText) {
		// 转发到外部日志回调
		if (m_logCallback)
		{
			m_logCallback("状态更新: " + statusText);
		}
	});

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

	// 设置任务回调（集成智能进度管理）
	m_currentTask->SetProgressCallback([this](const TransmissionProgress& progress) {
		// 先通过智能进度管理器处理
		SmartProgressManager& progressManager = SmartProgressManagerSingleton::GetInstance();
		progressManager.HandleSenderProgress(progress);

		// 再转发到外部回调
		OnProgress(progress);
		});

	m_currentTask->SetCompletionCallback([this](const TransmissionResult& result) {
		// 通知智能进度管理器发送完成
		SmartProgressManager& progressManager = SmartProgressManagerSingleton::GetInstance();
		if (result.finalState == TransmissionTaskState::Completed)
		{
			progressManager.MarkSendComplete();
		}

		// 转发到外部回调
		OnCompletion(result);
		});

	m_currentTask->SetLogCallback([this](const std::string& message) {
		// 转发到外部回调
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
	// 【调试】CleanupTransmissionTask 开始
	if (m_logCallback) {
		m_logCallback("=== TransmissionCoordinator::CleanupTransmissionTask 开始 ===");
	}

	if (m_currentTask)
	{
		// 【调试】准备重置智能指针
		if (m_logCallback) {
			m_logCallback("CleanupTransmissionTask: 准备重置 m_currentTask 智能指针");
		}

		// 【修复】安全清理任务，直接重置智能指针
		// TransmissionTask析构函数已修复为不调用WriteLog，避免调试错误
		m_currentTask.reset();

		// 【调试】智能指针重置完成
		if (m_logCallback) {
			m_logCallback("CleanupTransmissionTask: m_currentTask 智能指针重置完成");
		}
	}
	else
	{
		// 【调试】当前任务为空
		if (m_logCallback) {
			m_logCallback("CleanupTransmissionTask: m_currentTask 为空，无需清理");
		}
	}

	// 【调试】CleanupTransmissionTask 结束
	if (m_logCallback) {
		m_logCallback("=== TransmissionCoordinator::CleanupTransmissionTask 结束 ===");
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

// ==================== 智能进度报告接口 ====================

SmartProgressManager& TransmissionCoordinator::GetProgressManager()
{
	return SmartProgressManagerSingleton::GetInstance();
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