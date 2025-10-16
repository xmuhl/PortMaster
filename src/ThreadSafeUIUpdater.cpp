// ThreadSafeUIUpdater.cpp - 线程安全UI更新器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ThreadSafeUIUpdater.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// 全局UI更新器实例
ThreadSafeUIUpdater* g_threadSafeUIUpdater = nullptr;

ThreadSafeUIUpdater::ThreadSafeUIUpdater()
	: m_running(false)
	, m_processedCount(0)
	, m_queuedCount(0)
	, m_droppedCount(0)
{
}

ThreadSafeUIUpdater::~ThreadSafeUIUpdater()
{
	Stop();

	// 清理全局指针
	if (g_threadSafeUIUpdater == this) {
		g_threadSafeUIUpdater = nullptr;
	}
}

bool ThreadSafeUIUpdater::Start()
{
	if (m_running.load()) {
		return true; // 已经在运行
	}

	m_running.store(true);
	m_workerThread = std::thread(&ThreadSafeUIUpdater::WorkerThread, this);

	return true;
}

void ThreadSafeUIUpdater::Stop()
{
	if (!m_running.load()) {
		return; // 已经停止
	}

	m_running.store(false);
	m_queueCondition.notify_all();

	if (m_workerThread.joinable()) {
		m_workerThread.join();
	}
}

void ThreadSafeUIUpdater::RegisterControl(int controlId, void* control)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_controlMap[controlId] = control;
}

void ThreadSafeUIUpdater::UnregisterControl(int controlId)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_controlMap.erase(controlId);
}

bool ThreadSafeUIUpdater::QueueUpdate(UIUpdateType type, int controlId, const std::string& text, const std::string& reason)
{
	if (!m_running.load()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 检查队列大小限制（简单实现，默认1000个操作）
	if (m_updateQueue.size() >= 1000) {
		m_droppedCount.fetch_add(1);
		return false;
	}

	UIUpdateOperation operation(type, controlId, text, 0, reason);
	m_updateQueue.push(operation);
	m_queuedCount.fetch_add(1);

	m_queueCondition.notify_one();
	return true;
}

bool ThreadSafeUIUpdater::QueueUpdate(UIUpdateType type, int controlId, int numericValue, const std::string& reason)
{
	if (!m_running.load()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 检查队列大小限制
	if (m_updateQueue.size() >= 1000) {
		m_droppedCount.fetch_add(1);
		return false;
	}

	UIUpdateOperation operation(type, controlId, "", numericValue, reason);
	m_updateQueue.push(operation);
	m_queuedCount.fetch_add(1);

	m_queueCondition.notify_one();
	return true;
}

bool ThreadSafeUIUpdater::QueueUpdate(std::function<void()> customFunction, const std::string& reason)
{
	if (!m_running.load() || !customFunction) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 检查队列大小限制
	if (m_updateQueue.size() >= 1000) {
		m_droppedCount.fetch_add(1);
		return false;
	}

	UIUpdateOperation operation(customFunction, reason);
	m_updateQueue.push(operation);
	m_queuedCount.fetch_add(1);

	m_queueCondition.notify_one();
	return true;
}

bool ThreadSafeUIUpdater::QueueStatusUpdate(int controlId, const std::string& status, const std::string& reason)
{
	return QueueUpdate(UIUpdateType::UpdateStatusText, controlId, status, reason);
}

bool ThreadSafeUIUpdater::QueueProgressUpdate(int controlId, int progress, const std::string& reason)
{
	return QueueUpdate(UIUpdateType::UpdateProgressBar, controlId, progress, reason);
}

bool ThreadSafeUIUpdater::QueueButtonTextUpdate(int controlId, const std::string& text, const std::string& reason)
{
	return QueueUpdate(UIUpdateType::UpdateButtonState, controlId, text, reason);
}

bool ThreadSafeUIUpdater::QueueEditTextUpdate(int controlId, const std::string& text, const std::string& reason)
{
	return QueueUpdate(UIUpdateType::UpdateEditText, controlId, text, reason);
}

bool ThreadSafeUIUpdater::QueueBatchUpdates(const std::vector<UIUpdateOperation>& operations)
{
	if (!m_running.load()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 批量添加，检查队列大小限制
	for (const auto& operation : operations) {
		if (m_updateQueue.size() >= 1000) {
			m_droppedCount.fetch_add(1);
			break;
		}
		m_updateQueue.push(operation);
		m_queuedCount.fetch_add(1);
	}

	m_queueCondition.notify_one();
	return true;
}

bool ThreadSafeUIUpdater::QueuePriorityUpdate(UIUpdateType type, int controlId, const std::string& text, const std::string& reason)
{
	if (!m_running.load()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 检查队列大小限制
	if (m_updateQueue.size() >= 1000) {
		m_droppedCount.fetch_add(1);
		return false;
	}

	// 创建临时队列用于插队操作
	std::queue<UIUpdateOperation> tempQueue;

	// 将当前队列中的所有操作移到临时队列
	while (!m_updateQueue.empty()) {
		tempQueue.push(m_updateQueue.front());
		m_updateQueue.pop();
	}

	// 将优先级操作添加到队列前端
	UIUpdateOperation priorityOperation(type, controlId, text, 0, reason);
	m_updateQueue.push(priorityOperation);
	m_queuedCount.fetch_add(1);

	// 将原来的操作重新添加到队列
	while (!tempQueue.empty()) {
		m_updateQueue.push(tempQueue.front());
		tempQueue.pop();
	}

	m_queueCondition.notify_one();
	return true;
}

void ThreadSafeUIUpdater::ClearQueue()
{
	std::lock_guard<std::mutex> lock(m_queueMutex);

	// 清空队列并记录丢弃的数量
	size_t droppedSize = m_updateQueue.size();
	while (!m_updateQueue.empty()) {
		m_updateQueue.pop();
	}

	m_droppedCount.fetch_add(droppedSize);
}

size_t ThreadSafeUIUpdater::GetQueueSize() const
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	return m_updateQueue.size();
}

bool ThreadSafeUIUpdater::IsRunning() const
{
	return m_running.load();
}

uint64_t ThreadSafeUIUpdater::GetProcessedCount() const
{
	return m_processedCount.load();
}

uint64_t ThreadSafeUIUpdater::GetQueuedCount() const
{
	return m_queuedCount.load();
}

uint64_t ThreadSafeUIUpdater::GetDroppedCount() const
{
	return m_droppedCount.load();
}

bool ThreadSafeUIUpdater::WaitForCompletion(int timeoutMs)
{
	auto startTime = std::chrono::steady_clock::now();
	auto timeoutDuration = std::chrono::milliseconds(timeoutMs);

	while (GetQueueSize() > 0) {
		auto currentTime = std::chrono::steady_clock::now();
		if (currentTime - startTime >= timeoutDuration) {
			return false; // 超时
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return true;
}

void ThreadSafeUIUpdater::DumpStatistics() const
{
	std::cout << "=== UI更新器统计信息 ===" << std::endl;
	std::cout << "运行状态: " << (m_running.load() ? "运行中" : "已停止") << std::endl;
	std::cout << "当前队列大小: " << GetQueueSize() << std::endl;
	std::cout << "已处理数量: " << m_processedCount.load() << std::endl;
	std::cout << "已排队数量: " << m_queuedCount.load() << std::endl;
	std::cout << "已丢弃数量: " << m_droppedCount.load() << std::endl;
	std::cout << "注册控件数量: " << m_controlMap.size() << std::endl;
	std::cout << "========================" << std::endl;
}

void ThreadSafeUIUpdater::SetMaxQueueSize(size_t maxSize)
{
	// 这里可以实现最大队列大小的设置
	// 当前是固定为1000，可以根据需要进行修改
	// 为了简单起见，这里只是记录日志
	std::cout << "设置最大队列大小为: " << maxSize << " (当前固定为1000)" << std::endl;
}

size_t ThreadSafeUIUpdater::GetMaxQueueSize() const
{
	return 1000; // 当前固定返回1000
}

// 私有方法实现

void ThreadSafeUIUpdater::WorkerThread()
{
	while (m_running.load()) {
		std::unique_lock<std::mutex> lock(m_queueMutex);

		// 等待队列中有数据或停止信号
		m_queueCondition.wait(lock, [this] {
			return !m_updateQueue.empty() || !m_running.load();
			});

		if (!m_running.load()) {
			break;
		}

		// 处理队列中的操作
		while (!m_updateQueue.empty() && m_running.load()) {
			UIUpdateOperation operation = m_updateQueue.front();
			m_updateQueue.pop();
			lock.unlock();

			// 处理更新操作
			ProcessUpdateOperation(operation);
			m_processedCount.fetch_add(1);

			lock.lock();
		}
	}
}

void ThreadSafeUIUpdater::ProcessUpdateOperation(const UIUpdateOperation& operation)
{
	// 这里确保UI更新在主线程中执行
	if (!EnsureUIThread()) {
		return;
	}

	try {
		switch (operation.type) {
		case UIUpdateType::UpdateStatusText:
			// 更新状态文本 - 这里需要根据具体的UI框架实现
			// 例如：SetDlgItemText(operation.controlId, operation.text.c_str());
			break;

		case UIUpdateType::UpdateProgressBar:
			// 更新进度条 - 这里需要根据具体的UI框架实现
			// 例如：SendDlgItemMessage(operation.controlId, PBM_SETPOS, operation.numericValue, 0);
			break;

		case UIUpdateType::UpdateButtonState:
			// 更新按钮状态/文本 - 这里需要根据具体的UI框架实现
			// 例如：SetDlgItemText(operation.controlId, operation.text.c_str());
			break;

		case UIUpdateType::UpdateEditText:
			// 更新编辑框文本 - 这里需要根据具体的UI框架实现
			// 例如：SetDlgItemText(operation.controlId, operation.text.c_str());
			break;

		case UIUpdateType::UpdateListView:
			// 更新列表视图 - 这里需要根据具体的UI框架实现
			break;

		case UIUpdateType::CustomUpdate:
			// 执行自定义更新函数
			if (operation.customFunction) {
				operation.customFunction();
			}
			break;

		default:
			break;
		}

		// 记录更新日志（可选）
		if (!operation.reason.empty()) {
			// std::cout << "UI更新: " << operation.reason << std::endl;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "UI更新异常: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "UI更新未知异常" << std::endl;
	}
}

bool ThreadSafeUIUpdater::EnsureUIThread()
{
	// 这里需要检查当前是否在UI线程中
	// 对于MFC应用程序，可以通过AfxGetApp()->GetMainWnd()->GetSafeHwnd()等检查
	// 或者使用GetCurrentThreadId()与主线程ID比较

	// 简化实现：假设总是在UI线程中
	// 在实际应用中，这里应该检查线程ID，如果不是UI线程，则应该通过
	// PostMessage等方式将更新操作发送到UI线程执行

	return true; // 简化实现，总是返回true
}