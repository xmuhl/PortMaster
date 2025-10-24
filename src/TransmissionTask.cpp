#include "pch.h"
#include "TransmissionTask.h"
#include <algorithm>
#include <future>

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

TransmissionTask::~TransmissionTask() {
    // 【修复】先保存日志回调，避免清空后无法记录析构日志
    LogCallback logCallback = m_logCallback;

    // 先清空回调，避免在析构过程中调用已销毁的外部对象
    m_progressCallback = nullptr;
    m_completionCallback = nullptr;
    m_logCallback = nullptr;

    // 使用保存的回调记录日志
    if (logCallback) {
        try {
            logCallback("TransmissionTask析构 - 开始清理资源");
        } catch (...) {
            // 静默处理，避免析构异常
        }
    }

    // 设置取消标志（无锁操作，避免死锁）
    m_state.store(TransmissionTaskState::Cancelled);

    // 通知所有等待的线程（使用try_lock避免死锁）
    try {
        std::unique_lock<std::mutex> lock(m_stateMutex, std::try_to_lock);
        if (lock.owns_lock()) {
            m_stateCV.notify_all();
            lock.unlock(); // 显式释放锁
        } else {
            WriteLog("TransmissionTask析构 - 无法获取状态锁，跳过通知");
        }
    } catch (...) {
        WriteLog("TransmissionTask析构 - 通知线程时发生异常");
    }

    // 【修复析构停滞】立即分离线程，避免等待超时
    if (m_workerThread && m_workerThread->joinable()) {
        WriteLog("TransmissionTask析构 - 立即分离工作线程，避免等待阻塞");

        try {
            // 直接分离线程，不等待其自然退出
            m_workerThread->detach();
            WriteLog("TransmissionTask析构 - 工作线程已分离");
        } catch (const std::exception& e) {
            WriteLog("TransmissionTask析构 - 分离线程时发生异常: " + std::string(e.what()));
        } catch (...) {
            WriteLog("TransmissionTask析构 - 分离线程时发生未知异常");
        }
    }

    // 最后安全地重置线程指针
    try {
        m_workerThread.reset();
    } catch (...) {
        WriteLog("TransmissionTask析构 - 重置线程指针时发生异常");
    }

    WriteLog("TransmissionTask析构 - 资源清理完成");
}

bool TransmissionTask::Start(const std::vector<uint8_t>& data)
{
	// 【彻底死锁修复】完全避免在持有锁时创建线程

	// 预检查 - 不持有锁
	if (!IsTransportReady())
	{
		WriteLog("TransmissionTask::Start - 传输通道未就绪: " + GetTransportDescription());
		return false;
	}

	if (data.empty())
	{
		WriteLog("TransmissionTask::Start - 数据为空，无法开始传输");
		return false;
	}

	// 预先保存数据到局部变量，避免在锁内进行大容量拷贝
	std::vector<uint8_t> localData = data;
	size_t totalSize = localData.size();

	// 最小化锁持有时间 - 只保护关键状态变更
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);

		if (m_state != TransmissionTaskState::Ready)
		{
			WriteLog("TransmissionTask::Start - 任务状态错误，当前状态: " + std::to_string(static_cast<int>(m_state.load())));
			return false;
		}

		// 快速状态设置
		m_data = std::move(localData);  // 使用移动语义减少拷贝
		m_totalBytes = totalSize;
		m_bytesTransmitted = 0;
		m_state = TransmissionTaskState::Running;
		m_startTime = std::chrono::steady_clock::now();
		m_lastProgressUpdate = m_startTime;

		// 锁立即释放
	}

	// 在完全无锁的情况下进行后续操作
	WriteLog("TransmissionTask::Start - 开始传输任务，数据大小: " + std::to_string(m_totalBytes) +
		" 字节，传输通道: " + GetTransportDescription());

	// 更新初始进度（在锁外）
	UpdateProgress(0, m_totalBytes, "传输开始");

	// 【关键修复】完全无锁的线程创建
	try {
		m_workerThread = std::make_unique<std::thread>(&TransmissionTask::ExecuteTransmission, this);
	}
	catch (const std::exception& e) {
		WriteLog("TransmissionTask::Start - 创建工作线程失败: " + std::string(e.what()));
		m_state.store(TransmissionTaskState::Failed);
		return false;
	}
	catch (...) {
		WriteLog("TransmissionTask::Start - 创建工作线程时发生未知异常");
		m_state.store(TransmissionTaskState::Failed);
		return false;
	}

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

		// 通知等待的线程
		m_stateCV.notify_all();
	}
}

void TransmissionTask::Cancel()
{
	bool wasRunningOrPaused = false;
	size_t transmitted = 0;

	{
		std::lock_guard<std::mutex> lock(m_stateMutex);

		if (m_state == TransmissionTaskState::Running || m_state == TransmissionTaskState::Paused)
		{
			m_state = TransmissionTaskState::Cancelled;
			wasRunningOrPaused = true;
			transmitted = m_bytesTransmitted.load();

			// 通知可能在等待的线程
			m_stateCV.notify_all();
		}
		// 锁在这里自动释放
	}

	// 在锁外调用WriteLog，避免死锁
	if (wasRunningOrPaused)
	{
		WriteLog("TransmissionTask::Cancel - 传输任务已取消");
		UpdateProgress(transmitted, m_totalBytes, "传输已取消");
	}
}

void TransmissionTask::Stop()
{
	// 【阶段四修复】改为异步停止：不在UI线程中同步join
	// 取消任务（这会设置Cancelled状态，让工作线程自行退出）
	Cancel();

	// 【阶段四修复】删除同步join代码，让工作线程自行安全退出
	// 工作线程会检查Cancelled状态并主动调用ReportCompletion
	// 线程清理延迟至完成回调或析构函数进行

	WriteLog("TransmissionTask::Stop - 已请求取消，工作线程将自行安全退出");

	// 【注意】不再在此处执行m_workerThread->join()或reset()
	// 这样做可以避免UI线程被阻塞，而工作线程会在ExecuteTransmission中
	// 检查Cancelled状态并自行结束，然后调用ReportCompletion通知UI
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

		// 【死锁修复】添加启动延迟，确保主线程完全释放锁
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		while (totalSent < dataSize)
		{
			// 【死锁修复】使用原子操作快速检查状态，避免锁争用
			TransmissionTaskState currentState = m_state.load();
			if (currentState == TransmissionTaskState::Cancelled) {
				WriteLog("TransmissionTask::ExecuteTransmission - 传输被取消");
				break;
			}

			// 只有在暂停状态时才使用复杂的锁机制
			if (currentState == TransmissionTaskState::Paused) {
				if (!CheckPauseAndCancel()) {
					WriteLog("TransmissionTask::ExecuteTransmission - 传输被取消或出错");
					break;
				}
				continue; // 暂停处理后重新开始循环
			}

			// 对于Running状态，直接继续传输，无需锁检查

			// 计算当前块大小
			size_t currentChunkSize = (m_chunkSize < (dataSize - totalSent)) ? m_chunkSize : (dataSize - totalSent);

			// 注释掉冗余的块发送日志，避免日志文件过度增长
			// WriteLog("TransmissionTask::ExecuteTransmission - 发送块 " +
			//	std::to_string(totalSent / m_chunkSize + 1) +
			//	"/" + std::to_string((dataSize + m_chunkSize - 1) / m_chunkSize) +
			//	"，大小: " + std::to_string(currentChunkSize));

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
	// 【死锁修复】使用try_lock避免WriteLog本身的死锁
	LogCallback callback;
	{
		std::unique_lock<std::mutex> lock(m_stateMutex, std::try_to_lock);
		if (lock.owns_lock()) {
			callback = m_logCallback;
		} else {
			// 如果无法获取锁，可能是正在析构，直接返回避免死锁
			return;
		}
		// lock会在这里自动释放
	}

	if (callback)
	{
		try {
			callback(message);
		} catch (...) {
			// 日志回调失败时静默处理，避免异常传播
		}
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
			// 暂停状态，使用条件变量等待恢复或取消
			// 使用更短的超时时间，提高响应性
			std::unique_lock<std::mutex> lock(m_stateMutex);

			// 再次检查状态，防止在获取锁期间状态已改变
			currentState = m_state.load();
			if (currentState != TransmissionTaskState::Paused) {
				continue; // 状态已改变，重新循环检查
			}

			// 等待状态变更，使用更短的超时时间（50ms）以提高响应性
			if (m_stateCV.wait_for(lock, std::chrono::milliseconds(50), [this]() {
				TransmissionTaskState state = m_state.load();
				return state != TransmissionTaskState::Paused;
			})) {
				// 状态已改变，释放锁后重新循环检查
				lock.unlock();
				continue;
			}

			// 超时后释放锁，重新循环检查状态
			lock.unlock();

			// 添加短暂延迟，避免CPU占用过高
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		// 其他状态认为是错误
		return false;
	}
}

// 【P1修复】可靠传输任务实现类

ReliableTransmissionTask::ReliableTransmissionTask(std::shared_ptr<ReliableChannel> reliableChannel)
	: m_reliableChannel(reliableChannel)
{
	// 为可靠传输设置较小的块大小以提供更精确的进度
	SetChunkSize(1024);
}

TransportError ReliableTransmissionTask::DoSendChunk(const uint8_t* data, size_t size)
{
	if (!m_reliableChannel || !m_reliableChannel->IsConnected())
	{
		return TransportError::NotOpen;
	}

	std::vector<uint8_t> chunk(data, data + size);
	bool success = m_reliableChannel->Send(chunk);

	// 【回退修复】Send()现在会阻塞等待，只有通道关闭时才返回false
	// 因此false应该视为失败而非重试
	return success ? TransportError::Success : TransportError::WriteFailed;
}

bool ReliableTransmissionTask::IsTransportReady() const
{
	return m_reliableChannel && m_reliableChannel->IsConnected();
}

std::string ReliableTransmissionTask::GetTransportDescription() const
{
	return "可靠传输通道";
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