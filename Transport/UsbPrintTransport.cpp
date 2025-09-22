#pragma execution_character_set("utf-8")

#include "pch.h"
#include "UsbPrintTransport.h"
#include <algorithm>
#include <sstream>

// 构造函数
UsbPrintTransport::UsbPrintTransport()
    : m_state(TransportState::Closed)
    , m_hDevice(INVALID_HANDLE_VALUE)
    , m_asyncReadRunning(false)
    , m_asyncWriteRunning(false)
    , m_statusThreadRunning(false)
    , m_lastStatus(UsbDeviceStatus::Unknown)
{
    memset(&m_stats, 0, sizeof(m_stats));
}

// 析构函数
UsbPrintTransport::~UsbPrintTransport()
{
    Close();
}

// 打开传输通道
TransportError UsbPrintTransport::Open(const TransportConfig& baseConfig)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != TransportState::Closed)
    {
        return TransportError::AlreadyOpen;
    }
    
    // 转换配置类型
    const UsbPrintConfig* config = dynamic_cast<const UsbPrintConfig*>(&baseConfig);
    if (!config)
    {
        UsbPrintConfig defaultConfig;
        defaultConfig.portName = baseConfig.portName;
        defaultConfig.readTimeout = baseConfig.readTimeout;
        defaultConfig.writeTimeout = baseConfig.writeTimeout;
        defaultConfig.bufferSize = baseConfig.bufferSize;
        defaultConfig.asyncMode = baseConfig.asyncMode;
        m_config = defaultConfig;
    }
    else
    {
        m_config = *config;
    }
    
    if (!ValidateConfig(m_config))
    {
        return TransportError::InvalidConfig;
    }
    
    m_config.deviceName = NormalizePortName(m_config.portName);
    m_config.portName = m_config.deviceName;
    
    SetState(TransportState::Opening);
    
    TransportError result = OpenDeviceHandle();
    if (result != TransportError::Success)
    {
        SetState(TransportState::Error);
        return result;
    }
    
    result = SetDeviceTimeouts();
    if (result != TransportError::Success)
    {
        CloseDeviceHandle();
        SetState(TransportState::Error);
        return result;
    }
    
    // 启动状态监控线程
    if (m_config.checkStatus)
    {
        m_statusThreadRunning = true;
        m_statusThread = std::thread(&UsbPrintTransport::StatusMonitorThread, this);
    }
    
    if (m_config.asyncMode)
    {
        result = StartAsyncRead();
        if (result != TransportError::Success)
        {
            CloseDeviceHandle();
            SetState(TransportState::Error);
            return result;
        }
    }
    
    SetState(TransportState::Open);
    return TransportError::Success;
}

// 关闭传输通道
TransportError UsbPrintTransport::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == TransportState::Closed)
    {
        return TransportError::Success;
    }
    
    SetState(TransportState::Closing);
    
    StopAsyncRead();
    
    if (m_statusThreadRunning)
    {
        m_statusThreadRunning = false;
        if (m_statusThread.joinable())
        {
            m_statusThread.join();
        }
    }
    
    if (m_asyncWriteRunning)
    {
        m_asyncWriteRunning = false;
        if (m_asyncWriteThread.joinable())
        {
            m_asyncWriteThread.join();
        }
    }
    
    CloseDeviceHandle();
    SetState(TransportState::Closed);
    return TransportError::Success;
}

// 同步写入数据
TransportError UsbPrintTransport::Write(const void* data, size_t size, size_t* written)
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (!data || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    return WriteToDevice(data, size, written);
}

// 同步读取数据
TransportError UsbPrintTransport::Read(void* buffer, size_t size, size_t* read, DWORD timeout)
{
    if (!IsOpen())
    {
        return TransportError::NotOpen;
    }
    
    if (!buffer || size == 0)
    {
        return TransportError::InvalidParameter;
    }
    
    return ReadFromDevice(buffer, size, read, timeout);
}

// 其他接口方法的基本实现
TransportError UsbPrintTransport::WriteAsync(const void* data, size_t size)
{
    if (!IsOpen()) return TransportError::NotOpen;
    if (!data || size == 0) return TransportError::InvalidParameter;
    
    std::vector<uint8_t> buffer(static_cast<const uint8_t*>(data), 
                                static_cast<const uint8_t*>(data) + size);
    
    {
        std::lock_guard<std::mutex> lock(m_writeQueueMutex);
        m_writeQueue.push(std::move(buffer));
    }
    
    if (!m_asyncWriteRunning)
    {
        m_asyncWriteRunning = true;
        m_asyncWriteThread = std::thread(&UsbPrintTransport::AsyncWriteThread, this);
    }
    
    return TransportError::Success;
}

TransportError UsbPrintTransport::StartAsyncRead()
{
    if (!IsOpen()) return TransportError::NotOpen;
    if (m_asyncReadRunning) return TransportError::Success;
    
    m_asyncReadRunning = true;
    m_asyncReadThread = std::thread(&UsbPrintTransport::AsyncReadThread, this);
    return TransportError::Success;
}

TransportError UsbPrintTransport::StopAsyncRead()
{
    if (!m_asyncReadRunning) return TransportError::Success;
    
    m_asyncReadRunning = false;
    if (m_asyncReadThread.joinable())
    {
        m_asyncReadThread.join();
    }
    return TransportError::Success;
}

TransportState UsbPrintTransport::GetState() const { return m_state; }
bool UsbPrintTransport::IsOpen() const { return m_state == TransportState::Open && m_hDevice != INVALID_HANDLE_VALUE; }

TransportStats UsbPrintTransport::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void UsbPrintTransport::ResetStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

std::string UsbPrintTransport::GetPortName() const { return m_config.portName; }

void UsbPrintTransport::SetDataReceivedCallback(DataReceivedCallback callback) { m_dataReceivedCallback = callback; }
void UsbPrintTransport::SetStateChangedCallback(StateChangedCallback callback) { m_stateChangedCallback = callback; }
void UsbPrintTransport::SetErrorOccurredCallback(ErrorOccurredCallback callback) { m_errorOccurredCallback = callback; }

TransportError UsbPrintTransport::FlushBuffers()
{
    if (!IsOpen()) return TransportError::NotOpen;
    if (!FlushFileBuffers(m_hDevice)) return GetLastError();
    return TransportError::Success;
}

size_t UsbPrintTransport::GetAvailableBytes() const { return 0; }

// USB专用方法
UsbDeviceStatus UsbPrintTransport::GetDeviceStatus() const { return QueryDeviceStatus(); }

std::string UsbPrintTransport::GetDeviceInfo() const
{
    return "USB设备: " + m_config.deviceName;
}

bool UsbPrintTransport::IsDeviceConnected() const
{
    UsbDeviceStatus status = GetDeviceStatus();
    return status != UsbDeviceStatus::NotConnected;
}

// 静态方法
std::vector<std::string> UsbPrintTransport::EnumerateUsbPorts()
{
    std::vector<std::string> ports;
    std::vector<std::string> commonPorts = {"USB001", "USB002", "USB003", "USB004"};
    
    for (const auto& port : commonPorts)
    {
        if (IsUsbPortAvailable(port))
        {
            ports.push_back(port);
        }
    }
    return ports;
}

bool UsbPrintTransport::IsUsbPortAvailable(const std::string& portName)
{
    std::string devicePath = "\\\\.\\" + portName;
    HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hDevice);
        return true;
    }
    return false;
}

std::string UsbPrintTransport::GetDeviceStatusString(UsbDeviceStatus status)
{
    std::vector<std::string> statusStrings;
    
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Ready))
        statusStrings.push_back("就绪");
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Busy))
        statusStrings.push_back("忙碌");
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Offline))
        statusStrings.push_back("离线");
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::OutOfPaper))
        statusStrings.push_back("缺纸");
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::Error))
        statusStrings.push_back("错误");
    if (static_cast<int>(status) & static_cast<int>(UsbDeviceStatus::NotConnected))
        statusStrings.push_back("未连接");
    
    if (statusStrings.empty()) return "未知";
    
    std::string result;
    for (size_t i = 0; i < statusStrings.size(); ++i)
    {
        if (i > 0) result += ", ";
        result += statusStrings[i];
    }
    return result;
}

// 内部实现方法
void UsbPrintTransport::SetState(TransportState newState)
{
    TransportState oldState = m_state;
    m_state = newState;
    
    if (oldState != newState && m_stateChangedCallback)
    {
        m_stateChangedCallback(newState);
    }
}

void UsbPrintTransport::NotifyError(TransportError error, const std::string& message)
{
    if (m_errorOccurredCallback)
    {
        m_errorOccurredCallback(error, message);
    }
}

void UsbPrintTransport::UpdateStats(uint64_t bytesSent, uint64_t bytesReceived)
{
    m_stats.bytesSent += bytesSent;
    m_stats.bytesReceived += bytesReceived;
    m_stats.packetsTotal++;
}

TransportError UsbPrintTransport::OpenDeviceHandle()
{
    std::string devicePath = "\\\\.\\" + m_config.deviceName;
    
    m_hDevice = CreateFileA(
        devicePath.c_str(),
        m_config.accessMode,
        m_config.shareMode,
        nullptr,
        m_config.creationDisposition,
        m_config.flagsAndAttributes,
        nullptr
    );
    
    if (m_hDevice == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }
    
    return TransportError::Success;
}

void UsbPrintTransport::CloseDeviceHandle()
{
    if (m_hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

TransportError UsbPrintTransport::WriteToDevice(const void* data, size_t size, size_t* written)
{
    DWORD bytesWritten = 0;
    BOOL success = WriteFile(m_hDevice, data, static_cast<DWORD>(size), &bytesWritten, nullptr);
    
    if (written) *written = bytesWritten;
    
    if (!success) return GetLastError();
    
    UpdateStats(bytesWritten, 0);
    return TransportError::Success;
}

TransportError UsbPrintTransport::ReadFromDevice(void* buffer, size_t size, size_t* read, DWORD timeout)
{
    DWORD bytesRead = 0;
    BOOL success = ReadFile(m_hDevice, buffer, static_cast<DWORD>(size), &bytesRead, nullptr);
    
    if (read) *read = bytesRead;
    
    if (!success) return GetLastError();
    
    UpdateStats(0, bytesRead);
    return TransportError::Success;
}

UsbDeviceStatus UsbPrintTransport::QueryDeviceStatus() const
{
    if (!IsOpen()) return UsbDeviceStatus::Unknown;
    return UsbDeviceStatus::Ready;  // 简化实现
}

void UsbPrintTransport::StatusMonitorThread()
{
    while (m_statusThreadRunning)
    {
        UsbDeviceStatus currentStatus = QueryDeviceStatus();
        if (currentStatus != m_lastStatus)
        {
            m_lastStatus = currentStatus;
        }
        Sleep(m_config.statusCheckInterval);
    }
}

void UsbPrintTransport::AsyncReadThread()
{
    const size_t bufferSize = 1024;
    std::vector<uint8_t> buffer(bufferSize);
    
    while (m_asyncReadRunning && IsOpen())
    {
        size_t bytesRead = 0;
        TransportError result = ReadFromDevice(buffer.data(), bufferSize, &bytesRead, m_config.readTimeout);
        
        if (result == TransportError::Success && bytesRead > 0)
        {
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
            if (m_dataReceivedCallback)
            {
                m_dataReceivedCallback(data);
            }
        }
        else if (result != TransportError::Timeout)
        {
            NotifyError(result, "异步读取失败");
            break;
        }
    }
}

void UsbPrintTransport::AsyncWriteThread()
{
    while (m_asyncWriteRunning)
    {
        std::vector<uint8_t> data;
        
        {
            std::lock_guard<std::mutex> lock(m_writeQueueMutex);
            if (m_writeQueue.empty())
            {
                Sleep(10);
                continue;
            }
            
            data = std::move(m_writeQueue.front());
            m_writeQueue.pop();
        }
        
        size_t written = 0;
        TransportError result = WriteToDevice(data.data(), data.size(), &written);
        
        if (result != TransportError::Success)
        {
            NotifyError(result, "异步写入失败");
        }
    }
}

TransportError UsbPrintTransport::GetLastError()
{
    DWORD error = ::GetLastError();
    m_stats.lastErrorCode = error;
    
    switch (error)
    {
        case ERROR_SUCCESS:
            return TransportError::Success;
        case ERROR_FILE_NOT_FOUND:
            return TransportError::OpenFailed;
        case ERROR_ACCESS_DENIED:
            return TransportError::Busy;
        case ERROR_INVALID_HANDLE:
            return TransportError::NotOpen;
        case ERROR_TIMEOUT:
            return TransportError::Timeout;
        default:
            return TransportError::WriteFailed;
    }
}

std::string UsbPrintTransport::GetSystemErrorMessage(DWORD errorCode) const
{
    LPSTR messageBuffer = nullptr;
    
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    
    return message;
}

bool UsbPrintTransport::ValidateConfig(const UsbPrintConfig& config) const
{
    return !config.deviceName.empty() && !config.portName.empty() && 
           config.readTimeout > 0 && config.writeTimeout > 0 && config.bufferSize > 0;
}

std::string UsbPrintTransport::NormalizePortName(const std::string& portName) const
{
    std::string normalized = portName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
    
    if (normalized.find("USB") != 0)
    {
        if (normalized == "1") normalized = "USB001";
        else if (normalized == "2") normalized = "USB002";
        else normalized = "USB001";
    }
    
    return normalized;
}

TransportError UsbPrintTransport::SetDeviceTimeouts()
{
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = m_config.readTimeout;
    timeouts.ReadTotalTimeoutConstant = m_config.readTimeout;
    timeouts.WriteTotalTimeoutConstant = m_config.writeTimeout;
    
    if (!SetCommTimeouts(m_hDevice, &timeouts))
    {
        return GetLastError();
    }
    
    return TransportError::Success;
}