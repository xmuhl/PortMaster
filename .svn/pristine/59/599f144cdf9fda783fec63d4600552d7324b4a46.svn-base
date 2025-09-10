#include "pch.h"
#include "LoopbackTransport.h"
#include <random>

LoopbackTransport::LoopbackTransport()
    : m_stopThread(false),
      m_randomGen(std::random_device{}()),  // 使用随机种子初始化
      m_randomDist(0.0, 1.0)               // 初始化分布为0-1区间
{
    m_state = TRANSPORT_CLOSED;
    // 设置默认延迟为5ms，提供更快的直接传输响应
    m_delayMs = 5;
    // 确保错误率为0，避免数据丢失
    m_errorRate = 0.0;
}

LoopbackTransport::~LoopbackTransport()
{
    Close();
}

bool LoopbackTransport::Open(const TransportConfig& config)
{
    m_config = config;
    m_stopThread = false;
    
    // 启动回环处理线程
    if (!m_loopbackThread.joinable())
    {
        m_loopbackThread = std::thread(&LoopbackTransport::LoopbackThreadFunc, this);
    }
    
    NotifyStateChanged(TRANSPORT_OPEN, "回环已打开");
    return true;
}

void LoopbackTransport::Close()
{
    m_stopThread = true;
    
    // 等待线程结束
    if (m_loopbackThread.joinable())
    {
        m_loopbackThread.join();
    }
    
    // 清空队列
    ClearBuffers();
    
    NotifyStateChanged(TRANSPORT_CLOSED, "回环已关闭");
}

bool LoopbackTransport::IsOpen() const
{
    return m_state == TRANSPORT_OPEN;
}

TransportState LoopbackTransport::GetState() const
{
    return m_state;
}

bool LoopbackTransport::Configure(const TransportConfig& config)
{
    m_config = config;
    return true;
}

TransportConfig LoopbackTransport::GetConfiguration() const
{
    return m_config;
}

size_t LoopbackTransport::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

size_t LoopbackTransport::Write(const uint8_t* data, size_t length)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (length == 0) {
        return 0;
    }
    
    // 创建延迟交付数据
    DelayedData delayedData;
    delayedData.data = std::vector<uint8_t>(data, data + length);
    delayedData.deliveryTime = std::chrono::steady_clock::now() + 
                               std::chrono::milliseconds(m_delayMs);
    
    // 加入延迟队列而不是立即交付
    m_delayedQueue.push(delayedData);
    
    // 记录写入操作（调试用）
    // printf("[DEBUG] LoopbackTransport::Write: 写入数据 %zu 字节，延迟 %dms\n", length, m_delayMs);
    
    return length;
}

size_t LoopbackTransport::Read(std::vector<uint8_t>& data, size_t maxLength)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dataQueue.empty())
    {
        data.clear();
        return 0;
    }
    
    // 模拟错误，如果应该产生错误则返回0
    if (ShouldSimulateError())
    {
        data.clear();
        return 0;
    }
    
    data = m_dataQueue.front();
    m_dataQueue.pop();
    return data.size();
}

size_t LoopbackTransport::Available() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dataQueue.size();
}

std::string LoopbackTransport::GetLastError() const
{
    return m_lastError;
}

std::string LoopbackTransport::GetPortName() const
{
    return "Loopback";
}

std::string LoopbackTransport::GetTransportType() const
{
    return "Loopback";
}

void LoopbackTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataCallback = callback;
}

void LoopbackTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateCallback = callback;
}

bool LoopbackTransport::Flush()
{
    return true;
}

bool LoopbackTransport::ClearBuffers()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_dataQueue.empty())
        m_dataQueue.pop();
    while (!m_delayedQueue.empty())
        m_delayedQueue.pop();
    return true;
}

void LoopbackTransport::NotifyDataReceived(const std::vector<uint8_t>& data)
{
    if (m_dataCallback)
    {
        // SOLID-D: 依赖抽象接口，添加异常边界保护 (修复回调异常保护缺失)
        try {
            m_dataCallback(data);
        }
        catch (const std::exception& e) {
            SetLastError("回调异常: " + std::string(e.what()));
        }
        catch (...) {
            SetLastError("回调发生未知异常");
        }
    }
}

void LoopbackTransport::NotifyStateChanged(TransportState state, const std::string& message)
{
    m_state = state;
    if (m_stateCallback)
        m_stateCallback(state, message);
}

void LoopbackTransport::SetLastError(const std::string& error)
{
    m_lastError = error;
}

void LoopbackTransport::SimulateError(const std::string& error)
{
    SetLastError(error);
    NotifyStateChanged(TRANSPORT_ERROR, error);
}

void LoopbackTransport::LoopbackThreadFunc()
{
    while (!m_stopThread)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        std::unique_lock<std::mutex> lock(m_mutex);
        
        // 检查延迟队列中是否有到达交付时间的数据
        auto now = std::chrono::steady_clock::now();
        std::vector<std::vector<uint8_t>> readyData;
        
        while (!m_delayedQueue.empty())
        {
            const auto& delayedData = m_delayedQueue.front();
            if (delayedData.deliveryTime <= now)
            {
                // 延迟时间到达，收集准备交付的数据
                readyData.push_back(delayedData.data);
                m_delayedQueue.pop();
            }
            else
            {
                break;  // 队列是按时间排序的，后续数据肯定还没到时间
            }
        }
        
        // 释放锁后交付数据，避免回调中的死锁
        lock.unlock();
        
        // 依次交付准备好的数据（优化：错误率为0时跳过检查）
        for (const auto& data : readyData)
        {
            if (m_errorRate <= 0.0 || !ShouldSimulateError())
            {
                if (m_dataCallback)
                {
                    // 在回调前记录日志以便调试
                    // printf("[DEBUG] LoopbackTransport: 准备交付数据，大小: %zu\n", data.size());
                    m_dataCallback(data);
                }
                else
                {
                    SetLastError("数据回调未设置");
                }
            }
            else
            {
                // 记录模拟丢包情况（调试用）
                SetLastError("模拟数据包丢失");
            }
        }
    }
}

bool LoopbackTransport::ShouldSimulateError()
{
    if (m_errorRate <= 0.0)
        return false;
    
    // 使用线程安全的实例级随机数生成器 (SOLID-S: 解决静态变量线程安全问题)
    std::lock_guard<std::mutex> lock(m_randomMutex);
    return m_randomDist(m_randomGen) < m_errorRate;
}