#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <stdexcept>

// 环形缓冲区模板类
template<typename T>
class RingBuffer
{
public:
	// 构造函数
	explicit RingBuffer(size_t capacity = 1024)
		: m_capacity(capacity + 1) // 额外一个位置用于区分空和满
		, m_buffer(capacity + 1)
		, m_head(0)
		, m_tail(0)
		, m_size(0)
	{
		if (capacity == 0)
		{
			throw std::invalid_argument("Buffer capacity cannot be zero");
		}
	}

	// 析构函数
	~RingBuffer() = default;

	// 禁用拷贝构造和赋值
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

	// 移动构造和赋值
	RingBuffer(RingBuffer&& other) noexcept
		: m_capacity(other.m_capacity)
		, m_buffer(std::move(other.m_buffer))
		, m_head(other.m_head.load())
		, m_tail(other.m_tail.load())
		, m_size(other.m_size.load())
	{
		other.m_capacity = 0;
		other.m_head = 0;
		other.m_tail = 0;
		other.m_size = 0;
	}

	RingBuffer& operator=(RingBuffer&& other) noexcept
	{
		if (this != &other)
		{
			m_capacity = other.m_capacity;
			m_buffer = std::move(other.m_buffer);
			m_head = other.m_head.load();
			m_tail = other.m_tail.load();
			m_size = other.m_size.load();

			other.m_capacity = 0;
			other.m_head = 0;
			other.m_tail = 0;
			other.m_size = 0;
		}
		return *this;
	}

	// 写入数据
	bool Write(const T* data, size_t count)
	{
		if (data == nullptr || count == 0)
		{
			return true;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		// 检查是否有足够空间
		if (GetFreeSpaceLocked() < count)
		{
			return false;
		}

		// 写入数据
		WriteDataLocked(data, count);

		// 通知等待的读取线程
		m_notEmpty.notify_one();

		return true;
	}

	// 读取数据
	size_t Read(T* data, size_t maxCount)
	{
		if (data == nullptr || maxCount == 0)
		{
			return 0;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		size_t available = GetSizeLocked();
		size_t toRead = std::min(maxCount, available);

		if (toRead == 0)
		{
			return 0;
		}

		// 读取数据
		ReadDataLocked(data, toRead);

		// 通知等待的写入线程
		m_notFull.notify_one();

		return toRead;
	}

	// 等待写入数据（阻塞模式）
	bool WriteWait(const T* data, size_t count, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)())
	{
		if (data == nullptr || count == 0)
		{
			return true;
		}

		std::unique_lock<std::mutex> lock(m_mutex);

		// 等待有足够空间
		if (!m_notFull.wait_for(lock, timeout, [this, count] { return GetFreeSpaceLocked() >= count; }))
		{
			return false; // 超时
		}

		// 写入数据
		WriteDataLocked(data, count);

		// 通知等待的读取线程
		m_notEmpty.notify_one();

		return true;
	}

	// 等待读取数据（阻塞模式）
	size_t ReadWait(T* data, size_t maxCount, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)())
	{
		if (data == nullptr || maxCount == 0)
		{
			return 0;
		}

		std::unique_lock<std::mutex> lock(m_mutex);

		// 等待有数据可读
		if (!m_notEmpty.wait_for(lock, timeout, [this] { return GetSizeLocked() > 0; }))
		{
			return 0; // 超时
		}

		size_t available = GetSizeLocked();
		size_t toRead = std::min(maxCount, available);

		// 读取数据
		ReadDataLocked(data, toRead);

		// 通知等待的写入线程
		m_notFull.notify_one();

		return toRead;
	}

	// 查看数据但不移除（窥视）
	size_t Peek(T* data, size_t maxCount) const
	{
		if (data == nullptr || maxCount == 0)
		{
			return 0;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		size_t available = GetSizeLocked();
		size_t toPeek = std::min(maxCount, available);

		if (toPeek == 0)
		{
			return 0;
		}

		// 复制数据但不移除
		size_t head = m_head.load();
		for (size_t i = 0; i < toPeek; ++i)
		{
			data[i] = m_buffer[head];
			head = (head + 1) % m_capacity;
		}

		return toPeek;
	}

	// 清空缓冲区
	void Clear()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_head = 0;
		m_tail = 0;
		m_size = 0;
		m_notFull.notify_all();
	}

	// 获取当前大小
	size_t GetSize() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return GetSizeLocked();
	}

	// 获取容量
	size_t GetCapacity() const
	{
		return m_capacity - 1; // 减1因为额外一个位置用于区分空和满
	}

	// 获取可用空间
	size_t GetAvailableSpace() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return GetFreeSpaceLocked();
	}

	// 检查是否为空
	bool IsEmpty() const
	{
		return GetSize() == 0;
	}

	// 检查是否已满
	bool IsFull() const
	{
		return GetAvailableSpace() == 0;
	}

	// 调整容量（会清空现有数据）
	void Resize(size_t newCapacity)
	{
		if (newCapacity == 0)
		{
			throw std::invalid_argument("Buffer capacity cannot be zero");
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		m_buffer.resize(newCapacity + 1);
		m_capacity = newCapacity + 1;
		m_head = 0;
		m_tail = 0;
		m_size = 0;

		m_notFull.notify_all();
		m_notEmpty.notify_all();
	}

private:
	// 获取当前大小（内部使用，不加锁）
	size_t GetSizeLocked() const
	{
		size_t head = m_head.load();
		size_t tail = m_tail.load();

		if (tail >= head)
		{
			return tail - head;
		}
		else
		{
			return m_capacity - head + tail;
		}
	}

	// 获取可用空间（内部使用，不加锁）
	size_t GetFreeSpaceLocked() const
	{
		return m_capacity - 1 - GetSizeLocked();
	}

	// 写入数据（内部使用，不加锁）
	void WriteDataLocked(const T* data, size_t count)
	{
		size_t tail = m_tail.load();

		// 处理环形写入
		size_t firstPart = std::min(count, m_capacity - tail);
		std::memcpy(&m_buffer[tail], data, firstPart * sizeof(T));

		if (firstPart < count)
		{
			std::memcpy(&m_buffer[0], data + firstPart, (count - firstPart) * sizeof(T));
			m_tail = count - firstPart;
		}
		else
		{
			m_tail = (tail + count) % m_capacity;
		}

		m_size += count;
	}

	// 读取数据（内部使用，不加锁）
	void ReadDataLocked(T* data, size_t count)
	{
		size_t head = m_head.load();

		// 处理环形读取
		size_t firstPart = std::min(count, m_capacity - head);
		std::memcpy(data, &m_buffer[head], firstPart * sizeof(T));

		if (firstPart < count)
		{
			std::memcpy(data + firstPart, &m_buffer[0], (count - firstPart) * sizeof(T));
			m_head = count - firstPart;
		}
		else
		{
			m_head = (head + count) % m_capacity;
		}

		m_size -= count;
	}

private:
	size_t m_capacity;      // 缓冲区容量（实际大小+1）
	std::vector<T> m_buffer; // 缓冲区数据

	std::atomic<size_t> m_head; // 读指针
	std::atomic<size_t> m_tail; // 写指针
	std::atomic<size_t> m_size; // 当前大小

	mutable std::mutex m_mutex;           // 互斥锁
	std::condition_variable m_notEmpty;   // 非空条件变量
	std::condition_variable m_notFull;    // 非满条件变量
};

// 特化版本：字节缓冲区
using ByteRingBuffer = RingBuffer<uint8_t>;

// 特化版本：字符缓冲区
using CharRingBuffer = RingBuffer<char>;

// 线程安全的环形缓冲区工厂类
class RingBufferFactory
{
public:
	// 创建字节缓冲区
	static std::unique_ptr<ByteRingBuffer> CreateByteBuffer(size_t capacity = 4096)
	{
		return std::make_unique<ByteRingBuffer>(capacity);
	}

	// 创建字符缓冲区
	static std::unique_ptr<CharRingBuffer> CreateCharBuffer(size_t capacity = 4096)
	{
		return std::make_unique<CharRingBuffer>(capacity);
	}

	// 创建指定类型的缓冲区
	template<typename T>
	static std::unique_ptr<RingBuffer<T>> CreateBuffer(size_t capacity = 1024)
	{
		return std::make_unique<RingBuffer<T>>(capacity);
	}
};