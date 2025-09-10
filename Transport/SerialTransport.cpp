#pragma execution_character_set("utf-8")
#include "pch.h"
#include "SerialTransport.h"
#include <setupapi.h>
#include <devguid.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

SerialTransport::SerialTransport()
    : m_continuousReading(false)
{
    m_state = TRANSPORT_CLOSED;
    m_readBuffer.resize(4096); // 默认读取缓冲区大小
    
    // 创建共享的IOWorker实例
    m_ioWorker = std::make_shared<IOWorker>();
    m_readBuffer.reserve(4096);  // 预分配读缓冲区
}

SerialTransport::~SerialTransport()
{
    Close();
}

bool SerialTransport::Open(const TransportConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != TRANSPORT_CLOSED)
    {
        SetLastError("串口已打开或正在操作中");
        return false;
    }
    
    m_config = config;
    
    // 根据配置确定串口名称
    m_portName = !config.ipAddress.empty() ? config.ipAddress : "COM1";
    
    NotifyStateChanged(TRANSPORT_OPENING, "正在打开串口 " + m_portName);
    
    // 打开串口设备
    HANDLE hComm = CreateFileA(
        m_portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                           // 不共享
        NULL,                        // 默认安全属性
        OPEN_EXISTING,               // 必须已存在
        FILE_FLAG_OVERLAPPED,        // 异步I/O
        NULL                         // 无模板文件
    );
    
    if (hComm == INVALID_HANDLE_VALUE)
    {
        DWORD error = ::GetLastError();
        SetLastError("无法打开串口 " + m_portName + "，错误代码：" + std::to_string(error));
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    m_hComm.Reset(hComm);
    
    // 配置串口参数
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    // 获取当前配置
    if (!GetCommState(m_hComm.Get(), &dcb))
    {
        SetLastError("获取串口状态失败");
        m_hComm.Reset();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 设置串口参数
    dcb.BaudRate = config.baudRate;
    dcb.ByteSize = config.dataBits;
    dcb.Parity = config.parity;      // 0=NOPARITY, 1=ODDPARITY, 2=EVENPARITY
    dcb.StopBits = (config.stopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;
    
    // 流控制设置
    dcb.fBinary = TRUE;
    dcb.fParity = (config.parity != 0);
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    
    if (!SetCommState(m_hComm.Get(), &dcb))
    {
        SetLastError("设置串口参数失败");
        m_hComm.Reset();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 设置超时
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = config.readTimeoutMs;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = config.writeTimeoutMs;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    
    if (!SetCommTimeouts(m_hComm.Get(), &timeouts))
    {
        SetLastError("设置串口超时失败");
        m_hComm.Reset();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 创建事件对象用于异步I/O
    if (!m_readEvent.CreateManualResetEvent(FALSE) || !m_writeEvent.CreateManualResetEvent(FALSE))
    {
        SetLastError("创建I/O事件失败");
        Close();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    m_readOverlapped.hEvent = m_readEvent.Get();
    m_writeOverlapped.hEvent = m_writeEvent.Get();
    
    // 清空缓冲区
    PurgeComm(m_hComm.Get(), PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    // 启动IOWorker
    if (!m_ioWorker->Start())
    {
        SetLastError("启动IOWorker失败");
        m_hComm.Reset();
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 开始连续异步读取
    StartContinuousReading();
    
    NotifyStateChanged(TRANSPORT_OPEN, "串口 " + m_portName + " 打开成功");
    return true;
}

void SerialTransport::Close()
{
    if (m_state == TRANSPORT_CLOSED)
        return;
    
    NotifyStateChanged(TRANSPORT_CLOSING, "正在关闭串口");
    
    // 停止连续异步读取
    StopContinuousReading();
    
    // 停止并清理IOWorker
    if (m_ioWorker)
    {
        m_ioWorker->Stop();
    }
    
    // 重置事件对象（RAIIEvent会自动释放）
    m_readEvent.Reset();
    m_writeEvent.Reset();
    m_readOverlapped.hEvent = NULL;
    m_writeOverlapped.hEvent = NULL;
    
    // 重置串口句柄（RAIIHandle会自动释放）
    m_hComm.Reset();
    
    NotifyStateChanged(TRANSPORT_CLOSED, "串口已关闭");
}

bool SerialTransport::IsOpen() const
{
    return m_hComm.IsValid();
}

TransportState SerialTransport::GetState() const
{
    return m_state;
}

bool SerialTransport::Configure(const TransportConfig& config)
{
    m_config = config;
    return true;
}

TransportConfig SerialTransport::GetConfiguration() const
{
    return m_config;
}

size_t SerialTransport::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

size_t SerialTransport::Write(const uint8_t* data, size_t length)
{
    if (!IsOpen() || !data || length == 0)
    {
        return 0;
    }
    
    // 使用IOWorker进行异步写入，但同步等待结果
    std::vector<uint8_t> writeData(data, data + length);
    
    // 用于同步等待的事件和结果
    RAIIEvent writeCompleteEvent;
    if (!writeCompleteEvent.CreateManualResetEvent(FALSE))
    {
        SetLastError("创建写入完成事件失败");
        return 0;
    }
    
    size_t bytesWritten = 0;
    bool writeSuccess = false;
    std::string writeError;
    
    // 异步写入回调
    auto writeCallback = [&](const IOResult& result) {
        writeSuccess = result.success;
        bytesWritten = result.bytesTransferred;
        if (!result.success)
        {
            writeError = "异步写入失败：" + std::to_string(result.errorCode);
        }
        SetEvent(writeCompleteEvent.Get());
    };
    
    // 发起异步写入
    if (!m_ioWorker->AsyncWrite(m_hComm.Get(), writeData, writeCallback))
    {
        SetLastError("发起异步写入失败");
        return 0;
    }
    
    // 等待写入完成
    DWORD waitResult = WaitForSingleObject(writeCompleteEvent.Get(), 
                                         m_config.writeTimeoutMs > 0 ? m_config.writeTimeoutMs : 5000);
    
    if (waitResult == WAIT_OBJECT_0)
    {
        if (writeSuccess)
        {
            return bytesWritten;
        }
        else
        {
            SetLastError(writeError);
            return 0;
        }
    }
    else if (waitResult == WAIT_TIMEOUT)
    {
        SetLastError("写入操作超时");
        return 0;
    }
    else
    {
        SetLastError("等待写入完成失败");
        return 0;
    }
}

size_t SerialTransport::Read(std::vector<uint8_t>& data, size_t maxLength)
{
    if (!IsOpen())
    {
        data.clear();
        return 0;
    }
    
    // 对于串口，我们主要依赖ReadThreadProc进行连续读取
    // 这个方法可以用于同步读取剩余数据
    data.clear();
    
    if (maxLength == 0)
        maxLength = 4096;
    
    std::vector<uint8_t> buffer(maxLength);
    DWORD bytesRead = 0;
    OVERLAPPED overlapped = {0};
    RAIIEvent readEvent;
    
    if (!readEvent.CreateManualResetEvent(FALSE))
    {
        return 0;
    }
    
    overlapped.hEvent = readEvent.Get();
    
    BOOL result = ReadFile(m_hComm.Get(), buffer.data(), static_cast<DWORD>(maxLength), &bytesRead, &overlapped);
    
    if (!result)
    {
        DWORD error = ::GetLastError();
        if (error == ERROR_IO_PENDING)
        {
            // 等待读取完成，使用较短的超时
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
            if (waitResult == WAIT_OBJECT_0)
            {
                GetOverlappedResult(m_hComm.Get(), &overlapped, &bytesRead, FALSE);
            }
            else
            {
                CancelIo(m_hComm.Get());
                bytesRead = 0;
            }
        }
        else
        {
            bytesRead = 0;
        }
    }
    
    if (bytesRead > 0)
    {
        data.assign(buffer.begin(), buffer.begin() + bytesRead);
    }
    
    return static_cast<size_t>(bytesRead);
}

size_t SerialTransport::Available() const
{
    if (!m_hComm.IsValid())
        return 0;
    
    // 获取串口状态，准确报告硬件缓冲区中可读字节数
    COMSTAT comStat;
    DWORD errors;
    
    if (!ClearCommError(m_hComm.Get(), &errors, &comStat))
        return 0;
    
    // 返回接收缓冲区中的实际字节数 + 内部缓冲区字节数
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<size_t>(comStat.cbInQue) + m_readBuffer.size();
}

std::string SerialTransport::GetLastError() const
{
    return m_lastError;
}

std::string SerialTransport::GetPortName() const
{
    return m_portName;
}

std::string SerialTransport::GetTransportType() const
{
    return "Serial";
}

void SerialTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataCallback = callback;
}

void SerialTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateCallback = callback;
}

void SerialTransport::NotifyDataReceived(const std::vector<uint8_t>& data)
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

void SerialTransport::NotifyStateChanged(TransportState state, const std::string& message)
{
    m_state = state;
    if (m_stateCallback)
        m_stateCallback(state, message);
}

void SerialTransport::SetLastError(const std::string& error)
{
    m_lastError = error;
}

bool SerialTransport::Flush()
{
    if (!m_hComm.IsValid())
    {
        SetLastError("串口未打开");
        return false;
    }
    
    // 刷新发送缓冲区，确保数据发送完成
    if (!FlushFileBuffers(m_hComm.Get()))
    {
        SetLastError("刷新失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    return true;
}

bool SerialTransport::ClearBuffers()
{
    if (!m_hComm.IsValid())
    {
        SetLastError("串口未打开");
        return false;
    }
    
    // 清空发送和接收缓冲区
    if (!PurgeComm(m_hComm.Get(), PURGE_TXCLEAR | PURGE_RXCLEAR))
    {
        SetLastError("清空缓冲区失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    // 清空内部读取缓冲区
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_readBuffer.clear();
    }
    
    return true;
}

std::vector<std::string> SerialTransport::EnumeratePorts()
{
    std::vector<std::string> ports;
    
    // 方法1：使用QueryDosDevice枚举所有串口设备
    char deviceNames[65536];
    DWORD result = QueryDosDeviceA(NULL, deviceNames, sizeof(deviceNames));
    if (result != 0)
    {
        char* device = deviceNames;
        while (*device)
        {
            // 检查是否是COM端口
            if (strncmp(device, "COM", 3) == 0 && strlen(device) <= 8)
            {
                // 验证端口是否可访问
                std::string devicePath = "\\\\.\\" + std::string(device);
                HANDLE hPort = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                    0, NULL, OPEN_EXISTING, 0, NULL);
                if (hPort != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(hPort);
                    ports.push_back(device);
                }
            }
            device += strlen(device) + 1;
        }
    }
    
    // 方法2：如果上述方法失败，使用传统方法测试COM1-COM256
    if (ports.empty())
    {
        for (int i = 1; i <= 256; i++)
        {
            std::string portName = "COM" + std::to_string(i);
            std::string devicePath = "\\\\.\\" + portName;
            
            HANDLE hPort = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, NULL, OPEN_EXISTING, 0, NULL);
            if (hPort != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hPort);
                ports.push_back(portName);
            }
        }
    }
    
    // 按端口号排序
    std::sort(ports.begin(), ports.end(), [](const std::string& a, const std::string& b) {
        int numA = atoi(a.c_str() + 3);  // 跳过"COM"
        int numB = atoi(b.c_str() + 3);
        return numA < numB;
    });
    
    return ports;
}

void SerialTransport::SetPortName(const std::string& portName)
{
    if (m_state == TRANSPORT_OPEN)
    {
        SetLastError("无法在连接打开时更改端口名称");
        return;
    }
    m_portName = portName;
}

bool SerialTransport::SetDTR(bool enable)
{
    if (!m_hComm.IsValid())
    {
        SetLastError("串口未打开");
        return false;
    }
    
    DWORD function = enable ? SETDTR : CLRDTR;
    if (!EscapeCommFunction(m_hComm.Get(), function))
    {
        SetLastError("DTR控制失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    return true;
}

bool SerialTransport::SetRTS(bool enable)
{
    if (!m_hComm.IsValid())
    {
        SetLastError("串口未打开");
        return false;
    }
    
    DWORD function = enable ? SETRTS : CLRRTS;
    if (!EscapeCommFunction(m_hComm.Get(), function))
    {
        SetLastError("RTS控制失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    return true;
}

bool SerialTransport::GetCTS() const
{
    if (!m_hComm.IsValid())
        return false;
    
    DWORD status;
    if (!GetCommModemStatus(m_hComm.Get(), &status))
        return false;
    
    return (status & MS_CTS_ON) != 0;
}

bool SerialTransport::GetDSR() const
{
    if (!m_hComm.IsValid())
        return false;
    
    DWORD status;
    if (!GetCommModemStatus(m_hComm.Get(), &status))
        return false;
    
    return (status & MS_DSR_ON) != 0;
}

bool SerialTransport::ConfigurePort()
{
    if (!m_hComm.IsValid())
        return false;
    
    // 获取当前串口配置
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(m_hComm.Get(), &dcb))
    {
        SetLastError("获取串口配置失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    // 设置串口参数
    dcb.BaudRate = m_config.baudRate;
    dcb.ByteSize = m_config.dataBits;
    dcb.Parity = m_config.parity;
    dcb.StopBits = (m_config.stopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;
    
    // 设置流控制
    dcb.fBinary = TRUE;
    dcb.fParity = (m_config.parity != 0);
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    
    // 应用配置
    if (!SetCommState(m_hComm.Get(), &dcb))
    {
        SetLastError("设置串口配置失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    // 设置超时参数
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = m_config.readTimeoutMs;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = m_config.writeTimeoutMs;
    
    if (!SetCommTimeouts(m_hComm.Get(), &timeouts))
    {
        SetLastError("设置超时参数失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    // 设置缓冲区大小
    if (!SetupComm(m_hComm.Get(), static_cast<DWORD>(m_config.rxBufferSize), static_cast<DWORD>(m_config.txBufferSize)))
    {
        SetLastError("设置缓冲区失败: " + GetSystemErrorString(::GetLastError()));
        return false;
    }
    
    // 清空缓冲区
    PurgeComm(m_hComm.Get(), PURGE_TXCLEAR | PURGE_RXCLEAR);
    
    return true;
}

void SerialTransport::ReadThreadFunc()
{
    const size_t BUFFER_SIZE = 1024;
    std::vector<uint8_t> tempBuffer(BUFFER_SIZE);
    
    OVERLAPPED readOverlapped = {0};
    readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (readOverlapped.hEvent == NULL)
    {
        SetLastError("创建读事件失败");
        return;
    }
    
    while (!m_stopRead && m_hComm.IsValid())
    {
        DWORD bytesRead = 0;
        ResetEvent(readOverlapped.hEvent);
        
        // 异步读取数据
        BOOL result = ReadFile(m_hComm.Get(), tempBuffer.data(), BUFFER_SIZE, &bytesRead, &readOverlapped);
        
        if (result)
        {
            // 立即完成
            if (bytesRead > 0)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_readBuffer.insert(m_readBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
                
                // 通知数据到达
                std::vector<uint8_t> receivedData(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
                NotifyDataReceived(receivedData);
            }
        }
        else
        {
            DWORD winError = ::GetLastError();
            if (winError == ERROR_IO_PENDING)
            {
                // 等待读操作完成或超时
                DWORD waitResult = WaitForSingleObject(readOverlapped.hEvent, 100);
                if (waitResult == WAIT_OBJECT_0)
                {
                    if (GetOverlappedResult(m_hComm.Get(), &readOverlapped, &bytesRead, FALSE))
                    {
                        if (bytesRead > 0)
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_readBuffer.insert(m_readBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
                            
                            // 通知数据到达
                            std::vector<uint8_t> receivedData(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
                            NotifyDataReceived(receivedData);
                        }
                    }
                }
                else if (waitResult == WAIT_TIMEOUT)
                {
                    // 超时，继续循环
                    continue;
                }
                else
                {
                    // 其他错误
                    break;
                }
            }
            else
            {
                // 读取错误
                SetLastError("读取失败: " + GetSystemErrorString(winError));
                break;
            }
        }
        
        // 短暂休息避免占用过多CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    CloseHandle(readOverlapped.hEvent);
}

bool SerialTransport::SetupOverlapped()
{
    // 初始化OVERLAPPED结构
    ZeroMemory(&m_readOverlapped, sizeof(m_readOverlapped));
    ZeroMemory(&m_writeOverlapped, sizeof(m_writeOverlapped));
    
    // 创建事件对象
    m_readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_writeOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    return (m_readOverlapped.hEvent != NULL && m_writeOverlapped.hEvent != NULL);
}

void SerialTransport::CleanupOverlapped()
{
    // 清理事件对象
    if (m_readOverlapped.hEvent)
    {
        CloseHandle(m_readOverlapped.hEvent);
        m_readOverlapped.hEvent = NULL;
    }
    
    if (m_writeOverlapped.hEvent)
    {
        CloseHandle(m_writeOverlapped.hEvent);
        m_writeOverlapped.hEvent = NULL;
    }
}

std::string SerialTransport::GetSystemErrorString(DWORD error) const
{
    // TODO: 瀹炵幇Windows閿欒淇℃伅杞崲
    // 实现Windows错误信息转换
    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, NULL);
    
    if (size == 0) {
        return "Unknown error (code: " + std::to_string(error) + ")";
    }
    
    std::string result(buffer, size);
    LocalFree(buffer);
    
    // 移除末尾的换行符
    if (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
        if (!result.empty() && result.back() == '\r') {
            result.pop_back();
        }
    }
    
    return result + " (code: " + std::to_string(error) + ")";
}

void SerialTransport::StartContinuousReading()
{
    if (!IsOpen() || !m_ioWorker || m_continuousReading.load())
        return;

    m_continuousReading = true;
    
    // 准备读取缓冲区
    m_readBuffer.resize(4096);
    
    // 发起第一次异步读取
    m_ioWorker->AsyncRead(m_hComm.Get(), m_readBuffer, 
        [this](const IOResult& result) { OnReadCompleted(result); });
}

void SerialTransport::StopContinuousReading()
{
    m_continuousReading = false;
    
    // 取消所有待处理的I/O操作
    if (m_hComm.IsValid())
    {
        CancelIo(m_hComm.Get());
    }
}

void SerialTransport::OnReadCompleted(const IOResult& result)
{
    if (!m_continuousReading.load() || !IsOpen())
        return;
        
    if (result.success && result.bytesTransferred > 0)
    {
        // 通知数据接收
        std::vector<uint8_t> receivedData(result.data.begin(), 
                                         result.data.begin() + result.bytesTransferred);
        NotifyDataReceived(receivedData);
    }
    else if (!result.success && result.errorCode != ERROR_OPERATION_ABORTED)
    {
        // 处理读取错误（除了操作取消）
        SetLastError("异步读取失败：" + std::to_string(result.errorCode));
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return;
    }
    
    // 如果仍在连续读取模式，发起下一次异步读取
    if (m_continuousReading.load() && IsOpen())
    {
        m_ioWorker->AsyncRead(m_hComm.Get(), m_readBuffer, 
            [this](const IOResult& result) { OnReadCompleted(result); });
    }
}

void SerialTransport::OnWriteCompleted(const IOResult& result)
{
    if (!result.success)
    {
        SetLastError("异步写入失败：" + std::to_string(result.errorCode));
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
    }
}