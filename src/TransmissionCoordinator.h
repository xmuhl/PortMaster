#pragma once
#pragma execution_character_set("utf-8")

#include "TransmissionTask.h"
#include "../Protocol/ReliableChannel.h"
#include "../Transport/ITransport.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

/**
 * @brief 传输协调器
 *
 * 职责：管理TransmissionTask的创建、暂停、恢复、取消，以及进度/状态消息转发
 * 位置：src/ 目录（UI辅助服务）
 *
 * 功能说明：
 * - 根据传输模式（可靠/直接）创建对应的TransmissionTask
 * - 封装WM_USER+10~15自定义消息处理，对外暴露类型化回调
 * - 中央化处理传输任务的生命周期管理
 * - 提供统一的进度、完成、日志回调接口
 *
 * 线程安全性：
 * - 回调函数在传输任务线程中执行，需确保回调内部线程安全
 * - Start/Pause/Resume/Cancel 可在UI线程安全调用
 *
 * 使用示例：
 * @code
 * auto coordinator = std::make_unique<TransmissionCoordinator>();
 *
 * // 设置回调
 * coordinator->SetProgressCallback([this](const TransmissionProgress& progress) {
 *     PostMessage(WM_USER + 11, 0, progress.progressPercent);
 * });
 * coordinator->SetCompletionCallback([this](const TransmissionResult& result) {
 *     if (result.finalState == TransmissionTaskState::Completed) {
 *         MessageBox(L"传输成功");
 *     }
 * });
 *
 * // 启动传输
 * std::vector<uint8_t> data = {...};
 * if (!coordinator->Start(data, m_reliableChannel, m_transport)) {
 *     MessageBox(L"启动传输失败");
 * }
 *
 * // 暂停/恢复/取消
 * coordinator->Pause();
 * coordinator->Resume();
 * coordinator->Cancel();
 * @endcode
 */
class TransmissionCoordinator
{
public:
	TransmissionCoordinator();
	~TransmissionCoordinator();

	// 禁止拷贝和赋值
	TransmissionCoordinator(const TransmissionCoordinator&) = delete;
	TransmissionCoordinator& operator=(const TransmissionCoordinator&) = delete;

	// ========== 传输控制接口 ==========

	/**
	 * @brief 启动传输任务
	 * @param data 待传输的数据
	 * @param reliableChannel 可靠传输通道（如果使用可靠模式）
	 * @param transport 原始传输通道（如果使用直接模式）
	 * @return 启动是否成功
	 *
	 * 说明：
	 * - 根据reliableChannel和transport的可用性自动选择传输模式
	 * - 如果reliableChannel可用且已连接，使用可靠模式
	 * - 否则使用直接模式
	 * - 启动前会检查是否有正在运行的任务，如有则忽略
	 */
	bool Start(const std::vector<uint8_t>& data,
		std::shared_ptr<ReliableChannel> reliableChannel,
		std::shared_ptr<ITransport> transport);

	/**
	 * @brief 暂停当前传输任务
	 *
	 * 说明：
	 * - 只有正在运行的任务才能暂停
	 * - 暂停后可以通过Resume()恢复
	 */
	void Pause();

	/**
	 * @brief 恢复暂停的传输任务
	 *
	 * 说明：
	 * - 只有已暂停的任务才能恢复
	 */
	void Resume();

	/**
	 * @brief 取消当前传输任务
	 *
	 * 说明：
	 * - 取消后任务将停止，不可恢复
	 * - 【阶段四修复】改为异步取消，不在此处等待线程退出
	 */
	void Cancel();

	/**
	 * @brief 【阶段四修复】清理传输任务资源
	 *
	 * 说明：
	 * - 延迟reset，确保工作线程已完全退出
	 * - 应在OnTransmissionComplete后调用
	 */
	void CleanupTransmissionTask();

	// ========== 状态查询接口 ==========

	/**
	 * @brief 获取当前传输任务状态
	 * @return 任务状态枚举
	 */
	TransmissionTaskState GetState() const;

	/**
	 * @brief 查询是否有任务正在运行
	 * @return 是否正在运行
	 */
	bool IsRunning() const;

	/**
	 * @brief 查询任务是否已暂停
	 * @return 是否已暂停
	 */
	bool IsPaused() const;

	/**
	 * @brief 查询任务是否已完成
	 * @return 是否已完成
	 */
	bool IsCompleted() const;

	// ========== 回调接口 ==========

	/**
	 * @brief 进度回调类型
	 * @param progress 传输进度信息
	 */
	using ProgressCallback = std::function<void(const TransmissionProgress&)>;

	/**
	 * @brief 完成回调类型
	 * @param result 传输结果信息
	 */
	using CompletionCallback = std::function<void(const TransmissionResult&)>;

	/**
	 * @brief 日志回调类型
	 * @param message 日志消息
	 */
	using LogCallback = std::function<void(const std::string&)>;

	/**
	 * @brief 设置进度回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 回调在传输任务线程中执行
	 * - 建议在回调中使用PostMessage等线程安全方式更新UI
	 */
	void SetProgressCallback(ProgressCallback callback);

	/**
	 * @brief 设置完成回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 回调在传输任务线程中执行
	 * - 注意不要在回调中直接析构TransmissionCoordinator对象
	 */
	void SetCompletionCallback(CompletionCallback callback);

	/**
	 * @brief 设置日志回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 回调在传输任务线程中执行
	 */
	void SetLogCallback(LogCallback callback);

	// ========== 配置接口 ==========

	/**
	 * @brief 设置传输块大小
	 * @param chunkSize 块大小（字节）
	 */
	void SetChunkSize(size_t chunkSize);

	/**
	 * @brief 设置重试参数
	 * @param maxRetries 最大重试次数
	 * @param retryDelayMs 重试延迟（毫秒）
	 */
	void SetRetrySettings(int maxRetries, int retryDelayMs);

	/**
	 * @brief 设置进度更新间隔
	 * @param intervalMs 更新间隔（毫秒）
	 */
	void SetProgressUpdateInterval(int intervalMs);

private:
	// ========== 内部方法 ==========

	/**
	 * @brief 创建传输任务
	 * @param reliableChannel 可靠传输通道
	 * @param transport 原始传输通道
	 * @return 传输任务实例，失败返回nullptr
	 */
	std::unique_ptr<TransmissionTask> CreateTask(
		std::shared_ptr<ReliableChannel> reliableChannel,
		std::shared_ptr<ITransport> transport);

	/**
	 * @brief 内部进度回调处理
	 * @param progress 进度信息
	 */
	void OnProgress(const TransmissionProgress& progress);

	/**
	 * @brief 内部完成回调处理
	 * @param result 结果信息
	 */
	void OnCompletion(const TransmissionResult& result);

	/**
	 * @brief 内部日志回调处理
	 * @param message 日志消息
	 */
	void OnLog(const std::string& message);

private:
	// ========== 成员变量 ==========

	// 当前传输任务
	std::unique_ptr<TransmissionTask> m_currentTask;

	// 回调函数
	ProgressCallback m_progressCallback;
	CompletionCallback m_completionCallback;
	LogCallback m_logCallback;

	// 配置参数
	size_t m_chunkSize;
	int m_maxRetries;
	int m_retryDelayMs;
	int m_progressUpdateIntervalMs;
};
