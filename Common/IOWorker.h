#pragma once

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

// I/O 鎿嶄綔绫诲瀷
enum class IOOperation
{
    Read,
    Write
};

// I/O 鎿嶄綔缁撴灉
struct IOResult
{
    IOOperation operation;
    bool success;
    DWORD bytesTransferred;
    DWORD errorCode;
    std::vector<uint8_t> data;
    
    IOResult() : operation(IOOperation::Read), success(false), bytesTransferred(0), errorCode(0) {}
};

// I/O 瀹屾垚鍥炶皟
using IOCompletionCallback = std::function<void(const IOResult&)>;

// OVERLAPPED I/O 宸ヤ綔鍣?
class IOWorker
{
public:
    IOWorker();
    ~IOWorker();
    
    // 鐢熷懡鍛ㄦ湡绠＄悊
    bool Start();
    void Stop();
    bool IsRunning() const;
    
    // 寮傛璇诲啓鎿嶄綔
    bool AsyncRead(HANDLE handle, std::vector<uint8_t>& buffer, 
                   IOCompletionCallback callback = nullptr);
    bool AsyncWrite(HANDLE handle, const std::vector<uint8_t>& data,
                    IOCompletionCallback callback = nullptr);
    
    // 璁剧疆鍏ㄥ眬鍥炶皟
    void SetGlobalCallback(IOCompletionCallback callback);
    
    // 閰嶇疆
    void SetThreadCount(int count);
    void SetTimeout(DWORD timeoutMs);
    
    int GetThreadCount() const { return m_threadCount; }
    DWORD GetTimeout() const { return m_timeoutMs; }

private:
    struct IOContext
    {
        OVERLAPPED overlapped;
        IOOperation operation;
        HANDLE handle;
        std::vector<uint8_t> buffer;
        IOCompletionCallback callback;
        IOWorker* worker;
        
        IOContext() : operation(IOOperation::Read), handle(INVALID_HANDLE_VALUE), worker(nullptr)
        {
            ZeroMemory(&overlapped, sizeof(overlapped));
        }
    };
    
    std::atomic<bool> m_running;
    HANDLE m_completionPort;
    std::vector<std::thread> m_workerThreads;
    int m_threadCount;
    DWORD m_timeoutMs;
    
    mutable std::mutex m_contextMutex;
    std::queue<std::unique_ptr<IOContext>> m_contextPool;
    
    IOCompletionCallback m_globalCallback;
    
    // 宸ヤ綔绾跨▼鍑芥暟
    // 工作线程函数
    void WorkerThreadFunc();
    
    // 上下文管理
    std::unique_ptr<IOContext> GetContext();
    void ReturnContext(std::unique_ptr<IOContext> context);
    
    // IO完成处理
    void HandleIOCompletion(IOContext* context, DWORD bytesTransferred, DWORD errorCode);
    
    // 套接字操作辅助方法
    bool IsSocketHandle(HANDLE handle) const;
    bool AsyncSocketRead(IOContext* context);
    bool AsyncSocketWrite(IOContext* context, const std::vector<uint8_t>& data);
};