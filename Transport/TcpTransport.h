#pragma once

#include "ITransport.h"
#include "../Common/IOWorker.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

class TcpTransport : public ITransport
{
public:
    TcpTransport();
    virtual ~TcpTransport();

    // ITransport 接口实现
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

protected:
    // SOLID-L: 里氏替换原则 - 实现ITransport抽象方法
    virtual void NotifyDataReceived(const std::vector<uint8_t>& data) override;
    virtual void NotifyStateChanged(TransportState state, const std::string& message = "") override;
    virtual void SetLastError(const std::string& error) override;

public:
    // TCP 特有功能
    bool IsServerMode() const { return m_config.isServer; }
    std::string GetRemoteEndpoint() const;
    std::string GetLocalEndpoint() const;

private:
    SOCKET m_socket;
    SOCKET m_listenSocket; // 浠呮湇鍔＄妯″紡浣跨敤
    std::string m_remoteEndpoint;
    
    // 璇诲彇绾跨▼
    std::thread m_acceptThread; // 浠呮湇鍔＄妯″紡浣跨敤
    std::atomic<bool> m_stopThreads;
    std::atomic<bool> m_stopAccept;
    mutable std::mutex m_mutex;
    std::vector<uint8_t> m_readBuffer;
    
    // 异步I/O支持
    std::shared_ptr<IOWorker> m_ioWorker;
    std::atomic<bool> m_continuousReading;
    
    // 注意：基类状态变量已在ITransport中定义，这里不需要重复声明

    // 鍐呴儴鏂规硶
    bool InitializeWinsock();
    void CleanupWinsock();
    bool CreateSocket();
    bool ConnectClient();
    bool StartServer();
    void StartContinuousReading();
    void StopContinuousReading();
    void OnReadCompleted(const IOResult& result);
    void OnWriteCompleted(const IOResult& result);
    void AcceptThreadFunc();
    void ReadThreadFunc(); // 待迁移的传统线程函数
    std::string GetSocketErrorString(int error) const;
    
    // 继承自ITransport的基类方法实现在.cpp中
    
    static bool s_wsaInitialized;
    static int s_wsaRefCount;
};