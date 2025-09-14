#pragma execution_character_set("utf-8")
#pragma once

#include "../Transport/ITransport.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/LptSpoolerTransport.h"
#include "../Transport/UsbPrinterTransport.h"
#include "../Transport/TcpTransport.h"
#include "../Transport/UdpTransport.h"
#include "../Transport/LoopbackTransport.h"
#include <memory>
#include <string>

// 传输类型枚举
enum class TransportType : int
{
    SERIAL = 0,
    LPT = 1,
    USB_PRINTER = 2,
    TCP_CLIENT = 3,
    TCP_SERVER = 4,
    UDP = 5,
    LOOPBACK = 6
};

/**
 * 传输层工厂类 - 专职负责传输对象创建
 * SOLID-S: 单一职责 - 专注传输对象创建
 * SOLID-O: 开闭原则 - 可扩展新传输类型
 */
class TransportFactory
{
public:
    /**
     * 根据传输类型创建传输对象
     */
    static std::shared_ptr<ITransport> Create(TransportType transportType);
    
    /**
     * 根据索引创建传输对象（兼容现有代码）
     */
    static std::shared_ptr<ITransport> CreateByIndex(int transportIndex);
    
    /**
     * 获取传输类型显示名称
     */
    static const char* GetTransportName(TransportType transportType);
    
    /**
     * 验证传输类型有效性
     */
    static bool IsValidTransportType(int transportIndex);

private:
    TransportFactory() = delete;
    ~TransportFactory() = delete;
};