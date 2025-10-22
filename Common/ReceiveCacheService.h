#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>
#include <fstream>
#include <queue>
#include <mutex>
#include <functional>
#include <cstdint>
#include <atomic>

/**
 * @brief 接收缓存服务
 *
 * 职责：线程安全的临时缓存文件操作，数据追加（接收线程）、数据读取（显示/保存）、文件完整性校验与恢复、统计追踪
 * 位置：Common/ 目录
 *
 * 功能说明：
 * - 管理临时缓存文件的创建、写入、读取、关闭
 * - 线程安全的数据追加操作（支持多线程并发写入）
 * - 内存缓存+磁盘文件双重保障（确保数据不丢失）
 * - 文件完整性校验与自动恢复机制
 * - 统计信息跟踪（总接收字节数、文件大小等）
 * - 支持分块循环读取大文件（避免内存溢出）
 *
 * 线程安全性：
 * - 所有数据追加和读取操作使用互斥锁保护
 * - 写入流与读取流分离，避免读写冲突
 * - 待写入队列机制处理读取期间的新数据
 *
 * 使用示例：
 * @code
 * // 初始化服务
 * ReceiveCacheService cacheService;
 * cacheService.SetLogCallback([](const std::string& msg) {
 *     std::cout << msg << std::endl;
 * });
 * cacheService.SetVerboseLogging(true);
 *
 * // 初始化临时文件
 * if (!cacheService.Initialize()) {
 *     std::cerr << "初始化失败" << std::endl;
 *     return;
 * }
 *
 * // 接收线程：追加数据
 * std::vector<uint8_t> receivedData = {...};
 * cacheService.AppendData(receivedData);
 *
 * // 主线程：读取数据用于显示或保存
 * std::vector<uint8_t> cachedData = cacheService.ReadAllData();
 *
 * // 获取统计信息
 * uint64_t totalBytes = cacheService.GetTotalReceivedBytes();
 * uint64_t fileSize = cacheService.GetFileSize();
 *
 * // 关闭服务
 * cacheService.Shutdown();
 * @endcode
 */
class ReceiveCacheService
{
public:
	ReceiveCacheService();
	~ReceiveCacheService();

	// 禁止拷贝和赋值
	ReceiveCacheService(const ReceiveCacheService&) = delete;
	ReceiveCacheService& operator=(const ReceiveCacheService&) = delete;

	// ========== 生命周期管理 ==========

	/**
	 * @brief 初始化临时缓存文件
	 * @return 初始化是否成功
	 *
	 * 说明：
	 * - 在系统临时目录创建临时文件（前缀"PM_"）
	 * - 打开文件流用于二进制写入（trunc模式）
	 * - 重置统计计数器（总接收字节数、总发送字节数）
	 * - 清空内存缓存
	 */
	bool Initialize();

	/**
	 * @brief 关闭并删除临时缓存文件
	 *
	 * 说明：
	 * - 关闭文件流
	 * - 删除临时文件（如果存在）
	 * - 重置所有状态和统计数据
	 */
	void Shutdown();

	/**
	 * @brief 检查临时文件是否已初始化
	 * @return 是否已初始化
	 */
	bool IsInitialized() const;

	// ========== 数据操作接口 ==========

	/**
	 * @brief 线程安全追加接收数据
	 * @param data 接收到的数据
	 * @return 追加是否成功
	 *
	 * 说明：
	 * - 使用接收文件专用互斥锁，确保与保存操作完全互斥
	 * - 立即更新内存缓存（作为备份，保证数据完整性）
	 * - 如果临时文件流关闭，自动尝试恢复
	 * - 强制同步写入临时文件（flush确保数据真正落盘）
	 * - 更新总接收字节数统计
	 */
	bool AppendData(const std::vector<uint8_t>& data);

	/**
	 * @brief 从临时缓存读取指定范围的数据
	 * @param offset 起始偏移量（字节）
	 * @param length 读取长度（字节），0表示读取全部
	 * @return 读取的数据，失败返回空向量
	 *
	 * 说明：
	 * - 使用接收文件专用互斥锁，与写入操作完全互斥
	 * - 刷新写入流但保持打开（避免读写冲突）
	 * - 使用独立的ifstream进行读取（不影响写入流）
	 * - 采用分块循环读取机制（64KB块大小）
	 * - 数据完整性验证（检测读取期间是否有新数据写入）
	 */
	std::vector<uint8_t> ReadData(uint64_t offset, size_t length);

	/**
	 * @brief 读取所有缓存数据
	 * @return 所有缓存数据，失败返回空向量
	 */
	std::vector<uint8_t> ReadAllData();

	/**
	 * @brief 流式复制缓存数据到目标文件
	 * @param targetPath 目标文件路径（宽字符）
	 * @param bytesWritten 输出参数：成功写入的字节数
	 * @return 复制是否成功
	 *
	 * 说明：
	 * - 使用流式I/O，按64KB块大小循环读取和写入，避免大文件内存溢出
	 * - 复用m_fileMutex，确保与AppendData操作互斥，保证数据一致性
	 * - 提供详细的进度日志（开始/进度/完成/异常）
	 * - 若临时文件不存在或未初始化，返回false并记录日志
	 * - 成功后通过bytesWritten参数返回实际写入的字节数
	 */
	bool CopyToFile(const std::wstring& targetPath, size_t& bytesWritten);

	/**
	 * @brief 获取内存缓存数据（已删除）
	 * 【第七轮修复】内存缓存已完全删除，仅保留文件缓存
	 */
	// const std::vector<uint8_t>& GetMemoryCache() const;  // 已删除

	// ========== 文件完整性管理 ==========

	/**
	 * @brief 验证临时缓存文件完整性
	 * @return 验证是否通过
	 *
	 * 说明：
	 * - 检查临时文件是否存在
	 * - 获取文件实际大小
	 * - 与统计的总接收字节数比对
	 * - 不匹配则返回false并记录详细日志
	 */
	bool VerifyFileIntegrity();

	/**
	 * @brief 检查并恢复临时文件状态
	 * @return 恢复是否成功
	 *
	 * 说明：
	 * - 检查文件流状态（is_open、good）
	 * - 如果文件流关闭或异常，尝试重新打开（append模式）
	 * - 如果文件路径为空，重新初始化临时文件
	 * - 恢复成功后执行完整性验证
	 */
	bool CheckAndRecover();

	// ========== 统计信息接口 ==========

	/**
	 * @brief 获取总接收字节数
	 * @return 总接收字节数
	 */
	uint64_t GetTotalReceivedBytes() const;

	/**
	 * @brief 获取临时缓存文件大小
	 * @return 文件大小（字节），文件不存在返回0
	 *
	 * 说明：
	 * - 使用GetFileAttributesEx获取实际文件大小
	 * - 不依赖内部统计计数器（可用于验证数据一致性）
	 */
	uint64_t GetFileSize() const;

	/**
	 * @brief 获取临时缓存文件路径
	 * @return 文件路径（宽字符串），未初始化返回空字符串
	 */
	std::wstring GetFilePath() const;

	/**
	 * @brief 检查内存缓存是否有效（已删除）
	 * 【第七轮修复】内存缓存已完全删除，此接口不再有效
	 */
	// bool IsMemoryCacheValid() const;  // 已删除

	// ========== 配置接口 ==========

	/**
	 * @brief 设置详细日志开关
	 * @param enabled 是否启用详细日志
	 *
	 * 说明：
	 * - 详细日志包括数据追加细节、文件状态、读写进度等
	 * - 适用于调试和问题诊断
	 */
	void SetVerboseLogging(bool enabled);

	/**
	 * @brief 设置日志回调函数
	 * @param callback 日志回调函数
	 *
	 * 说明：
	 * - 回调函数在发生日志事件时被调用
	 * - 回调函数在调用线程中执行（可能是UI线程或接收线程）
	 * - 回调函数应尽快返回，避免阻塞操作
	 */
	using LogCallback = std::function<void(const std::string&)>;
	void SetLogCallback(LogCallback callback);

private:
	// ========== 内部方法 ==========

	/**
	 * @brief 写入数据到临时缓存（内部实现，无锁版本）
	 * @param data 待写入数据
	 * @return 写入是否成功
	 *
	 * 说明：
	 * - 调用前必须已持有m_fileMutex锁
	 * - 处理待写入队列中的数据（读取期间积累的数据）
	 * - 写入当前数据并flush确保落盘
	 * - 更新总接收字节数统计
	 */
	bool WriteDataUnlocked(const std::vector<uint8_t>& data);

	/**
	 * @brief 从临时缓存读取数据（内部实现，无锁版本）
	 * @param offset 起始偏移量
	 * @param length 读取长度
	 * @return 读取的数据
	 *
	 * 说明：
	 * - 调用前必须已持有m_fileMutex锁
	 * - 实施分块循环读取机制（64KB块大小）
	 * - 记录读取前后的总接收字节数，检测并发写入
	 * - 详细的进度日志（每10MB输出一次进度）
	 */
	std::vector<uint8_t> ReadDataUnlocked(uint64_t offset, size_t length);

	/**
	 * @brief 记录日志消息
	 * @param message 日志消息
	 */
	void Log(const std::string& message);

	/**
	 * @brief 记录详细日志消息（仅当详细日志开启时）
	 * @param message 日志消息
	 */
	void LogDetail(const std::string& message);

	/**
	 * @brief 记录临时缓存文件状态（调试用）
	 * @param context 上下文描述
	 */
	void LogFileStatus(const std::string& context);

private:
	// ========== 成员变量 ==========

	// 文件流和路径
	std::ofstream m_tempCacheFile;              // 临时缓存文件输出流（写入）
	std::wstring m_tempCacheFilePath;           // 临时缓存文件路径（宽字符）
	bool m_useTempCacheFile;                    // 是否启用临时文件机制

	// 【第七轮修复】删除内存缓存 - 仅使用文件缓存，避免大数据内存溢出
	// std::vector<uint8_t> m_memoryCache;      // 已删除 - 不再维护内存镜像
	// bool m_memoryCacheValid;                 // 已删除 - 不再需要有效性标志

	// 统计信息
	std::atomic<uint64_t> m_totalReceivedBytes; // 总接收字节数（原子变量）
	std::atomic<uint64_t> m_totalSentBytes;     // 总发送字节数（原子变量）

	// 线程同步
	mutable std::mutex m_fileMutex;             // 文件操作互斥锁
	std::queue<std::vector<uint8_t>> m_pendingWrites; // 待写入队列（读取期间的新数据）

	// 日志配置
	LogCallback m_logCallback;                  // 日志回调函数
	bool m_verboseLogging;                      // 详细日志开关
};
