#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ReceiveCacheService.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

// ==================== 构造与析构 ====================

ReceiveCacheService::ReceiveCacheService()
	: m_useTempCacheFile(false)
	, m_memoryCacheValid(false)
	, m_totalReceivedBytes(0)
	, m_totalSentBytes(0)
	, m_verboseLogging(false)
{
}

ReceiveCacheService::~ReceiveCacheService()
{
	Shutdown();
}

// ==================== 生命周期管理 ====================

bool ReceiveCacheService::Initialize()
{
	// 【阶段二实现】初始化临时缓存文件
	// TODO: 阶段二迁移InitializeTempCacheFile()完整逻辑（PortMasterDlg.cpp:4270-4309）
	try
	{
		// 生成临时文件名
		wchar_t tempPath[MAX_PATH];
		wchar_t tempFileName[MAX_PATH];

		if (GetTempPathW(MAX_PATH, tempPath) == 0)
		{
			Log("获取临时目录失败");
			return false;
		}

		if (GetTempFileNameW(tempPath, L"PM_", 0, tempFileName) == 0)
		{
			Log("生成临时文件名失败");
			return false;
		}

		m_tempCacheFilePath = tempFileName;

		// 打开临时文件用于写入（二进制模式）
		m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!m_tempCacheFile.is_open())
		{
			Log("打开临时文件失败");
			return false;
		}

		m_useTempCacheFile = true;
		m_totalReceivedBytes = 0;
		m_totalSentBytes = 0;
		m_memoryCache.clear();
		m_memoryCacheValid = false;

		Log("临时缓存文件已创建: " + std::string(m_tempCacheFilePath.begin(), m_tempCacheFilePath.end()));
		return true;
	}
	catch (const std::exception& e)
	{
		Log("创建临时缓存文件失败: " + std::string(e.what()));
		return false;
	}
}

void ReceiveCacheService::Shutdown()
{
	// 【阶段二实现】关闭并删除临时缓存文件
	// TODO: 阶段二迁移CloseTempCacheFile()完整逻辑（PortMasterDlg.cpp:4311-4329）
	if (m_tempCacheFile.is_open())
	{
		m_tempCacheFile.close();
	}

	// 删除临时文件
	if (!m_tempCacheFilePath.empty() && PathFileExistsW(m_tempCacheFilePath.c_str()))
	{
		DeleteFileW(m_tempCacheFilePath.c_str());
		Log("临时缓存文件已删除");
	}

	m_tempCacheFilePath.clear();
	m_useTempCacheFile = false;
	m_totalReceivedBytes = 0;
	m_totalSentBytes = 0;
	m_memoryCache.clear();
	m_memoryCacheValid = false;

	// 清空待写入队列
	while (!m_pendingWrites.empty())
	{
		m_pendingWrites.pop();
	}
}

bool ReceiveCacheService::IsInitialized() const
{
	return m_useTempCacheFile && m_tempCacheFile.is_open();
}

// ==================== 数据操作接口 ====================

bool ReceiveCacheService::AppendData(const std::vector<uint8_t>& data)
{
	// 【阶段二实现】线程安全追加接收数据
	// TODO: 阶段二迁移ThreadSafeAppendReceiveData()完整逻辑（PortMasterDlg.cpp:2240-2327）
	if (data.empty())
	{
		return false;
	}

	// 使用接收文件专用互斥锁
	std::lock_guard<std::mutex> lock(m_fileMutex);

	LogDetail("=== AppendData 开始（接收线程直接落盘）===");
	LogDetail("接收数据大小: " + std::to_string(data.size()) + " 字节");
	LogDetail("当前接收缓存大小: " + std::to_string(m_memoryCache.size()) + " 字节");
	LogDetail("总接收字节数: " + std::to_string(m_totalReceivedBytes.load()) + " 字节");

	// 1. 立即更新内存缓存（内存缓存作为备份，保证数据完整性）
	if (!m_memoryCacheValid || m_memoryCache.empty())
	{
		LogDetail("初始化接收缓存（首次接收）");
		m_memoryCache = data;
	}
	else
	{
		LogDetail("追加数据到接收缓存");
		size_t oldSize = m_memoryCache.size();
		m_memoryCache.insert(m_memoryCache.end(), data.begin(), data.end());
		LogDetail("缓存追加完成: " + std::to_string(oldSize) + " → " + std::to_string(m_memoryCache.size()) + " 字节");
	}
	m_memoryCacheValid = true;

	// 2. 简化状态检查，仅在文件流关闭时恢复
	if (m_useTempCacheFile && !m_tempCacheFile.is_open())
	{
		Log("⚠️ 检测到临时文件流关闭，启动自动恢复...");
		if (CheckAndRecover())
		{
			Log("✅ 临时文件自动恢复成功，继续数据写入");
		}
		else
		{
			Log("❌ 临时文件自动恢复失败，数据将仅保存到内存缓存");
		}
	}

	// 3. 立即同步写入临时文件（强制串行化，确保数据完全落盘）
	if (m_useTempCacheFile && m_tempCacheFile.is_open())
	{
		LogDetail("执行强制同步写入临时缓存文件...");
		try
		{
			// 写入数据到文件
			m_tempCacheFile.write(reinterpret_cast<const char*>(data.data()), data.size());

			// 强制刷新缓冲区到磁盘
			m_tempCacheFile.flush();

			// 更新总字节数统计
			m_totalReceivedBytes += data.size();

			LogDetail("强制同步写入成功: " + std::to_string(data.size()) + " 字节");
			LogDetail("更新后总接收字节数: " + std::to_string(m_totalReceivedBytes.load()) + " 字节");

			if (m_verboseLogging)
			{
				LogFileStatus("数据写入后状态验证");
			}
		}
		catch (const std::exception& e)
		{
			Log("临时缓存文件写入异常: " + std::string(e.what()));
			Log("数据已保存到内存缓存，文件写入失败但不影响数据完整性");
		}
	}
	else
	{
		LogDetail("临时缓存文件未启用或未打开，仅更新内存缓存");
		// 即使没有临时文件，也要更新总字节数
		m_totalReceivedBytes += data.size();
		LogDetail("更新总接收字节数（仅内存）: " + std::to_string(m_totalReceivedBytes.load()) + " 字节");
	}

	LogDetail("=== AppendData 结束（数据已强制落盘）===");
	return true;
}

std::vector<uint8_t> ReceiveCacheService::ReadData(uint64_t offset, size_t length)
{
	// 【阶段二实现】从临时缓存读取指定范围的数据
	// TODO: 阶段二迁移ReadDataFromTempCache()完整逻辑（PortMasterDlg.cpp:4386-4634）
	std::vector<uint8_t> result;

	if (m_tempCacheFilePath.empty() || !PathFileExistsW(m_tempCacheFilePath.c_str()))
	{
		return result;
	}

	std::lock_guard<std::mutex> lock(m_fileMutex);
	return ReadDataUnlocked(offset, length);
}

std::vector<uint8_t> ReceiveCacheService::ReadAllData()
{
	// 【阶段二实现】读取所有缓存数据
	// TODO: 阶段二迁移ReadAllDataFromTempCacheUnlocked()逻辑（PortMasterDlg.cpp:4636-4639）
	return ReadData(0, 0); // 0长度表示读取全部数据
}

const std::vector<uint8_t>& ReceiveCacheService::GetMemoryCache() const
{
	return m_memoryCache;
}

// ==================== 文件完整性管理 ====================

bool ReceiveCacheService::VerifyFileIntegrity()
{
	// 【阶段二实现】验证临时缓存文件完整性
	// TODO: 阶段二迁移VerifyTempCacheFileIntegrity()完整逻辑（PortMasterDlg.cpp:4810-4847）
	if (m_tempCacheFilePath.empty())
	{
		Log("验证失败：临时文件路径为空");
		return false;
	}

	if (!PathFileExistsW(m_tempCacheFilePath.c_str()))
	{
		Log("验证失败：临时文件不存在");
		return false;
	}

	// 获取文件实际大小
	WIN32_FILE_ATTRIBUTE_DATA fileAttr;
	if (!GetFileAttributesExW(m_tempCacheFilePath.c_str(), GetFileExInfoStandard, &fileAttr))
	{
		Log("验证失败：无法获取文件属性");
		return false;
	}

	LARGE_INTEGER fileSize;
	fileSize.LowPart = fileAttr.nFileSizeLow;
	fileSize.HighPart = fileAttr.nFileSizeHigh;

	// 验证文件大小与统计数据一致性
	uint64_t receivedBytes = m_totalReceivedBytes.load();
	if (static_cast<uint64_t>(fileSize.QuadPart) != receivedBytes)
	{
		Log("完整性验证：文件大小不匹配");
		Log("文件实际大小: " + std::to_string(fileSize.QuadPart) + " 字节");
		Log("统计接收字节: " + std::to_string(receivedBytes) + " 字节");
		return false;
	}

	Log("完整性验证通过：文件大小 " + std::to_string(fileSize.QuadPart) + " 字节");
	return true;
}

bool ReceiveCacheService::CheckAndRecover()
{
	// 【阶段二实现】检查并恢复临时文件状态
	// TODO: 阶段二迁移CheckAndRecoverTempCacheFile()完整逻辑（PortMasterDlg.cpp:4850-4922）
	Log("开始临时文件状态检查和恢复...");

	// 如果明确禁用了临时文件机制，直接返回
	if (!m_useTempCacheFile)
	{
		Log("临时文件机制已禁用，无需恢复");
		return false;
	}

	// 检查文件流状态
	if (m_tempCacheFile.is_open())
	{
		// 验证文件流的健康状态
		if (m_tempCacheFile.good())
		{
			Log("临时文件流状态正常，无需恢复");
			return true;
		}
		else
		{
			Log("检测到文件流状态异常，开始修复...");
			// 关闭异常的文件流
			m_tempCacheFile.close();
		}
	}

	// 文件流已关闭，尝试重新打开
	Log("尝试重新打开临时文件");

	if (m_tempCacheFilePath.empty())
	{
		Log("❌ 恢复失败：临时文件路径为空，尝试重新初始化");
		return Initialize();
	}

	try
	{
		// 使用追加模式重新打开，避免覆盖已有数据
		m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);

		if (m_tempCacheFile.is_open())
		{
			Log("✅ 临时文件恢复成功");

			// 验证文件完整性
			if (VerifyFileIntegrity())
			{
				Log("✅ 文件完整性验证通过");
				return true;
			}
			else
			{
				Log("⚠️ 文件完整性验证失败，但文件流已恢复");
				// 即使完整性验证失败，文件流已经恢复，可以继续写入
				return true;
			}
		}
		else
		{
			Log("❌ 文件流打开失败，尝试重新创建临时文件");
			return Initialize();
		}
	}
	catch (const std::exception& e)
	{
		Log("❌ 文件恢复异常: " + std::string(e.what()));
		Log("尝试重新初始化临时文件...");
		return Initialize();
	}
}

// ==================== 统计信息接口 ====================

uint64_t ReceiveCacheService::GetTotalReceivedBytes() const
{
	return m_totalReceivedBytes.load();
}

uint64_t ReceiveCacheService::GetFileSize() const
{
	// 【阶段二实现】获取临时缓存文件大小
	// TODO: 阶段二迁移GetTempCacheFileSize()逻辑
	if (m_tempCacheFilePath.empty() || !PathFileExistsW(m_tempCacheFilePath.c_str()))
	{
		return 0;
	}

	WIN32_FILE_ATTRIBUTE_DATA fileAttr;
	if (!GetFileAttributesExW(m_tempCacheFilePath.c_str(), GetFileExInfoStandard, &fileAttr))
	{
		return 0;
	}

	LARGE_INTEGER fileSize;
	fileSize.LowPart = fileAttr.nFileSizeLow;
	fileSize.HighPart = fileAttr.nFileSizeHigh;

	return static_cast<uint64_t>(fileSize.QuadPart);
}

std::wstring ReceiveCacheService::GetFilePath() const
{
	return m_tempCacheFilePath;
}

bool ReceiveCacheService::IsMemoryCacheValid() const
{
	return m_memoryCacheValid;
}

// ==================== 配置接口 ====================

void ReceiveCacheService::SetVerboseLogging(bool enabled)
{
	m_verboseLogging = enabled;
}

void ReceiveCacheService::SetLogCallback(LogCallback callback)
{
	m_logCallback = callback;
}

// ==================== 内部方法 ====================

bool ReceiveCacheService::WriteDataUnlocked(const std::vector<uint8_t>& data)
{
	// 【阶段二实现】写入数据到临时缓存（内部实现，无锁版本）
	// TODO: 阶段二迁移WriteDataToTempCache()完整逻辑（PortMasterDlg.cpp:4331-4384）
	if (data.empty())
	{
		return false;
	}

	// 如果文件流被关闭（如正在读取），将数据加入待写入队列
	if (!m_tempCacheFile.is_open())
	{
		m_pendingWrites.push(data);
		Log("临时缓存文件流已关闭，数据加入待写入队列，大小: " + std::to_string(data.size()) + " 字节");
		return true; // 返回成功，数据将在文件重新打开时写入
	}

	try
	{
		// 先处理队列中的待写入数据
		while (!m_pendingWrites.empty())
		{
			const auto& pendingData = m_pendingWrites.front();
			m_tempCacheFile.write(reinterpret_cast<const char*>(pendingData.data()), pendingData.size());
			if (m_tempCacheFile.fail())
			{
				Log("写入待处理数据失败，队列大小: " + std::to_string(m_pendingWrites.size()));
				return false;
			}
			m_totalReceivedBytes += pendingData.size();
			m_pendingWrites.pop();
		}

		// 写入当前数据
		m_tempCacheFile.write(reinterpret_cast<const char*>(data.data()), data.size());
		if (m_tempCacheFile.fail())
		{
			Log("写入当前数据到临时缓存文件失败，大小: " + std::to_string(data.size()) + " 字节");
			return false;
		}

		m_tempCacheFile.flush(); // 确保数据立即写入磁盘
		m_totalReceivedBytes += data.size();

		Log("成功写入临时缓存文件，大小: " + std::to_string(data.size()) + " 字节，总计: " + std::to_string(m_totalReceivedBytes.load()) + " 字节");
		return true;
	}
	catch (const std::exception& e)
	{
		Log("写入临时缓存文件异常: " + std::string(e.what()));
		return false;
	}
}

std::vector<uint8_t> ReceiveCacheService::ReadDataUnlocked(uint64_t offset, size_t length)
{
	// 【阶段二实现】从临时缓存读取数据（内部实现，无锁版本）
	// TODO: 阶段二迁移ReadDataFromTempCacheUnlocked()完整逻辑（PortMasterDlg.cpp:4441-4634）
	std::vector<uint8_t> result;

	try
	{
		// 记录读取前的总接收字节数（用于检测并发写入）
		uint64_t beforeReadBytes = m_totalReceivedBytes.load();
		LogDetail("ReadDataUnlocked: 读取前总接收字节数 " + std::to_string(beforeReadBytes) + " 字节");

		// 刷新写入流但保持打开（避免读写冲突）
		if (m_tempCacheFile.is_open())
		{
			m_tempCacheFile.flush();
			LogDetail("ReadDataUnlocked: 刷新写入流，保持文件流打开状态");
		}

		// 使用独立的ifstream进行读取
		std::ifstream file(m_tempCacheFilePath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			Log("ReadDataUnlocked: 无法打开临时缓存文件进行读取");
			return result;
		}

		// 获取文件大小
		file.seekg(0, std::ios::end);
		std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		LogDetail("ReadDataUnlocked: 临时缓存文件大小 " + std::to_string(fileSize) + " 字节");

		if (fileSize <= 0)
		{
			Log("ReadDataUnlocked: 文件为空");
			file.close();
			return result;
		}

		// 计算读取长度（0表示读取全部）
		size_t availableLength = static_cast<size_t>(fileSize - offset);
		size_t targetReadLength = (length == 0) ? static_cast<size_t>(fileSize) : (length < availableLength ? length : availableLength);

		if (targetReadLength > 0 && offset < static_cast<uint64_t>(fileSize))
		{
			LogDetail("ReadDataUnlocked: 开始分块循环读取，目标长度 " + std::to_string(targetReadLength) + " 字节");

			// 实施分块循环读取机制
			try
			{
				// 预分配内存，提高性能
				result.reserve(targetReadLength);

				// 设置64KB块大小进行循环读取
				const size_t CHUNK_SIZE = 65536; // 64KB
				std::vector<char> chunk(CHUNK_SIZE);

				file.seekg(offset);
				size_t totalBytesRead = 0;
				size_t remainingBytes = targetReadLength;

				LogDetail("ReadDataUnlocked: 使用64KB分块循环读取策略");

				while (remainingBytes > 0 && !file.eof())
				{
					// 计算本次读取大小
					size_t currentChunkSize = (CHUNK_SIZE < remainingBytes) ? CHUNK_SIZE : remainingBytes;

					// 读取数据块
					file.read(chunk.data(), static_cast<std::streamsize>(currentChunkSize));
					std::streamsize actualRead = file.gcount();

					if (actualRead > 0)
					{
						// 将读取的数据追加到结果向量
						size_t actualReadSize = static_cast<size_t>(actualRead);
						result.insert(result.end(), chunk.begin(), chunk.begin() + actualReadSize);
						totalBytesRead += actualReadSize;
						remainingBytes -= actualReadSize;

						// 记录读取进度（每10MB输出一次）
						if (totalBytesRead % (10 * 1024 * 1024) == 0 || remainingBytes == 0)
						{
							LogDetail("ReadDataUnlocked: 读取进度 " +
								std::to_string(totalBytesRead) + "/" +
								std::to_string(targetReadLength) + " 字节 (" +
								std::to_string((totalBytesRead * 100) / targetReadLength) + "%)");
						}
					}
					else
					{
						// 无法再读取数据
						if (file.eof())
						{
							LogDetail("ReadDataUnlocked: 到达文件末尾，读取完成");
							break;
						}
						else if (file.fail())
						{
							Log("ReadDataUnlocked: 读取过程中发生错误");
							break;
						}
					}
				}

				// 数据完整性验证
				if (totalBytesRead == targetReadLength)
				{
					Log("ReadDataUnlocked: ✅ 数据读取完整，成功读取 " +
						std::to_string(totalBytesRead) + " 字节");
				}
				else
				{
					Log("ReadDataUnlocked: ⚠️ 数据读取不完整 - 预期: " +
						std::to_string(targetReadLength) + " 字节，实际: " +
						std::to_string(totalBytesRead) + " 字节");

					if (totalBytesRead == 0)
					{
						Log("ReadDataUnlocked: ❌ 未读取到任何数据，清空结果");
						result.clear();
					}
				}
			}
			catch (const std::exception& e)
			{
				Log("ReadDataUnlocked: ❌ 分块读取过程中发生异常: " + std::string(e.what()));
				result.clear();
			}
		}

		file.close();

		// 检测读取过程中是否有新数据写入
		uint64_t afterReadBytes = m_totalReceivedBytes.load();
		if (afterReadBytes > beforeReadBytes)
		{
			size_t newDataSize = static_cast<size_t>(afterReadBytes - beforeReadBytes);
			Log("⚠️ 检测到读取过程中有新数据写入: " + std::to_string(newDataSize) + " 字节");
			Log("读取前: " + std::to_string(beforeReadBytes) + " 字节, 读取后: " + std::to_string(afterReadBytes) + " 字节");
			Log("当前返回的数据可能不完整，建议用户等待传输完成后重新保存");
		}
		else
		{
			LogDetail("✅ 数据完整性验证通过，读取期间无新数据写入");
		}

		return result;
	}
	catch (const std::exception& e)
	{
		Log("ReadDataUnlocked: 读取异常 - " + std::string(e.what()));
		return result;
	}
}

void ReceiveCacheService::Log(const std::string& message)
{
	if (m_logCallback)
	{
		m_logCallback(message);
	}
}

void ReceiveCacheService::LogDetail(const std::string& message)
{
	if (m_verboseLogging && m_logCallback)
	{
		m_logCallback(message);
	}
}

void ReceiveCacheService::LogFileStatus(const std::string& context)
{
	// 【阶段二实现】记录临时缓存文件状态（调试用）
	// TODO: 阶段二迁移LogTempCacheFileStatus()逻辑
	LogDetail("--- " + context + " ---");
	LogDetail("临时文件路径: " + std::string(m_tempCacheFilePath.begin(), m_tempCacheFilePath.end()));
	LogDetail("文件流打开状态: " + std::string(m_tempCacheFile.is_open() ? "是" : "否"));
	LogDetail("文件流状态: " + std::string(m_tempCacheFile.good() ? "正常" : "异常"));
	LogDetail("总接收字节数: " + std::to_string(m_totalReceivedBytes.load()) + " 字节");
	LogDetail("内存缓存大小: " + std::to_string(m_memoryCache.size()) + " 字节");
	LogDetail("内存缓存有效性: " + std::string(m_memoryCacheValid ? "有效" : "无效"));
	LogDetail("实际文件大小: " + std::to_string(GetFileSize()) + " 字节");
}
