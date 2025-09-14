#pragma execution_character_set("utf-8")
#include "pch.h"
#include "TransportFactory.h"

// =====================================================================================
// TransportFactory 实现 - 传输层工厂类
// =====================================================================================

std::shared_ptr<ITransport> TransportFactory::Create(TransportType transportType)
{
    switch (transportType)
    {
    case TransportType::SERIAL:
        return std::make_shared<SerialTransport>();
        
    case TransportType::LPT:
        return std::make_shared<LptSpoolerTransport>();
        
    case TransportType::USB_PRINTER:
        return std::make_shared<UsbPrinterTransport>();
        
    case TransportType::TCP_CLIENT:
    case TransportType::TCP_SERVER:
        return std::make_shared<TcpTransport>();
        
    case TransportType::UDP:
        return std::make_shared<UdpTransport>();
        
    case TransportType::LOOPBACK:
        return std::make_shared<LoopbackTransport>();
        
    default:
        return nullptr;
    }
}

std::shared_ptr<ITransport> TransportFactory::CreateByIndex(int transportIndex)
{
    if (!IsValidTransportType(transportIndex))
        return nullptr;
        
    return Create(static_cast<TransportType>(transportIndex));
}

const char* TransportFactory::GetTransportName(TransportType transportType)
{
    switch (transportType)
    {
    case TransportType::SERIAL:
        return "Serial";
    case TransportType::LPT:
        return "LPT";
    case TransportType::USB_PRINTER:
        return "USB Printer";
    case TransportType::TCP_CLIENT:
        return "TCP Client";
    case TransportType::TCP_SERVER:
        return "TCP Server";
    case TransportType::UDP:
        return "UDP";
    case TransportType::LOOPBACK:
        return "Loopback";
    default:
        return "Unknown";
    }
}

bool TransportFactory::IsValidTransportType(int transportIndex)
{
    return transportIndex >= 0 && 
           transportIndex <= static_cast<int>(TransportType::LOOPBACK);
}