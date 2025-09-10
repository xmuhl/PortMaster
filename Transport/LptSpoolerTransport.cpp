#pragma execution_character_set("utf-8")
#include "pch.h"
#include "LptSpoolerTransport.h"
#include <winspool.h>
#include <string>
#include <locale>
#include <codecvt>
#include <vector>

#pragma comment(lib, "winspool.lib")

LptSpoolerTransport::LptSpoolerTransport()
    : m_hPrinter(NULL)
    , m_jobId(0)
{
    m_state = TRANSPORT_CLOSED;
}

LptSpoolerTransport::~LptSpoolerTransport()
{
    Close();
}

bool LptSpoolerTransport::Open(const TransportConfig& config)
{
    if (m_state != TRANSPORT_CLOSED)
    {
        SetLastError("LPT连接已打开或正在操作中");
        return false;
    }
    
    m_config = config;
    
    // 从配置中获取打印机名称，默认为LPT1:
    m_printerName = config.portName.empty() ? "LPT1:" : config.portName;
    
    NotifyStateChanged(TRANSPORT_OPENING, "正在打开LPT打印机: " + m_printerName);
    
    // 打开打印机
    PRINTER_DEFAULTSA printerDefaults = {0};
    printerDefaults.pDatatype = "RAW";  // 使用RAW数据类型进行直接打印
    printerDefaults.DesiredAccess = PRINTER_ACCESS_USE;
    
    if (!OpenPrinterA(const_cast<char*>(m_printerName.c_str()), &m_hPrinter, &printerDefaults))
    {
        DWORD winError = ::GetLastError();
        SetLastError("打开打印机失败: " + GetPrinterErrorString(winError));
        NotifyStateChanged(TRANSPORT_ERROR, m_lastError);
        return false;
    }
    
    // 启动RAW打印作业
    if (!StartRawPrintJob())
    {
        ClosePrinter(m_hPrinter);
        m_hPrinter = NULL;
        NotifyStateChanged(TRANSPORT_ERROR, "启动打印作业失败");
        return false;
    }
    
    NotifyStateChanged(TRANSPORT_OPEN, "LPT打印机已打开: " + m_printerName);
    return true;
}

void LptSpoolerTransport::Close()
{
    if (m_state == TRANSPORT_CLOSED)
        return;
    
    NotifyStateChanged(TRANSPORT_CLOSING, "正在关闭LPT打印机");
    
    // 结束当前打印作业（如果有）
    if (m_jobId != 0)
    {
        EndPrintJob();
        m_jobId = 0;
    }
    
    // 关闭打印机句柄
    if (m_hPrinter != NULL)
    {
        ClosePrinter(m_hPrinter);
        m_hPrinter = NULL;
    }
    
    m_printerName.clear();
    
    NotifyStateChanged(TRANSPORT_CLOSED, "LPT打印机已关闭");
}

bool LptSpoolerTransport::IsOpen() const
{
    return m_state == TRANSPORT_OPEN && m_hPrinter != NULL;
}

TransportState LptSpoolerTransport::GetState() const
{
    return m_state;
}

bool LptSpoolerTransport::Configure(const TransportConfig& config)
{
    m_config = config;
    return true;
}

TransportConfig LptSpoolerTransport::GetConfiguration() const
{
    return m_config;
}

size_t LptSpoolerTransport::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

size_t LptSpoolerTransport::Write(const uint8_t* data, size_t length)
{
    if (m_state != TRANSPORT_OPEN || m_hPrinter == NULL)
    {
        SetLastError("LPT打印机未打开");
        return 0;
    }
    
    if (!data || length == 0)
    {
        return 0;
    }
    
    // 如果没有活动的打印作业，启动一个
    if (m_jobId == 0)
    {
        if (!StartRawPrintJob())
        {
            return 0;
        }
    }
    
    DWORD bytesWritten = 0;
    if (!WritePrinter(m_hPrinter, const_cast<void*>(static_cast<const void*>(data)), 
                      static_cast<DWORD>(length), &bytesWritten))
    {
        DWORD error = ::GetLastError();
        SetLastError("写入打印机失败: " + GetPrinterErrorString(error));
        
        // 如果写入失败，结束打印作业
        EndPrintJob();
        return 0;
    }
    
    return static_cast<size_t>(bytesWritten);
}

size_t LptSpoolerTransport::Read(std::vector<uint8_t>& data, size_t maxLength)
{
    // LPT打印机通常不支持读取，但可以返回打印机状态信息
    data.clear();
    
    if (m_state != TRANSPORT_OPEN || m_hPrinter == NULL)
    {
        return 0;
    }
    
    // 获取打印机状态并转换为状态报告
    std::string statusText;
    if (GetPrinterStatus(statusText))
    {
        // 将状态文本转换为字节数据
        data.assign(statusText.begin(), statusText.end());
        data.push_back('\n'); // 添加换行符
        return data.size();
    }
    
    return 0;
}

size_t LptSpoolerTransport::Available() const
{
    // LPT打印机没有接收缓冲区的概念，返回0
    return 0;
}

std::string LptSpoolerTransport::GetLastError() const
{
    return m_lastError;
}

std::string LptSpoolerTransport::GetPortName() const
{
    return m_printerName;
}

std::string LptSpoolerTransport::GetTransportType() const
{
    return "LPT";
}

void LptSpoolerTransport::SetDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataCallback = callback;
}

void LptSpoolerTransport::SetStateChangedCallback(StateChangedCallback callback)
{
    m_stateCallback = callback;
}

bool LptSpoolerTransport::Flush()
{
    if (m_state != TRANSPORT_OPEN || m_hPrinter == NULL)
    {
        return false;
    }
    
    // 结束当前打印作业以确保数据被发送
    if (m_jobId != 0)
    {
        return EndPrintJob();
    }
    
    return true;
}

bool LptSpoolerTransport::ClearBuffers()
{
    // 结束当前打印作业并清除队列
    if (m_jobId != 0)
    {
        EndPrintJob();
    }
    
    return true;
}

std::vector<std::string> LptSpoolerTransport::EnumeratePrinters()
{
    std::vector<std::string> printers;
    
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD level = 2;
    DWORD needed = 0;
    DWORD returned = 0;
    
    // 首先获取所需缓冲区大小
    EnumPrinters(flags, NULL, level, NULL, 0, &needed, &returned);
    
    if (needed > 0)
    {
        std::vector<BYTE> buffer(needed);
        PRINTER_INFO_2* printerInfo = reinterpret_cast<PRINTER_INFO_2*>(buffer.data());
        
        if (EnumPrinters(flags, NULL, level, buffer.data(), needed, &needed, &returned))
        {
            for (DWORD i = 0; i < returned; i++)
            {
                if (printerInfo[i].pPrinterName != NULL)
                {
                    // 转换Unicode字符串到ANSI
                    std::string printerName;
                    std::string portName;
                    
                    if (printerInfo[i].pPrinterName != NULL) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPrinterName, -1, NULL, 0, NULL, NULL);
                        if (len > 0) {
                            std::vector<char> buffer(len);
                            WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPrinterName, -1, buffer.data(), len, NULL, NULL);
                            printerName = buffer.data();
                        }
                    }
                    
                    if (printerInfo[i].pPortName != NULL) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPortName, -1, NULL, 0, NULL, NULL);
                        if (len > 0) {
                            std::vector<char> buffer(len);
                            WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPortName, -1, buffer.data(), len, NULL, NULL);
                            portName = buffer.data();
                        }
                    }
                    
                    if (!printerName.empty() && !portName.empty())
                    {
                        if (portName.find("LPT") != std::string::npos)
                        {
                            printers.push_back(printerName + " (" + portName + ")");
                        }
                    }
                }
            }
        }
    }
    
    // 如果没有找到LPT打印机，添加默认的LPT端口
    if (printers.empty())
    {
        printers.push_back("LPT1:");
        printers.push_back("LPT2:");
        printers.push_back("LPT3:");
    }
    
    return printers;
}

void LptSpoolerTransport::SetPrinterName(const std::string& printerName)
{
    if (m_state == TRANSPORT_OPEN)
    {
        SetLastError("无法在连接打开时更改打印机名称");
        return;
    }
    m_printerName = printerName;
}

bool LptSpoolerTransport::GetPrinterStatus(std::string& status)
{
    if (m_hPrinter == NULL)
    {
        status = "打印机未打开";
        return false;
    }
    
    DWORD needed = 0;
    GetPrinter(m_hPrinter, 2, NULL, 0, &needed);
    
    if (needed > 0)
    {
        std::vector<BYTE> buffer(needed);
        PRINTER_INFO_2* printerInfo = reinterpret_cast<PRINTER_INFO_2*>(buffer.data());
        
        if (GetPrinter(m_hPrinter, 2, buffer.data(), needed, &needed))
        {
            // 解析打印机状态
            DWORD printerStatus = printerInfo->Status;
            
            if (printerStatus == 0)
            {
                status = "就绪";
            }
            else
            {
                status = "状态: ";
                if (printerStatus & PRINTER_STATUS_BUSY) status += "忙碌 ";
                if (printerStatus & PRINTER_STATUS_ERROR) status += "错误 ";
                if (printerStatus & PRINTER_STATUS_OFFLINE) status += "脱机 ";
                if (printerStatus & PRINTER_STATUS_OUT_OF_MEMORY) status += "内存不足 ";
                if (printerStatus & PRINTER_STATUS_PAPER_OUT) status += "缺纸 ";
                if (printerStatus & PRINTER_STATUS_PAPER_JAM) status += "卡纸 ";
                if (printerStatus & PRINTER_STATUS_PAUSED) status += "暂停 ";
                if (printerStatus & PRINTER_STATUS_PENDING_DELETION) status += "待删除 ";
            }
            
            // 添加作业数量信息
            status += " (队列: " + std::to_string(printerInfo->cJobs) + "个作业)";
            return true;
        }
    }
    
    status = "无法获取状态";
    return false;
}

bool LptSpoolerTransport::StartRawPrintJob()
{
    if (m_hPrinter == NULL)
    {
        SetLastError("打印机未打开");
        return false;
    }
    
    if (m_jobId != 0)
    {
        // 已经有活动的打印作业
        return true;
    }
    
    DOC_INFO_1 docInfo;
    ZeroMemory(&docInfo, sizeof(docInfo));
    docInfo.pDocName = const_cast<LPWSTR>(L"PortMaster RAW Data");
    docInfo.pOutputFile = NULL;
    docInfo.pDatatype = const_cast<LPWSTR>(L"RAW");
    
    m_jobId = StartDocPrinter(m_hPrinter, 1, reinterpret_cast<BYTE*>(&docInfo));
    if (m_jobId == 0)
    {
        DWORD error = ::GetLastError();
        SetLastError("启动打印作业失败: " + GetPrinterErrorString(error));
        return false;
    }
    
    if (!StartPagePrinter(m_hPrinter))
    {
        DWORD error = ::GetLastError();
        SetLastError("启动打印页面失败: " + GetPrinterErrorString(error));
        EndDocPrinter(m_hPrinter);
        m_jobId = 0;
        return false;
    }
    
    return true;
}
bool LptSpoolerTransport::EndPrintJob()
{
    if (m_hPrinter == NULL || m_jobId == 0)
    {
        return true;
    }
    
    bool success = true;
    
    if (!EndPagePrinter(m_hPrinter))
    {
        DWORD error = ::GetLastError();
        SetLastError("结束打印页面失败: " + GetPrinterErrorString(error));
        success = false;
    }
    
    if (!EndDocPrinter(m_hPrinter))
    {
        DWORD error = ::GetLastError();
        SetLastError("结束打印文档失败: " + GetPrinterErrorString(error));
        success = false;
    }
    
    m_jobId = 0;
    return success;
}

void LptSpoolerTransport::NotifyDataReceived(const std::vector<uint8_t>& data)
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

void LptSpoolerTransport::NotifyStateChanged(TransportState state, const std::string& message)
{
    m_state = state;
    if (m_stateCallback)
        m_stateCallback(state, message);
}

void LptSpoolerTransport::SetLastError(const std::string& error)
{
    m_lastError = error;
}

std::string LptSpoolerTransport::GetPrinterErrorString(DWORD error) const
{
    switch (error)
    {
    case ERROR_ACCESS_DENIED:
        return "访问被拒绝";
    case ERROR_FILE_NOT_FOUND:
        return "找不到打印机";
    case ERROR_INVALID_HANDLE:
        return "无效的句柄";
    case ERROR_NOT_ENOUGH_MEMORY:
        return "内存不足";
    case ERROR_INVALID_PARAMETER:
        return "无效的参数";
    default:
        {
            LPVOID msgBuf;
            DWORD result = FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&msgBuf,
                0,
                NULL
            );
            
            if (result > 0)
            {
                std::string errorMsg = static_cast<char*>(msgBuf);
                LocalFree(msgBuf);
                return errorMsg;
            }
            
            return "未知错误 (代码: " + std::to_string(error) + ")";
        }
    }
}