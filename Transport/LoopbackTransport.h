#pragma once

#include "ITransport.h"
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

class LoopbackTransport : public ITransport
{
public:
    LoopbackTransport();
    virtual ~LoopbackTransport();

    // ITransport 鎺ュ彛瀹炵幇
    virtual bool Open(const TransportConfig& config) override;
    virtual void Close() override;
    virtual bool IsOpen() const override;
    virtual TransportState GetState() const override;

    virtual bool Configure(const TransportConfig& config) override;
    virtual TransportConfig GetConfiguration() const override;

    virtual size_t Write(const std::vector<uint8_t>& data) override;
    virtual size_t Write(const uint8_t* data, size_t length) override;
    virtual size_t Read(std::vector<uint8_t>& data, size_t maxLength = 0) override;
    virtual size_t Available() const override;

    virtual std::string GetLastError() const override;
    virtual std::string GetPortName() const override;
    virtual std::string GetTransportType() const override;

    virtual void SetDataReceivedCallback(DataReceivedCallback callback) override;
    virtual void SetStateChangedCallback(StateChangedCallback callback) override;

    virtual bool Flush() override;
    virtual bool ClearBuffers() override;

    // 杈呭姪鍑芥暟瀹炵幇
    virtual void NotifyDataReceived(const std::vector<uint8_t>& data) override;
    virtual void NotifyStateChanged(TransportState state, const std::string& message = "") override;
    virtual void SetLastError(const std::string& error) override;

    // 鍥炵幆鐗规湁鍔熻兘
    void SetDelay(int delayMs) { m_delayMs = delayMs; }
    void SetErrorRate(double errorRate) { m_errorRate = errorRate; }
    void SimulateError(const std::string& error);

private:
    // 延迟交付数据结构
    struct DelayedData {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point deliveryTime;
    };
    
    std::queue<DelayedData> m_delayedQueue;  // 延迟队列
    std::queue<std::vector<uint8_t>> m_dataQueue;  // 保留原队列用于兼容性
    mutable std::mutex m_mutex;
    std::thread m_loopbackThread;
    std::atomic<bool> m_stopThread;
    
    // 妯℃嫙鍙傛暟
    int m_delayMs = 10;          // 鐜洖寤惰繜
    double m_errorRate = 0.0;    // 閿欒鐜?(0.0 - 1.0)
    
    // 线程安全的随机数生成器 (SOLID-S: 单一职责 - 每个实例管理自己的随机状态)
    mutable std::mt19937 m_randomGen;
    mutable std::uniform_real_distribution<> m_randomDist;
    mutable std::mutex m_randomMutex;
    
    void LoopbackThreadFunc();
    bool ShouldSimulateError();
};