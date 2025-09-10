#pragma execution_character_set("utf-8")
#include "pch.h"
#include "UsbPrinterTransport.h"
#include <winspool.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

UsbPrinterTransport::UsbPrinterTransport()
{
}

UsbPrinterTransport::~UsbPrinterTransport()
{
}

std::string UsbPrinterTransport::GetTransportType() const
{
    return "USB Printer";
}

std::vector<std::string> UsbPrinterTransport::EnumerateUsbPrinters()
{
    std::vector<std::string> usbPrinters;
    
    // 首先获取所有打印机
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD level = 2;
    DWORD needed = 0;
    DWORD returned = 0;
    
    // 获取所需缓冲区大小
    EnumPrinters(flags, NULL, level, NULL, 0, &needed, &returned);
    
    if (needed > 0)
    {
        std::vector<BYTE> buffer(needed);
        PRINTER_INFO_2* printerInfo = reinterpret_cast<PRINTER_INFO_2*>(buffer.data());
        
        if (EnumPrinters(flags, NULL, level, buffer.data(), needed, &needed, &returned))
        {
            for (DWORD i = 0; i < returned; i++)
            {
                if (printerInfo[i].pPrinterName != NULL && printerInfo[i].pPortName != NULL)
                {
                    // KISS: 安全的字符串编码转换
                    std::string printerName;
                    std::string portName;
                    
                    // 安全处理TCHAR*到std::string的转换
                    if (printerInfo[i].pPrinterName != NULL) {
#ifdef UNICODE
                        // Unicode环境：TCHAR*是wchar_t*，需要转换为多字节字符串
                        int nameSize = WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPrinterName, -1, NULL, 0, NULL, NULL);
                        if (nameSize > 0) {
                            std::vector<char> nameBuffer(nameSize);
                            WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPrinterName, -1, nameBuffer.data(), nameSize, NULL, NULL);
                            printerName = nameBuffer.data();
                        }
#else
                        // ANSI环境：TCHAR*就是char*，直接转换
                        printerName = printerInfo[i].pPrinterName;
#endif
                    }
                    
                    if (printerInfo[i].pPortName != NULL) {
#ifdef UNICODE
                        int portSize = WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPortName, -1, NULL, 0, NULL, NULL);
                        if (portSize > 0) {
                            std::vector<char> portBuffer(portSize);
                            WideCharToMultiByte(CP_UTF8, 0, printerInfo[i].pPortName, -1, portBuffer.data(), portSize, NULL, NULL);
                            portName = portBuffer.data();
                        }
#else
                        portName = printerInfo[i].pPortName;
#endif
                    }
                    
                    // 检查是否是USB端口
                    if (IsUsbPort(portName))
                    {
                        // 获取设备详细信息
                        std::string deviceId = GetUsbDeviceId(portName);
                        if (!deviceId.empty())
                        {
                            usbPrinters.push_back(printerName + " (" + portName + ") [" + deviceId + "]");
                        }
                        else
                        {
                            usbPrinters.push_back(printerName + " (" + portName + ")");
                        }
                    }
                }
            }
        }
    }
    
    // 如果通过打印机枚举没有找到USB打印机，直接枚举USB设备
    if (usbPrinters.empty())
    {
        EnumerateUsbPrinterDevices(usbPrinters);
    }
    
    return usbPrinters;
}

bool UsbPrinterTransport::IsUsbPrinter(const std::string& printerName)
{
    // 检查打印机名称是否包含USB关键字
    std::string upperName = printerName;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    
    return upperName.find("USB") != std::string::npos ||
           upperName.find("001") != std::string::npos ||  // USB端口通常包含001
           upperName.find("002") != std::string::npos ||
           upperName.find("003") != std::string::npos;
}

bool UsbPrinterTransport::IsUsbPort(const std::string& portName)
{
    std::string upperPort = portName;
    std::transform(upperPort.begin(), upperPort.end(), upperPort.begin(), ::toupper);
    
    // USB端口通常以USB开头，或者包含特定模式
    return upperPort.find("USB") == 0 ||
           upperPort.find("DOT4_") == 0 ||
           upperPort.find("WSD-") == 0 ||
           (upperPort.find("001") != std::string::npos && upperPort.find(":") != std::string::npos);
}

std::string UsbPrinterTransport::GetUsbDeviceId(const std::string& portName)
{
    // 尝试从端口名称解析设备ID
    // USB端口名称格式通常是: USB001, USB002 等
    
    if (portName.find("USB") == 0)
    {
        return "USB Device Port: " + portName;
    }
    
    if (portName.find("DOT4_") == 0)
    {
        return "DOT4 USB Device: " + portName;
    }
    
    if (portName.find("WSD-") == 0)
    {
        return "Web Services Device: " + portName;
    }
    
    return "";
}

void UsbPrinterTransport::EnumerateUsbPrinterDevices(std::vector<std::string>& devices)
{
    // 使用SetupAPI枚举USB打印设备
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PRINTER,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        return;
    }
    
    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    for (DWORD deviceIndex = 0; SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData); deviceIndex++)
    {
        // 获取设备描述 - SOLID-S: 使用安全缓冲区大小
        std::vector<char> deviceDesc(DEVICE_DESC_MAX_SIZE);
        DWORD requiredSize = 0;
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_DEVICEDESC,
            NULL,
            (BYTE*)deviceDesc.data(),
            static_cast<DWORD>(deviceDesc.size() - SAFETY_BUFFER_MARGIN),
            &requiredSize))
        {
            // 获取硬件ID - SOLID-S: 使用安全缓冲区大小
            std::vector<char> hardwareId(HARDWARE_ID_MAX_SIZE);
            DWORD hwIdRequiredSize = 0;
            if (SetupDiGetDeviceRegistryProperty(
                deviceInfoSet,
                &deviceInfoData,
                SPDRP_HARDWAREID,
                NULL,
                (BYTE*)hardwareId.data(),
                static_cast<DWORD>(hardwareId.size() - SAFETY_BUFFER_MARGIN),
                &hwIdRequiredSize))
            {
                std::string hwId = hardwareId.data();
                std::transform(hwId.begin(), hwId.end(), hwId.begin(), ::toupper);
                
                // 检查是否是USB设备
                if (hwId.find("USB\\") == 0)
                {
                    devices.push_back(std::string(deviceDesc.data()) + " [" + hardwareId.data() + "]");
                }
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    
    // YAGNI: 移除不必要的默认端口生成 - 只显示真实存在的USB设备
    // KISS: 简化过滤逻辑 - 空列表表示没有可用的USB打印机
}

bool UsbPrinterTransport::Open(const TransportConfig& config)
{
    // USB打印机通过Windows打印后台程序访问，因此使用父类的实现
    // 但需要验证是否真的是USB设备
    
    if (!config.portName.empty() && !IsUsbPort(config.portName))
    {
        SetLastError("指定的端口不是USB端口: " + config.portName);
        return false;
    }
    
    // 调用父类的打开方法
    return LptSpoolerTransport::Open(config);
}

std::string UsbPrinterTransport::GetPortName() const
{
    std::string portName = LptSpoolerTransport::GetPortName();
    
    // 如果端口名称不明确是USB，添加标识
    if (!IsUsbPort(portName))
    {
        return "USB:" + portName;
    }
    
    return portName;
}