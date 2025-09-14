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

// Transport type enumeration
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
 * Transport Factory Class - Responsible for creating transport objects
 * SOLID-S: Single Responsibility - Focus on transport object creation
 * SOLID-O: Open/Closed Principle - Extensible for new transport types
 */
class TransportFactory
{
public:
    /**
     * Create transport object by transport type
     */
    static std::shared_ptr<ITransport> Create(TransportType transportType);
    
    /**
     * Create transport object by index (compatible with existing code)
     */
    static std::shared_ptr<ITransport> CreateByIndex(int transportIndex);
    
    /**
     * Get transport type display name
     */
    static const char* GetTransportName(TransportType transportType);
    
    /**
     * Validate transport type validity
     */
    static bool IsValidTransportType(int transportIndex);

private:
    TransportFactory() = delete;
    ~TransportFactory() = delete;
};