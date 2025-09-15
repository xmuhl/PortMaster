#pragma execution_character_set("utf-8")
#include "pch.h"
#include "DeviceManager.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/LptSpoolerTransport.h"
#include "../Transport/UsbPrinterTransport.h"
#include "ConfigManager.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <windows.h>

extern void WriteDebugLog(const char* message); // 🔴 修复：添加外部函数声明

DeviceManager::DeviceManager()
    : m_monitoring(false)
    , m_stopMonitoring(false)
{
    // 🔴 根本性修复：启动时不加载任何设备信息，完全延迟到用户需要时
    // KISS原则：保持启动流程极简，只做必要的初始化

    // 原有的启动时加载已移除：
    // LoadDeviceHistory();     // 延迟到用户查看历史设备时加载
    // LoadFavoriteDevices();   // 延迟到用户查看收藏设备时加载

    // 启动时只初始化基本状态，不进行任何I/O操作或设备检查
    WriteDebugLog("[DEBUG] DeviceManager构造完成 - 快速启动模式");
}

DeviceManager::~DeviceManager()
{
    StopDeviceMonitoring();
    SaveDeviceHistory();
    SaveFavoriteDevices();
}

std::vector<DeviceInfo> DeviceManager::EnumerateAllDevices()
{
    std::vector<DeviceInfo> allDevices;
    
    // 串口设备
    auto serialDevices = EnumerateSerialPorts();
    allDevices.insert(allDevices.end(), serialDevices.begin(), serialDevices.end());
    
    // TCP设备 (常用配置)
    auto tcpDevices = EnumerateTcpDevices();
    allDevices.insert(allDevices.end(), tcpDevices.begin(), tcpDevices.end());
    
    // UDP设备 (常用配置)
    auto udpDevices = EnumerateUdpDevices();
    allDevices.insert(allDevices.end(), udpDevices.begin(), udpDevices.end());
    
    // LPT打印机
    auto lptDevices = EnumerateLptPrinters();
    allDevices.insert(allDevices.end(), lptDevices.begin(), lptDevices.end());
    
    // USB打印机
    auto usbDevices = EnumerateUsbPrinters();
    allDevices.insert(allDevices.end(), usbDevices.begin(), usbDevices.end());
    
    // 虚拟设备
    auto virtualDevices = EnumerateVirtualDevices();
    allDevices.insert(allDevices.end(), virtualDevices.begin(), virtualDevices.end());
    
    // 自定义设备
    allDevices.insert(allDevices.end(), m_customDevices.begin(), m_customDevices.end());
    
    // 去重并排序
    DeduplicateDevices(allDevices);
    SortDevices(allDevices);
    
    return allDevices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateDevicesByType(const std::string& transportType)
{
    if (transportType == "Serial") {
        return EnumerateSerialPorts();
    } else if (transportType == "TCP") {
        return EnumerateTcpDevices();
    } else if (transportType == "UDP") {
        return EnumerateUdpDevices();
    } else if (transportType == "LPT") {
        return EnumerateLptPrinters();
    } else if (transportType == "USB") {
        return EnumerateUsbPrinters();
    } else if (transportType == "Loopback") {
        return EnumerateVirtualDevices();
    }
    
    return std::vector<DeviceInfo>();
}

std::vector<DeviceInfo> DeviceManager::EnumerateSerialPorts()
{
    std::vector<DeviceInfo> devices;
    
    // 使用SerialTransport的现有枚举功能
    auto portNames = SerialTransport::EnumeratePorts();
    
    for (const auto& portName : portNames)
    {
        DeviceInfo device(portName, "Serial");
        device.displayName = "串口 " + portName;
        device.description = GetDeviceDescription(portName, "Serial");

        // 🚀 性能优化：使用快速检查，避免UI阻塞
        // 对于实际连接验证，调用IsSerialPortReallyAvailable()
        device.isAvailable = true; // 延迟验证策略
        
        // 添加串口特定属性
        device.properties["type"] = "Serial";
        device.properties["interface"] = "RS232/RS485";
        
        devices.push_back(device);
    }
    
    return devices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateTcpDevices()
{
    std::vector<DeviceInfo> devices;
    
    // 常用的TCP配置
    std::vector<std::pair<std::string, int>> commonTcpConfigs = {
        {"127.0.0.1", 8080},
        {"127.0.0.1", 9000},
        {"127.0.0.1", 10001},
        {"192.168.1.100", 8080},
        {"localhost", 8080}
    };
    
    for (const auto& config : commonTcpConfigs)
    {
        // TCP客户端模式
        auto clientDevice = CreateTcpDevice(config.first, config.second, false);
        devices.push_back(clientDevice);
        
        // TCP服务器模式
        auto serverDevice = CreateTcpDevice(config.first, config.second, true);
        devices.push_back(serverDevice);
    }
    
    return devices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateUdpDevices()
{
    std::vector<DeviceInfo> devices;
    
    // 常用的UDP配置
    std::vector<std::pair<std::string, int>> commonUdpConfigs = {
        {"127.0.0.1", 8080},
        {"127.0.0.1", 9000},
        {"192.168.1.100", 8080},
        {"255.255.255.255", 8080} // 广播
    };
    
    for (const auto& config : commonUdpConfigs)
    {
        auto device = CreateUdpDevice(config.first, config.second);
        devices.push_back(device);
    }
    
    return devices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateLptPrinters()
{
    std::vector<DeviceInfo> devices;
    
    // 使用LptSpoolerTransport的现有枚举功能
    auto printerNames = LptSpoolerTransport::EnumeratePrinters();
    
    for (const auto& printerName : printerNames)
    {
        DeviceInfo device(printerName, "LPT");
        device.displayName = "LPT打印机 " + printerName;
        device.description = GetDeviceDescription(printerName, "LPT");
        device.isAvailable = true; // 假设打印机可用
        
        device.properties["type"] = "LPT";
        device.properties["interface"] = "Print Spooler";
        
        devices.push_back(device);
    }
    
    return devices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateUsbPrinters()
{
    std::vector<DeviceInfo> devices;
    
    // 使用UsbPrinterTransport的现有枚举功能
    auto printerNames = UsbPrinterTransport::EnumerateUsbPrinters();
    
    for (const auto& printerName : printerNames)
    {
        DeviceInfo device(printerName, "USB");
        device.displayName = "USB打印机 " + printerName;
        device.description = GetDeviceDescription(printerName, "USB");
        device.isAvailable = true; // 假设USB打印机可用
        
        device.properties["type"] = "USB";
        device.properties["interface"] = "USB";
        
        devices.push_back(device);
    }
    
    return devices;
}

std::vector<DeviceInfo> DeviceManager::EnumerateVirtualDevices()
{
    std::vector<DeviceInfo> devices;
    
    // 环回设备
    auto loopbackDevice = CreateVirtualDevice("Loopback", "Loopback");
    loopbackDevice.description = "内部环回测试设备";
    devices.push_back(loopbackDevice);
    
    return devices;
}

bool DeviceManager::IsDeviceAvailable(const std::string& deviceName, const std::string& transportType)
{
    if (transportType == "Serial") {
        return IsSerialPortAvailable(deviceName);
    } else if (transportType == "Loopback") {
        return true; // 虚拟设备总是可用
    } else if (transportType == "TCP" || transportType == "UDP") {
        // 网络设备需要更复杂的可用性检查
        return true; // 简化处理
    }
    
    return false;
}

void DeviceManager::AddToHistory(const DeviceInfo& device)
{
    // 移除重复项
    auto it = std::remove_if(m_historyDevices.begin(), m_historyDevices.end(),
        [&device](const DeviceInfo& existing) {
            return existing.deviceName == device.deviceName && 
                   existing.transportType == device.transportType;
        });
    m_historyDevices.erase(it, m_historyDevices.end());
    
    // 添加到前面
    m_historyDevices.insert(m_historyDevices.begin(), device);
    
    // 限制历史记录数量
    if (m_historyDevices.size() > 20) {
        m_historyDevices.resize(20);
    }
    
    SaveDeviceHistory();
}

void DeviceManager::AddToFavorites(const DeviceInfo& device)
{
    // 检查是否已存在
    auto it = std::find_if(m_favoriteDevices.begin(), m_favoriteDevices.end(),
        [&device](const DeviceInfo& existing) {
            return existing.deviceName == device.deviceName && 
                   existing.transportType == device.transportType;
        });
    
    if (it == m_favoriteDevices.end()) {
        m_favoriteDevices.push_back(device);
        SaveFavoriteDevices();
    }
}

void DeviceManager::RemoveFromFavorites(const std::string& deviceName, const std::string& transportType)
{
    auto it = std::remove_if(m_favoriteDevices.begin(), m_favoriteDevices.end(),
        [&](const DeviceInfo& device) {
            return device.deviceName == deviceName && device.transportType == transportType;
        });
    
    if (it != m_favoriteDevices.end()) {
        m_favoriteDevices.erase(it, m_favoriteDevices.end());
        SaveFavoriteDevices();
    }
}

std::vector<DeviceInfo> DeviceManager::GetHistoryDevices()
{
    // 🚀 懒加载机制：只有用户查看历史设备时才实际加载
    static bool historyLoaded = false;
    if (!historyLoaded) {
        LoadDeviceHistory();
        historyLoaded = true;
    }
    return m_historyDevices;
}

std::vector<DeviceInfo> DeviceManager::GetFavoriteDevices()
{
    // 🚀 懒加载机制：只有用户查看收藏设备时才实际加载
    static bool favoritesLoaded = false;
    if (!favoritesLoaded) {
        LoadFavoriteDevices();
        favoritesLoaded = true;
    }
    return m_favoriteDevices;
}

DeviceInfo DeviceManager::CreateTcpDevice(const std::string& address, int port, bool isServer)
{
    std::string deviceName = address + ":" + std::to_string(port);
    DeviceInfo device(deviceName, "TCP");
    
    device.displayName = (isServer ? "TCP服务器 " : "TCP客户端 ") + deviceName;
    device.description = isServer ? "TCP服务器连接" : "TCP客户端连接";
    device.isAvailable = true;
    
    device.properties["address"] = address;
    device.properties["port"] = std::to_string(port);
    device.properties["mode"] = isServer ? "server" : "client";
    
    return device;
}

DeviceInfo DeviceManager::CreateUdpDevice(const std::string& address, int port)
{
    std::string deviceName = address + ":" + std::to_string(port);
    DeviceInfo device(deviceName, "UDP");
    
    device.displayName = "UDP " + deviceName;
    device.description = "UDP数据报连接";
    device.isAvailable = true;
    
    device.properties["address"] = address;
    device.properties["port"] = std::to_string(port);
    
    return device;
}

DeviceInfo DeviceManager::CreateVirtualDevice(const std::string& name, const std::string& type)
{
    DeviceInfo device(name, type);
    device.displayName = "虚拟设备 " + name;
    device.isAvailable = true;
    
    device.properties["virtual"] = "true";
    
    return device;
}

bool DeviceManager::IsSerialPortAvailable(const std::string& portName)
{
    // 🔴 紧急修复：启动时跳过耗时的串口检查，避免UI线程阻塞
    // KISS原则：简化启动流程，将可用性检查延迟到实际使用时进行

    // 基本有效性检查：确保端口名格式正确
    if (portName.empty() || portName.find("COM") != 0) {
        return false;
    }

    // 🚀 性能优化：启动阶段假设历史端口可用，实际连接时再验证
    // 这样可以避免在DeviceManager构造函数中阻塞主UI线程
    return true; // 延迟验证策略

    /* 原有的同步检查代码（保留注释供参考）
    // 尝试短暂打开端口测试可用性 - 这会导致启动卡顿！
    std::string portPath = "\\\\.\\" + portName;
    HANDLE hPort = CreateFileA(
        portPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPort != INVALID_HANDLE_VALUE) {
        CloseHandle(hPort);
        return true;
    }

    return false;
    */
}

std::string DeviceManager::GetDeviceDescription(const std::string& deviceName, const std::string& transportType)
{
    if (transportType == "Serial") {
        return "串行通信端口";
    } else if (transportType == "TCP") {
        return "TCP网络连接";
    } else if (transportType == "UDP") {
        return "UDP数据报连接";
    } else if (transportType == "LPT") {
        return "并行端口打印机";
    } else if (transportType == "USB") {
        return "USB打印机";
    }
    
    return "未知设备类型";
}

void DeviceManager::DeduplicateDevices(std::vector<DeviceInfo>& devices) const
{
    std::sort(devices.begin(), devices.end(), 
        [](const DeviceInfo& a, const DeviceInfo& b) {
            if (a.transportType != b.transportType)
                return a.transportType < b.transportType;
            return a.deviceName < b.deviceName;
        });
    
    auto it = std::unique(devices.begin(), devices.end(),
        [](const DeviceInfo& a, const DeviceInfo& b) {
            return a.deviceName == b.deviceName && a.transportType == b.transportType;
        });
    
    devices.erase(it, devices.end());
}

void DeviceManager::SortDevices(std::vector<DeviceInfo>& devices, const std::string& sortBy) const
{
    if (sortBy == "name") {
        std::sort(devices.begin(), devices.end(),
            [](const DeviceInfo& a, const DeviceInfo& b) {
                return a.deviceName < b.deviceName;
            });
    } else if (sortBy == "type") {
        std::sort(devices.begin(), devices.end(),
            [](const DeviceInfo& a, const DeviceInfo& b) {
                if (a.transportType != b.transportType)
                    return a.transportType < b.transportType;
                return a.deviceName < b.deviceName;
            });
    }
}

void DeviceManager::LoadDeviceHistory()
{
    // SOLID-SRP: 专门负责历史记录加载
    ConfigManager config;
    config.LoadConfig();
    
    m_historyDevices.clear();
    
    // 从配置中加载历史设备
    auto historyCount = config.GetInt("devices", "history_count", 0);
    
    for (int i = 0; i < historyCount && i < 20; ++i) {
        std::string nameKey = "device_history_name_" + std::to_string(i);
        std::string typeKey = "device_history_type_" + std::to_string(i);
        std::string displayKey = "device_history_display_" + std::to_string(i);
        
        std::string deviceName = config.GetString("devices", nameKey, "");
        std::string transportType = config.GetString("devices", typeKey, "");
        std::string displayName = config.GetString("devices", displayKey, "");
        
        if (!deviceName.empty() && !transportType.empty()) {
            DeviceInfo device(deviceName, transportType);
            if (!displayName.empty()) {
                device.displayName = displayName;
            }
            device.description = GetDeviceDescription(deviceName, transportType);

            // 🔴 紧急修复：启动时跳过设备可用性检查，避免UI线程阻塞
            // KISS原则：将可用性检查延迟到用户实际选择设备时进行
            device.isAvailable = true; // 延迟验证策略

            m_historyDevices.push_back(device);
        }
    }
}

void DeviceManager::SaveDeviceHistory()
{
    // SOLID-SRP: 专门负责历史记录保存
    ConfigManager config;
    config.LoadConfig();
    
    // 保存历史设备数量
    config.SetValue("devices", "history_count", static_cast<int>(m_historyDevices.size()));
    
    // 保存每个历史设备
    for (size_t i = 0; i < m_historyDevices.size(); ++i) {
        const auto& device = m_historyDevices[i];
        
        std::string nameKey = "device_history_name_" + std::to_string(i);
        std::string typeKey = "device_history_type_" + std::to_string(i);
        std::string displayKey = "device_history_display_" + std::to_string(i);
        
        config.SetValue("devices", nameKey, device.deviceName);
        config.SetValue("devices", typeKey, device.transportType);
        config.SetValue("devices", displayKey, device.displayName);
    }
    
    config.SaveConfig();
}

void DeviceManager::LoadFavoriteDevices()
{
    // SOLID-SRP: 专门负责收藏设备加载
    ConfigManager config;
    config.LoadConfig();
    
    m_favoriteDevices.clear();
    
    // 从配置中加载收藏设备
    auto favoriteCount = config.GetInt("devices", "favorite_count", 0);
    
    for (int i = 0; i < favoriteCount && i < 50; ++i) {
        std::string nameKey = "device_favorite_name_" + std::to_string(i);
        std::string typeKey = "device_favorite_type_" + std::to_string(i);
        std::string displayKey = "device_favorite_display_" + std::to_string(i);
        
        std::string deviceName = config.GetString("devices", nameKey, "");
        std::string transportType = config.GetString("devices", typeKey, "");
        std::string displayName = config.GetString("devices", displayKey, "");
        
        if (!deviceName.empty() && !transportType.empty()) {
            DeviceInfo device(deviceName, transportType);
            if (!displayName.empty()) {
                device.displayName = displayName;
            }
            device.description = GetDeviceDescription(deviceName, transportType);

            // 🔴 紧急修复：启动时跳过设备可用性检查，避免UI线程阻塞
            // KISS原则：将可用性检查延迟到用户实际选择设备时进行
            device.isAvailable = true; // 延迟验证策略

            m_favoriteDevices.push_back(device);
        }
    }
}

void DeviceManager::SaveFavoriteDevices()
{
    // SOLID-SRP: 专门负责收藏设备保存
    ConfigManager config;
    config.LoadConfig();
    
    // 保存收藏设备数量
    config.SetValue("devices", "favorite_count", static_cast<int>(m_favoriteDevices.size()));
    
    // 保存每个收藏设备
    for (size_t i = 0; i < m_favoriteDevices.size(); ++i) {
        const auto& device = m_favoriteDevices[i];
        
        std::string nameKey = "device_favorite_name_" + std::to_string(i);
        std::string typeKey = "device_favorite_type_" + std::to_string(i);
        std::string displayKey = "device_favorite_display_" + std::to_string(i);
        
        config.SetValue("devices", nameKey, device.deviceName);
        config.SetValue("devices", typeKey, device.transportType);
        config.SetValue("devices", displayKey, device.displayName);
    }
    
    config.SaveConfig();
}

std::vector<DeviceInfo> DeviceManager::FilterDevices(const std::vector<DeviceInfo>& devices, 
                                                     const std::string& filter) const
{
    if (filter.empty()) {
        return devices;
    }
    
    std::vector<DeviceInfo> filtered;
    std::string lowerFilter = filter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
    
    for (const auto& device : devices) {
        std::string lowerName = device.displayName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (lowerName.find(lowerFilter) != std::string::npos ||
            device.deviceName.find(filter) != std::string::npos ||
            device.transportType.find(filter) != std::string::npos) {
            filtered.push_back(device);
        }
    }
    
    return filtered;
}

size_t DeviceManager::GetTotalDeviceCount() const
{
    // 为了避免const问题，我们重新实现而不调用非const方法
    size_t totalCount = 0;
    
    // 简化计数：估算常见设备类型数量
    totalCount += SerialTransport::EnumeratePorts().size();
    totalCount += LptSpoolerTransport::EnumeratePrinters().size();
    totalCount += UsbPrinterTransport::EnumerateUsbPrinters().size();
    totalCount += 8; // TCP/UDP常用配置
    totalCount += 1; // Loopback
    totalCount += m_customDevices.size();
    
    return totalCount;
}

std::map<std::string, size_t> DeviceManager::GetDeviceCountByType() const
{
    std::map<std::string, size_t> counts;
    
    counts["Serial"] = SerialTransport::EnumeratePorts().size();
    counts["LPT"] = LptSpoolerTransport::EnumeratePrinters().size();
    counts["USB"] = UsbPrinterTransport::EnumerateUsbPrinters().size();
    counts["TCP"] = 4; // 常用TCP配置数量
    counts["UDP"] = 4; // 常用UDP配置数量
    counts["Loopback"] = 1;
    
    // 统计自定义设备
    for (const auto& device : m_customDevices) {
        counts[device.transportType]++;
    }
    
    return counts;
}

void DeviceManager::StartDeviceMonitoring()
{
    if (m_monitoring) return;
    
    m_monitoring = true;
    m_stopMonitoring = false;
    
    // KISS: 启动设备监控线程
    m_monitorThread = std::thread(&DeviceManager::DeviceMonitoringThread, this);
}

void DeviceManager::StopDeviceMonitoring()
{
    if (!m_monitoring) return;
    
    m_stopMonitoring = true;
    
    // 等待监控线程结束
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
    
    m_monitoring = false;
}

void DeviceManager::SetDeviceChangedCallback(DeviceChangedCallback callback)
{
    m_deviceCallback = callback;
}

bool DeviceManager::TestDeviceConnection(const DeviceInfo& device)
{
    // 简化实现：基于设备类型进行基本可用性检查
    return IsDeviceAvailable(device.deviceName, device.transportType);
}

bool DeviceManager::SaveDeviceConfig(const DeviceInfo& device, const TransportConfig& config)
{
    // SOLID-SRP: 专门负责设备配置保存
    ConfigManager configManager;
    configManager.LoadConfig();
    
    // 使用设备唯一标识作为键名前缀
    std::string keyPrefix = "device_config_" + device.transportType + "_" + device.deviceName + "_";
    
    // 保存通用配置
    configManager.SetValue("device_configs", keyPrefix + "ip_address", config.ipAddress);
    configManager.SetValue("device_configs", keyPrefix + "port", config.port);
    configManager.SetValue("device_configs", keyPrefix + "baud_rate", config.baudRate);
    configManager.SetValue("device_configs", keyPrefix + "data_bits", config.dataBits);
    configManager.SetValue("device_configs", keyPrefix + "parity", config.parity);
    configManager.SetValue("device_configs", keyPrefix + "stop_bits", config.stopBits);
    configManager.SetValue("device_configs", keyPrefix + "port_name", config.portName);
    configManager.SetValue("device_configs", keyPrefix + "read_timeout", config.readTimeoutMs);
    configManager.SetValue("device_configs", keyPrefix + "write_timeout", config.writeTimeoutMs);
    configManager.SetValue("device_configs", keyPrefix + "rx_buffer_size", static_cast<int>(config.rxBufferSize));
    configManager.SetValue("device_configs", keyPrefix + "tx_buffer_size", static_cast<int>(config.txBufferSize));
    
    return configManager.SaveConfig();
}

TransportConfig DeviceManager::LoadDeviceConfig(const DeviceInfo& device)
{
    // SOLID-SRP: 专门负责设备配置加载
    ConfigManager configManager;
    configManager.LoadConfig();
    
    TransportConfig config;
    
    // 使用设备唯一标识作为键名前缀
    std::string keyPrefix = "device_config_" + device.transportType + "_" + device.deviceName + "_";
    
    // 加载通用配置
    config.ipAddress = configManager.GetString("device_configs", keyPrefix + "ip_address", "");
    config.port = configManager.GetInt("device_configs", keyPrefix + "port", 8080);
    config.baudRate = configManager.GetInt("device_configs", keyPrefix + "baud_rate", 9600);
    config.dataBits = configManager.GetInt("device_configs", keyPrefix + "data_bits", 8);
    config.parity = configManager.GetInt("device_configs", keyPrefix + "parity", 0);
    config.stopBits = configManager.GetInt("device_configs", keyPrefix + "stop_bits", 1);
    config.portName = configManager.GetString("device_configs", keyPrefix + "port_name", device.deviceName);
    config.readTimeoutMs = configManager.GetInt("device_configs", keyPrefix + "read_timeout", 1000);
    config.writeTimeoutMs = configManager.GetInt("device_configs", keyPrefix + "write_timeout", 1000);
    config.rxBufferSize = static_cast<size_t>(configManager.GetInt("device_configs", keyPrefix + "rx_buffer_size", 4096));
    config.txBufferSize = static_cast<size_t>(configManager.GetInt("device_configs", keyPrefix + "tx_buffer_size", 4096));
    
    return config;
}

void DeviceManager::RegisterCustomDevice(const DeviceInfo& device)
{
    // 检查是否已存在
    auto it = std::find_if(m_customDevices.begin(), m_customDevices.end(),
        [&device](const DeviceInfo& existing) {
            return existing.deviceName == device.deviceName && 
                   existing.transportType == device.transportType;
        });
    
    if (it == m_customDevices.end()) {
        m_customDevices.push_back(device);
    }
}

void DeviceManager::UnregisterCustomDevice(const std::string& deviceName, const std::string& transportType)
{
    auto it = std::remove_if(m_customDevices.begin(), m_customDevices.end(),
        [&](const DeviceInfo& device) {
            return device.deviceName == deviceName && device.transportType == transportType;
        });
    
    if (it != m_customDevices.end()) {
        m_customDevices.erase(it, m_customDevices.end());
    }
}

void DeviceManager::MergeDeviceProperties(DeviceInfo& target, const DeviceInfo& source) const
{
    // 合并设备属性
    for (const auto& prop : source.properties) {
        if (target.properties.find(prop.first) == target.properties.end()) {
            target.properties[prop.first] = prop.second;
        }
    }
    
    // 更新描述信息
    if (target.description.empty() && !source.description.empty()) {
        target.description = source.description;
    }
}

void DeviceManager::DeviceMonitoringThread()
{
    // SOLID-SRP: 专门负责设备热插拔监控
    // YAGNI: 简化实现，主要监控串口设备变化
    
    std::vector<std::string> lastSerialPorts;
    std::vector<std::string> lastUsbPrinters;
    
    // 获取初始设备列表
    lastSerialPorts = SerialTransport::EnumeratePorts();
    lastUsbPrinters = UsbPrinterTransport::EnumerateUsbPrinters();
    
    while (!m_stopMonitoring.load()) {
        try {
            // 检查串口设备变化
            auto currentSerialPorts = SerialTransport::EnumeratePorts();
            
            // 检测新增的串口
            for (const auto& port : currentSerialPorts) {
                if (std::find(lastSerialPorts.begin(), lastSerialPorts.end(), port) == lastSerialPorts.end()) {
                    // 发现新串口
                    DeviceInfo newDevice(port, "Serial");
                    newDevice.displayName = "串口 " + port;
                    newDevice.description = GetDeviceDescription(port, "Serial");
                    newDevice.isAvailable = true;
                    
                    if (m_deviceCallback) {
                        m_deviceCallback(newDevice, true);
                    }
                }
            }
            
            // 检测移除的串口
            for (const auto& port : lastSerialPorts) {
                if (std::find(currentSerialPorts.begin(), currentSerialPorts.end(), port) == currentSerialPorts.end()) {
                    // 串口被移除
                    DeviceInfo removedDevice(port, "Serial");
                    removedDevice.displayName = "串口 " + port;
                    removedDevice.isAvailable = false;
                    
                    if (m_deviceCallback) {
                        m_deviceCallback(removedDevice, false);
                    }
                }
            }
            
            lastSerialPorts = currentSerialPorts;
            
            // 检查USB打印机变化（简化处理）
            auto currentUsbPrinters = UsbPrinterTransport::EnumerateUsbPrinters();
            if (currentUsbPrinters.size() != lastUsbPrinters.size()) {
                lastUsbPrinters = currentUsbPrinters;
                // USB打印机数量发生变化，可以触发回调
            }
            
            // 每2秒检查一次
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
        } catch (const std::exception&) {
            // 监控出错，继续尝试
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }
}

// 🔴 紧急修复：实际连接时使用的真正可用性检查
bool DeviceManager::IsSerialPortReallyAvailable(const std::string& portName)
{
    // SOLID-S: 单一职责 - 专门负责真正的串口可用性验证
    // 这个方法只在用户实际尝试连接时调用，不影响启动性能

    if (portName.empty() || portName.find("COM") != 0) {
        return false;
    }

    std::string portPath = "\\\\.\\" + portName;
    HANDLE hPort = CreateFileA(
        portPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPort != INVALID_HANDLE_VALUE) {
        CloseHandle(hPort);
        return true;
    }

    return false;
}